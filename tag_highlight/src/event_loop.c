#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include <signal.h>

/*======================================================================================*/

#ifndef DOSISH
pthread_t event_loop_thread;
#endif

#define CTX _event_loop_talloc_ctx
void *_event_loop_talloc_ctx = NULL;
static pthread_mutex_t event_loop_cb_mutex;

/*
 * Explicitly initializing every mutex seems strictly necessary under MinGW's
 * implementation of pthreads on Windows. Things break otherwise.
 */
__attribute__((__constructor__))
static void event_loop_initializer(void)
{
        pthread_mutex_init(&event_loop_cb_mutex);
}

/*======================================================================================*
 * Main Event Loop                                                                      *
 *======================================================================================*/

#if USE_EVENT_LIB == EVENT_LIB_EV
/*
 * Using libev
 */

# include <ev.h>
static struct ev_io     input_watcher;
static struct ev_signal signal_watcher[4];
static void event_loop_io_cb(EV_P, ev_io *w, int revents);
static void event_loop_graceful_signal_cb(EV_P, ev_signal *w, int revents);
static noreturn void event_loop_signal_cb(EV_P, ev_signal *w, int revents);

/*extern*/ void
run_event_loop(int const fd)
{
        struct ev_loop *loop = EV_DEFAULT;
        event_loop_thread    = pthread_self();
        ev_io_init(&input_watcher, event_loop_io_cb, fd, EV_READ);
        ev_io_start(loop, &input_watcher);

        ev_signal_init(&signal_watcher[0], event_loop_signal_cb, SIGTERM);
        // ev_signal_init(&signal_watcher[1], event_loop_signal_cb, SIGPIPE);
        // ev_signal_init(&signal_watcher[2], event_loop_signal_cb, SIGHUP);
        ev_signal_init(&signal_watcher[3], event_loop_graceful_signal_cb, SIGUSR1);
        ev_signal_start(loop, &signal_watcher[0]);
        // ev_signal_start(loop, &signal_watcher[1]);
        // ev_signal_start(loop, &signal_watcher[2]);
        ev_signal_start(loop, &signal_watcher[3]);

        /* This actually runs the show. */
        ev_run(loop, 0);
}

static void
event_loop_io_cb(UNUSED EV_P, ev_io *w, UNUSED int revents)
{
        //pthread_mutex_lock(&event_loop_cb_mutex);

        int const  fd  = w->fd;
        mpack_obj *obj = mpack_decode_stream(fd);
        talloc_steal(CTX, obj);
        /* handle_nvim_message(fd, obj); */

        struct event_data *data = malloc(sizeof *data);
        data->obj = obj;
        data->fd  = fd;
        START_DETACHED_PTHREAD(handle_nvim_message, data);

        //pthread_mutex_unlock(&event_loop_cb_mutex);
}

static void
event_loop_graceful_signal_cb(struct ev_loop *loop,
                              UNUSED ev_signal *w, UNUSED int revents)
{
        ev_signal_stop(loop, &signal_watcher[0]);
        // ev_signal_stop(loop, &signal_watcher[1]);
        // ev_signal_stop(loop, &signal_watcher[2]);
        ev_signal_stop(loop, &signal_watcher[3]);
        ev_io_stop(loop, &input_watcher);
}

static noreturn void
event_loop_signal_cb(UNUSED EV_P, UNUSED ev_signal *w, UNUSED int revents)
{
        exit(0);
}

/*======================================================================================*/
#elif USE_EVENT_LIB == EVENT_LIB_NONE
/*
 * Using no event library
 */

static noreturn void event_loop(int fd);
# ifndef DOSISH
static jmp_buf event_loop_jmp_buf;

static noreturn void
event_loop_sighandler(int signum)
{
        if (signum == SIGUSR1)
                longjmp(event_loop_jmp_buf, 1);
        else
                exit(0);
        //longjmp(event_loop_jmp_buf, signum);
}
# endif

/*extern*/ void
run_event_loop(int const fd)
{
        /* I wanted to use pthread_once but it requires a function that takes no
         * arguments. Getting around that would defeat the whole point. */
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;

        if (!atomic_flag_test_and_set(&event_loop_called)) {
                /* Don't bother handling signals at all on Windows. */
# ifndef DOSISH
                event_loop_thread = pthread_self();
                int signum;
                if ((signum = setjmp(event_loop_jmp_buf)) != 0) {
                        eprintf("I gone and got a (%d)\n", signum);
                        return;
                }
                struct sigaction act;
                memset(&act, 0, sizeof(act));
                act.sa_handler = event_loop_sighandler;
                sigaction(SIGUSR1, &act, NULL);
                sigaction(SIGTERM, &act, NULL);
                sigaction(SIGPIPE, &act, NULL);
                //sigaction(SIGINT, &act, NULL);
                sigaction(SIGHUP, &act, NULL);
                //sigaction(SIGCHLD, &act, NULL);
# endif
                /* Run the show. */
                event_loop(fd);
        }
}

/*
 * Very sophisticated loop.
 */
static noreturn void
event_loop(int const fd)
{
        for (;;) {
                struct event_data *data = malloc(sizeof *data);
                data->fd  = fd;
                data->obj = mpack_decode_stream(fd);
                START_DETACHED_PTHREAD(handle_nvim_message, data);
        }
}

#endif /* No event lib */
