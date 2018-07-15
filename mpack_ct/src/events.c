#include "util.h"
#include <fcntl.h>

#include "data.h"
#include "mpack.h"

#define BT bt_init

extern pthread_mutex_t event_mutex, update_mutex;

static const struct event_id {
        const bstring name;
        const enum event_types id;
} event_list[] = {
    { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
    { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
    { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
};


static inline void b_write_ll(int fd, linked_list *ll);
static        void handle_line_event(unsigned index, mpack_obj **items);
static        void replace_line(unsigned index, b_list *new_lines,
                                unsigned lineno, unsigned replno);
static const struct event_id * id_event(mpack_obj *event);


/*============================================================================*/


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
        const struct event_id *type   = id_event(event);
        const unsigned         bufnum = D[0]->data.ext->num;
        const int              index  = find_buffer_ind(bufnum);

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

        pthread_mutex_unlock(&event_mutex);
        pthread_mutex_unlock(&update_mutex);
        return type->id;
#undef D
}


/*============================================================================*/


static void
handle_line_event(const unsigned index, mpack_obj **items)
{
        assert(!items[5]->data.boolean);
        struct bufdata *bdata     = buffers.lst[index];
        bdata->ctick              = items[1]->data.num;
        const unsigned  first     = items[2]->data.num;
        const unsigned  last      = items[3]->data.num;
        unsigned        diff      = (last - first);
        b_list         *new_lines = mpack_array_to_blist(items[4]->data.arr, true);
        const unsigned  iters     = MAX(diff, new_lines->qty);
        items[4]->data.arr        = NULL;

        echo("first: %u, last: %u, llqty: %d, newqty: %u\n",
             first, last, bdata->lines->qty, new_lines->qty);

        if (bdata->lines->qty == 0) {
                ll_insert_blist_after_at(bdata->lines, 0, new_lines, 0, (-1));
        } else if (new_lines->qty == 0) {
                if (first == last) {
                        echo("ERROR: First (%d) == last (%d)!\n", first, last);
                } else {
                        echo("Removing lines %u to %u, bdata has %d lines\n",
                             first, last, bdata->lines->qty);
                        ll_delete_range_at(bdata->lines, first, diff);
                }
        } else {
                const unsigned olen = bdata->lines->qty;

                for (unsigned i = 0; i < iters; ++i) {
                        if (diff) {
                                --diff;
                                if (i < new_lines->qty) {
                                        replace_line(index, new_lines, first + i, i);
                                } else {
                                        echo("Removing lines %u to %u, bdata has %d lines\n",
                                             first + i, last, bdata->lines->qty);
                                        ll_delete_range_at(bdata->lines, first + i, last);
                                        echo("Now there are %d lines\n", bdata->lines->qty);
                                        break;
                                }
                        } else {
                                if ((first + i) >= olen) {
                                        echo("after!");
                                        ll_insert_blist_after_at(bdata->lines, first + i, new_lines, i, (-1));
                                } else {
                                        echo("before!");
                                        ll_insert_blist_before_at(bdata->lines, first + i, new_lines, i, (-1));
                                }

                                break;
                        }
                }
        }

        assert(ll_verify_size(bdata->lines));
        unsigned ctick = nvim_buf_get_changedtick(0, bdata->num, 1);
        if (bdata->ctick == ctick)
                assert((unsigned)bdata->lines->qty == nvim_buf_line_count(0, bdata->num));

        /* const unsigned linecount = nvim_buf_line_count(0, bdata->num);
        ASSERTX(linecount == (unsigned)bdata->lines->qty,
                "ERROR: Linecount: %u, qty: %d! (odiff %u)",
                linecount, bdata->lines->qty, odiff); */

#if 0
#ifdef DEBUG
        bstring *fn = nvim_call_function(0, b_tmp("tempname"), MPACK_STRING, NULL, 1);
        int tempfd  = open(BS(fn), O_CREAT|O_WRONLY|O_TRUNC|O_BINARY, 0600);

        b_write_ll(tempfd, bdata->lines);
        close(tempfd);
        b_free(fn);

        echo("Done writing file");
#endif
#endif
        
        free(new_lines->lst);
        free(new_lines);
}


static void
replace_line(const unsigned index, b_list *new_lines,
             const unsigned lineno, const unsigned replno)
{
        struct bufdata *bdata = buffers.lst[index];
        ll_node        *node  = ll_at(bdata->lines, lineno);

        /* echo("Replacing line %u with replno %u, list is %d long and "
                 "newlist is %u long\n",
                 lineno, replno, bdata->lines->qty, new_lines->qty); */

        b_destroy(node->data);

        node->data             = new_lines->lst[replno];
        new_lines->lst[replno] = NULL;
}


/*============================================================================*/


static inline void
b_write_ll(int fd, linked_list *ll)
{
        echo("Writing list, size: %d, head: %p, tail: %p\n",
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


static const struct event_id *
id_event(mpack_obj *event)
{
        const struct event_id *type = NULL;
        bstring *typename = event->data.arr->items[1]->data.str;

        for (unsigned i = 0; i < ARRSIZ(event_list); ++i)
                if (b_iseq(typename, &event_list[i].name)) {
                        type = &event_list[i];
                        break;
                }

        if (!type)
                errx(1, "Failed to identify event type.\n");

        return type;
}
