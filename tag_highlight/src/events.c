#include "tag_highlight.h"

#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "clang/clang.h"
#include <signal.h>

#include "my_p99_common.h"
#include "p99/p99_atomic.h"
#include "p99/p99_futex.h"

#define BT bt_init
#ifdef DOSISH
#  define KILL_SIG SIGTERM
#else
#  define KILL_SIG SIGUSR1
#endif

/*======================================================================================*/

extern void          update_line          (struct bufdata *, int, int);
static void          handle_nvim_response (int fd, mpack_obj *obj, volatile p99_futex *fut);
static void          notify_waiting_thread(mpack_obj *obj, struct nvim_wait *cur);
static void          handle_line_event    (struct bufdata *bdata, mpack_obj **items);
ALWAYS_INLINE   void replace_line         (struct bufdata *bdata, b_list *repl_list, int lineno, int replno);
ALWAYS_INLINE   void line_event_multi_op  (struct bufdata *bdata, b_list *repl_list, int first, int diff);
static noreturn void event_loop           (void);
static void          interrupt_call       (int val);
static void         *handle_nvim_event    (void *vdata);

extern pthread_mutex_t    update_mutex;
extern volatile p99_futex event_loop_futex;
extern FILE              *main_log, *api_buffer_log;
FILE                     *api_buffer_log;
volatile p99_futex        event_loop_futex        = P99_FUTEX_INITIALIZER(0);
static pthread_once_t     event_loop_once_control = PTHREAD_ONCE_INIT;

struct line_event_data {
        mpack_obj *obj;
        unsigned   val;
};

static const struct event_id {
        const bstring          name;
        const enum event_types id;
} event_list[] = {
    { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
    { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
    { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
    { BT("vim_event_update"),           EVENT_VIM_UPDATE       },
};

static const struct event_id *id_event(mpack_obj *event);

/*======================================================================================* 
 * Main Event Loop                                                                      * 
 *======================================================================================*/

static noreturn void *
do_launch_event_loop(UNUSED void *notused)
{
        pthread_once(&event_loop_once_control, event_loop);
        pthread_exit();
}
void
launch_event_loop(void)
{
        START_DETACHED_PTHREAD(do_launch_event_loop, NULL);
}

static noreturn void
event_loop(void)
{
        static volatile atomic_uint counter = ATOMIC_VAR_INIT(0);
        const int fd = 1;

        for (;;) {
                /* 
                 * Constantly wait for messages.
                 */
                mpack_obj *obj = decode_stream(fd);
                if (!obj)
                        errx(1, "Got NULL object from decoder; cannot continue.");

                Auto const mtype = (enum message_types)m_expect(m_index(obj, 0), E_NUM).num;

                switch (mtype) {
                case MES_NOTIFICATION: {
                        struct line_event_data *data = xmalloc(sizeof(*data));
                        data->obj = obj;
                        data->val = atomic_fetch_add(&counter, 1u);
                        START_DETACHED_PTHREAD((void *(*)(void *))handle_nvim_event, data);
                        break;
                }
                case MES_RESPONSE:
                        handle_nvim_response(fd, obj, &event_loop_futex);
                        break;
                case MES_REQUEST:
                case MES_ANY:
                default:
                        abort();
                }
        }
}

static void
handle_nvim_response(const int fd, mpack_obj *obj, volatile p99_futex *fut)
{
        char *blah = "hello" "hi";
        p99_futex_wait(fut);
        for (unsigned i = 0; i < wait_list->qty; ++i) {
                struct nvim_wait *cur = wait_list->lst[i];
                if (!cur)
                        errx("Got an invalid wait list object in %s\n", FUNC_NAME);
                if (cur->fd == fd) {
                        const int count = (int)m_expect(m_index(obj, 1), E_NUM).num;
                        if (cur->count == count) {
                                notify_waiting_thread(obj, cur);
                                return;
                        }
                }
        }

        Errx("Object not found");
}

static void
notify_waiting_thread(mpack_obj *obj, struct nvim_wait *cur)
{
        const unsigned fut_expect_ = NVIM_GET_FUTEX_EXPECT(cur->fd, cur->count);
        cur->obj                   = obj;
        P99_FUTEX_COMPARE_EXCHANGE(&cur->fut, val, true, fut_expect_, 0, P99_FUTEX_MAX_WAITERS);
}

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

#if defined(DEBUG) && !defined(DOSISH)
__attribute__((__constructor__, __used__)) static void
openbufferlog(void)
{
        api_buffer_log = safe_fopen_fmt("%s/.tag_highlight_log/buf.log", "wb",
                                        getenv("HOME"));
}
#endif

static void *
handle_nvim_event(void *vdata)
{
        static pthread_mutex_t    handle_mutex = PTHREAD_MUTEX_INITIALIZER;
        static volatile p99_futex event_fut    = P99_FUTEX_INITIALIZER(0);

        struct line_event_data *data  = vdata;
        mpack_obj              *event = data->obj;
        const unsigned          want  = data->val;
        xfree(vdata);

        if (!event)
                pthread_exit();

        P99_FUTEX_COMPARE_EXCHANGE(&event_fut, value, value == want, value, 0, 0);
        pthread_mutex_lock(&handle_mutex);

        mpack_print_object(api_buffer_log, event);
        mpack_array_t *arr = m_expect(m_index(event, 2), E_MPACK_ARRAY).ptr;
        const struct event_id *type = id_event(event);

        if (type->id == EVENT_VIM_UPDATE) {
                interrupt_call(arr->items[0]->data.str->data[0]);
        } else {
                const int       bufnum = (int)m_expect(arr->items[0], E_NUM).num;
                struct bufdata *bdata  = find_buffer(bufnum);
                if (!bdata)
                        errx("Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_LINES:
                        if (arr->qty < 5)
                                errx(1, "Array is too small (%d, expect >= 5)", arr->qty);
                        handle_line_event(bdata, arr->items);
                        break;
                case EVENT_BUF_CHANGED_TICK:
                        bdata->ctick = (unsigned)m_expect(arr->items[1], E_NUM).num;
                        break;
                case EVENT_BUF_DETACH:
                        destroy_bufdata(&bdata);
                        echo("Detaching from buffer %d\n", bufnum);
                        break;
                default:
                        abort();
                }
        }

        P99_FUTEX_COMPARE_EXCHANGE(&event_fut, value, true, value + 1, 0, P99_FUTEX_MAX_WAITERS);
        pthread_mutex_unlock(&handle_mutex);
        mpack_destroy(event);
        pthread_exit();
}

/*======================================================================================*/

#ifdef DEBUG
#  define LINE_EVENT_DEBUG()                                                        \
        do {                                                                        \
                const unsigned ctick = nvim_buf_get_changedtick(0, bdata->num);     \
                const unsigned n     = nvim_buf_line_count(0, bdata->num);          \
                if (bdata->ctick == ctick) {                                        \
                        if (bdata->lines->qty != (int)n)                            \
                                errx(1, "Internal line count (%d) is incorrect. "   \
                                     "Actual: %u. Aborting", bdata->lines->qty, n); \
                }                                                                   \
        } while (0)
#else
#  define LINE_EVENT_DEBUG()
#endif

static void
handle_line_event(struct bufdata *bdata, mpack_obj **items)
{
        assert(!items[5]->data.boolean);
        pthread_mutex_lock(&update_mutex);

        bdata->ctick        = (unsigned)m_expect(items[1], E_NUM).num;
        const int first     = (int)m_expect(items[2], E_NUM).num;
        const int last      = (int)m_expect(items[3], E_NUM).num;
        const int diff      = last - first;
        b_list   *repl_list = m_expect(items[4], E_STRLIST).ptr;
        bool      empty     = false;
        items[4]->data.arr  = NULL;

        if (repl_list->qty) {
                if (last == (-1)) {
                        /*
                         * An "initial" update, recieved only if asked for when attaching
                         * to a buffer. We never ask for this, so this shouldn't occur.
                         */
                        errx(1, "Got initial update somehow...");
                } else if (bdata->lines->qty <= 1 && first == 0 && /* Empty buffer... */
                           repl_list->qty == 1 &&                  /* one string...   */
                           repl_list->lst[0]->slen == 0            /* which is emtpy. */
                           ) {
                        /*
                         * Useless update, one empty string in an empty buffer.
                         */
                        empty = true;
                } else if (first == 0 && last == 0) {
                        /*
                         * Inserting above the first line in the file.
                         */
                        ll_insert_blist_before_at(bdata->lines, first, repl_list, 0, -1);
                } else {
                        /* The most common scenario: we recieved at least one string,
                         * which may be an empty string only if the buffer is not empty.
                         * Moved to a helper function for clarity..
                         */
                        line_event_multi_op(bdata, repl_list, first, diff);
                }
        } else if (first != last) {
                /* If the replacement list is empty then we're just deleting lines. */
                ll_delete_range_at(bdata->lines, first, diff);
        }

        /* For some reason neovim sometimes sends updates with an empty list in
         * which both the first and last line are the same. God knows what this is
         * supposed to indicate. I'll just ignore them.
         *
         * Neovim always considers there to be at least one line in any buffer.
         * An empty buffer therefore must have one empty line.
         */
        if (bdata->lines->qty == 0)
                ll_append(bdata->lines, b_fromlit(""));

        if (!bdata->initialized && !empty)
                bdata->initialized = true;

        LINE_EVENT_DEBUG();

        xfree(repl_list->lst);
        xfree(repl_list);
        pthread_mutex_unlock(&update_mutex);
        START_DETACHED_PTHREAD(libclang_threaded_highlight, bdata);
}

static inline void
replace_line(struct bufdata *bdata, b_list *repl_list,
             const int lineno, const int replno)
{
        ll_node *node = ll_at(bdata->lines, lineno);
        b_destroy(node->data);
        node->data             = repl_list->lst[replno];
        repl_list->lst[replno] = NULL;
}

/** 
 * Hanldes a neovim line update event in which we received at least one string in a buffer
 * that is not empty. If diff is non-zero, we first delete the lines in the range 
 * `first + diff`, and then insert the new line(s) after `first` if it is now the last
 * line in the file, and before it otherwise.
 */
static inline void
line_event_multi_op(struct bufdata *bdata, b_list *repl_list, const int first, int diff)
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

/* 
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided clear command.
 */
static void
interrupt_call(const int val)
{
        struct timer t;
        static int   bufnum = (-1);
        if (val != 'H')
                echo("Recieved \"%c\"; waking up!", val);

        switch (val) {
        /*
         * New buffer was opened or current buffer changed.
         */
        case 'A':  /* FIXME Fix these damn letters, they've gotten totally out of order. */
        case 'D': {
                const int prev = bufnum;
                TIMER_START_BAR(t);
                bufnum                = nvim_get_current_buf();
                struct bufdata *bdata = find_buffer(bufnum);

                if (!bdata) {
                try_attach:
                        if (new_buffer(,bufnum)) {
                                nvim_buf_attach(BUFFER_ATTACH_FD, bufnum);
                                bdata = find_buffer(bufnum);

                                get_initial_lines(bdata);
                                get_initial_taglist(bdata);
                                update_highlight(bdata);

                                TIMER_REPORT(t, "initialization");
                        }
                } else if (prev != bufnum) {
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
        case 'B':
        case 'F': {
                TIMER_START_BAR(t);
                bufnum                 = nvim_get_current_buf();
                struct bufdata *bdata  = find_buffer(bufnum);

                if (!bdata) {
                        echo("Failed to find buffer! %d -> p: %p\n",
                             bufnum, (void *)bdata);
                        goto try_attach;
                }

                if (update_taglist(bdata, (val == 'F'))) {
                        clear_highlight(bdata);
                        update_highlight(bdata);
                        TIMER_REPORT(t, "update");
                }

                break;
        }
        /*
         * User called the kill command.
         */
        case 'C': {
                clear_highlight();
                extern pthread_t top_thread;
                pthread_kill(top_thread, KILL_SIG);
                pthread_exit();
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
        case 'I': {
                bufnum = nvim_get_current_buf();
                update_highlight(bufnum, true);
                break;
        }
        default:
                ECHO("Hmm, nothing to do...");
                break;
        }
}

/*======================================================================================*/

static const struct event_id *
id_event(mpack_obj *event)
{
        const struct event_id *type = NULL;
        bstring *typename = event->data.arr->items[1]->data.str;

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
