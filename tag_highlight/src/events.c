#include "util/util.h"

#include "data.h"
#include "api.h"
#include "mpack/mpack.h"

#define BT bt_init
/* #define WRITE_BUF_UPDATES */

extern FILE *main_log;
extern pthread_mutex_t update_mutex;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;

static const struct event_id {
        const bstring name;
        const enum event_types id;
} event_list[] = {
    { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
    { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
    { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
    { BT("vim_event_update"),           EVENT_VIM_UPDATE       },
};


#if defined(DEBUG) && defined(WRITE_BUF_UPDATES)
static inline void b_write_ll(int fd, linked_list *ll);
#endif
static void handle_line_event(struct bufdata *bdata, mpack_obj **items);
static void replace_line(struct bufdata *bdata, b_list *repl_list,
                         unsigned lineno, unsigned replno);
static const struct event_id *id_event(mpack_obj *event);


/*======================================================================================*/


void
handle_unexpected_notification(mpack_obj *note)
{
        mpack_print_object(mpack_log, note);
        fflush(mpack_log);
        if (!note)
                ECHO("Object is null!!!!");
        else if (note->DAI[0]->data.num == MES_NOTIFICATION)
                handle_nvim_event(note);
        else
                echo("Object isn't a notification at all! -> %ld", note->DAI[0]->data.num);

        mpack_destroy(note);
}

extern void update_line(struct bufdata *, int, int);

enum event_types
handle_nvim_event(mpack_obj *event)
{
        if (!event)
                return (-1);

        mpack_array_t *arr = m_expect(m_index(event, 2), E_MPACK_ARRAY, false).ptr;

#ifdef DEBUG
        mpack_print_object(main_log, event);
#endif
        const struct event_id *type = id_event(event);

        if (type->id == EVENT_VIM_UPDATE) {
                /* The update came from the vimscript plugin. Call the handler defined
                 * in main.c in a separate thread since it might wait a while. */
                pthread_t         tmp;
                pthread_attr_t    attr;
                struct int_pdata *data = xmalloc(sizeof(*data));
                const int         val  = arr->items[0]->data.str->data[0];
                *data = (struct int_pdata){val, pthread_self()};

                MAKE_PTHREAD_ATTR_DETATCHED(&attr);
                pthread_create(&tmp, &attr, interrupt_call, data);
        } else {
                const unsigned  bufnum = m_expect(arr->items[0], E_NUM, false).num;
                struct bufdata *bdata  = find_buffer(bufnum);
                if (!bdata)
                        errx(1, "Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_LINES:
                        if (arr->qty < 5)
                                errx(1, "Array is too small (%d, expect >= 5)", arr->qty);
                        handle_line_event(bdata, arr->items);
                        break;
                case EVENT_BUF_CHANGED_TICK:
                        bdata->ctick = m_expect(arr->items[1], E_NUM, false).num;
                        break;
                case EVENT_BUF_DETACH:
                        destroy_bufdata(&bdata);
                        ECHO("Detaching from buffer %d\n", bufnum);
                        break;
                default:
                        abort();
                }
        }

        return type->id;
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
        /* assert(ll_verify_size(bdata->lines)); */
        const unsigned ctick = nvim_buf_get_changedtick(0, bdata->num);
        const int      n     = nvim_buf_line_count(0, bdata->num);

        if (bdata->ctick == ctick) {
                if (bdata->lines->qty != n)
                        errx(1, "Internal line count (%d) is incorrect. Actual: %d. Aborting",
                             bdata->lines->qty, n);
        }
#endif

        free(repl_list->lst);
        free(repl_list);
        if (first + last >= 1)
                update_line(bdata, first, last);
        pthread_mutex_unlock(&event_mutex);
}


static void
replace_line(struct bufdata *bdata, b_list *repl_list,
             const unsigned lineno, const unsigned replno)
{
        ll_node *node = ll_at(bdata->lines, lineno);
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
