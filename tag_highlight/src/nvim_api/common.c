#include "tag_highlight.h"
#include <dirent.h>

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "p99/p99_fifo.h"
#include "p99/p99_futex.h"

#include "my_p99_common.h"
#include "read.h"

extern genlist  *_nvim_wait_list;
extern vfutex_t  _nvim_wait_futex;
       vfutex_t  _nvim_wait_futex = P99_FUTEX_INITIALIZER(0);
static vfutex_t  once_futex       = P99_FUTEX_INITIALIZER(0);
extern vfutex_t event_futex;

#if 0
P44_DECLARE_FIFO(nvim_wait_queue);
struct nvim_wait_queue {
        volatile p99_futex *volatile fut;
        mpack_obj        *obj;
        nvim_wait_queue *p99_lifo;
        unsigned    count;
};
extern P99_FIFO(nvim_wait_queue_ptr) nvim_wait_queue_head;
#endif

volatile p99_count _nvim_count;
P99_FIFO(mpack_obj_node_ptr) mpack_obj_queue;
P99_LIFO(mpack_obj_node_ptr) mpack_obj_stack;

/*======================================================================================*/

#if 0
static mpack_obj *
check_queue(const int fd, const int count)
{
        /* pthread_mutex_lock(&_nvim_wait_list->mut); */

        /* unsigned bla = 0;
        atomic_compare_exchange_strong(&once_futex, &bla, 1); */

        for (unsigned i = 0; i < _nvim_wait_list->qty; ++i) {
                struct nvim_wait *wt = _nvim_wait_list->lst[i];
                if (wt->fd == fd && (int)wt->count == count) {
                        mpack_obj *ret = wt->obj;
                        /* pthread_mutex_unlock(&_nvim_wait_list->mut); */
                        genlist_remove(_nvim_wait_list, wt);
                        P99_FUTEX_COMPARE_EXCHANGE(&_nvim_wait_futex, value,
                                /* Never lock, decrement value, wake nobody */
                                   true, (value - 1), 0, 0
                        );

                        p99_futex_wakeup(&event_futex, 1u, 1u);

                        P99_FUTEX_COMPARE_EXCHANGE(&once_futex, value,
                                true, 0, 0, P99_FUTEX_MAX_WAITERS);
                        return ret;
                }
        }

        /* pthread_mutex_unlock(&_nvim_wait_list->mut); */
        return NULL;
}
#endif

/**
 * If I tried to explain how many hours and attempts it took to write what
 * became this function I would probably be locked in a mental instatution. For
 * reference, at one point this funcion was over 100 lines long.
 */
mpack_obj *
await_package(const int fd, const unsigned count, UNUSED const nvim_message_type mtype)
{
#if 0
        mpack_obj *ret = NULL;

        while (!ret) {
                /* P99_FUTEX_COMPARE_EXCHANGE(&once_futex, value,
                        value == 0,  [>Unlock when the value is not 0<]
                        value, 0, 0  [>Don't change the value or wake anyone<]
                ); */

                P99_FUTEX_COMPARE_EXCHANGE(&_nvim_wait_futex, value,
                        value > 0,  /* Unlock when the value is not 0 */
                        value, 0, 0 /* Don't change the value or wake anyone */
                );

                ret = check_queue(fd, count);
        }

        return ret;
#endif
        /* *node = (nvim_wait_queue){&(volatile p99_futex){0}, NULL, NULL, count}; */

        static mtx_t await_mutex;
        mpack_obj   *obj = NULL;

        for (;;) {
                p99_count_inc(&_nvim_count);
                mtx_lock(&await_mutex);

                mpack_obj_node *node;
                while ((node = P99_FIFO_POP(&mpack_obj_queue))) {
                        if (count == node->count) {
                                obj = node->obj;
                                xfree(node);
                                break;
                        }
                        P99_LIFO_PUSH(&mpack_obj_stack, node);
                }

                while ((node = P99_LIFO_POP(&mpack_obj_stack)))
                        P99_FIFO_APPEND(&mpack_obj_queue, node);

                mtx_unlock(&await_mutex);
                p99_count_dec(&_nvim_count);
                if (obj)
                        break;
                p99_count_wait(&_nvim_count);
        }

        return obj;

#if 0
        vfutex_t         fut  = P99_FUTEX_INITIALIZER(0);
        nvim_wait_queue *node = &(nvim_wait_queue){&fut, NULL, NULL, NULL, count};

        P99_FIFO_APPEND(&nvim_wait_queue_head, node);
        /* p99_futex_wakeup(&event_futex, 1u, 1u); */
        
        P99_FUTEX_COMPARE_EXCHANGE(&event_futex, value,
                true, value + 1, 0, P99_FUTEX_MAX_WAITERS);

        p99_futex_wait(node->fut);

        return node->obj;
#endif
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
        mpack_destroy_object(pack);
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

        mpack_destroy_object(root);
        return ret;
}
