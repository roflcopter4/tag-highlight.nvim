#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "util/initializer_hack.h"

#include <signal.h>

/*======================================================================================*/

#ifndef DOSISH
pthread_t event_loop_thread;
#endif

#define CTX event_loop_talloc_ctx_
void *event_loop_talloc_ctx_ = NULL;
static pthread_mutex_t event_loop_cb_mutex;

static noreturn void * handle_nvim_message_wrapper(void *data);

/*
 * @Explicitly initializing every mutex seems strictly necessary under MinGW's
 * implementation of pthreads on Windows. Things break otherwise.
 */
__attribute__((__constructor__(400)))
static void event_loop_initializer(void)
{
        pthread_mutex_init(&event_loop_cb_mutex);
}

/*======================================================================================*
 * Main Event Loop                                                                      *
 *======================================================================================*/

#if USE_EVENT_LIB == EVENT_LIB_LIBUV
/*
 * Using libuv
 */

#define USE_UV_POLL 1

# include <uv.h>

static void event_loop_init_watchers(uv_loop_t *loop, uv_signal_t signal_watcher[5]);
static void event_loop_start_watchers(uv_signal_t signal_watcher[5]);
static void event_loop_io_cb(uv_poll_t* handle, int status, int events);
static void event_loop_graceful_signal_cb(uv_signal_t *handle, int signum);
static void event_loop_signal_cb(uv_signal_t *handle, int signum);

UNUSED static void do_event_loop(uv_loop_t *loop, uv_poll_t *upoll, uv_signal_t signal_watcher[5]);
UNUSED static void do_event_loop_pipe(uv_loop_t *loop, uv_pipe_t *phand, uv_signal_t signal_watchers[5]);

struct userdata {
      uv_loop_t   *loop_handle;
      uv_poll_t   *poll_handle;
      uv_pipe_t   *pipe_handle;
      uv_signal_t *signal_watchers;
      bool         grace;
};


void
run_event_loop(int const fd)
{
      static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
      if (atomic_flag_test_and_set(&event_loop_called))
            return;

      uv_loop_t  *loop = uv_default_loop();
      uv_signal_t signal_watchers[5];
      memset(signal_watchers, 0, sizeof signal_watchers);
      //uv_loop_init(&loop);
      event_loop_init_watchers(loop, signal_watchers);
      event_loop_thread = pthread_self();


#if defined USE_UV_POLL
      uv_poll_t   upoll;
      memset(&upoll, 0, sizeof upoll);
# ifdef _WIN32
      uv_poll_init(loop, &upoll, fd);
# else
      uv_poll_init_socket(loop, &upoll, fd);
# endif

      do_event_loop(loop, &upoll, signal_watchers);

#else

      uv_pipe_t phand;
      memset(&phand, 0, sizeof phand);
      uv_pipe_init(loop, &phand, false);
      uv_pipe_open(&phand, fd);

      do_event_loop_pipe(loop, &phand, signal_watchers);
#endif


      uv_loop_close(loop);
      uv_library_shutdown();
}


static void
do_event_loop(uv_loop_t *loop, uv_poll_t *upoll, uv_signal_t signal_watchers[5])
{
      struct userdata data = {loop, upoll, NULL, signal_watchers, false};
      loop->data = upoll->data = signal_watchers[0].data = signal_watchers[1].data =
                                 signal_watchers[2].data = signal_watchers[3].data =
                                 signal_watchers[4].data =
                   &data;

      event_loop_start_watchers(signal_watchers);
      uv_poll_start(upoll, UV_READABLE, &event_loop_io_cb);
      uv_run(loop, UV_RUN_DEFAULT);

      if (!data.grace)
            errx(1, "This shouldn't be reachable?...");
}

static void
event_loop_init_watchers(uv_loop_t *loop, uv_signal_t signal_watchers[5])
{
      uv_signal_init(loop, &signal_watchers[0]);
      uv_signal_init(loop, &signal_watchers[1]);
      uv_signal_init(loop, &signal_watchers[2]);
      uv_signal_init(loop, &signal_watchers[3]);
      uv_signal_init(loop, &signal_watchers[4]);
}

static void
event_loop_start_watchers(uv_signal_t signal_watchers[5])
{
      uv_signal_start_oneshot(&signal_watchers[0], &event_loop_graceful_signal_cb, SIGUSR1);
      uv_signal_start_oneshot(&signal_watchers[1], &event_loop_signal_cb, SIGINT);
      uv_signal_start_oneshot(&signal_watchers[2], &event_loop_signal_cb, SIGPIPE);
      uv_signal_start_oneshot(&signal_watchers[3], &event_loop_signal_cb, SIGHUP);
      uv_signal_start_oneshot(&signal_watchers[4], &event_loop_signal_cb, SIGTERM);
}

static void
event_loop_io_cb(uv_poll_t *handle, UNUSED int const status, int const events)
{
      if (events & UV_READABLE) {
            struct event_data data;
            if (uv_fileno((uv_handle_t const *)handle, &data.fd))
                  err(1, "uv_fileno()");

            data.obj = mpack_decode_stream(data.fd);
            talloc_steal(CTX, data.obj);

            handle_nvim_message(&data);
      }
}

static void
event_loop_graceful_signal_cb(uv_signal_t *handle, UNUSED int const signum)
{
      struct userdata *data = handle->data;
      data->grace           = true;

      uv_signal_stop(&data->signal_watchers[0]);
      uv_signal_stop(&data->signal_watchers[1]);
      uv_signal_stop(&data->signal_watchers[2]);
      uv_signal_stop(&data->signal_watchers[3]);
      uv_signal_stop(&data->signal_watchers[4]);
      if (data->poll_handle)
            uv_poll_stop(data->poll_handle);
      if (data->pipe_handle)
            uv_read_stop((uv_stream_t *)data->pipe_handle);
      uv_stop(data->loop_handle);
}

static void
event_loop_signal_cb(uv_signal_t *handle, int const signum)
{
      struct userdata *data = handle->data;
      data->grace           = false;

      switch (signum) {
      case SIGTERM:
      case SIGHUP:
      case SIGPIPE:
      case SIGINT:
            quick_exit(0);
      default:
            exit(0);
      }
}


static void
pipe_alloc_callback(UNUSED uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
      char *tmp = realloc(buf->base, suggested_size);
      if (!tmp)
            err(1, "realloc()");
      buf->base = tmp;
      buf->len  = suggested_size;
}

static void
pipe_read_callback(UNUSED uv_stream_t *stream, ssize_t nread, uv_buf_t const *buf)
{
      if (nread <= 0)
            errx(1, "Invalid read.");
      bstring wrapper = {
          .data  = (uchar *)buf->base,
          .slen  = nread,
          .mlen  = 0,
          .flags = 0
      };
      struct event_data data;

      if (uv_fileno((uv_handle_t const *)stream, &data.fd))
            err(1, "uv_fileno()");

      while (wrapper.slen > 0) {
            data.obj = mpack_decode_obj(&wrapper);
            talloc_steal(CTX, data.obj);
            handle_nvim_message(&data);
      }
}

static void
do_event_loop_pipe(uv_loop_t *loop, uv_pipe_t *phand, uv_signal_t signal_watchers[5])
{
      struct userdata data = {loop, NULL, phand, signal_watchers, false};
      loop->data = phand->data = signal_watchers[0].data = signal_watchers[1].data =
                                 signal_watchers[2].data = signal_watchers[3].data =
                                 signal_watchers[4].data =
                   &data;

      event_loop_start_watchers(signal_watchers);
      uv_read_start((uv_stream_t *)phand, &pipe_alloc_callback, &pipe_read_callback);
      uv_run(loop, UV_RUN_DEFAULT);

      if (!data.grace)
            errx(1, "This shouldn't be reachable?...");
}

/*--------------------------------------------------------------------------------------*/
#elif USE_EVENT_LIB == EVENT_LIB_EV
/*
 * Using libev
 */

# include <ev.h>
static struct ev_io     input_watcher;
static struct ev_signal signal_watcher[5];
static void event_loop_io_cb(EV_P, ev_io *w, int revents);
static void event_loop_graceful_signal_cb(EV_P, ev_signal *w, int revents);
static void event_loop_signal_cb(EV_P, ev_signal *w, int revents);

static inline void event_loop_stop(struct ev_loop *loop);
static inline void event_loop_init_watchers(struct ev_loop *loop);

struct userdata {
        bool grace;
};

/*extern*/ void
run_event_loop(int const fd)
{
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
        if (atomic_flag_test_and_set(&event_loop_called))
                return;

        struct userdata data = {0};
        struct ev_loop *loop = EV_DEFAULT;
        event_loop_thread    = pthread_self();

        ev_set_userdata(loop, &data);
        event_loop_init_watchers(loop);
        ev_io_init(&input_watcher, event_loop_io_cb, fd, EV_READ);
        ev_io_start(loop, &input_watcher);

        /* This actually runs the show. */
        ev_run(loop, 0);

        if (!data.grace)
                pthread_exit();
}

static void
event_loop_io_cb(UNUSED EV_P, ev_io *w, UNUSED int revents)
{
        int const  fd  = w->fd;
        mpack_obj *obj = mpack_decode_stream(fd);
        talloc_steal(CTX, obj);

        //struct event_data *data = malloc(sizeof *data);
        struct event_data data;
        data.obj = obj;
        data.fd  = fd;
        //START_DETACHED_PTHREAD(handle_nvim_message, data);
        handle_nvim_message(&data);
        //free(data);
}

static void
event_loop_graceful_signal_cb(struct ev_loop *loop,
                              UNUSED ev_signal *w, UNUSED int revents)
{
        struct userdata *data = ev_userdata(loop);
        data->grace = true;
        event_loop_stop(loop);
}

static void
event_loop_signal_cb(UNUSED EV_P, UNUSED ev_signal *w, UNUSED int revents)
{
        struct userdata *data = ev_userdata(loop);
        data->grace = false;
        event_loop_stop(loop);
}

static inline void
event_loop_stop(struct ev_loop *loop)
{
        ev_io_stop(loop, &input_watcher);
        ev_signal_stop(loop, &signal_watcher[0]);
        ev_signal_stop(loop, &signal_watcher[1]);
        ev_signal_stop(loop, &signal_watcher[2]);
        ev_signal_stop(loop, &signal_watcher[3]);
        ev_signal_stop(loop, &signal_watcher[4]);
}

static inline void
event_loop_init_watchers(struct ev_loop *loop)
{
        ev_signal_init(&signal_watcher[0], event_loop_graceful_signal_cb, SIGUSR1);
        ev_signal_init(&signal_watcher[1], event_loop_signal_cb, SIGINT);
        ev_signal_init(&signal_watcher[2], event_loop_signal_cb, SIGPIPE);
        ev_signal_init(&signal_watcher[3], event_loop_signal_cb, SIGHUP);
        ev_signal_init(&signal_watcher[4], event_loop_signal_cb, SIGTERM);
        ev_signal_start(loop, &signal_watcher[0]);
        ev_signal_start(loop, &signal_watcher[1]);
        ev_signal_start(loop, &signal_watcher[2]);
        ev_signal_start(loop, &signal_watcher[3]);
        ev_signal_start(loop, &signal_watcher[4]);
}


/*--------------------------------------------------------------------------------------*/
#elif USE_EVENT_LIB == EVENT_LIB_NONE
/*
 * Using no event library
 */

static noreturn void event_loop(int fd);

# ifndef DOSISH
#  include <sys/wait.h>
static jmp_buf event_loop_jmp_buf;

static noreturn void
event_loop_sighandler(int signum)
{
        switch (signum) {
        case SIGUSR1:
                longjmp(event_loop_jmp_buf, 1);
        case SIGTERM:
        case SIGHUP:
        case SIGPIPE:
        case SIGINT:
                quick_exit(0);
        default:
                exit(0);
        }
}
# endif

/*extern*/ void
run_event_loop(int const fd)
{
        /* I wanted to use pthread_once but it requires a function that takes no
         * arguments. Getting around that would defeat the whole point. */
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
        if (atomic_flag_test_and_set(&event_loop_called))
                return;

# ifndef DOSISH
        /* Don't bother handling signals at all on Windows. */
        event_loop_thread = pthread_self();

        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = event_loop_sighandler;
        sigaction(SIGUSR1, &act, NULL);
        sigaction(SIGPIPE, &act, NULL);
        sigaction(SIGHUP, &act, NULL);
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGTERM, &act, NULL);

        if (setjmp(event_loop_jmp_buf) != 0)
                return;
# endif
        /* Run the show. */
        event_loop(fd);
}


/*
 * Very sophisticated loop.
 */
static noreturn void
event_loop(int const fd)
{
        for (;;) {
                struct event_data data;
                data.fd  = fd;
                data.obj = mpack_decode_stream(fd);
                handle_nvim_message(&data);
        }
}

#endif /* No event lib */
/*======================================================================================*/

UNUSED
static noreturn void *
handle_nvim_message_wrapper(void *data)
{
        handle_nvim_message(data);
        free(data);
        pthread_exit();
}
