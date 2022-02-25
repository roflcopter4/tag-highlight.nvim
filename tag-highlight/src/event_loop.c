#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "util/initializer_hack.h"

#include <signal.h>

#if defined __GNUC__
#  define FUNCTION_NAME __extension__ __PRETTY_FUNCTION__
#elif defined _MSC_VER
#  define FUNCTION_NAME __FUNCTION__
#else
#  define FUNCTION_NAME __func__
#endif

intptr_t global_output_descriptor = 1;
#ifdef _WIN32
static void init_wsa(void);
static int socket_to_int(SOCKET orig);
static SOCKET fuck_my_ass(char *servername);
#endif

/*=====================================================================================*/

//#ifndef _WIN32
pthread_t event_loop_thread;
//#endif

#define CTX event_loop_talloc_ctx_
void *event_loop_talloc_ctx_ = NULL;
static pthread_mutex_t event_loop_cb_mutex;

static NORETURN void * handle_nvim_message_wrapper(void *data);

/*
 * @Explicitly initializing every mutex seems strictly necessary under MinGW's
 * implementation of pthreads on Windows. Things break otherwise.
 */
__attribute__((__constructor__(400)))
static void event_loop_initializer(void)
{
        pthread_mutex_init(&event_loop_cb_mutex);
}


/*=====================================================================================*
 *  Main Event Loop                                                                    *
 *=====================================================================================*/


#if USE_EVENT_LIB == EVENT_LIB_LIBEVENT

/*=====================================================================================*
 *  /--------------------------\                                                       *
 *  |Event loop is libevevent2.|                                                       *
 *  \--------------------------/                                                       *
 *=====================================================================================*/

# include <event2/event.h>
# include <event2/thread.h>

struct userdata {
      struct event_base *base;
      atomic_bool        grace;
};

static int const signals_to_handle[] = {
      KILL_SIG,
      SIGTERM,
      SIGINT,
#ifndef _WIN32
      SIGHUP,
      SIGPIPE,
      ECHILD,
#endif
};


static int do_event_loop(struct event_base *base);
static void event_loop_io_cb(evutil_socket_t fd, short flags, void *vdata);
static void clean_signal_handlers(struct event **handlers);
static void init_signal_handlers(struct event_base *base, struct event **handlers,
                                 struct userdata *data, struct timeval const *tv);
static void event_loop_signal_cb(evutil_socket_t signum, short flags, void *vdata);
static void event_loop_graceful_signal_cb(evutil_socket_t signum, short flags, void *vdata);

static struct event_base *base;


void
run_event_loop(int const fd, char *servername)
{
      static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
      if (atomic_flag_test_and_set(&event_loop_called))
            return;

      init_wsa();
      evthread_use_pthreads();
      event_enable_debug_mode();
      event_enable_debug_logging(EVENT_DBG_ALL);
      static struct timeval const tv = {.tv_sec = INT_MAX, .tv_usec = 5000000};
      event_loop_thread = pthread_self();

      SOCKET sock = fuck_my_ass(servername);
      /* HANDLE sock = GetStdHandle(STD_INPUT_HANDLE); */
      /* int sock = 0; */
      /* global_output_descriptor = (intptr_t)GetStdHandle(STD_OUTPUT_HANDLE); */
      global_output_descriptor = (intptr_t)sock;
      {
            struct event_config *cfg = event_config_new();
            //event_config_require_features(cfg, EV_FEATURE_EARLY_CLOSE);
#ifdef _WIN32
            event_config_set_flag(cfg, EVENT_BASE_FLAG_STARTUP_IOCP);
#endif
            /* event_config_require_features(cfg, EV_FEATURE_FDS); */
            event_config_avoid_method(cfg, "select");
            base = event_base_new_with_config(cfg);

            if (!base) {
                  warnx("Failed to initialize libevent.");
                  fflush(stderr);
                  NANOSLEEP(1, 0);
                  return;
            }
            event_config_free(cfg);
      }

      struct userdata data = {base, ATOMIC_VAR_INIT(false)};
      struct event *sighandlers[ARRSIZ(signals_to_handle)];
      struct event *rd_handle;

      rd_handle = event_new(base, (intptr_t)sock, EV_READ|EV_PERSIST, event_loop_io_cb, &data);
      init_signal_handlers(base, sighandlers, &data, NULL);
      
#if 1
      event_add(rd_handle, &tv);
      do_event_loop(base);
#else
      for (;;) {
            event_add(rd_handle, &tv);
            if (do_event_loop(base) != 0)
                  break;;
      }
#endif

      clean_signal_handlers(sighandlers);
      event_free(rd_handle);
      event_base_free(base);
}

void stop_event_loop(int status)
{
      if (status)
            quick_exit(0);
      else
            event_base_loopbreak(base);
}

static int
do_event_loop(struct event_base *evbase)
{
      int ret = 0;
      for (;;) {
            int const ret = event_base_loop(evbase, EVLOOP_NO_EXIT_ON_EMPTY);

            if (event_base_got_break(evbase))
                  break;
            if (ret == 1)
                  break;
            if (ret == (-1)) {
                  warnx("event_base_loop() exited with an error.");
                  break;
            }
      }
      return ret;
}

static void
event_loop_io_cb(evutil_socket_t fd, short const flags, UNUSED void *vdata)
{
      static _Atomic(uint64_t) call_no = 1;
      atomic_fetch_add_explicit(&call_no, 1, memory_order_relaxed);

      if ((flags & EV_TIMEOUT) || (flags & EV_CLOSED))
            return;

      if (flags & EV_READ) {
            pthread_mutex_lock(&event_loop_cb_mutex);

            struct event_data data;
            data.fd  = fd;
            data.obj = mpack_decode_stream((intptr_t)fd);
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

      fprintf(stderr, "hi from '%s' %u\n", FUNCTION_NAME, flags), fflush(stderr);

      struct userdata *data = vdata;
      atomic_store_explicit(&data->grace, true, memory_order_release);
      event_base_loopbreak(data->base);
}

static void
event_loop_signal_cb(evutil_socket_t signum, short const flags, void *vdata)
{
      if (!(flags & EV_SIGNAL))
            return;

      fprintf(stderr, "hi from '%s' %u\n", FUNCTION_NAME, flags), fflush(stderr);

      struct userdata *data = vdata;
      atomic_store_explicit(&data->grace, true, memory_order_release);
      event_base_loopbreak(data->base);

      switch (signum) {
      case ECHILD:
            warnx("ECHILD!!?!!?");
            break;
      case SIGSEGV:
            err(127, "Segfault");
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

      for (unsigned i = 1; i < ARRSIZ(signals_to_handle); ++i)
            handlers[i] = event_new(evbase, signals_to_handle[i], EV_SIGNAL|EV_PERSIST, event_loop_signal_cb, data);
      for (unsigned i = 0; i < ARRSIZ(signals_to_handle); ++i)
            event_add(handlers[i], tv);
}

static inline void
clean_signal_handlers(struct event **handlers)
{
      for (unsigned i = 0; i < ARRSIZ(signals_to_handle); ++i)
           event_free(handlers[i]);
}


/*=====================================================================================*/
#elif USE_EVENT_LIB == EVENT_LIB_LIBUV

/*=====================================================================================*
 *  /--------------------\                                                             *
 *  |Event loop is libuv.|                                                             *
 *  \--------------------/                                                             *
 *=====================================================================================*/

/* #define USE_UV_POLL 1 */

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
run_event_loop(int const fd, UNUSED char *servername)
{
      static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
      if (atomic_flag_test_and_set(&event_loop_called))
            return;
      
#ifdef _WIN32
      init_wsa();
#endif

      uv_loop_t  *loop = uv_default_loop();
      uv_signal_t signal_watchers[5];
      memset(signal_watchers, 0, sizeof signal_watchers);
      //uv_loop_init(&loop);
      event_loop_init_watchers(loop, signal_watchers);
      event_loop_thread = pthread_self();


#if defined USE_UV_POLL
      uv_poll_t   upoll;
      memset(&upoll, 0, sizeof upoll);
# if defined _WIN32
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
event_loop_io_cb(UNUSED uv_poll_t *handle, UNUSED int const status, int const events)
{
      if (events & UV_READABLE) {
            //if (uv_fileno((uv_handle_t const *)handle, &data.fd))
            //      err(1, "uv_fileno()");

            struct event_data data;
            data.obj = mpack_decode_stream(data.fd);
            talloc_steal(CTX, data.obj);
            handle_nvim_message(&data);

            //struct event_data *data = calloc(1, sizeof(struct event_data));
            //data->obj = talloc_steal(data, mpack_decode_stream(data->fd));
            //START_DETACHED_PTHREAD(handle_nvim_message_wrapper, data);
            //handle_nvim_message(data);
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


/*=====================================================================================*/
#elif USE_EVENT_LIB == EVENT_LIB_LIBEV

/*=====================================================================================*
 *  /--------------------\                                                             *
 *  |Event loop is libev.|                                                             *
 *  \--------------------/                                                             *
 *=====================================================================================*/

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
run_event_loop(int const fd, char *servername)
{
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
        if (atomic_flag_test_and_set(&event_loop_called))
                return;
          
#ifdef _WIN32
        init_wsa();
#endif

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


/*=====================================================================================*/
#elif USE_EVENT_LIB == EVENT_LIB_NONE

/*=====================================================================================*
 *  /-------------------------------------\                                            *
 *  |Not using any sensible event library.|                                            *
 *  \-------------------------------------/                                            *
 *=====================================================================================*/

static NORETURN void event_loop(int fd);

# ifndef _WIN32
#  include <sys/wait.h>
static jmp_buf event_loop_jmp_buf;

static NORETURN void
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
run_event_loop(int const fd, char *servername)
{
        /* I wanted to use pthread_once but it requires a function that takes no
         * arguments. Getting around that would defeat the whole point. */
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
        if (atomic_flag_test_and_set(&event_loop_called))
                return;
          
#ifdef _WIN32
      init_wsa();
#endif

# ifndef _WIN32
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
static NORETURN void
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
/*=====================================================================================*/

UNUSED
static NORETURN void *
handle_nvim_message_wrapper(void *data)
{
        handle_nvim_message(data);
        free(data);
        pthread_exit();
}


#ifdef _WIN32

static void init_wsa(void)
{
      WORD const w_version_requested = MAKEWORD(2, 2);
      WSADATA    wsa_data;

      if (WSAStartup(w_version_requested, &wsa_data) != 0)
            errx(1, "Bad winsock dll");

      if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
            WSACleanup();
            errx(1, "Could not find a usable version of Winsock.dll\n");
      }
}


static int socket_to_int(SOCKET orig)
{
    WSAPROTOCOL_INFO wsa_pi;

    // dupicate the SOCKET from libmysql
    int r = WSADuplicateSocket(orig, GetCurrentProcessId(), &wsa_pi);
    SOCKET s = WSASocket(wsa_pi.iAddressFamily, wsa_pi.iSocketType, wsa_pi.iProtocol, &wsa_pi, 0, 0);

    // create the CRT fd so ruby can get back to the SOCKET
    int fd = _open_osfhandle(s, O_RDWR|O_BINARY);
    return fd;
}


static SOCKET fuck_my_ass(char *servername)
{
      char const *server   = servername;
      char       *servport = strchr(servername, ':');
      if (!servport)
            errx(1, "no port identified");
      *servport++ = '\0';

      SOCKET sd = -1;
      int    rc;

      struct in6_addr  serveraddr;
      struct addrinfo  hints;
      struct addrinfo *res = NULL;

      do {
            memset(&serveraddr, 0, sizeof(serveraddr));
            memset(&hints, 0, sizeof(hints));
            hints.ai_flags    = AI_NUMERICSERV | AI_NUMERICHOST;
            hints.ai_family   = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            rc = inet_pton(AF_INET6, server, &serveraddr);

            if (rc == 1) /* valid IPv4 text address? */
            {
                  hints.ai_family = AF_INET;
                  hints.ai_flags |= AI_NUMERICHOST;
            } else {
                  //err(1, "inet_pton");
                  rc = inet_pton(AF_INET6, server, &serveraddr);

                  if (rc == 1) /* valid IPv6 text address? */
                  {
                        hints.ai_family = AF_INET6;
                        hints.ai_flags |= AI_NUMERICHOST;
                  }
            }

            rc = getaddrinfo(server, servport, &hints, &res);
            if (rc != 0) {
                  warnx("Host not found --> %s", gai_strerror(rc));
                  if (rc == EAI_FAIL)
                        err(1, "getaddrinfo() failed");
                  break;
            }

            sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sd == (SOCKET)(-1)) {
                  err(1, "socket() failed %s", gai_strerror(WSAGetLastError()));
                  break;
            }

            rc = connect(sd, res->ai_addr, res->ai_addrlen);
            if (rc < 0) {
                  /*****************************************************************/
                  /* Note: the res is a linked list of addresses found for server. */
                  /* If the connect() fails to the first one, subsequent addresses */
                  /* (if any) in the list can be tried if required.               */
                  /*****************************************************************/
                  err(1, "connect() %s", gai_strerror(WSAGetLastError()));
                  break;
            }
      } while (0);


      if (res != NULL)
            freeaddrinfo(res);
      return sd;
}

#endif

