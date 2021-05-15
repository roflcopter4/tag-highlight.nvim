#include "Common.h"

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "nvim_api/wait_node.h"

P99_FIFO(nvim_wait_node_ptr) nvim_wait_queue;

typedef volatile p99_futex vfutex_t;

struct gencall {
        const bstring *fn;
        mpack_obj     *pack;
        int            count;
};

extern _Atomic(mpack_obj *) event_loop_mpack_obj;
extern vfutex_t             event_loop_futex, nvim_wait_futex;
static pthread_mutex_t      await_package_mutex = PTHREAD_MUTEX_INITIALIZER;
       vfutex_t             nvim_wait_futex    = P99_FUTEX_INITIALIZER(0);

static mpack_obj *make_call(bool blocking, const bstring *fn, mpack_obj *pack,  int count);
static noreturn void *make_async_call(void *arg);
static mpack_obj *write_and_clean(mpack_obj *pack, int count, const bstring *func, FILE *logfp);

#define write_and_clean(...) P99_CALL_DEFARG(write_and_clean, 4, __VA_ARGS__)
#define write_and_clean_defarg_3() (NULL)

static          mpack_obj *await_package  (nvim_wait_node *node) __aWUR;
static noreturn void      *discard_package(void *arg);

void *nvim_common_talloc_ctx_ = NULL;
#define CTX nvim_common_talloc_ctx_

/*======================================================================================*/

static mpack_obj *
make_call(const bool blocking, const bstring *fn, mpack_obj *pack, const int count)
{
        mpack_obj *result;

        if (blocking) {
                result = write_and_clean(pack, count, fn);
        } else {
                struct gencall *gc = malloc(sizeof *gc);
                /* *gc    = (struct gencall){fn, pack, count}; */
                gc->fn = fn;
                gc->pack = pack;
                gc->count = count;
                result = NULL;
                START_DETACHED_PTHREAD(make_async_call, gc);
        }

        return result;
}

static noreturn void *
make_async_call(void *arg)
{
        struct gencall *gc  = arg;
        mpack_obj      *ret = write_and_clean(gc->pack, gc->count, gc->fn);
        talloc_free(ret);
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
        p99_futex_init((p99_futex *)&nvim_wait_futex, 0u);
}

/**
 * If I tried to explain how many hours and attempts it took to write what
 * became this function I would probably be locked in a mental instatution. For
 * reference, at one point this funcion was over 100 lines long.
 */
static mpack_obj *
await_package(nvim_wait_node *node)
{
#if 0
        /* mpack_obj *obj; */
        /* p99_futex_wait(&_nvim_wait_futex); */
        /* obj = atomic_load(&event_loop_mpack_obj); */
        p99_futex_wait(&node->fut);
        /* mpack_obj *ret = atomic_load_explicit(&node->obj, memory_order_relaxed); */
        /* mpack_obj *ret = atomic_load_explicit(&node->obj, memory_order_seq_cst); */
        mpack_obj *ret = atomic_load(&node->obj);
#endif

        pthread_mutex_lock(&node->mtx);
        mpack_obj *obj = NULL;

        while (!obj) {
                if (pthread_cond_wait(&node->cond, &node->mtx))
                        err(1, "pthread_cond_wait");

                obj = atomic_load(&node->obj);
        }

        if (!obj)
                errx(1, "null object");

        /* p99_futex_wakeup(&event_loop_futex, 1u, 1u); */

        return obj;
}

static noreturn void *
discard_package(void *arg)
{
        nvim_wait_node *node = arg;

        /* mpack_obj *obj; */
        /* p99_futex_wait(&_nvim_wait_futex); */
        /* obj = atomic_load(&event_loop_mpack_obj); */

        /* p99_futex_wait(&node->fut); */
        pthread_mutex_lock(&node->mtx);
        mpack_obj *obj = NULL;

        while (!obj) {
                if (pthread_cond_wait(&node->cond, &node->mtx))
                        err(1, "pthread_cond_wait");

                obj = atomic_load(&node->obj);
        }

        if (!node->obj)
                errx(1, "null object");

        talloc_free(obj);
        free(node);
        pthread_exit();

        /* p99_futex_wakeup(&event_loop_futex, 1u, 1u); */
        /* return node->obj; */
}

static mpack_obj *
(write_and_clean)(mpack_obj *pack, const int count, const bstring *func, FILE *logfp)
{
#if 0 && defined DEBUG
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
        nvim_wait_node *node;

        b_write(1, *pack->packed);

        node        = calloc(1, sizeof(*node));
        node->fd    = 1;
        node->count = count;

        pthread_mutex_init(&node->mtx, NULL);
        pthread_cond_init(&node->cond, NULL);

        p99_futex_init(&node->fut, 0);
        P99_FIFO_APPEND(&nvim_wait_queue, node);
        /* mpack_destroy_object(pack); */
        //b_free(*pack->packed);
        talloc_free(pack);

#if 0
        if (blocking) {
                ret = await_package(node);
                free(node);
        } else {
                ret = NULL;
                START_DETACHED_PTHREAD(discard_package, node);
        }
#endif

        ret = await_package(node);
        free(node);
        return ret;
}

/*======================================================================================*/

mpack_retval
m_expect_intern(mpack_obj *root, mpack_expect_t type)
{
        mpack_obj *errmsg = mpack_index(root, 2);
        mpack_obj *data   = mpack_index(root, 3);
        mpack_retval ret  = { .ptr = NULL };

        if (mpack_type(errmsg) != MPACK_NIL) {
                bstring *err_str = mpack_expect(mpack_index(errmsg, 1), E_STRING, false).ptr;
                if (err_str) {
                        echo("Neovim returned with an err_str: '%s'", BS(err_str));
                        b_destroy(err_str);
                }
        } else {
                ret = mpack_expect(data, type, false);
                switch (type) {
                case E_STRING:
                        talloc_set_name(ret.ptr, "nvim/common.c: m_expect_intern -> %s: (%s)",
                                        mpack_expect_t_getname(type),
                                        (char *)(((bstring *)ret.ptr)->data));
                case E_DICT2ARR:
                case E_MPACK_ARRAY:
                case E_MPACK_DICT:
                case E_MPACK_EXT:
                case E_STRLIST:
                case E_WSTRING:
                        talloc_steal(CTX, ret.ptr);
                        break;
                default:;
                }
        }

        //talloc_free(data);
        talloc_free(root);
        return ret;
}
