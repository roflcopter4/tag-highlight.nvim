#include "util/util.h"

#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "clang/clang.h"
#include <signal.h>

#include "p99/p99_futex.h"
#include "p99/p99_threads.h"

/*======================================================================================*/
/* Main Event Loop */
/*======================================================================================*/

extern void update_line(struct bufdata *, int, int);

static void  handle_nvim_response (int fd, mpack_obj *obj, volatile p99_futex *fut);
static void  notify_waiting_thread(mpack_obj *obj, struct nvim_wait *cur);
static void  handle_line_event    (struct bufdata *bdata, mpack_obj **items);
static void  replace_line         (struct bufdata *bdata, b_list *repl_list, unsigned lineno, unsigned replno);
static void  make_update_thread   (struct bufdata *bdata, int first, int last);

static noreturn void event_loop       (void);
static void          interrupt_call   (void *vdata);
static void         *handle_nvim_event(mpack_obj *event);
static const struct event_id *id_event  (mpack_obj *event);

#if 0
extern pthread_cond_t event_loop_cond;
pthread_cond_t event_loop_cond = PTHREAD_COND_INITIALIZER;
extern p99_futex event_loop_futex;
p99_futex event_loop_futex = P99_FUTEX_INITIALIZER(0);
extern pthread_rwlock_t event_loop_rwlock;
pthread_rwlock_t event_loop_rwlock = PTHREAD_RWLOCK_INITIALIZER;
extern p99_futex event_loop_futexes[3];
p99_futex event_loop_futexes[3] = {P99_SEQ(P99_FUTEX_INITIALIZER, 0, 0, 0)};
__attribute__((__constructor__)) static void futex_init(void) { p99_futex_init(&event_loop_futex, 0); }
#endif

extern volatile p99_futex event_loop_futex;
volatile p99_futex        event_loop_futex        = P99_FUTEX_INITIALIZER(0);
static pthread_once_t     event_loop_once_control = PTHREAD_ONCE_INIT;

/*======================================================================================*/

noreturn static void *
do_launch_event_loop(UNUSED void *notused)
{
        pthread_once(&event_loop_once_control, event_loop);
        pthread_exit(NULL);
}
void
launch_event_loop(void)
{
        START_DETACHED_PTHREAD(do_launch_event_loop, NULL);
}

static noreturn void
event_loop(void)
{
#if 0
        static int count = 0;
        /* static pthread_mutex_t el_mutex = PTHREAD_MUTEX_INITIALIZER; */
        int fd;
        if (vdata)
                fd = *(int *)vdata;
        else
#endif
        /* pthread_mutex_lock(&el_mutex); */
        /* p99_futex *fut = &event_loop_futexes[count++]; */
        /* pthread_mutex_unlock(&el_mutex); */

        const int fd = 1;

        for (;;) {
                mpack_obj *obj = decode_stream(fd);
                if (!obj)
                        errx(1, "Got NULL object from decoder, cannot continue.");

                const enum message_types mtype = m_expect(m_index(obj, 0), E_NUM, false).num;

                switch (mtype) {
                case MES_NOTIFICATION:
                        START_DETACHED_PTHREAD((void *(*)(void *))handle_nvim_event, obj);
                        /* handle_nvim_event(obj); */
                        /* mpack_destroy(obj); */
                        break;
                case MES_RESPONSE:
                        handle_nvim_response(fd, obj, &event_loop_futex);
                        break;
                case MES_REQUEST:
                default:
                        abort();
                }
        }
}

static void
handle_nvim_response(const int fd, mpack_obj *obj, volatile p99_futex *fut)
{
        p99_futex_wait(fut);
        for (unsigned i = 0; i < wait_list->qty; ++i) {
                struct nvim_wait *cur = wait_list->lst[i];
                if (!cur)
                        errx(1, "Got an invalid wait list object in %s\n", FUNC_NAME);
                if (cur->fd == fd) {
                        const int count = (int)m_expect(m_index(obj, 1), E_NUM, false).num;
                        if (cur->count == count) {
                                notify_waiting_thread(obj, cur);
                                return;
                        }
                }
        }

        errx(1, "Object not found");
}

static void
notify_waiting_thread(mpack_obj *obj, struct nvim_wait *cur)
{
        const unsigned fut_expect_ = NVIM_GET_FUTEX_EXPECT(cur->fd, cur->count);
        cur->obj                   = obj;
        P99_FUTEX_COMPARE_EXCHANGE(&cur->fut, val, true, fut_expect_, 0, P99_FUTEX_MAX_WAITERS);
}

/*======================================================================================*/

/* 
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided clear command.
 */
static void
interrupt_call(void *vdata)
{
        struct timer      t;
        static int        bufnum = (-1);
        struct int_pdata *data   = vdata;
        if (data->val != 'H')
                echo("Recieved \"%c\"; waking up!", data->val);

        switch (data->val) {
        /*
         * New buffer was opened or current buffer changed.
         */
        case 'A':  /* FIXME Fix these damn letters, they've gotten totally out of order. */
        case 'D': {
                const int prev = bufnum;
                TIMER_START_BAR(t);
                bufnum                = nvim_get_current_buf(0);
                struct bufdata *bdata = find_buffer(bufnum);

                if (!bdata) {
                try_attach:
                        if (new_buffer(0, bufnum)) {
                                nvim_buf_attach(BUFFER_ATTACH_FD, bufnum);
                                bdata = find_buffer(bufnum);

                                get_initial_lines(bdata);
                                get_initial_taglist(bdata);
                                update_highlight(bufnum, bdata);

                                TIMER_REPORT(t, "initialization");
                        }
                } else if (prev != bufnum) {
                        if (!bdata->calls)
                                get_initial_taglist(bdata);

                        update_highlight(bufnum, bdata);
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
                bufnum                 = nvim_get_current_buf(0);
                struct bufdata *bdata  = find_buffer(bufnum);

                if (!bdata) {
                        echo("Failed to find buffer! %d -> p: %p\n",
                             bufnum, (void *)bdata);
                        goto try_attach;
                }

                if (update_taglist(bdata, (data->val == 'F'))) {
                        clear_highlight(nvim_get_current_buf(0));
                        update_highlight(bufnum, bdata);
                        TIMER_REPORT(t, "update");
                }

                break;
        }
        /*
         * User called the kill command.
         */
        case 'C': {
                clear_highlight(nvim_get_current_buf(0));
                extern pthread_t top_thread;
#ifdef DOSISH
                pthread_kill(top_thread, SIGTERM);
#else
                pthread_kill(top_thread, SIGUSR1);
                /* raise(SIGUSR1); */
#endif
                xfree(vdata);
                pthread_exit(NULL);
                /* break; */
        }
        /*
         * User called the clear highlight command.
         */
        case 'E':
                clear_highlight(nvim_get_current_buf(0));
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
                bufnum                = nvim_get_current_buf(0);
                struct bufdata *bdata = find_buffer(bufnum);
                update_highlight(bufnum, bdata, true);
                break;
        }
        default:
                ECHO("Hmm, nothing to do...");
                break;
        }

        xfree(vdata);
}

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

#define BT bt_init
#if defined(DEBUG) && defined(WRITE_BUF_UPDATES)
static inline void b_write_ll(int fd, linked_list *ll);
#endif

extern FILE           *main_log;
extern pthread_mutex_t update_mutex;

static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;
static const struct event_id {
        const bstring          name;
        const enum event_types id;
} event_list[] = {
    { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
    { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
    { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
    { BT("vim_event_update"),           EVENT_VIM_UPDATE       },
};

/*======================================================================================*/

void
handle_unexpected_notification(mpack_obj *note)
{
        mpack_print_object(mpack_log, note);
        fflush(mpack_log);
        if (!note)
                ECHO("Object is null!!!!");
        else if (note->DAI[0]->data.num == MES_NOTIFICATION) {
                pthread_t tid;
                pthread_create(&tid, NULL, (void *(*)(void *))handle_nvim_event, note);
                pthread_join(tid, NULL);
        } else
                echo("Object isn't a notification at all! -> %ld", note->DAI[0]->data.num);
}

FILE *api_buffer_log;
__attribute__((__constructor__, __used__)) static void
openbufferlog(void)
{
        api_buffer_log = safe_fopen_fmt("%s/.tag_highlight_log/buf.log", "wb",
                                        getenv("HOME"));
}
/* static void closebufferlog(void) { if (buffer_log) fclose(buffer_log); } __attribute__((__destructor__)) */

static void *
handle_nvim_event(mpack_obj *event)
{
        if (!event)
                pthread_exit(NULL);

        mpack_array_t *arr = m_expect(m_index(event, 2), E_MPACK_ARRAY, false).ptr;

#ifdef DEBUG
        mpack_print_object(api_buffer_log, event);
#endif
        const struct event_id *type = id_event(event);

        if (type->id == EVENT_VIM_UPDATE) {
                /* The update came from the vimscript plugin. Call the handler defined
                 * in main.c in a separate thread since it might wait a while. */
                struct int_pdata *data = xmalloc(sizeof(*data));
                const int         val  = arr->items[0]->data.str->data[0];
                *data = (struct int_pdata){val, pthread_self()};
                interrupt_call(data);
                mpack_destroy(event);
                /* START_DETACHED_PTHREAD(interrupt_call, data);
                mpack_destroy(event); */
        } else {
                const int       bufnum = (int)m_expect(arr->items[0], E_NUM, false).num;
                struct bufdata *bdata  = find_buffer(bufnum);
                if (!bdata)
                        errx(1, "Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_LINES:
                        if (arr->qty < 5)
                                errx(1, "Array is too small (%d, expect >= 5)", arr->qty);
                        handle_line_event(bdata, arr->items);
                        mpack_destroy(event);
                        break;
                case EVENT_BUF_CHANGED_TICK:
                        bdata->ctick = (unsigned)m_expect(arr->items[1], E_NUM, false).num;
                        mpack_destroy(event);
                        break;
                case EVENT_BUF_DETACH:
                        destroy_bufdata(&bdata);
                        ECHO("Detaching from buffer %d\n", bufnum);
                        mpack_destroy(event);
                        break;
                default:
                        abort();
                }
        }

        pthread_exit(NULL);
}

/*======================================================================================*/

static void
handle_line_event(struct bufdata *bdata, mpack_obj **items)
{
        assert(!items[5]->data.boolean);
        pthread_mutex_lock(&event_mutex);

        bdata->ctick            = m_expect(items[1], E_NUM, false).num;
        const int64_t first     = m_expect(items[2], E_NUM, false).num;
        const int64_t last      = m_expect(items[3], E_NUM, false).num;
        b_list       *repl_list = m_expect(items[4], E_STRLIST, false).ptr;
        int64_t       diff      = (last - first);
        const int64_t iters     = MAX(diff, repl_list->qty);
        bool          empty     = false;
        items[4]->data.arr      = NULL;

        if (repl_list->qty) {
                if (last == (-1)) {
                        ECHO("Got initial update somehow...");
                        abort();
                } else if (bdata->lines->qty <= 1 && first == 0 &&
                           repl_list->qty == 1 && repl_list->lst[0]->slen == 0) {
                        /* Useless update, one empty string in an empty buffer.
                         * Just ignore it. */
                        ECHO("empty update, ignoring");
                        empty = true;
                } else if (first == 0 && last == 0) {
                        /* Inserting above the first line in the file. */
                        ll_insert_blist_before_at(bdata->lines, first,
                                                  repl_list, 0, (-1));
                } else {
                        const unsigned olen = bdata->lines->qty;

                        /* This loop is only meaningful when replacing lines.
                         * All other paths break after the first iteration. */
                        for (unsigned i = 0; i < iters; ++i) {
                                if (diff && i < olen) {
                                        --diff;
                                        if (i < repl_list->qty) {
                                                replace_line(bdata, repl_list,
                                                             first + i, i);
                                        } else {
                                                ll_delete_range_at(
                                                    bdata->lines, first + i, diff+1);
                                                break;
                                        }
                                } else {
                                        /* If the first line not being replaced
                                         * (first + i) is at the end of the file, then we
                                         * append. Otherwise the update is prepended. */
                                        if ((first + i) >= (unsigned)bdata->lines->qty)
                                                ll_insert_blist_after_at(
                                                    bdata->lines, (first + i),
                                                    repl_list, i, (-1));
                                        else
                                                ll_insert_blist_before_at(
                                                    bdata->lines, (first + i),
                                                    repl_list, i, (-1));
                                        break;
                                }
                        }
                }
        } else if (first != last) {
                /* If the replacement list is empty then all we're doing is deleting
                 * lines. However, for some reason neovim sometimes sends updates with an
                 * empty list in which both the first and last line are the same. God
                 * knows what this is supposed to indicate. I'll just ignore them. */
                ll_delete_range_at(bdata->lines, first, diff);
        }

        /* Neovim always considers there to be at least one line in any buffer.
         * An empty buffer therefore must have one empty line. */
        if (bdata->lines->qty == 0)
                ll_append(bdata->lines, b_fromlit(""));

        if (!bdata->initialized && !empty)
                bdata->initialized = true;

#ifdef DEBUG
#  ifdef WRITE_BUF_UPDATES
        bstring *fn = nvim_call_function(0, B("tempname"), MPACK_STRING, NULL, 1);
        int tempfd  = open(BS(fn), O_CREAT|O_WRONLY|O_TRUNC|O_BINARY, 0600);

        b_write_ll(tempfd, bdata->lines);
        close(tempfd);
        ECHO("Done writing file - %s", fn);
        b_free(fn);

#  endif
        const unsigned ctick = nvim_buf_get_changedtick(0, bdata->num);
        const unsigned n     = nvim_buf_line_count(0, bdata->num);

        if (bdata->ctick == ctick) {
                if (bdata->lines->qty != (int)n)
                        errx(1, "Internal line count (%d) is incorrect. Actual: %u. Aborting",
                             bdata->lines->qty, n);
        }
#endif
        xfree(repl_list->lst);
        xfree(repl_list);
        START_DETACHED_PTHREAD(libclang_threaded_highlight, bdata);
#if 0
        const unsigned mx = (bdata->lines->qty > last) ? bdata->lines->qty : last; 
        if (first + mx >= 1)
                update_line(bdata, first, mx);
        const unsigned mx = (bdata->lines->qty > last) ? bdata->lines->qty : last; 
        make_update_thread(bdata, first, mx);
        /* if (first + mx >= 1) { */
                struct lc_thread {
                        struct bufdata *bdata;
                        int       first;
                        int       last;
                        int       ctick;
                } *tdata = malloc(sizeof(struct lc_thread));
                *tdata = (struct lc_thread){bdata, first, mx, bdata->ctick};

                pthread_t      tid;
                pthread_attr_t attr;
                MAKE_PTHREAD_ATTR_DETATCHED(&attr);
                pthread_create(&tid, &attr, &libclang_threaded_highlight, tdata);
        /* } */
#endif

        pthread_mutex_unlock(&event_mutex);
}

static void
replace_line(struct bufdata *bdata, b_list *repl_list,
             const unsigned lineno, const unsigned replno)
{
        ll_node *node = ll_at(bdata->lines, (int)lineno);
        b_destroy(node->data);
        node->data             = repl_list->lst[replno];
        repl_list->lst[replno] = NULL;
}

/*======================================================================================*/

#if defined(DEBUG) && defined(WRITE_BUF_UPDATES)
static inline void
b_write_ll(int fd, linked_list *ll)
{
        echo("Writing list, size: %d, head: %p, tail: %p",
             ll->qty, (void *)ll->head, (void *)ll->tail);

        bool done = false;
        LL_FOREACH_F (ll, node) {
                if (node == ll->tail)
                        done = true;
                if (!done && node != ll->tail)
                        assert(node && node->data);
                b_write(fd, node->data, B("\n"));
        }
}
#endif

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

static void
make_update_thread(struct bufdata *bdata, const int first, const int last)
{
        struct lc_thread {
                struct bufdata *bdata;
                int             first;
                int             last;
                unsigned        ctick;
        } *tdata = xmalloc(sizeof(struct lc_thread));
        *tdata = (struct lc_thread){bdata, first, last, bdata->ctick};

        pthread_t      tid;
        pthread_attr_t attr;
        MAKE_PTHREAD_ATTR_DETATCHED(&attr);
        pthread_create(&tid, &attr, &libclang_threaded_highlight, tdata);
}
