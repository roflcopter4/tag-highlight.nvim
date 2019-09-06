#include "Common.h"

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "nvim_api/wait_node.h"

typedef volatile p99_futex vfutex_t;

struct gencall {
        const bstring *fn;
        mpack_obj     *pack;
        int            count;
};

extern _Atomic(mpack_obj *) event_loop_mpack_obj;
extern vfutex_t             event_loop_futex, _nvim_wait_futex;
static pthread_mutex_t      await_package_mutex = PTHREAD_MUTEX_INITIALIZER;
       vfutex_t             _nvim_wait_futex    = P99_FUTEX_INITIALIZER(0);

static mpack_obj *make_call(const bool blocking, const bstring *fn, mpack_obj *pack, const int count);
static noreturn void *make_async_call(void *arg);
static mpack_obj *write_and_clean(mpack_obj *pack, int count, const bstring *func, bool blocking, FILE *logfp);

#define write_and_clean(...) P99_CALL_DEFARG(write_and_clean, 5, __VA_ARGS__)
#define write_and_clean_defarg_3() (true)
#define write_and_clean_defarg_4() (NULL)
#define write_and_clean_defarg_5() (mpack_log)

static          mpack_obj *await_package  (_nvim_wait_node *node) __aWUR;
static noreturn void      *discard_package(void *arg);

/*======================================================================================*/

static mpack_obj *
make_call(const bool blocking, const bstring *fn, mpack_obj *pack, const int count)
{
        mpack_obj *result;

        if (blocking) {
                result = write_and_clean(pack, count, fn, blocking);
        } else {
                struct gencall *gc = malloc(sizeof *gc);
                *gc    = (struct gencall){fn, pack, count};
                result = NULL;
                START_DETACHED_PTHREAD(make_async_call, gc);
        }

        return result;
}

static noreturn void *
make_async_call(void *arg)
{
        struct gencall *gc  = arg;
        mpack_obj      *ret = write_and_clean(gc->pack, gc->count, gc->fn, true);
        mpack_destroy_object(ret);
        free(gc);
        pthread_exit();
}

mpack_obj *
_nvim_api_generic_call(const bool blocking, const bstring *fn, const bstring *const fmt, ...)
{
        mpack_obj *pack;
        const int  count = INC_COUNT();

        if (fmt) {
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

        return make_call(blocking, fn, pack, count);
}

mpack_obj *
_nvim_api_special_call(const bool blocking, const bstring *fn, mpack_obj *pack, const int count)
{
        return make_call(blocking, fn, pack, count);
}

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
static mpack_obj *
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

static noreturn void *
discard_package(void *arg)
{
        _nvim_wait_node *node = arg;

        /* mpack_obj *obj; */
        /* p99_futex_wait(&_nvim_wait_futex); */
        /* obj = atomic_load(&event_loop_mpack_obj); */
        p99_futex_wait(&node->fut);

        if (!node->obj)
                errx(1, "null object");

        mpack_destroy_object(node->obj);
        free(node);
        pthread_exit();

        /* p99_futex_wakeup(&event_loop_futex, 1u, 1u); */
        /* return node->obj; */
}

static mpack_obj *
(write_and_clean)(mpack_obj *pack, const int count, const bstring *func, const bool blocking, FILE *logfp)
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
                        "Writing request no %d: \"%s\"\n", 0, BS(func));

        mpack_print_object(logfp, pack);
#endif

        mpack_obj       *ret;
        _nvim_wait_node *node = calloc(1, sizeof(*node));
        node->fd              = 1;
        node->count           = count;

        p99_futex_init(&node->fut, 0);
        P99_FIFO_APPEND(&_nvim_wait_queue, node);
        b_write(1, *pack->packed);
        mpack_destroy_object(pack);

        if (blocking) {
                ret = await_package(node);
                free(node);
        } else {
                ret = NULL;
                START_DETACHED_PTHREAD(discard_package, node);
        }

        return ret;
}

/*======================================================================================*/

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
