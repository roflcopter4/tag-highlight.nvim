#include "tag_highlight.h"

#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "util/list.h"
#include "clang/clang.h"
#include <signal.h>

#include "my_p99_common.h"
#include "p99/p99_atomic.h"
#include "p99/p99_futex.h"
/* #include "p99/p99_cm.h" */
/* #include "p99/p99_new.h" */

#define BT bt_init
#ifdef DOSISH
#  define KILL_SIG SIGTERM
#else
#  define KILL_SIG SIGUSR1
#endif

#include "nvim_api/read.h"

/*======================================================================================*/

extern void          update_line         (struct bufdata *, int, int);
static void          handle_line_event   (struct bufdata *bdata, mpack_obj **items);
/* ALWAYS_INLINE   void replace_line        (ll_node *node, b_list *repl_list, int replno); */
ALWAYS_INLINE   void line_event_multi_op (struct bufdata *bdata, b_list *repl_list, int first, int diff);
static noreturn void event_loop          (void);
static void          vimscript_interrupt (int val);
static void          handle_nvim_event   (void *vdata);
static void          post_nvim_response  (void *vdata);
static void          super_debug         (struct bufdata *bdata);

extern FILE              *main_log, *api_buffer_log;
       FILE              *api_buffer_log;
/* extern vfutex_t           event_futex; */
       /* vfutex_t           event_futex             = P99_FUTEX_INITIALIZER(0); */
static pthread_once_t     event_loop_once_control = PTHREAD_ONCE_INIT;
/* static volatile p99_count vimscript_lock; */


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

P99_FIFO(mpack_obj_node_ptr)  handle_fifo_head;

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
        START_DETACHED_PTHREAD(do_launch_event_loop);
}

static void *
emergency_debug(void *vdata)
{
        struct bufdata *bdata = vdata;
        const unsigned ctick = nvim_buf_get_changedtick(0, bdata->num);
        const unsigned n     = nvim_buf_line_count(0, bdata->num);
        pthread_mutex_lock(&bdata->lines->lock);
        if (atomic_load(&bdata->ctick) == ctick) {
                if (bdata->lines->qty != (int)n)
                        errx(1,
                             "Internal line count (%d) is incorrect. "
                             "Actual: %u -- %u vs %u. Aborting",
                             bdata->lines->qty, n, ctick, bdata->ctick);
        }
        pthread_mutex_unlock(&bdata->lines->lock);
        pthread_exit();
}

static noreturn void
event_loop(void)
{
        const int fd = 1;
        for (;;) {
                /* 
                 * Constantly wait for messages.
                 */
                mpack_obj *obj = decode_stream(fd);
                if (!obj)
                        errx(1, "Got NULL object from decoder. This should never be "
                                "possible. Cannot continue; aborting.");

                const nvim_message_type mtype = (nvim_message_type)
                                                m_expect(m_index(obj, 0), E_NUM).num;

                switch (mtype) {
                case MES_NOTIFICATION: {
                        handle_nvim_event(obj);
                        break;
                }
                case MES_RESPONSE:
                        post_nvim_response(obj);
                        break;
                case MES_REQUEST:
                case MES_ANY:
                default:
                        abort();
                }
        }
}

static void
post_nvim_response(void *vdata)
{
        mpack_obj      *obj  = vdata;
        mpack_obj_node *node = xmalloc(sizeof *node);
        node->obj            = obj;
        node->count          = (uint32_t)m_expect(m_index(obj, 1), E_NUM).num;
        P99_FIFO_APPEND(&mpack_obj_queue, node);
        /* pthread_exit(); */
        /* return NULL; */

        /* P99_FUTEX_COMPARE_EXCHANGE(&event_futex, value,
                value > 0, value, 0, 0); */
#if 0

        const uint32_t   count = (uint32_t)m_expect(m_index(obj, 1), E_NUM).num;
        nvim_wait_queue *node;

        while ((node = P99_FIFO_POP(&nvim_wait_queue_head))) {
                if (node->count == count) {
                        node->obj = obj;
                        p99_futex_wakeup(node->fut, 1u, 1u);
                        break;
                }
                P99_LIFO_PUSH(&wait_queue_stack_head, node);
        }

        while ((node = P99_LIFO_POP(&wait_queue_stack_head)))
                P99_FIFO_APPEND(&nvim_wait_queue_head, node);

        atomic_fetch_add(&event_futex, (-1));
#endif



#if 0
        extern genlist  *_nvim_wait_list;
        extern vfutex_t  _nvim_wait_futex;

        const uint32_t    count = (uint32_t)m_expect(m_index(obj, 1), E_NUM).num;
        struct nvim_wait *wt    = xmalloc(sizeof *wt);
        *wt                     = (struct nvim_wait){fd, count, obj};

        genlist_append(_nvim_wait_list, wt); /* genlist contains an integral rwlock */
        /* mpack_obj_queue *el = xmalloc(sizeof *el); */
        /* P99_FIFO_APPEND(&_nvim_wait_fifo_head, el); */

        P99_FUTEX_COMPARE_EXCHANGE(&_nvim_wait_futex, value,
                true, (value + 1u),      /* Never lock and increment the value */
                0, P99_FUTEX_MAX_WAITERS /* Wake up anyone currently waiting */
        );

        /* Wait for the receiving thread to acknowledge receipt of the data. */
        p99_futex_wait(&event_futex);
#endif
}

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

static void *destroy_bufdata_wrap(void *vdata);
static void *vimscript_interrupt_wrap(void *vdata);

static void
handle_nvim_event(void *vdata)
{
        /* static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER; */
        /* pthread_mutex_lock(&event_mutex); */

        /* mpack_obj_node *p = P99_FIFO_POP(&handle_fifo_head); */
        /* if (!p || !p->obj) */
                /* errx(1, "Null object from fifo!"); */

        /* mpack_obj             *event = p->obj; */
        mpack_obj *event = vdata;
        mpack_array_t         *arr   = m_expect(m_index(event, 2), E_MPACK_ARRAY).ptr;
        const struct event_id *type  = id_event(event);
        mpack_print_object(api_buffer_log, event);

        if (type->id == EVENT_VIM_UPDATE) {
                /* p99_count_wait(&vimscript_lock); */
                /* p99_count_inc(&vimscript_lock); */
                /* vimscript_interrupt(arr->items[0]->data.str->data[0]); */
                START_DETACHED_PTHREAD(&vimscript_interrupt_wrap,
                                       ((void *)((uintptr_t)arr->items[0]->data.str->data[0])));
        } else {
                const int       bufnum = (int)m_expect(arr->items[0], E_NUM).num;
                struct bufdata *bdata  = find_buffer(bufnum);
                if (!bdata)
                        errx(1, "Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_LINES:
                        handle_line_event(bdata, arr->items);
                        break;
                case EVENT_BUF_CHANGED_TICK:

                        pthread_mutex_lock(&bdata->lock.ctick);
                        const unsigned new_tick = (uint32_t)m_expect(arr->items[1], E_NUM).num;
                        const unsigned old_tick = atomic_load(&bdata->ctick);
                        if (new_tick > old_tick)
                                atomic_store(&bdata->ctick, new_tick);
                        pthread_mutex_unlock(&bdata->lock.ctick);

                        /* atomic_store(&bdata->ctick, (uint32_t)m_expect(arr->items[1], E_NUM).num); */
                        /* if (bdata->ft->is_c && bdata->initialized)
                                START_DETACHED_PTHREAD(libclang_threaded_highlight, bdata); */
                        break;
                case EVENT_BUF_DETACH:
                        /* destroy_bufdata(&bdata); */
                        START_DETACHED_PTHREAD(destroy_bufdata_wrap, &bdata);
                        eprintf("Detaching from buffer %d\n", bufnum);
                        break;
                default:
                        abort();
                }
                /* super_debug(bdata); */
        }

        /* pthread_mutex_unlock(&event_mutex); */
        mpack_destroy_object(event);
        /* pthread_exit(); */
}

static void *destroy_bufdata_wrap(void *vdata)
{
        destroy_bufdata(vdata);
        pthread_exit();
}

static void *vimscript_interrupt_wrap(void *vdata)
{
        vimscript_interrupt((int)((uintptr_t)vdata));
        pthread_exit();
}


/*======================================================================================*/

#define TMP_SPRINTF(FMT, ...)                                     \
        __extension__({                                           \
                char tmp_[PATH_MAX + 1];                          \
                snprintf(tmp_, PATH_MAX + 1, (FMT), __VA_ARGS__); \
                tmp_;                                             \
        })

static void
super_debug(struct bufdata *bdata)
{
        const unsigned ctick = nvim_buf_get_changedtick(0, bdata->num);
        const unsigned n     = nvim_buf_line_count(0, bdata->num);
        /* if (bdata->ctick != ctick)
                return; */

        b_list *lines = nvim_buf_get_lines(,bdata->num);
        bstring *j1 = b_list_join(lines, B("\n"));
        bstring *j2 = ll_join(bdata->lines, '\n');

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

        /* else
                eprintf("All is well.\n"); */
}

#ifdef DEBUG
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
handle_line_event(struct bufdata *bdata, mpack_obj **items)
{
        assert(!items[5]->data.boolean);
        static mtx_t handle_mutex;
        mtx_lock(&handle_mutex);
        /* pthread_mutex_lock(&bdata->mut); */

        pthread_mutex_lock(&bdata->lock.ctick);
        const unsigned new_tick = (unsigned)m_expect(items[1], E_NUM).num;
        const unsigned old_tick = atomic_load(&bdata->ctick);
        if (new_tick > old_tick)
                atomic_store(&bdata->ctick, new_tick);
        pthread_mutex_unlock(&bdata->lock.ctick);

        const int first     = (int)m_expect(items[2], E_NUM).num;
        const int last      = (int)m_expect(items[3], E_NUM).num;
        const int diff      = last - first;
        b_list   *repl_list = m_expect(items[4], E_STRLIST).ptr;
        bool      empty     = false;
        items[4]->data.arr  = NULL;

        pthread_mutex_lock(&bdata->lines->lock);

        if (repl_list->qty) {
                if (last == (-1)) {
                        /* An "initial" update, recieved only if asked for when attaching
                         * to a buffer. We never ask for this, so this shouldn't occur. */
                        errx(1, "Got initial update somehow...");
                } else if (bdata->lines->qty <= 1 && first == 0 && /* Empty buffer... */
                           repl_list->qty == 1 &&                  /* one string...   */
                           repl_list->lst[0]->slen == 0            /* which is emtpy. */
                           ) {
                        /* Useless update, one empty string in an empty buffer. */
                        empty = true;
                } else if (first == 0 && last == 0) {
                        /* Inserting above the first line in the file. */
                        /* ll_insert_blist_before(bdata->lines, bdata->lines->head, repl_list, 0, -1); */
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
         * supposed to indicate. I'll just ignore them.
         *
         * Neovim always considers there to be at least one line in any buffer.
         * An empty buffer therefore must have one empty line. */
        if (bdata->lines->qty == 0)
                ll_append(bdata->lines, b_fromlit(""));

        if (!bdata->initialized && !empty)
                bdata->initialized = true;

        /* LINE_EVENT_DEBUG(); */
#ifdef DEBUG
        START_DETACHED_PTHREAD(emergency_debug, bdata);
#endif

        xfree(repl_list->lst);
        xfree(repl_list);
        pthread_mutex_unlock(&bdata->lines->lock);
        mtx_unlock(&handle_mutex);
        /* pthread_mutex_unlock(&bdata->mut); */
        if (!empty && bdata->ft->is_c)
                START_DETACHED_PTHREAD(libclang_threaded_highlight, bdata);
}

#if 0

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

        volatile ll_node *volatile node = ll_at(bdata->lines, first);

        /* This loop is only meaningful when replacing lines.
         * All other paths break after the first iteration. */
        for (int i = 0; i < iters; ++i) {
                if (diff && i < olen) {
                        --diff;
                        if (i < (int)repl_list->qty) {
                                b_free(node->data);
                                node->data        = repl_list->lst[i];
                                repl_list->lst[i] = NULL;
                                if (node)
                                        node = node->next;
                        } else {
                                ll_delete_range(bdata->lines, node, diff+1);
                                break;
                        }
                } else {
                        /* If the first line not being replaced (first + i) is at the end
                         * of the file, then we append. Otherwise the update must be prepended.  */
                        if ((first + i) >= bdata->lines->qty)
                                ll_insert_blist_after(bdata->lines, node, repl_list, i, -1);
                        else
                                ll_insert_blist_before(bdata->lines, node, repl_list, i, -1);
                        break;
                }
        }
}

#else /* Pointless garbage */

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
#endif

/*======================================================================================*/

/**
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided clear command.
 */
static void
vimscript_interrupt(const int val)
{
        static mtx_t      vs_mutex;
        static atomic_int bufnum = ATOMIC_VAR_INIT(-1);
        struct timer      t;
        int               num = 0;

        mtx_lock(&vs_mutex);

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
                struct bufdata *bdata = find_buffer(num);

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
                struct bufdata *bdata = find_buffer(num);

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
                extern pthread_t top_thread;
                clear_highlight();
                /* p99_count_dec(&vimscript_lock); */
                mtx_unlock(&vs_mutex);
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
        case 'F': {
                bufnum                = nvim_get_current_buf();
                struct bufdata *bdata = find_buffer(bufnum);
                update_taglist(bdata, UPDATE_TAGLIST_FORCE);
                update_highlight(bdata, HIGHLIGHT_UPDATE);
                break;
        }
        default:
                echo("Hmm, nothing to do...");
                break;
        }

        /* p99_count_dec(&vimscript_lock); */
        mtx_unlock(&vs_mutex);
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

void
_b_list_dump_nvim(const b_list *list, const char *const listname)
{
        echo("Dumping list \"%s\"\n", listname);
        for (unsigned i = 0; i < list->qty; ++i)
                echo("%s\n", BS(list->lst[i]));
}
