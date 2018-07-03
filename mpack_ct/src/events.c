#include "mytags.h"
#include <fcntl.h>

#include "data.h"
#include "mpack.h"

#define BT bt_init
#define MIN(A, B) (((A) <= (B)) ? (A) : (B))

extern int sockfd;
extern FILE *mpack_log;
extern pthread_mutex_t event_mutex;

static const struct event_type {
        const bstring name;
        const enum events {
                EVENT_BUF_LINES,
                EVENT_BUF_CHANGED_TICK,
                EVENT_BUF_DETACH,
                EVENT_COCKSUCKER,
        } id;
} event_list[] = {
        { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
        { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
        { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
        { BT("cocksucker_event"),           EVENT_COCKSUCKER       },
};


static inline void b_write_ll(int fd, linked_list *ll);
static void handle_line_event(uint index, mpack_obj **items);
static void replace_line(uint index, b_list *new_lines, uint lineno, uint replno);
static const struct event_type * id_event(mpack_obj *event);


/*============================================================================*/


void
handle_unexpected_notification(mpack_obj *note)
{
        const struct event_type *type = id_event(note);
        mpack_print_object(note, mpack_log);
        mpack_destroy(note);
}


void
handle_nvim_event(mpack_obj *event)
{
#define D (event->data.arr->items[2]->data.arr->items)

        pthread_mutex_lock(&event_mutex);

        const struct event_type *type   = id_event(event);
        const uint               bufnum = D[0]->data.ext->num;
        const int                index  = find_buffer_ind(bufnum);

        if (type->id == EVENT_COCKSUCKER) {
                echo("NO I DON'T!!!!!");
                return;
        }

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
                nvprintf("Detaching from buffer %d\n", bufnum);
                break;

        default: abort();
        }

        pthread_mutex_unlock(&event_mutex);

#undef D
}


/*============================================================================*/


static void
handle_line_event(const uint index, mpack_obj **items)
{
        assert(!items[5]->data.boolean);
        struct bufdata *bdata  = buffers.lst[index];

        bdata->ctick       = items[1]->data.num;
        unsigned first     = items[2]->data.num;
        unsigned last      = items[3]->data.num;
        b_list * new_lines = mpack_array_to_blist(items[4]->data.arr);
        items[4]->data.arr = NULL;

        nvprintf("first: %u, last: %u, llqty: %d, newqty: %u\n",
                 first, last, bdata->lines->qty, new_lines->qty);

        unsigned diff    = last - first;
        unsigned listlen = new_lines->qty;

        if (bdata->lines->qty == 0) {
                echo("path 1");
                ll_insert_blist_at(after, bdata->lines, 0, new_lines, 0, (-1));

        } else if (listlen == 0) {
                echo("path 2");
                nvprintf("Removing lines %u to %u, bdata has %d lines\n",
                         first, last, bdata->lines->qty);
                ll_delete_range_at(bdata->lines, first, diff);

        } else {
                echo("path 3");
                int olen = bdata->lines->qty;

                for (unsigned i = 0; i < listlen; ++i) {
                        if (diff) {
                                --diff;
                                if (i < listlen) {
                                        echo("Replacing a line");
                                        replace_line(index, new_lines, first + i, i);
                                } else {
                                        ll_delete_range_at(bdata->lines, first + i, diff);
                                        break;
                                }
                        } else {
                                if ((int)(first + i) >= olen)
                                        ll_insert_blist_at(after, bdata->lines, first + i,
                                                           new_lines, i, (-1));
                                else
                                        ll_insert_blist_at(before, bdata->lines,
                                                           first + i, new_lines, i, (-1));
                                break;
                        }
                }
        }

        assert(ll_verify_size(bdata->lines));

        bstring *fn = nvim_call_function(sockfd, b_tmp("tempname"),
                                         MPACK_STRING, NULL, 1);
        int tempfd  = open(BS(fn), O_CREAT|O_WRONLY, 0600);

        b_write_ll(tempfd, bdata->lines);
        close(tempfd);
        b_free(fn);

        echo("Done writing file");
        
        free(new_lines->lst);
        free(new_lines);
}


static void
replace_line(const uint index, b_list *new_lines, const uint lineno, const uint replno)
{
        struct bufdata *bdata = buffers.lst[index];
        ll_node        *node  = ll_at(bdata->lines, lineno);

        nvprintf("Replacing line %u with replno %u, list is %d long and newlist is %u long\n",
                 lineno, replno, bdata->lines->qty, new_lines->qty);

        b_destroy(node->data);

        node->data             = new_lines->lst[replno];
        new_lines->lst[replno] = NULL;
}


/*============================================================================*/


static inline void
b_write_ll(int fd, linked_list *ll)
{
        nvprintf("Writing list, size: %d, head: %p, tail: %p\n",
                 ll->qty, (void *)ll->head, (void *)ll->tail);

        LL_FOREACH_F (ll, node)
                b_write(fd, node->data, b_tmp("\n"));
}


static const struct event_type *
id_event(mpack_obj *event)
{
        const struct event_type *type = NULL;
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
