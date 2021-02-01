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
#  define USE_EVENT_LIB  EVENT_LIB_EV
/* #  define USE_EVENT_LIB  EVENT_LIB_NONE */
#  define KILL_SIG       SIGUSR1
pthread_t event_loop_thread;
#endif

/*======================================================================================*/

P99_DECLARE_FIFO(event_node);
P99_DECLARE_STRUCT(event_id);

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
extern void           update_line                 (Buffer *bdata, int first, int last);

static inline   void      handle_message      (int fd, mpack_obj *obj);
static inline   void      replace_line        (Buffer *bdata, b_list *new_strings, int lineno, int index);
static inline   void      line_event_multi_op (Buffer *bdata, b_list *new_strings, int first, int num_to_modify);
static          void      handle_line_event   (Buffer *bdata, mpack_array *arr);
static          void      handle_nvim_event   (void *vdata);
static noreturn void *    post_nvim_response  (void *vdata);
static noreturn void *    nvim_event_handler  (void *unused);
static const    event_id *id_event            (mpack_obj *event) __attribute__((pure));

extern FILE *             main_log;
extern FILE *             api_buffer_log;
extern p99_futex volatile _nvim_wait_futex;
extern p99_futex volatile event_loop_futex;
       p99_futex volatile event_loop_futex = P99_FUTEX_INITIALIZER(0);
       FILE *             api_buffer_log   = NULL;

P99_FIFO(event_node_ptr) nvim_event_queue;

/*======================================================================================*/

const struct event_id event_list[] = {
        { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
        { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
        { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
        { BT("vim_event_update"),           EVENT_VIM_UPDATE       },
};

static pthread_mutex_t event_loop_cb_mutex;
static pthread_mutex_t handle_mutex;
static pthread_mutex_t nvim_event_handler_mutex;
static pthread_mutex_t vs_mutex;

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
        pthread_mutex_init(&vs_mutex);
        p99_futex_init((p99_futex *)&event_loop_futex, 0);
}

/*======================================================================================*
 * Main Event Loop                                                                      *
 *======================================================================================*/

static noreturn void *
post_nvim_response(void *vdata)
{
        _nvim_wait_node   *node;
        struct event_data *data  = vdata;
        mpack_obj         *obj   = data->obj;
        int                fd    = data->fd;
        unsigned const     count = mpack_expect(mpack_index(obj, 1), E_NUM).num;

        free(data);
        if (fd == 0)
                ++fd;

        for (;;) {
                node = P99_FIFO_POP(&_nvim_wait_queue);
                /* if (!node) */
                        /* errx(1, "Queue is empty."); */
                if (!node) {
                        eprintf("Queue is empty.");
                        mpack_destroy_object(obj);
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

static noreturn void *
nvim_event_handler(UNUSED void *unused)
{
        pthread_mutex_lock(&nvim_event_handler_mutex);

        event_node *node = P99_FIFO_POP(&nvim_event_queue);
        if (!node)
                errx(1, "Impossible, shut up clang.");
        handle_nvim_event(node->obj);

        free(node);
        pthread_mutex_unlock(&nvim_event_handler_mutex);
        pthread_exit();
}

static inline void
handle_message(int const fd, mpack_obj *obj)
{
        nvim_message_type const mtype = mpack_expect(mpack_index(obj, 0), E_NUM).num;

        switch (mtype) {
        case MES_NOTIFICATION: {
                event_node *node = calloc(1, sizeof *node);
                atomic_store_explicit(&node->obj, obj, memory_order_relaxed);
                P99_FIFO_APPEND(&nvim_event_queue, node);
                START_DETACHED_PTHREAD(nvim_event_handler);
                break;
        }
        case MES_RESPONSE: {
                struct event_data *data = malloc(sizeof *data);
                *data                   = (struct event_data){fd, obj};
                START_DETACHED_PTHREAD(post_nvim_response, data);
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

/*======================================================================================*/
/*
 * Using libev
 */
#if USE_EVENT_LIB == EVENT_LIB_EV

# include <ev.h>

static struct ev_io     input_watcher;
static struct ev_signal signal_watcher[4];

static void
event_loop_io_cb(UNUSED EV_P, ev_io *w, UNUSED int revents)
{
        pthread_mutex_lock(&event_loop_cb_mutex);

        int const  fd  = w->fd;
        mpack_obj *obj = mpack_decode_stream(fd);
        handle_message(fd, obj);

        pthread_mutex_unlock(&event_loop_cb_mutex);
}

static noreturn void
event_loop_signal_cb(UNUSED EV_P, UNUSED ev_signal *w, UNUSED int revents)
{
        quick_exit(0);
}

static void
event_loop_graceful_signal_cb(struct ev_loop *loop,
                              UNUSED ev_signal *w, UNUSED int revents)
{
        ev_signal_stop(loop, &signal_watcher[0]);
        ev_signal_stop(loop, &signal_watcher[1]);
        ev_signal_stop(loop, &signal_watcher[2]);
        ev_signal_stop(loop, &signal_watcher[3]);
        ev_io_stop(loop, &input_watcher);
}

void
run_event_loop(int const fd)
{
        struct ev_loop *loop = EV_DEFAULT;
        event_loop_thread    = pthread_self();
        ev_io_init(&input_watcher, event_loop_io_cb, fd, EV_READ);
        ev_io_start(loop, &input_watcher);

        ev_signal_init(&signal_watcher[0], event_loop_signal_cb, SIGTERM);
        ev_signal_init(&signal_watcher[1], event_loop_signal_cb, SIGPIPE);
        ev_signal_init(&signal_watcher[2], event_loop_signal_cb, SIGINT);
        ev_signal_init(&signal_watcher[3], event_loop_graceful_signal_cb, SIGUSR1);
        ev_signal_start(loop, &signal_watcher[0]);
        ev_signal_start(loop, &signal_watcher[1]);
        ev_signal_start(loop, &signal_watcher[2]);
        ev_signal_start(loop, &signal_watcher[3]);

        /* This actually runs the show. */
        ev_run(loop, 0);
}

/*======================================================================================*/
/*
 * Using no event library
 */
#elif USE_EVENT_LIB == EVENT_LIB_NONE

# ifndef DOSISH
static jmp_buf event_loop_jmp_buf;

static noreturn void
event_loop_sighandler(int signum)
{
        if (signum == SIGUSR1)
                longjmp(event_loop_jmp_buf, 1);
        else
                quick_exit(0);
}
# endif

/*
 * Very sophisticated loop.
 */
static noreturn void
event_loop(int const fd)
{
        for (;;) {
                mpack_obj *obj = mpack_decode_stream(fd);
                handle_message(fd, obj);
        }
}

void
run_event_loop(int const fd)
{
        /* I wanted to use pthread_once but it requires a function that takes no
         * arguments. Getting around that would defeat the whole point. */
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;

        if (!atomic_flag_test_and_set(&event_loop_called)) {
                /* Don't bother handling signals at all on Windows. */
# ifndef DOSISH
                event_loop_thread = pthread_self();
                if (setjmp(event_loop_jmp_buf) != 0)
                        return;
                struct sigaction act;
                memset(&act, 0, sizeof(act));
                act.sa_handler = event_loop_sighandler;
                sigaction(SIGUSR1, &act, NULL);
                sigaction(SIGTERM, &act, NULL);
                sigaction(SIGPIPE, &act, NULL);
                sigaction(SIGINT, &act, NULL);
# endif
                /* Run the show. */
                event_loop(fd);
        }
}

#endif /* No event lib */

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

static void
handle_nvim_event(void *vdata)
{
        mpack_obj      *event = vdata;
        mpack_array    *arr   = mpack_expect(mpack_index(event, 2), E_MPACK_ARRAY).ptr;
        event_id const *type  = id_event(event);
        mpack_print_object(api_buffer_log, event);

        if (type->id == EVENT_VIM_UPDATE) {
                /* Ugly and possibly undefined (?) but it works. Usually. */
                void *hack = (void *)((uintptr_t)arr->items[0]->data.num);
                START_DETACHED_PTHREAD(event_autocmd, hack);
        } else {
                int const bufnum = mpack_expect(arr->items[0], E_NUM).num;
                Buffer   *bdata  = find_buffer(bufnum);

                if (!bdata)
                        errx(1, "Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_LINES:
                        handle_line_event(bdata, arr);
                        break;
                case EVENT_BUF_CHANGED_TICK: {
                        pthread_mutex_lock(&bdata->lock.ctick);
                        uint32_t const new_tick = mpack_expect(arr->items[1], E_NUM).num;
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

        mpack_destroy_object(event);
}

static const event_id *
id_event(mpack_obj *event)
{
        bstring const *typename = event->DAI[1]->data.str;

        for (unsigned i = 0, size = (unsigned)ARRSIZ(event_list); i < size; ++i)
                if (b_iseq(typename, &event_list[i].name))
                        return &event_list[i];

        errx(1, "Failed to identify event type \"%s\".\n", BS(typename));
}

/*======================================================================================*/

static void
handle_line_event(Buffer *bdata, mpack_array *arr)
{
        pthread_mutex_lock(&handle_mutex);

        if (arr->qty < 5)
                errx(1, "Received an array from neovim that is too small. This shouldn't be possible.");
        else if (arr->items[5]->data.boolean)
                errx(1, "Error: Continuation condition is unexpectedly true, cannot continue.");

        pthread_mutex_lock(&bdata->lock.ctick);
        mpack_obj **   items    = arr->items;
        unsigned const new_tick = mpack_expect(items[1], E_NUM).num;
        if (new_tick > atomic_load(&bdata->ctick))
                atomic_store(&bdata->ctick, new_tick);
        pthread_mutex_unlock(&bdata->lock.ctick);

        int const first       = mpack_expect(items[2], E_NUM).num;
        int const last        = mpack_expect(items[3], E_NUM).num;
        b_list *  new_strings = mpack_expect(items[4], E_STRLIST).ptr;
        int const diff        = last - first;
        bool      empty       = false;
        items[4]->data.arr    = NULL;

        pthread_rwlock_wrlock(&bdata->lines->lock);

        /*
         * NOTE: For some reason neovim sometimes sends updates with an empty
         *       list in which both the first and last line are the same. God
         *       knows what this is supposed to indicate. I'll just ignore them.
         */

        if (new_strings->qty) {
                /* An "initial" update, recieved only if asked for when attaching
                 * to a buffer. We never ask for this, so this shouldn't occur. */
                if (last == (-1))
                        errx(1, "Got initial update somehow...");
                /* Useless update, one empty string in an empty buffer. */
                else if (bdata->lines->qty         <= 1 &&
                         first                     == 0 && /* Empty buffer... */
                         new_strings->qty          == 1 && /* with one string... */
                         new_strings->lst[0]->slen == 0    /* which is emtpy. */)
                {
                        empty = true;
                } 
                /* Inserting above the first line in the file. */
                else if (first == 0 && last == 0)
                        ll_insert_blist_before_at(bdata->lines, first, new_strings,
                                                  0, (-1));
                /* The most common scenario: we recieved at least one string which
                 * may be empty only if the buffer is not empty. Moved to a helper
                 * function for clarity. */
                else
                        line_event_multi_op(bdata, new_strings, first, diff);
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

        free(new_strings->lst);
        free(new_strings);
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
        b_free(node->data);
        node->data              = new_strings->lst[index];
        new_strings->lst[index] = NULL;
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
                        if ((first + i) >= bdata->lines->qty)
                                ll_insert_blist_after_at(bdata->lines, first + i,
                                                         new_strings, i, (-1));
                        else
                                ll_insert_blist_before_at(bdata->lines, first + i,
                                                          new_strings, i, (-1));
                        break;
                }
        }
}
