#include "Common.h"
#include "contrib/p99/p99_atomic.h"
#include "contrib/p99/p99_fifo.h"
#include "contrib/p99/p99_futex.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "my_p99_common.h"
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
/* #  define USE_EVENT_LIB  EVENT_LIB_EV */
#  define USE_EVENT_LIB  EVENT_LIB_NONE
#  define KILL_SIG       SIGUSR1
static pthread_t loop_thread;
#endif

typedef volatile p99_futex vfutex_t;

/*======================================================================================*/

extern void *highlight_go_pthread_wrapper(void *vdata);

extern void           update_line         (Buffer *, int, int);
static void           handle_line_event   (Buffer *bdata, mpack_array_t *arr);
ALWAYS_INLINE   void  replace_line        (Buffer *bdata, b_list *repl_list, int lineno, int replno);
ALWAYS_INLINE   void  line_event_multi_op (Buffer *bdata, b_list *repl_list, int first, int diff);
static noreturn void *vimscript_message   (void *vdata);
static void           handle_nvim_event   (void *vdata);
static noreturn void *post_nvim_response  (void *vdata);
static noreturn void *nvim_event_handler  (void *unused);

extern vfutex_t             _nvim_wait_futex;
extern FILE                *main_log;
static pthread_mutex_t      event_loop_cb_mutex      = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t      handle_mutex             = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t      nvim_event_handler_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t      vs_mutex                 = PTHREAD_MUTEX_INITIALIZER;
       vfutex_t             event_loop_futex         = P99_FUTEX_INITIALIZER(0);
       _Atomic(mpack_obj *) event_loop_mpack_obj     = ATOMIC_VAR_INIT(NULL);
       FILE                *api_buffer_log;

P44_DECLARE_FIFO(event_node);
struct event_node {
        mpack_obj  *obj;
        event_node *p99_fifo;
};
P99_FIFO(event_node_ptr) nvim_event_queue;

P99_DECLARE_STRUCT(event_id);
static const struct event_id {
        const bstring          name;
        const enum event_types id;
} event_list[] = {
    { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
    { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
    { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
    { BT("vim_event_update"),           EVENT_VIM_UPDATE       },
};

static const event_id *id_event(mpack_obj *event);

__attribute__((__constructor__))
static void events_mutex_initializer(void) {
        pthread_mutex_init(&handle_mutex);
        pthread_mutex_init(&vs_mutex);
        pthread_mutex_init(&event_loop_cb_mutex);
        pthread_mutex_init(&nvim_event_handler_mutex);
}

/*======================================================================================* 
 * Main Event Loop                                                                      * 
 *======================================================================================*/

struct event_data {
        int          fd;
        mpack_obj   *obj;
};

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
        xfree(data);

        for (;;) {
                node = P99_FIFO_POP(&_nvim_wait_queue);
                if (!node)
                        errx(1, "Queue is empty.");
                
                if (node->fd == fd && node->count == count)
                        break;

                P99_FIFO_APPEND(&_nvim_wait_queue, node);
        }

        node->obj = obj;
        p99_futex_wakeup(&node->fut, 1U, 1U);
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

        xfree(node);
        pthread_mutex_unlock(&nvim_event_handler_mutex);
        pthread_exit();
}

/*======================================================================================*/
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

        if (!obj)
                errx(1, "Got NULL object from decoder. This should never be "
                        "possible. Cannot continue; aborting.");

        const nvim_message_type mtype = (nvim_message_type)
                                        m_expect(m_index(obj, 0), E_NUM).num;

        switch (mtype) {
        case MES_NOTIFICATION: {
                event_node *node = xcalloc(1, sizeof(event_node));
                node->obj        = obj;
                P99_FIFO_APPEND(&nvim_event_queue, node);
                START_DETACHED_PTHREAD(nvim_event_handler);
        } break;
        case MES_RESPONSE:
                post_nvim_response(fd, obj);
                break;
        case MES_REQUEST:
        case MES_ANY:
        default:
                abort();
        }

        pthread_mutex_unlock(&event_loop_cb_mutex);
}

static noreturn void sig_cb(UEVP_, UNUSED ev_signal *w, UNUSED int revents)
{
        quick_exit(0);
}

static void graceful_sig_cb(struct ev_loop *loop, UNUSED ev_signal *w, UNUSED int revents)
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

        ev_run(loop, 0);
}

/*======================================================================================*/
#elif USE_EVENT_LIB == EVENT_LIB_NONE

#if 0
static noreturn void *
event_loop_io_callback(void *vdata)
{
        pthread_mutex_lock(&event_loop_cb_mutex);
        struct event_data      *data  = vdata;
        mpack_obj              *obj   = data->obj;

        switch (data->type) {
        case MES_NOTIFICATION:
                break;
        case MES_RESPONSE:
                post_nvim_response(data->fd, obj);
                break;
        case MES_REQUEST:
        case MES_ANY:
        default:
                abort();
        }

        pthread_mutex_unlock(&event_loop_cb_mutex);
        xfree(data);
        pthread_exit();
}
#endif

static noreturn void
event_loop(const int fd)
{
        for (;;) {
                mpack_obj *obj = mpack_decode_stream(fd);

                const nvim_message_type mtype = (nvim_message_type)
                        m_expect(m_index(obj, 0), E_NUM).num;

                switch (mtype) {
                case MES_NOTIFICATION: {
                        event_node *node = xcalloc(1, sizeof(event_node));
                        node->obj        = obj;
                        P99_FIFO_APPEND(&nvim_event_queue, node);
                        START_DETACHED_PTHREAD(nvim_event_handler);
                        break;
                }
                case MES_RESPONSE: {
                        struct event_data *data = xmalloc(sizeof(struct event_data));
                        *data = (struct event_data){fd, obj};
                        START_DETACHED_PTHREAD(post_nvim_response, data);
                        break;
                }
                case MES_REQUEST:
                default:
                        abort();
                }
        }
}

void
event_loop_init(const int fd)
{
        /* I wanted to use pthread_once but it requires a function that takes no
         * arguments. Getting around that would defeat the whole point. */
        static atomic_flag event_loop_called = ATOMIC_FLAG_INIT;
        if (!atomic_flag_test_and_set(&event_loop_called))
                event_loop(fd);
}
#endif

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

static void
handle_nvim_event(void *vdata)
{
        mpack_obj      *event = vdata;
        mpack_array_t  *arr   = m_expect(m_index(event, 2), E_MPACK_ARRAY).ptr;
        const event_id *type  = id_event(event);
        mpack_print_object(api_buffer_log, event);

        if (type->id == EVENT_VIM_UPDATE) {
                START_DETACHED_PTHREAD(vimscript_message, (void *)((uintptr_t)arr->items[0]->data.str->data[0]));
                /* vimscript_message((int)arr->items[0]->data.str->data[0]); */
        } else {
                const int bufnum = (int)m_expect(arr->items[0], E_NUM).num;
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
                        old_tick = atomic_load(&bdata->ctick);
                        if (new_tick > old_tick)
                                atomic_store(&bdata->ctick, new_tick);
                        pthread_mutex_unlock(&bdata->lock.ctick);
                        break;
                }
                case EVENT_BUF_DETACH:
                        clear_highlight(bdata);
                        destroy_bufdata(&bdata);
                        eprintf("Detaching from buffer %d\n", bufnum);
                        break;
                default:
                        abort();
                }
        }

        mpack_destroy_object(event);
}

/*======================================================================================*/

#define TMP_SPRINTF(FMT, ...)                                          \
        __extension__({                                                \
                char tmp_[SAFE_PATH_MAX + 1];                          \
                snprintf(tmp_, SAFE_PATH_MAX + 1, (FMT), __VA_ARGS__); \
                tmp_;                                                  \
        })

#if defined(DEBUG) && defined(UNNECESSARY_OBSESSIVE_LOGGING)
static void
super_debug(Buffer *bdata)
{
        const unsigned ctick = nvim_buf_get_changedtick(0, bdata->num);
        const unsigned n     = nvim_buf_line_count(0, bdata->num);
        b_list        *lines = nvim_buf_get_lines(,bdata->num);
        bstring       *j1    = b_list_join(lines, B("\n"));
        bstring       *j2    = ll_join(bdata->lines, '\n');

        if (strcmp(BS(j1), BS(j2)) != 0) {
                extern char LOGDIR[];
                char *ch1 = TMP_SPRINTF("%s/vim_buffer.c", LOGDIR);
                char *ch2 = TMP_SPRINTF("%s/my_buffer.c", LOGDIR);
                unlink(ch1);
                unlink(ch2);
                FILE *vimbuf = fopen(ch1, "wb");
                FILE *mybuf  = fopen(ch2, "wb");
                b_fwrite(vimbuf, j1);
                b_fwrite(mybuf, j2);
                fclose(vimbuf);
                fclose(mybuf);

                eprintf("Internal buffer is not the same %u - %u (%d - %d)",
                        j1->slen, j2->slen, bdata->ctick, ctick);
        }

        b_list_destroy(lines);
        P99_SEQ(b_destroy, j1, j2);
}
#endif

#if 1
#define LINE_EVENT_DEBUG()                                                       \
        do {                                                                     \
                const unsigned ctick = nvim_buf_get_changedtick(0, bdata->num);  \
                const unsigned n     = nvim_buf_line_count(0, bdata->num);       \
                if (atomic_load(&bdata->ctick) == ctick) {                       \
                        if (bdata->lines->qty != (int)n)                         \
                                errx(1,                                          \
                                     "Internal line count (%d) is incorrect. "   \
                                     "Actual: %u -- %u vs %u. Aborting",         \
                                     bdata->lines->qty, n, ctick, bdata->ctick); \
                }                                                                \
        } while (0)
#else
#  define LINE_EVENT_DEBUG()
#endif

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
        arr->items[4]->data.arr  = NULL;

        pthread_mutex_lock(&bdata->lines->lock);

        if (repl_list->qty) {
                if (last == (-1)) {
                        /* An "initial" update, recieved only if asked for when attaching
                         * to a buffer. We never ask for this, so this shouldn't occur. */
                        errx(1, "Got initial update somehow...");
                } else if (bdata->lines->qty <= 1 && first == 0 && // Empty buffer...
                           repl_list->qty == 1 &&                  // with one string...  
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
        /* For some reason neovim sometimes sends updates with an empty list in
         * which both the first and last line are the same. God knows what this is
         * supposed to indicate. I'll just ignore them. */

        /* Neovim always considers there to be at least one line in any buffer.
         * An empty buffer therefore must have one empty line. */
        if (bdata->lines->qty == 0)
                ll_append(bdata->lines, b_fromcstr(""));

        if (!bdata->initialized && !empty)
                bdata->initialized = true;

        /* LINE_EVENT_DEBUG(); */

        xfree(repl_list->lst);
        xfree(repl_list);
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

/** 
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

/**
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided clear command.
 */
static noreturn void *
vimscript_message(void *vdata)
{
        const  int        val    = (int)((uintptr_t)(vdata));
        static atomic_int bufnum = ATOMIC_VAR_INIT(-1);
        struct timer     *t      = TIMER_INITIALIZER;
        int               num = 0;

        if (val != 'H')
                echo("Recieved \"%c\"; waking up!", val);

        switch (val) {
        /*
         * New buffer was opened or current buffer changed.
         */
        case 'A':  /* FIXME These damn letters have gotten totally out of order. */
        case 'D': {
                num            = nvim_get_current_buf();
                const int prev = atomic_exchange(&bufnum, num);
                TIMER_START_BAR(t);
                Buffer *bdata = find_buffer(num);

                if (!bdata) {
                try_attach:
                        if (new_buffer(,num)) {
                                nvim_buf_attach(BUFFER_ATTACH_FD, num);
                                bdata = find_buffer(num);

                                get_initial_lines(bdata);
                                get_initial_taglist(bdata);
                                update_highlight(bdata, HIGHLIGHT_UPDATE);

                                TIMER_REPORT(t, "initialization");
                        }
                } else if (prev != num) {
                        if (!bdata->calls)
                                get_initial_taglist(bdata);

                        update_highlight(bdata);
                        TIMER_REPORT(t, "update");
                }

                break;
        }
        /*
         * Buffer was written, or filetype/syntax was changed.
         */
        case 'B': {
                TIMER_START_BAR(t);
                num = nvim_get_current_buf();
                atomic_store(&bufnum, num);
                Buffer *bdata = find_buffer(num);

                if (!bdata) {
                        echo("Failed to find buffer! %d -> p: %p\n",
                             num, (void *)bdata);
                        goto try_attach;
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
        case 'C': {
                clear_highlight();
                pthread_mutex_unlock(&vs_mutex);
#ifdef DOSISH
                exit(0);
#else
                pthread_kill(loop_thread, KILL_SIG);
                /* raise(KILL_SIG); */
                pthread_exit();
#endif
        }
        /*
         * User called the clear highlight command.
         */
        case 'E':
                clear_highlight();
                break;
        /* 
         * Not used...
         */
        case 'H':
                break;
        /* 
         * Force an update.
         */
        case 'F': {
                num = nvim_get_current_buf();
                atomic_store(&bufnum, num);
                Buffer *bdata = find_buffer(num);
                update_taglist(bdata, UPDATE_TAGLIST_FORCE);
                update_highlight(bdata, HIGHLIGHT_UPDATE);
                break;
        }
        default:
                echo("Hmm, nothing to do...");
                break;
        }

        pthread_exit();
}

/*======================================================================================*/

static const event_id *
id_event(mpack_obj *event)
{
        const event_id *type     = NULL;
        bstring        *typename = event->data.arr->items[1]->data.str;

        for (unsigned i = 0; i < ARRSIZ(event_list); ++i) {
                if (b_iseq(typename, &event_list[i].name)) {
                        type = &event_list[i];
                        break;
                }
        }

        if (!type)
                errx(1, "Failed to identify event type.\n");

        return type;
}
