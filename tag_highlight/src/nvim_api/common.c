#include "tag_highlight.h"
#include <dirent.h>

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "p99/p99_cm.h"
#include "p99/p99_futex.h"

extern          genlist         *_nvim_wait_list;
extern volatile p99_futex        _nvim_wait_futex;
       volatile p99_futex        _nvim_wait_futex = P99_FUTEX_INITIALIZER(0);

/*======================================================================================*/

static mpack_obj *
check_queue(const int fd, const int count)
{
        pthread_rwlock_rdlock(&_nvim_wait_list->lock);

        for (unsigned i = 0; i < _nvim_wait_list->qty; ++i) {
                struct nvim_wait *wt = _nvim_wait_list->lst[i];
                if (wt->fd == fd && (int)wt->count == count) {
                        mpack_obj *ret = wt->obj;
                        pthread_rwlock_unlock(&_nvim_wait_list->lock);
                        genlist_remove(_nvim_wait_list, wt);
                        P99_FUTEX_COMPARE_EXCHANGE(&_nvim_wait_futex, value,
                                /* Never lock, decrement value, wake nobody */
                                   true, (value - 1), 0, 0
                        );

                        /* extern pthread_cond_t event_cond; */
                        /* pthread_cond_signal(&event_cond); */
                        extern volatile p99_futex event_futex;
                        p99_futex_wakeup(&event_futex, 1u, 1u);
                        return ret;
                }
        }

        pthread_rwlock_unlock(&_nvim_wait_list->lock);
        return NULL;
}

/**
 * If I tried to explain how many hours and attempts it took to write what
 * became this function I would probably be locked in a mental instatution. For
 * reference, at one point this funcion was over 100 lines long.
 */
mpack_obj *
await_package(const int fd, const int count, UNUSED const nvim_message_type mtype)
{
        mpack_obj *ret = NULL;

        while (!ret) {
                P99_FUTEX_COMPARE_EXCHANGE(&_nvim_wait_futex, value,
                        value > 0,  /* Unlock when the value is not 0 */
                        value, 0, 0 /* Don't change the value or wake anyone */
                );

                ret = check_queue(fd, count);
        }

        return ret;
}

mpack_obj *
generic_call(int *fd, const bstring *fn, const bstring *fmt, ...)
{
        CHECK_DEF_FD(*fd);
        mpack_obj *pack;
        const int  count = INC_COUNT(*fd);

        if (fmt) {
                /* Skrew it, despite early efforts to the contrary, this will never
                 * compile with anything but gcc/clang. No reason not to use VLAs. */
                const unsigned size = fmt->slen + 16u;
                va_list        ap;
                char           buf[size];
                snprintf(buf, size, "[d,d,s:[!%s]]", BS(fmt));

                va_start(ap, fmt);
                pack = mpack_encode_fmt(0, buf, MES_REQUEST, count, fn, &ap);
                va_end(ap);
        } else {
                pack = mpack_encode_fmt(0, "[d,d,s:[]]", MES_REQUEST, count, fn);
        }

        write_and_clean(*fd, pack, fn);
        mpack_obj *result = await_package(*fd, count, MES_RESPONSE);
        mpack_print_object(mpack_log, result);
        return result;
}

/*======================================================================================*/

bstring *
get_notification(int fd)
{
        CHECK_DEF_FD(fd);
        mpack_obj *result = await_package(fd, (-1), MES_NOTIFICATION);
        bstring   *ret    = b_strcpy(result->DAI[1]->data.str);
        PRINT_AND_DESTROY(result);
        return ret;
}

void
(write_and_clean)(const int fd, mpack_obj *pack, const bstring *func, FILE *logfp)
{
#ifdef DEBUG
#  ifdef LOG_RAW_MPACK
        char tmp[512]; snprintf(tmp, 512, "%s/rawmpack.log", HOME);
        const int rawlog = safe_open(tmp, O_CREAT|O_APPEND|O_WRONLY|O_DSYNC|O_BINARY, 0644);
        b_write(rawlog, B("\n"), *pack->packed, B("\n"));
        close(rawlog);
#  endif
        if (func && logfp)
                fprintf(logfp, "=================================\n"
                        "Writing request no %d to fd %d: \"%s\"\n",
                        COUNT(fd) - 1, fd, BS(func));

        mpack_print_object(logfp, pack);
#endif
        b_write(fd, *pack->packed);
        mpack_destroy(pack);
}

retval_t
m_expect_intern(mpack_obj *root, mpack_expect_t type)
{
        mpack_obj *errmsg = m_index(root, 2);
        mpack_obj *data   = m_index(root, 3);
        retval_t   ret    = { .ptr = NULL };

        if (mpack_type(errmsg) != MPACK_NIL) {
                bstring *err_str = m_expect(m_index(errmsg, 1), E_STRING, true).ptr;
                if (err_str) {
                        warnx("Neovim returned with an err_str: '%s'", BS(err_str));
                        b_destroy(err_str);
                        root->DAI[2] = NULL;
                }
        } else {
                ret = m_expect(data, type, true);
                root->DAI[3] = NULL;
        }

        mpack_destroy(root);
        return ret;
}
