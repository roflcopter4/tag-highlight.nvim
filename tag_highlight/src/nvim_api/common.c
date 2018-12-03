#include "Common.h"

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "contrib/p99/p99_fifo.h"
#include "contrib/p99/p99_futex.h"

#include "my_p99_common.h"

typedef volatile p99_futex vfutex_t;

extern _Atomic(mpack_obj *) event_loop_mpack_obj;
extern vfutex_t             event_loop_futex, _nvim_wait_futex;
static pthread_mutex_t      await_package_mutex = PTHREAD_MUTEX_INITIALIZER;
       vfutex_t             _nvim_wait_futex    = P99_FUTEX_INITIALIZER(0);

/*======================================================================================*/

__attribute__((__constructor__)) static void
_nvim_api_wrapper_init(void)
{
        pthread_mutex_init(&await_package_mutex);
        p99_futex_init((p99_futex *)&event_loop_futex, 0u);
        p99_futex_init((p99_futex *)&_nvim_wait_futex, 0u);
}

/**
 * If I tried to explain how many hours and attempts it took to write what
 * became this function I would probably be locked in a mental instatution. For
 * reference, at one point this funcion was over 100 lines long.
 */
mpack_obj *
await_package(_nvim_wait_node *node)
{
        /* mpack_obj *obj; */
        /* p99_futex_wait(&_nvim_wait_futex); */
        /* obj = atomic_load(&event_loop_mpack_obj); */
        p99_futex_wait(&node->fut);

        if (!node->obj)
                errx(1, "null object");

        /* p99_futex_wakeup(&event_loop_futex, 1u, 1u); */

        return node->obj;
}

mpack_obj *
generic_call(int *fd, const bstring *fn, const bstring *const fmt, ...)
{
        CHECK_DEF_FD(*fd);
        mpack_obj *pack;
        const int  count = INC_COUNT(*fd);

        if (fmt) {
                /* Skrew it, despite early efforts to the contrary, this will never
                 * compile with anything but gcc/clang. No reason not to use VLAs. */
                const unsigned size = fmt->slen + 16U;
                va_list        ap;
                char           buf[size];
                snprintf(buf, size, "[d,d,s:[!%s]]", BS(fmt));

                va_start(ap, fmt);
                pack = mpack_encode_fmt(0, buf, MES_REQUEST, count, fn, &ap);
                va_end(ap);
        } else {
                pack = mpack_encode_fmt(0, "[d,d,s:[]]", MES_REQUEST, count, fn);
        }

        mpack_obj *result = write_and_clean(*fd, pack, count, fn);
        mpack_print_object(mpack_log, result);
        return result;
}

/*======================================================================================*/

mpack_obj *
(write_and_clean)(const int fd, mpack_obj *pack, const int count, const bstring *func, FILE *logfp)
{
#ifdef DEBUG
#  ifdef LOG_RAW_MPACK
        {
        extern char LOGDIR[];
        eprintf("Writing to log... %zu\n", (size_t)(*pack->packed)->slen);
        char tmp[512]; snprintf(tmp, 512, "%s/rawmpack.log", LOGDIR);
        FILE *fp = fopen(tmp, "wb");
        if (!fp)
                THROW("Well, we lose");
        size_t n = fwrite((*pack->packed)->data, 1, (*pack->packed)->slen, fp);
        if (n <= 0 || ferror(fp))
                errx(1, "All is lost");
        fflush(fp); fclose(fp);
        fprintf(stderr, "Wrote %zu chars\n", n); fflush(stderr);
        assert((unsigned)n == (*pack->packed)->slen);
        }
#  endif
        if (func && logfp)
                fprintf(logfp, "=================================\n"
                        "Writing request no %d to fd %d: \"%s\"\n",
                        COUNT(fd) - 1, fd, BS(func));

        mpack_print_object(logfp, pack);
#endif

        _nvim_wait_node *node = xcalloc(1, sizeof(*node));
        node->fd    = fd;
        node->count = count;
        p99_futex_init(&node->fut, 0);
        /* node->fut   = (p99_futex)P99_FUTEX_INITIALIZER(0); */
        /* p99_futex_init(&node->fut, 0); */
        P99_FIFO_APPEND(&_nvim_wait_queue, node);

        /* pthread_mutex_lock(&await_package_mutex); */
        b_write(fd, *pack->packed);
        /* pthread_mutex_unlock(&await_package_mutex); */

        mpack_destroy_object(pack);
        mpack_obj *ret = await_package(node);
        xfree(node);
        return ret;
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
                ret          = m_expect(data, type, true);
                root->DAI[3] = NULL;
        }

        mpack_destroy_object(root);
        return ret;
}
