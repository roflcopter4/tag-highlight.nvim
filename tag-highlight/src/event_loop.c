#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "util/initializer_hack.h"

#include <signal.h>

/*======================================================================================*/

//#ifndef DOSISH
pthread_t event_loop_thread;
//#endif

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
#ifndef _WIN32
      uv_signal_init(loop, &signal_watchers[3]);
      uv_signal_init(loop, &signal_watchers[4]);
#endif
}

static void
event_loop_start_watchers(uv_signal_t signal_watchers[5])
{
      uv_signal_start_oneshot(&signal_watchers[0], &event_loop_graceful_signal_cb, KILL_SIG);
      uv_signal_start_oneshot(&signal_watchers[1], &event_loop_signal_cb, SIGINT);
      uv_signal_start_oneshot(&signal_watchers[2], &event_loop_signal_cb, SIGTERM);
#ifndef _WIN32
      uv_signal_start_oneshot(&signal_watchers[3], &event_loop_signal_cb, SIGPIPE);
      uv_signal_start_oneshot(&signal_watchers[4], &event_loop_signal_cb, SIGHUP);
#endif
}

static void
event_loop_io_cb(uv_poll_t *handle, UNUSED int const status, int const events)
{
      if (events & UV_READABLE) {
            struct event_data data;
            //if (uv_fileno((uv_handle_t const *)handle, &data.fd))
            //      err(1, "uv_fileno()");

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
#ifndef _WIN32
      uv_signal_stop(&data->signal_watchers[3]);
      uv_signal_stop(&data->signal_watchers[4]);
#endif
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
      case SIGINT:
#ifndef _WIN32
      case SIGHUP:
      case SIGPIPE:
#endif
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

      //if (uv_fileno((uv_handle_t const *)stream, &data.fd))
      //      err(1, "uv_fileno()");

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

/*======================================================================================*/
#elif USE_EVENT_LIB == EVENT_LIB_LIBEVENT

# include <event2/event.h>

struct userdata {
      struct event_base *base;
      atomic_bool        grace;
};

static void do_event_loop(struct event_base *base);
static void event_loop_io_cb(evutil_socket_t fd, short flags, void *vdata);
static void event_loop_signal_cb(evutil_socket_t signum, short flags, void *vdata);
static void event_loop_graceful_signal_cb(evutil_socket_t signum, short flags, void *vdata);

static inline void init_signal_handlers(struct event_base *base, struct event **handlers, struct userdata *data, struct timeval const *tv);
static inline void clean_signal_handlers(struct event **handlers);

static struct event_base *base;

static void
event_loop_sighandler(int signum)
{
        switch (signum) {
        case KILL_SIG:
              event_base_loopbreak(base);
              break;
        case SIGINT:
        case SIGTERM:
#ifndef _WIN32
        case SIGHUP:
        case SIGPIPE:
#endif
                quick_exit(0);
        default:
                exit(0);
        }
}

void stop_event_loop(int status)
{
      if (status) {
            quick_exit(0);
      } else {
            event_base_loopbreak(base);
      }
}

void
run_event_loop(int const fd)
{
      static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
      if (atomic_flag_test_and_set(&event_loop_called))
            return;
        
      event_enable_debug_logging(EVENT_DBG_NONE);
      static struct timeval const tv = {.tv_sec = 0, .tv_usec = 5000000};
      event_loop_thread = pthread_self();

#if 0
      {
            struct sigaction act;
            memset(&act, 0, sizeof(act));
            act.sa_handler = event_loop_sighandler;
            sigaction(SIGUSR1, &act, NULL);
            sigaction(SIGPIPE, &act, NULL);
            sigaction(SIGHUP, &act, NULL);
            sigaction(SIGINT, &act, NULL);
            sigaction(SIGTERM, &act, NULL);
      }
#endif

      {
            struct event_config *cfg = event_config_new();
            event_config_require_features(cfg, EV_FEATURE_EARLY_CLOSE);
#ifdef _WIN32
            //event_config_set_flag(cfg, EVENT_BASE_FLAG_STARTUP_IOCP);
#endif
            base = event_base_new_with_config(cfg);
            event_config_free(cfg);
      }

      struct userdata data = {base, ATOMIC_VAR_INIT(false)};
      struct event *sighandlers[5];
      struct event *rd_handle;

      rd_handle = event_new(base, 0, EV_READ|EV_PERSIST, event_loop_io_cb, &data);
      init_signal_handlers(base, sighandlers, &data, NULL);
      event_add(rd_handle, &tv);

#if 0
      {
            sigset_t set;
            /* Block SIGQUIT and SIGUSR1; other threads created by main()
               will inherit a copy of the signal mask. */
            sigemptyset(&set);
            sigaddset(&set, SIGUSR1);
            int s = pthread_sigmask(SIG_UNBLOCK, &set, NULL);
            if (s != 0)
                  err(1, "pthread_sigmask");
      }

#endif

      do_event_loop(base);

      clean_signal_handlers(sighandlers);
      event_free(rd_handle);
      event_base_free(base);
}

static void
do_event_loop(struct event_base *evbase)
{
      for (;;) {
            int const ret = event_base_loop(evbase, 0);

            if (event_base_got_break(evbase))
                  break;
            if (ret == 1)
                  break;
            if (ret == (-1)) {
                  warnx("event_base_loop() exited with an error.");
                  break;
            }

            // event_add(rd_handle, nullptr);
            // event_base_dispatch(loop);
      }
}

static void
event_loop_io_cb(evutil_socket_t fd, short const flags, UNUSED void *vdata)
{
      static _Atomic(uint64_t) call_no = 1;

      atomic_fetch_add_explicit(&call_no, 1, memory_order_relaxed);
      if ((flags & EV_TIMEOUT) || (flags & EV_CLOSED))
            return;

      char const *foo;
      switch (flags) {
            case 0x01: foo = "timeout"; break;
            case 0x02: foo = "read"; break;
            case 0x04: foo = "write"; break;
            case 0x08: foo = "signal"; break;
            case 0x40: foo = "finalize"; break;
            case 0x80: foo = "close"; break;
            default:   foo = "unknown"; break;
      }
      //echo("HERE IN %s, call number %lu -> %s (%u)",
      //     __func__, atomic_load_explicit(&call_no, memory_order_relaxed), foo, flags);

      if (flags & EV_READ) {
            pthread_mutex_lock(&event_loop_cb_mutex);

            struct event_data data;
            data.fd  = fd;
            data.obj = mpack_decode_stream(data.fd);
            talloc_steal(CTX, data.obj);
            handle_nvim_message(&data);

            pthread_mutex_unlock(&event_loop_cb_mutex);
      }
}

static void
event_loop_graceful_signal_cb(UNUSED evutil_socket_t signum, short const flags, void *vdata)
{
      if (!(flags & EV_SIGNAL))
            return;
            //fprintf(stderr, "wtf? -> %u\n", flags), fflush(stderr);

      fprintf(stderr, "hi from '%s' %u\n", __PRETTY_FUNCTION__, flags), fflush(stderr);

      struct userdata *data = vdata;
      atomic_store_explicit(&data->grace, true, memory_order_release);
      event_base_loopbreak(data->base);
}

static void
event_loop_signal_cb(evutil_socket_t signum, short const flags, void *vdata)
{
      if (!(flags & EV_SIGNAL))
            return;

      fprintf(stderr, "hi from '%s' %u\n", __PRETTY_FUNCTION__, flags), fflush(stderr);

      struct userdata *data = vdata;
      atomic_store_explicit(&data->grace, true, memory_order_release);
      event_base_loopbreak(data->base);

      switch (signum) {
      case SIGTERM:
      case SIGINT:
#ifndef _WIN32
      case SIGHUP:
      case SIGPIPE:
#endif
            quick_exit(0);
      default:
            exit(0);
      }
}

static inline void
init_signal_handlers(struct event_base *evbase, struct event **handlers, struct userdata *data, struct timeval const *tv)
{
      handlers[0] = event_new(evbase, KILL_SIG, EV_SIGNAL|EV_PERSIST, event_loop_graceful_signal_cb, data);
      handlers[1] = event_new(evbase, SIGTERM,  EV_SIGNAL|EV_PERSIST, event_loop_signal_cb, data);
      handlers[2] = event_new(evbase, SIGINT,   EV_SIGNAL|EV_PERSIST, event_loop_signal_cb, data);
#ifndef _WIN32
      handlers[3] = event_new(evbase, SIGHUP,   EV_SIGNAL|EV_PERSIST, event_loop_signal_cb, data);
      handlers[4] = event_new(evbase, SIGPIPE,  EV_SIGNAL|EV_PERSIST, event_loop_signal_cb, data);
#endif

      event_add(handlers[0], tv);
      event_add(handlers[1], tv);
      event_add(handlers[2], tv);
#ifndef _WIN32
      event_add(handlers[3], tv);
      event_add(handlers[4], tv);
#endif
}

static inline void
clean_signal_handlers(struct event **handlers)
{
      event_free(handlers[0]);
      event_free(handlers[1]);
      event_free(handlers[2]);
#ifndef _WIN32
      event_free(handlers[3]);
      event_free(handlers[4]);
#endif
}


/*======================================================================================*/
#elif USE_EVENT_LIB == EVENT_LIB_LIBEV
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
