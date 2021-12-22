#include "Common.h"

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "nvim_api/wait_node.h"

P99_FIFO(nvim_wait_node_ptr) nvim_wait_queue;

typedef volatile p99_futex vfutex_t;
typedef unsigned char      byte;

struct gencall {
    //alignas(32)
      int count;
    //alignas(16)
      mpack_obj     *pack;
      bstring const *fn;
};

extern _Atomic(mpack_obj *) event_loop_mpack_obj;
extern vfutex_t             event_loop_futex, nvim_wait_futex;
static pthread_mutex_t      await_package_mutex = PTHREAD_MUTEX_INITIALIZER;
vfutex_t                    nvim_wait_futex     = P99_FUTEX_INITIALIZER(0);

static noreturn void *make_async_call(void *arg);
static mpack_obj *make_call(bool blocking, bstring const *fn, mpack_obj *pack, int count);
static mpack_obj *write_and_clean(mpack_obj *pack, int count, bstring const *func);
static mpack_obj *await_package(nvim_wait_node *node) __aWUR;

void *nvim_common_talloc_ctx_ = NULL;
#define CTX nvim_common_talloc_ctx_

/*======================================================================================*/

static mpack_obj *
make_call(bool const blocking, bstring const *fn, mpack_obj *pack, int const count)
{
      mpack_obj *result;

      if (blocking) {
            result = write_and_clean(pack, count, fn);
      } else {
            struct gencall *gc = malloc(sizeof(struct gencall));
            gc->count          = count;
            gc->pack           = pack;
            gc->fn             = fn;
            result             = NULL;
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
nvim_api_intern_make_generic_call(bool    const        blocking,
                                  bstring const *      fn,
                                  bstring const *const fmt,
                                  ...)
{
      mpack_obj *pack;
      int const  count = INC_COUNT();

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
nvim_api_intern_make_special_call(bool const     blocking,
                                  bstring const *fn,
                                  mpack_obj     *pack,
                                  int const      count)
{
      return make_call(blocking, fn, pack, count);
}

/*======================================================================================*/

__attribute__((__constructor__(200))) static void
nvim_api_wrapper_init(void)
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
      /* Now we wait for it to deposit the package, and load it. */
      p99_futex_wait(&node->fut);
      mpack_obj *obj = atomic_load_explicit(&node->obj, memory_order_seq_cst);

      if (!obj)
            errx(1, "Received NULL package from Neovim");
      return obj;
}

static mpack_obj *
write_and_clean(mpack_obj *pack, int const count, UNUSED bstring const *func)
{
#if defined DEBUG
      if (mpack_raw_write) {
            size_t n = fwrite((*pack->packed)->data, 1, (*pack->packed)->slen, mpack_raw_write);
            assert(!ferror(mpack_raw_write) && (unsigned)n == (*pack->packed)->slen);
      }
      mpack_print_object(mpack_log, pack, B("\033[1;34mSENDING MESSAGE\033[0m"));
#endif

      mpack_obj      *ret;
      nvim_wait_node *node;

      b_write(1, *pack->packed);

      node        = calloc(1, sizeof(*node));
      node->fd    = 1;
      node->count = count;

      p99_futex_init(&node->fut, 0);
      P99_FIFO_APPEND(&nvim_wait_queue, node);

      ret = await_package(node);

      talloc_free(pack);
      free(node);
      return ret;
}

/*======================================================================================*/

mpack_retval
nvim_api_intern_mpack_expect_wrapper(mpack_obj *root, mpack_expect_t type)
{
      mpack_obj   *errmsg = mpack_index(root, 2);
      mpack_obj   *data   = mpack_index(root, 3);
      mpack_retval ret    = {.ptr = NULL};

      if (mpack_type(errmsg) != MPACK_NIL) {
            bstring *err_str = mpack_expect(mpack_index(errmsg, 1), E_STRING, false).ptr;
            if (err_str)
                  echo("Neovim returned with an err_str: '%s'", BS(err_str));
      } else {
            ret = mpack_expect(data, type, false);
            switch (type) {
            case E_STRING:
            case E_DICT2ARR:
            case E_MPACK_ARRAY:
            case E_MPACK_DICT:
            case E_MPACK_EXT:
            case E_STRLIST:
            case E_WSTRING:
                  talloc_steal(NULL, ret.ptr);
                  break;
            default:;
            }
      }

      talloc_free(root);
      return ret;
}
