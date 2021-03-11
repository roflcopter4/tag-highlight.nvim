#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "contrib/p99/p99_atomic.h"
#include <signal.h>


#define BT bt_init

#define EVENT_LIB_EV   1
#define EVENT_LIB_NONE 2
#ifdef DOSISH
#  define USE_EVENT_LIB  EVENT_LIB_NONE
#  define KILL_SIG       SIGTERM
#else
#  ifdef HAVE_LIBEV
#    define USE_EVENT_LIB  EVENT_LIB_EV
#  else
#    define USE_EVENT_LIB  EVENT_LIB_NONE
#  endif
#  define KILL_SIG       SIGUSR1
pthread_t event_loop_thread;
#endif

extern FILE *main_log;
extern FILE *api_buffer_log;
extern p99_futex volatile _nvim_wait_futex;

/*======================================================================================*/

void *_events_object_talloc_ctx = NULL;
void *_events_nvim_notification_talloc_ctx = NULL;
void *_events_nvim_response_talloc_ctx = NULL;

__attribute__((__constructor__))
static void init_mpack_talloc_ctx(void) 
{
        _events_object_talloc_ctx = talloc_named_const(NULL, 0, __location__ ": TOP");
        _events_nvim_notification_talloc_ctx = talloc_named_const(NULL, 0, __location__ ": TOP");
        _events_nvim_response_talloc_ctx = talloc_named_const(NULL, 0, __location__ ": TOP");
}

/*======================================================================================*/

P99_DECLARE_STRUCT(event_id);
typedef const event_id *event_idp;

const event_id event_list[] = {
        { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
        { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
        { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
        { BT("vim_event_update"),           EVENT_VIM_UPDATE       },
};

P99_DECLARE_FIFO(event_node);

struct event_node {
        _Atomic(mpack_obj *) obj;
        event_node *p99_fifo;
};

struct event_data {
        int        fd;
        mpack_obj *obj;
};

/*======================================================================================*/

extern noreturn void *highlight_go_pthread_wrapper(void *vdata);
static noreturn void *handle_nvim_response    (void *vdata);
static noreturn void *handle_nvim_notification(void *unused);
static          void  handle_nvim_message     (int fd, mpack_obj *obj);

FILE *                   api_buffer_log   = NULL;
p99_futex volatile       event_loop_futex = P99_FUTEX_INITIALIZER(0);
static pthread_mutex_t   event_loop_cb_mutex;
static pthread_mutex_t   handle_mutex;
static pthread_mutex_t   nvim_event_handler_mutex;
P99_FIFO(event_node_ptr) nvim_event_queue;

/*
 * Explicitly initializing every mutex seems strictly necessary under MinGW's
 * implementation of pthreads on Windows. Things break otherwise.
 */
__attribute__((__constructor__))
static void events_mutex_initializer(void)
{
        pthread_mutex_init(&event_loop_cb_mutex);
        pthread_mutex_init(&handle_mutex);
        pthread_mutex_init(&nvim_event_handler_mutex);
        p99_futex_init((p99_futex *)&event_loop_futex, 0);
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
static void event_loop_io_cb(struct ev_loop *, ev_io *, int);
static void event_loop_graceful_signal_cb(struct ev_loop *, ev_signal *, int);
static noreturn void event_loop_signal_cb(struct ev_loop *, ev_signal *, int);

/*extern*/ void
run_event_loop(int const fd)
{
        struct ev_loop *loop = EV_DEFAULT;
        event_loop_thread    = pthread_self();
        ev_io_init(&input_watcher, event_loop_io_cb, fd, EV_READ);
        ev_io_start(loop, &input_watcher);

        ev_signal_init(&signal_watcher[0], event_loop_signal_cb, SIGTERM);
        ev_signal_init(&signal_watcher[1], event_loop_signal_cb, SIGPIPE);
        ev_signal_init(&signal_watcher[2], event_loop_signal_cb, SIGHUP);
        ev_signal_init(&signal_watcher[3], event_loop_graceful_signal_cb, SIGUSR1);
        ev_signal_start(loop, &signal_watcher[0]);
        ev_signal_start(loop, &signal_watcher[1]);
        ev_signal_start(loop, &signal_watcher[2]);
        ev_signal_start(loop, &signal_watcher[3]);

        /* This actually runs the show. */
        ev_run(loop, 0);
}

static void
event_loop_io_cb(UNUSED EV_P, ev_io *w, UNUSED int revents)
{
        pthread_mutex_lock(&event_loop_cb_mutex);

        int const  fd  = w->fd;
        mpack_obj *obj = mpack_decode_stream(fd);
        talloc_steal(_events_object_talloc_ctx, obj);
        handle_nvim_message(fd, obj);

        pthread_mutex_unlock(&event_loop_cb_mutex);
}

static void
event_loop_graceful_signal_cb(struct ev_loop *loop,
                              UNUSED ev_signal *w, UNUSED int revents)
{
        extern void exit_cleanup(void);
        exit_cleanup();

        ev_signal_stop(loop, &signal_watcher[0]);
        ev_signal_stop(loop, &signal_watcher[1]);
        ev_signal_stop(loop, &signal_watcher[2]);
        ev_signal_stop(loop, &signal_watcher[3]);
        ev_io_stop(loop, &input_watcher);
}

static noreturn void
event_loop_signal_cb(UNUSED EV_P, UNUSED ev_signal *w, UNUSED int revents)
{
        quick_exit(1);
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
#if 0
        if (signum == SIGUSR1)
                longjmp(event_loop_jmp_buf, 1);
        else
                exit(0);
#endif
        longjmp(event_loop_jmp_buf, signum);
}
# endif

/*extern*/ void
run_event_loop(int const fd)
{
        /* I wanted to use pthread_once but it requires a function that takes no
         * arguments. Getting around that would defeat the whole point. */
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;

        if (!atomic_flag_test_and_set(&event_loop_called)) {
# ifndef DOSISH
                /* Don't bother handling signals at all on Windows. */
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
                sigaction(SIGINT, &act, NULL);
                sigaction(SIGHUP, &act, NULL);
                sigaction(SIGCHLD, &act, NULL);
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
                mpack_obj *obj = mpack_decode_stream(fd);
                handle_nvim_message(fd, obj);
        }
}

#endif /* No event lib */

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

static void      handle_line_event(Buffer *bdata, mpack_array *arr);
static event_idp id_event         (mpack_obj *event);

static void
handle_nvim_message(int const fd, mpack_obj *obj)
{
        nvim_message_type const mtype = mpack_expect(mpack_index(obj, 0), E_NUM).num;

        switch (mtype) {
        case MES_NOTIFICATION: {
                talloc_steal(_events_nvim_notification_talloc_ctx, obj);
                event_node *node = calloc(1, sizeof *node);
                atomic_store_explicit(&node->obj, obj, memory_order_relaxed);
                P99_FIFO_APPEND(&nvim_event_queue, node);
                START_DETACHED_PTHREAD(handle_nvim_notification);
                break;
        }
        case MES_RESPONSE: {
                talloc_steal(_events_nvim_response_talloc_ctx, obj);
                struct event_data *data = malloc(sizeof *data);
                data->fd  = fd;
                data->obj = obj;
                START_DETACHED_PTHREAD(handle_nvim_response, data);
                break;
        }
        case MES_REQUEST:
                errx(1, "Recieved request in %s somehow. "
                     "This should be \"impossible\"?\n", FUNC_NAME);
        default:
                errx(1, "Recieved invalid object type from neovim. "
                     "This should be \"impossible\"?\n");
        }
}

static noreturn void *
handle_nvim_notification(UNUSED void *unused)
{
        pthread_mutex_lock(&nvim_event_handler_mutex);

        event_node *node = P99_FIFO_POP(&nvim_event_queue);
        if (!node)
                errx(1, "Impossible, shut up clang.");

        mpack_obj   *event = node->obj;
        mpack_array *arr   = mpack_expect(mpack_index(event, 2), E_MPACK_ARRAY).ptr;
        event_idp    type  = id_event(event);
        /* mpack_print_object(api_buffer_log, event); */

        if (type->id == EVENT_VIM_UPDATE) {
                uint64_t *tmp = malloc(sizeof(uint64_t));
                *tmp          = mpack_expect(arr->lst[0], E_NUM).num;
                START_DETACHED_PTHREAD(event_autocmd, tmp);
        } else {
                int const bufnum = mpack_expect(arr->lst[0], E_NUM).num;
                Buffer   *bdata  = find_buffer(bufnum);

                if (!bdata)
                        errx(1, "Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_LINES:
                        handle_line_event(bdata, arr);
                        break;
                case EVENT_BUF_CHANGED_TICK: {
                        pthread_mutex_lock(&bdata->lock.ctick);
                        uint32_t const new_tick = mpack_expect(arr->lst[1], E_NUM).num;
                        if (new_tick > atomic_load(&bdata->ctick))
                                atomic_store(&bdata->ctick, new_tick);
                        pthread_mutex_unlock(&bdata->lock.ctick);
                        break;
                }
                case EVENT_BUF_DETACH:
                        clear_highlight(bdata);
                        destroy_buffer(bdata);
                        warnx("Detaching from buffer %d", bufnum);
                        break;
                default:
                        abort();
                }
        }

        /* mpack_destroy_object(event); */
        talloc_free(event);
        free(node);

        pthread_mutex_unlock(&nvim_event_handler_mutex);
        pthread_exit();
}

static noreturn void *
handle_nvim_response(void *vdata)
{
        struct event_data *data  = vdata;
        mpack_obj         *obj   = data->obj;
        int                fd    = data->fd;
        unsigned const     count = mpack_expect(mpack_index(obj, 1), E_NUM).num;
        _nvim_wait_node   *node  = NULL;

        free(data);
        if (fd == 0)
                ++fd;

        for (;;) {
                node = P99_FIFO_POP(&_nvim_wait_queue);
                if (!node) {
                        eprintf("Queue is empty.");
                        talloc_free(obj);
                        pthread_exit();
                }

                if (node->fd == fd && node->count == count)
                        break;

                P99_FIFO_APPEND(&_nvim_wait_queue, node);
        }

        atomic_store_explicit(&node->obj, obj, memory_order_release);
        p99_futex_wakeup(&node->fut, 1U, P99_FUTEX_MAX_WAITERS);
        pthread_exit();
}

static event_idp
id_event(mpack_obj *event)
{
        bstring const *typename = event->arr->lst[1]->str;

        for (unsigned i = 0, size = (unsigned)ARRSIZ(event_list); i < size; ++i)
                if (b_iseq(typename, &event_list[i].name))
                        return &event_list[i];

        errx(1, "Failed to identify event type \"%s\".\n", BS(typename));
}

/*======================================================================================*/

static inline void replace_line(Buffer *bdata, b_list *new_strings,
                                int lineno, int index);
static inline void line_event_multi_op(Buffer *bdata, b_list *new_strings,
                                       int first, int num_to_modify);

static void
handle_line_event(Buffer *bdata, mpack_array *arr)
{
        pthread_mutex_lock(&handle_mutex);

        if (arr->qty < 5)
                errx(1, "Received an array from neovim that is too small. This "
                        "shouldn't be possible.");
        else if (arr->lst[5]->boolean)
                errx(1, "Error: Continuation condition is unexpectedly true, "
                        "cannot continue.");

        pthread_mutex_lock(&bdata->lock.ctick);
        unsigned const new_tick = mpack_expect(arr->lst[1], E_NUM, true).num;
        if (new_tick > atomic_load(&bdata->ctick))
                atomic_store(&bdata->ctick, new_tick);
        pthread_mutex_unlock(&bdata->lock.ctick);

        int const first       = mpack_expect(arr->lst[2], E_NUM, true).num;
        int const last        = mpack_expect(arr->lst[3], E_NUM, true).num;
        b_list   *new_strings = mpack_expect(arr->lst[4], E_STRLIST, true).ptr;
        int const diff        = last - first;
        bool      empty       = false;

        pthread_rwlock_wrlock(&bdata->lines->lock);

        /*
         * NOTE: For some reason neovim sometimes sends updates with an empty
         *       list in which both the first and last line are the same. God
         *       knows what this is supposed to indicate. I'll just ignore them.
         */

        if (new_strings->qty) {
                /* An "initial" update, recieved only if asked for when attaching
                 * to a buffer. We never ask for this, so this shouldn't occur. */
                if (last == (-1)) {
                        errx(1, "Got initial update somehow...");
                } 
                /* Useless update, one empty string in an empty buffer. */
                else if (bdata->lines->qty         <= 1 &&
                         first                     == 0 && /* Empty buffer... */
                         new_strings->qty          == 1 && /* with one string... */
                         new_strings->lst[0]->slen == 0    /* which is emtpy. */)
                {
                        empty = true;
                } 
                /* Inserting above the first line in the file. */
                else if (first == 0 && last == 0) {
                        ll_insert_blist_before_at(bdata->lines, first, new_strings,
                                                  0, (-1));
                } 
                /* The most common scenario: we recieved at least one string which
                 * may be empty only if the buffer is not empty. Moved to a helper
                 * function for clarity. */
                else {
                        line_event_multi_op(bdata, new_strings, first, diff);
                }
        }
        else if (first != last) {
                /* If the replacement list is empty then we're just deleting lines. */
                ll_delete_range_at(bdata->lines, first, diff);
        }

        /* Neovim always considers there to be at least one line in any buffer.
         * An empty buffer therefore must have one empty line. */
        if (bdata->lines->qty == 0)
                ll_append(bdata->lines, b_empty_string());

        if (!bdata->initialized && !empty)
                bdata->initialized = true;

        talloc_free(new_strings);
        /* free(new_strings->lst); */
        /* free(new_strings); */
        pthread_rwlock_unlock(&bdata->lines->lock);

        if (!empty && bdata->ft->has_parser) {
                if (bdata->ft->is_c)
                        START_DETACHED_PTHREAD(highlight_c_pthread_wrapper, bdata);
                else if (bdata->ft->id == FT_GO)
                        START_DETACHED_PTHREAD(highlight_go_pthread_wrapper, bdata);
        }
        pthread_mutex_unlock(&handle_mutex);
}

static inline void
replace_line(Buffer *bdata, b_list *new_strings,
             int const lineno, int const index)
{
        ll_node *node = ll_at(bdata->lines, lineno);
        talloc_free(node->data);
        node->data = talloc_move(node, &new_strings->lst[index]);
}

/*
 * first:         Index of the first string to replace in the buffer (if any)
 * num_to_modify: Number of existing buffer lines to replace and/or delete
 *
 * Handles a neovim line update event in which we received at least one string in a
 * buffer that is not empty. If diff is non-zero, we first delete the lines in the range
 * `first + diff`, and then insert the new line(s) after `first` if it is now the last
 * line in the file, and before it otherwise.
 */
static inline void
line_event_multi_op(Buffer *bdata, b_list *new_strings, int const first, int num_to_modify)
{
        int const num_new   = (int)new_strings->qty;
        int const num_lines = MAX(num_to_modify, num_new);
        int const olen      = bdata->lines->qty;

        /* This loop is only meaningful when replacing lines.
         * All other paths break after the first iteration. */
        for (int i = 0; i < num_lines; ++i) {
                if (num_to_modify-- > 0 && i < olen) {
                        /* There are still strings to be modified. If we still have a
                         * replacement available then we use it. Otherwise we are instead
                         * deleting a range of lines. */
                        if (i < num_new) {
                                replace_line(bdata, new_strings, first + i, i);
                        } else {
                                ll_delete_range_at(bdata->lines, first + i,
                                                   num_to_modify + 1);
                                break;
                        }
                } else {
                        /* There are no more strings to be modified and there is one or
                         * more strings remaining in the list. These are to be inserted
                         * into the buffer.
                         * If the first line to insert (first + i) is at the end of the
                         * file then we append it, otherwise we prepend. */
                        if ((first + i) >= bdata->lines->qty) {
                                ll_insert_blist_after_at(bdata->lines, first + i,
                                                         new_strings, i, (-1));
                        } else {
                                ll_insert_blist_before_at(bdata->lines, first + i,
                                                          new_strings, i, (-1));
                        }
                        break;
                }
        }
}
