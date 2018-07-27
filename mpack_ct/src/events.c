#include "util.h"

#include "data.h"
#include "mpack.h"

#define BT bt_init
/* #define WRITE_BUF_UPDATES */

extern pthread_mutex_t event_mutex, update_mutex;

static const struct event_id {
        const bstring name;
        const enum event_types id;
} event_list[] = {
    { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
    { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
    { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
    { BT("vim_event_update"),           EVENT_VIM_UPDATE }
};


#if defined(DEBUG) && defined(WRITE_BUF_UPDATES)
static inline void b_write_ll(int fd, linked_list *ll);
#endif
static void handle_line_event(unsigned index, mpack_obj **items);
static void replace_line(struct bufdata *bdata, b_list *repl_list,
                         unsigned lineno, unsigned replno);
static const struct event_id *id_event(mpack_obj *event);


/*======================================================================================*/


void
handle_unexpected_notification(mpack_obj *note)
{
        UNUSED const struct event_id *type = id_event(note);
        mpack_print_object(note, mpack_log);
        mpack_destroy(note);
}


enum event_types
handle_nvim_event(mpack_obj *event)
{
#define D (event->data.arr->items[2]->data.arr->items)
        pthread_mutex_lock(&event_mutex);
        pthread_mutex_lock(&update_mutex);
        const struct event_id *type = id_event(event);

        if (type->id == EVENT_VIM_UPDATE) {
                /* int *intp = xmalloc(sizeof(int));
                *intp = D[0]->data.str->data[0]; */
                pthread_t         tmp;
                pthread_attr_t    attr;
                struct int_pdata *data = xmalloc(sizeof(*data));
                const int         val  = D[0]->data.str->data[0];
                *data = (struct int_pdata){val, pthread_self()};

                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                pthread_create(&tmp, &attr, interrupt_call, data);
        } else {
                const unsigned bufnum = D[0]->data.ext->num;
                const int      index  = find_buffer_ind(bufnum);

                assert(index >= 0);
                struct bufdata *bdata = buffers.lst[index];
                assert(bdata != NULL);

                switch (type->id) {
                case EVENT_BUF_LINES:
                        handle_line_event(index, D);
                        break;
                case EVENT_BUF_CHANGED_TICK:
                        bdata->ctick = D[1]->data.num;
                        break;
                case EVENT_BUF_DETACH:
                        destroy_bufdata(buffers.lst + index);
                        echo("Detaching from buffer %d\n", bufnum);
                        break;
                default:
                        abort();
                }
        }

        pthread_mutex_unlock(&event_mutex);
        pthread_mutex_unlock(&update_mutex);
        return type->id;
#undef D
}


/*======================================================================================*/


static void
handle_line_event(const unsigned index, mpack_obj **items)
{
        assert(!items[5]->data.boolean);
        struct bufdata *bdata     = buffers.lst[index];
        b_list         *repl_list = mpack_array_to_blist(items[4]->data.arr, true);
        items[4]->data.arr        = NULL;
        bdata->ctick              = items[1]->data.num;
        const unsigned  first     = items[2]->data.num;
        const unsigned  last      = items[3]->data.num;
        unsigned        diff      = (last - first);
        const unsigned  iters     = MAX(diff, repl_list->qty);

        if (repl_list->qty) {
                if (first == 0 && last == 0) {
                        ll_insert_blist_before_at(bdata->lines, first, repl_list, 0, (-1));
                } else {
                        const unsigned olen = bdata->lines->qty;

                        /* This loop is only meaningful when replacing lines. All other
                         * paths break after the first iteration. */
                        for (unsigned i = 0; i < iters; ++i) {
                                if (diff && i < olen) {
                                        --diff;
                                        if (i < repl_list->qty) {
                                                replace_line(bdata, repl_list, first + i, i);
                                        } else {
                                                ll_delete_range_at(bdata->lines, first + i, diff+1);
                                                break;
                                        }
                                } else {
                                        if ((first + i) >= (unsigned)bdata->lines->qty)
                                                ll_insert_blist_after_at(
                                                    bdata->lines, (first + i), repl_list, i, (-1));
                                        else
                                                ll_insert_blist_before_at(
                                                    bdata->lines, (first + i), repl_list, i, (-1));

                                        break;
                                }
                        }
                }
        } else if (first != last) {
                ll_delete_range_at(bdata->lines, first, diff);
        }

        /* Neovim always considers there to be at least one line in any buffer.
         * An empty buffer therefore must have one empty line. */
        if (bdata->lines->qty == 0)
                ll_append(bdata->lines, b_fromlit(""));

#ifdef DEBUG
#  ifdef WRITE_BUF_UPDATES
        bstring *fn = nvim_call_function(0, B("tempname"), MPACK_STRING, NULL, 1);
        int tempfd  = open(BS(fn), O_CREAT|O_WRONLY|O_TRUNC|O_BINARY, 0600);

        b_write_ll(tempfd, bdata->lines);
        close(tempfd);
        echo("Done writing file - %s", BS(fn));
        b_free(fn);

#  endif

        assert(ll_verify_size(bdata->lines));
        const unsigned ctick = nvim_buf_get_changedtick(0, bdata->num, 1);
        const int      n     = nvim_buf_line_count(0, bdata->num);

        if (bdata->ctick == ctick) {
                if (bdata->lines->qty != n)
                        errx(1, "Internal line count (%d) is incorrect. Actual: %d. Aborting",
                             bdata->lines->qty, n);
        } else {
#  if 0
                if (bdata->lines->qty != n)
                        echo("Internal line count (%d) is incorrect. Actual: %d",
                             bdata->lines->qty, n);
#  endif
        }
#endif

        free(repl_list->lst);
        free(repl_list);
        /* echo("\n\n\n"); */
}


static void
replace_line(struct bufdata *bdata, b_list *repl_list,
             const unsigned lineno, const unsigned replno)
{
        ll_node *node = ll_at(bdata->lines, lineno);

        /* echo("Replacing line %u with replno %u, list is %d long and newlist is %u long\n",
             lineno, replno, bdata->lines->qty, repl_list->qty); */

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
