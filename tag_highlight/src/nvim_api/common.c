#include "util/util.h"
#include <dirent.h>

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "p99/p99_futex.h"

#define PTHREAD_MUTEX_ACTION(MUT, ...)     \
        do {                               \
                pthread_mutex_lock(MUT);   \
                __VA_ARGS__                \
                pthread_mutex_unlock(MUT); \
        } while (0)

pthread_mutex_t mpack_main_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t api_mutex        = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_list_mutex  = PTHREAD_MUTEX_INITIALIZER;
extern genlist *wait_list;
extern pthread_cond_t event_loop_cond;
extern volatile p99_futex event_loop_futex;
extern pthread_rwlock_t event_loop_rwlock;

/*======================================================================================*/

mpack_obj *
await_package(const int fd, const int count, const enum message_types mtype)
{
        struct nvim_wait *wt   = xmalloc(sizeof(struct nvim_wait));
        *wt                    = (struct nvim_wait){mtype, (int16_t)fd, (int32_t)count,
                                                    P99_FUTEX_INITIALIZER(0), NULL};
        genlist_append(wait_list, wt);

        /* pthread_cond_signal(&event_loop_cond); */
        /* pthread_mutex_lock(&api_mutex);
        pthread_cond_wait(&wt->cond, &api_mutex); */

        /* p99_futex_wakeup(&event_loop_futex, 1u, 1u);
        p99_futex_wait(&wt->fut); */

        p99_futex_wakeup(&event_loop_futex, 1u, 1u);

        const unsigned expected = NVIM_GET_FUTEX_EXPECT(fd, count);

        /* eprintf("Waiting for 0x%08X in await_package\n", expected); */
        P99_FUTEX_COMPARE_EXCHANGE(&wt->fut,
                                   val,
                                   val == expected,
                                   val,
                                   0u, 0u);
        /* p99_futex_wait(&wt->fut); */

        if (!wt->obj)
                errx(1, "Thread %u signalled with invalid object: aborting.",
                     (unsigned)pthread_self());

        const enum message_types mestype = m_expect(m_index(wt->obj, 0), E_NUM, false).num;

        if (mestype != MES_RESPONSE)
                errx(1, "Thread %u signalled object of incorrect type (%s vs %s): aborting.",
                     (unsigned)pthread_self(), m_message_type_repr[mestype],
                     m_message_type_repr[MES_RESPONSE]);

        mpack_obj *ret = wt->obj;

        genlist_remove(wait_list, wt);
        /* pthread_mutex_unlock(&api_mutex); */
        return ret;
}

mpack_obj *
generic_call(int *fd, const bstring *fn, const bstring *fmt, ...)
{
        pthread_mutex_lock(&mpack_main_mutex);
        CHECK_DEF_FD(*fd);
        mpack_obj *pack;
        const int  count = INC_COUNT(*fd);

        if (fmt) {
                va_list        ap;
                const unsigned size = fmt->slen + 16u;
                char *         buf  = alloca(size);
                snprintf(buf, size, "[d,d,s:[!%s]]", BS(fmt));

                va_start(ap, fmt);
                pack = mpack_encode_fmt(0, buf, MES_REQUEST, count, fn, &ap);
                va_end(ap);
        } else {
                pack = mpack_encode_fmt(0, "[d,d,s:[]]", MES_REQUEST, count, fn);
        }

        write_and_clean(*fd, pack, fn);

        /* mpack_obj *result = decode_stream(*fd, MES_RESPONSE); */
        mpack_obj *result = await_package(*fd, count, MES_RESPONSE);
        mpack_print_object(mpack_log, result);
        pthread_mutex_unlock(&mpack_main_mutex);
        return result;
}

/*======================================================================================*/

bstring *
get_notification(int fd)
{
        /* No mutex to lock here. */
        CHECK_DEF_FD(fd);
        /* mpack_obj *result = decode_stream(fd, MES_NOTIFICATION); */
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
