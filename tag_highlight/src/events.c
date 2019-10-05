#include "Common.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "contrib/p99/p99_atomic.h"
#include <signal.h>

#define BT bt_init

#define EVENT_LIB_EV     1
#define EVENT_LIB_EVENT2 2
#define EVENT_LIB_UV     3
#define EVENT_LIB_NONE   4
#ifdef DOSISH
#  define USE_EVENT_LIB  EVENT_LIB_NONE
#  define KILL_SIG       SIGTERM
#else
#  define USE_EVENT_LIB  EVENT_LIB_EV
/* #  define USE_EVENT_LIB  EVENT_LIB_NONE */
#  define KILL_SIG       SIGUSR1
static pthread_t loop_thread;
#endif

/*======================================================================================*/

P44_DECLARE_FIFO(event_node);
P99_DECLARE_STRUCT(event_id);

struct event_node {
        mpack_obj  *obj;
        event_node *p99_fifo;
};

struct event_data {
        int        fd;
        mpack_obj *obj;
};

/*======================================================================================*/

extern void *highlight_go_pthread_wrapper(void *vdata);
extern void  update_line                 (Buffer *bdata, int first, int last);

static inline   void      handle_message      (int fd, mpack_obj *obj);
static inline   void      replace_line        (Buffer *bdata, b_list *repl_list, int lineno, int replno);
static inline   void      line_event_multi_op (Buffer *bdata, b_list *repl_list, int first, int diff);
static          void      handle_line_event   (Buffer *bdata, mpack_array_t *arr);
static          void      handle_nvim_event   (void *vdata);
static noreturn void     *vimscript_message   (void *vdata);
static noreturn void     *post_nvim_response  (void *vdata);
static noreturn void     *nvim_event_handler  (void *unused);
static const    event_id *id_event            (mpack_obj *event);

extern FILE               *main_log;
extern FILE               *api_buffer_log;
extern p99_futex volatile  _nvim_wait_futex;
extern p99_futex volatile  event_loop_futex;
       p99_futex volatile  event_loop_futex = P99_FUTEX_INITIALIZER(0);
       FILE               *api_buffer_log   = NULL;

P99_FIFO(event_node_ptr) nvim_event_queue;

/*======================================================================================*/

static const struct event_id {
        const bstring          name;
        const enum event_types id;
} event_list[] = {
    { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
    { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
    { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
    { BT("vim_event_update"),           EVENT_VIM_UPDATE       },
};

static pthread_mutex_t event_loop_cb_mutex      = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t handle_mutex             = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t nvim_event_handler_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t vs_mutex                 = PTHREAD_MUTEX_INITIALIZER;

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
        struct event_data *data  = vdata;
        mpack_obj         *obj   = data->obj;
        int                fd    = data->fd;
        const unsigned     count = (unsigned)m_expect(m_index(obj, 1), E_NUM).num;
        _nvim_wait_node   *node;
        if (fd == 0)
                ++fd;
        free(data);

        for (;;) {
                node = P99_FIFO_POP(&_nvim_wait_queue);
                if (!node)
                        errx(1, "Queue is empty.");
                
                if (node->fd == fd && node->count == count)
                        break;

                P99_FIFO_APPEND(&_nvim_wait_queue, node);
        }

        node->obj = obj;
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
handle_message(const int fd, mpack_obj *obj)
{
        const nvim_message_type mtype = (nvim_message_type)m_expect(m_index(obj, 0),
                                                                    E_NUM).num;

        switch (mtype) {
        case MES_NOTIFICATION: {
                event_node *node = calloc(1, sizeof(event_node));
                node->obj        = obj;
                P99_FIFO_APPEND(&nvim_event_queue, node);
                START_DETACHED_PTHREAD(nvim_event_handler);
                break;
        }
        case MES_RESPONSE: {
                struct event_data *data = malloc(sizeof(struct event_data));
                *data = (struct event_data){fd, obj};
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

#  include <ev.h>
#  define UEVP_ __attribute__((__unused__)) EV_P

static struct ev_io     input_watcher;
static struct ev_signal signal_watcher[4];

static void
event_loop_io_callback(UEVP_, ev_io *w, UNUSED int revents)
{
        pthread_mutex_lock(&event_loop_cb_mutex);

        const int  fd  = w->fd;
        mpack_obj *obj = mpack_decode_stream(fd);
        handle_message(fd, obj);

        pthread_mutex_unlock(&event_loop_cb_mutex);
}

static noreturn void
sig_cb(UEVP_, UNUSED ev_signal *w, UNUSED int revents)
{
        quick_exit(0);
}

static void
graceful_sig_cb(struct ev_loop *loop, UNUSED ev_signal *w, UNUSED int revents)
{
        ev_signal_stop(loop, &signal_watcher[0]);
        ev_signal_stop(loop, &signal_watcher[1]);
        ev_signal_stop(loop, &signal_watcher[2]);
        ev_signal_stop(loop, &signal_watcher[3]);
        ev_io_stop(loop, &input_watcher);
}

void
event_loop_init(const int fd)
{
        struct ev_loop *loop = EV_DEFAULT;
        loop_thread          = pthread_self();
        ev_io_init(&input_watcher, event_loop_io_callback, fd, EV_READ);
        ev_io_start(loop, &input_watcher);

        ev_signal_init(&signal_watcher[0], sig_cb, SIGTERM);
        ev_signal_init(&signal_watcher[1], sig_cb, SIGPIPE);
        ev_signal_init(&signal_watcher[2], sig_cb, SIGINT);
        ev_signal_init(&signal_watcher[3], graceful_sig_cb, SIGUSR1);
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

#  ifndef DOSISH
static jmp_buf event_loop_jmp_buf;

static noreturn void
event_loop_sighandler(int signum)
{
        if (signum == SIGUSR1)
                longjmp(event_loop_jmp_buf, 1);
        else
                quick_exit(0);
}
#  endif

/*
 * Very sophisticated loop.
 */
static noreturn void
event_loop(const int fd)
{
        for (;;) {
                mpack_obj *obj = mpack_decode_stream(fd);
                handle_message(fd, obj);
        }
}

void
event_loop_init(const int fd)
{
        /* I wanted to use pthread_once but it requires a function that takes no
         * arguments. Getting around that would defeat the whole point. */
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;

        if (!atomic_flag_test_and_set(&event_loop_called)) {

        /* Don't bother handling signals at all on Windows. */
#  ifndef DOSISH
                loop_thread = pthread_self();
                if (setjmp(event_loop_jmp_buf) != 0)
                        return;
                struct sigaction act;
                memset(&act, 0, sizeof(act));
                act.sa_handler = event_loop_sighandler;
                sigaction(SIGUSR1, &act, NULL);
                sigaction(SIGTERM, &act, NULL);
                sigaction(SIGPIPE, &act, NULL);
                sigaction(SIGINT, &act, NULL);
#  endif

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
        mpack_array_t  *arr   = m_expect(m_index(event, 2), E_MPACK_ARRAY).ptr;
        event_id const *type  = id_event(event);
        mpack_print_object(api_buffer_log, event);

        if (type->id == EVENT_VIM_UPDATE) {
                /* Ugly and possibly undefined (?) but it works. Usually. */
                void *hack = (void *)((uintptr_t)arr->items[0]->data.num);
                START_DETACHED_PTHREAD(vimscript_message, hack);
        } else {
                int const bufnum = (int)m_expect(arr->items[0], E_NUM).num;
                Buffer   *bdata  = find_buffer(bufnum);

                if (!bdata)
                        errx(1, "Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_LINES:
                        handle_line_event(bdata, arr);
                        break;
                case EVENT_BUF_CHANGED_TICK: {
                        uint32_t new_tick, old_tick;
                        pthread_mutex_lock(&bdata->lock.ctick);
                        new_tick = (uint32_t)m_expect(arr->items[1], E_NUM).num;
                        old_tick = (uint32_t)atomic_load(&bdata->ctick);
                        if (new_tick > old_tick)
                                atomic_store(&bdata->ctick, new_tick);
                        pthread_mutex_unlock(&bdata->lock.ctick);
                        break;
                }
                case EVENT_BUF_DETACH:
                        clear_highlight(bdata);
                        destroy_bufdata(&bdata);
                        warnx("Detaching from buffer %d", bufnum);
                        break;
                default:
                        abort();
                }
        }

        mpack_destroy_object(event);
}

/*======================================================================================*/

static void
handle_line_event(Buffer *bdata, mpack_array_t *arr)
{
        pthread_mutex_lock(&handle_mutex);
        pthread_mutex_lock(&bdata->lock.ctick);
        if (arr->qty < 5 || arr->items[5]->data.boolean)
                errx(1, "Error: Continuation condition is unexpectedly true, cannot continue.");

        const unsigned new_tick = (unsigned)m_expect(arr->items[1], E_NUM).num;
        const unsigned old_tick = atomic_load(&bdata->ctick);
        if (new_tick > old_tick)
                atomic_store(&bdata->ctick, new_tick);
        pthread_mutex_unlock(&bdata->lock.ctick);

        const int first     = (int)m_expect(arr->items[2], E_NUM).num;
        const int last      = (int)m_expect(arr->items[3], E_NUM).num;
        const int diff      = last - first;
        b_list   *repl_list = m_expect(arr->items[4], E_STRLIST).ptr;
        bool      empty     = false;
        arr->items[4]->data.arr = NULL;

        pthread_mutex_lock(&bdata->lines->lock);

        /* NOTE: For some reason neovim sometimes sends updates with an empty
         *       list in which both the first and last line are the same. God
         *       knows what this is supposed to indicate. I'll just ignore them. */

        if (repl_list->qty) {
                if (last == (-1)) {
                        /* An "initial" update, recieved only if asked for when attaching
                         * to a buffer. We never ask for this, so this shouldn't occur. */
                        errx(1, "Got initial update somehow...");
                } else if (bdata->lines->qty <= 1 && first == 0 && /* Empty buffer... */
                           repl_list->qty == 1 &&                  /* with one string... */
                           repl_list->lst[0]->slen == 0            /* which is emtpy. */) {
                        /* Useless update, one empty string in an empty buffer. */
                        empty = true;
                } else if (first == 0 && last == 0) {
                        /* Inserting above the first line in the file. */
                        ll_insert_blist_before_at(bdata->lines, first, repl_list, 0, -1);
                } else {
                        /* The most common scenario: we recieved at least one string,
                         * which may be an empty string only if the buffer is not empty.
                         * Moved to a helper function for clarity.. */
                        line_event_multi_op(bdata, repl_list, first, diff);
                }
        } else if (first != last) {
                /* If the replacement list is empty then we're just deleting lines. */
                ll_delete_range_at(bdata->lines, first, diff);
        }

        /* Neovim always considers there to be at least one line in any buffer.
         * An empty buffer therefore must have one empty line. */
        if (bdata->lines->qty == 0)
                ll_append(bdata->lines, b_empty_string());

        if (!bdata->initialized && !empty)
                bdata->initialized = true;

        free(repl_list->lst);
        free(repl_list);
        pthread_mutex_unlock(&bdata->lines->lock);

        if (!empty && bdata->ft->has_parser) {
                if (bdata->ft->is_c)
                        START_DETACHED_PTHREAD(libclang_threaded_highlight, bdata);
                else if (bdata->ft->id == FT_GO)
                        START_DETACHED_PTHREAD(highlight_go_pthread_wrapper, bdata);
        }
        pthread_mutex_unlock(&handle_mutex);
}

static inline void
replace_line(Buffer *bdata, b_list *repl_list,
             const int lineno, const int replno)
{
        ll_node *node = ll_at(bdata->lines, lineno);
        b_destroy(node->data);
        node->data             = repl_list->lst[replno];
        repl_list->lst[replno] = NULL;
}

/*
 * Handles a neovim line update event in which we received at least one string in a buffer
 * that is not empty. If diff is non-zero, we first delete the lines in the range 
 * `first + diff`, and then insert the new line(s) after `first` if it is now the last
 * line in the file, and before it otherwise.
 */
static inline void
line_event_multi_op(Buffer *bdata, b_list *repl_list, const int first, int diff)
{
        const int olen  = bdata->lines->qty;
        const int iters = (int)MAX((unsigned)diff, repl_list->qty);

        /* This loop is only meaningful when replacing lines.
         * All other paths break after the first iteration. 
         */
        for (int i = 0; i < iters; ++i) {
                if (diff && i < olen) {
                        --diff;
                        if (i < (int)repl_list->qty) {
                                replace_line(bdata, repl_list, first+i, i);
                        } else {
                                ll_delete_range_at(bdata->lines, first+i, diff+1);
                                break;
                        }
                } else {
                        /* If the first line not being replaced (first + i) is at the end
                         * of the file, then we append. Otherwise the update must be prepended.  
                         */
                        if ((first + i) >= bdata->lines->qty)
                                ll_insert_blist_after_at(bdata->lines, first+i, repl_list, i, -1);
                        else
                                ll_insert_blist_before_at(bdata->lines, first+i, repl_list, i, -1);
                        break;
                }
        }
}

/*======================================================================================*/

P99_DECLARE_ENUM(vimscript_message_type,
        VIML_BUF_NEW,
        VIML_BUF_CHANGED,
        VIML_BUF_SYNTAX_CHANGED,
        VIML_UPDATE_TAGS,
        VIML_UPDATE_TAGS_FORCE,
        VIML_CLEAR_BUFFER,
        VIML_STOP
);
P99_DEFINE_ENUM(vimscript_message_type);

/*
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided clear command.
 */
static noreturn void *
vimscript_message(void *vdata)
{
        assert((uintptr_t)vdata < INT_MAX);
        static atomic_int      bufnum = ATOMIC_VAR_INIT(-1);
        vimscript_message_type val    = (int)((uintptr_t)vdata);
        struct timer          *t      = TIMER_INITIALIZER;
        int                    num    = 0;

        echo("Recieved \"%s\" (%d): waking up!",
             vimscript_message_type_getname(val), val);

        switch (val) {
        /*
         * New buffer was opened or current buffer changed.
         */
        case VIML_BUF_NEW:
                TIMER_START_BAR(t);
        case VIML_BUF_CHANGED: {
                num            = nvim_get_current_buf();
                const int prev = atomic_exchange(&bufnum, num);

                if (prev == num) 
                        break;

                Buffer *bdata = find_buffer(num);

                if (!bdata) {
                try_attach:
                        if (new_buffer(num)) {
                                nvim_buf_attach(num);
                                Buffer *bdata2 = find_buffer(num);

                                get_initial_lines(bdata2);
                                get_initial_taglist(bdata2);
                                update_highlight(bdata2, HIGHLIGHT_UPDATE);

                                TIMER_REPORT(t, "initialization");
                        } else {
                                ECHO("Failed to attach to buffer number %d.", num);
                        }
                } else  {
                        if (!bdata->calls)
                                get_initial_taglist(bdata);

                        update_highlight(bdata, HIGHLIGHT_NORMAL);
                        TIMER_REPORT(t, "update");
                }

                break;
        }

        case VIML_BUF_SYNTAX_CHANGED: {
                /* TODO */
                break;
        }

        /*
         * Buffer was written, or filetype/syntax was changed.
         */
        case VIML_UPDATE_TAGS: {
                num = nvim_get_current_buf();
                atomic_store(&bufnum, num);
                Buffer *bdata = find_buffer(num);

                if (!bdata) {
                        echo("Failed to find buffer! %d -> p: %p",
                             num, (void *)bdata);
                        /* goto try_attach; */
                        break;
                }

                if (update_taglist(bdata, (val == 'F'))) {
                        clear_highlight(bdata);
                        update_highlight(bdata, HIGHLIGHT_UPDATE);
                        TIMER_REPORT(t, "update");
                }

                break;
        }
        /*
         * User called the kill command.
         */
        case VIML_STOP: {
                clear_highlight();
                pthread_mutex_unlock(&vs_mutex);
#ifdef DOSISH
                exit(0);
#else
                pthread_kill(loop_thread, KILL_SIG);
                pthread_exit();
#endif
        }
        /*
         * User called the clear highlight command.
         */
        case VIML_CLEAR_BUFFER:
                clear_highlight();
                break;
        /* 
         * Force an update.
         */
        case VIML_UPDATE_TAGS_FORCE: {
                TIMER_START_BAR(t);
                num = nvim_get_current_buf();
                atomic_store(&bufnum, num);
                Buffer *bdata = find_buffer(num);
                if (!bdata)
                        goto try_attach;
                update_taglist(bdata, UPDATE_TAGLIST_FORCE);
                update_highlight(bdata, HIGHLIGHT_UPDATE);
                break;
        }

        default:
                break;
        }

        pthread_exit();
}

/*======================================================================================*/

static const event_id *
id_event(mpack_obj *event)
{
        const bstring *typename = event->DAI[1]->data.str;

        for (unsigned i = 0, size = (unsigned)ARRSIZ(event_list); i < size; ++i)
                if (b_iseq(typename, &event_list[i].name))
                        return &event_list[i];

        errx(1, "Failed to identify event type \"%s\".\n", BS(typename));
}
