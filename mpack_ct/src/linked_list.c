#include "mytags.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "linked_list.h"

#define V(PTR_) ((void *)(PTR_))


static void free_data(ll_node *node);


linked_list *
ll_make_new(void)
{
        linked_list *list = xmalloc(sizeof *list);
        list->head = NULL;
        list->tail = NULL;
        list->qty  = 0;
        return list;
}


void
ll_add(linked_list *list, bstring *data)
{
        ll_node *node = xmalloc(sizeof *node);

        if (list->head)
                list->head->prev = node;
        if (!list->tail)
                list->tail = node;

        node->data = data;
        node->prev = NULL;
        node->next = list->head;
        list->head = node;
        ++list->qty;
}


void
ll_append(linked_list *list, bstring *data)
{
        ll_node *node = xmalloc(sizeof *node);

        if (list->tail)
                list->tail->next = node;
        if (!list->head)
                list->head = node;

        node->data = data;
        node->prev = list->tail;
        node->next = NULL;
        list->tail = node;
        ++list->qty;
}


/*============================================================================*/


void
ll_insert_after(linked_list *list, ll_node *at, bstring *data)
{
        ll_node *node = xmalloc(sizeof *node);
        node->data    = data;
        node->prev    = at;

        if (at) {
                node->next = at->next;
                at->next   = node;
                if (node->next)
                        node->next->prev = node;
        } else {
                node->next = NULL;
        }

        if (!list->head)
                list->head = node;
        if (!list->tail || at == list->tail)
                list->tail = node;

        /* This shuts up clang's whining about a potential memory leak. */
        assert((at && at->next == node) || list->tail == node);
        ++list->qty;
}


void
ll_insert_before(linked_list *list, ll_node *at, bstring *data)
{
        ll_node *node = xmalloc(sizeof *node);
        node->data    = data;
        node->next    = at;

        if (at) {
                node->prev = at->prev;
                at->prev   = node;
                if (node->prev)
                        node->prev->next = node;
        } else {
                node->prev = NULL;
        }

        if (!list->head || at == list->head)
                list->head = node;
        if (!list->tail)
                list->tail = node;

        /* This shuts up clang's whining about a potential memory leak. */
        assert((at && at->prev == node) || list->head == node);
        ++list->qty;
}


/*============================================================================*/


void
ll_insert_blist_after(linked_list *list, ll_node *at, b_list *blist, int start, int end)
{
        eprintf("Start: %d, End: %d\n", start, end);
        start = (start >= 0) ? start : (start + (int)blist->qty + 1);
        end   = (end   >= 0) ? end   : (end   + (int)blist->qty + 1);

        assert(end > 0 && start >= 0 && start < end);
        assert(blist->qty > 0 && (unsigned)end <= blist->qty);  

        if ((end - start) == 1) {
                ll_insert_after(list, at, blist->lst[start]);
                return;
        }

        ll_node **tmp = nmalloc((end - start), sizeof *tmp);
        tmp[0]        = xmalloc(sizeof **tmp);
        tmp[0]->data  = blist->lst[start];
        tmp[0]->prev  = at;
        int last      = end - 1;

        eprintf("Start: %d, End: %d => at is %p, head: %p, tail: %p, qty: %d\n",
                start, end, V(at), V(list->head), V(list->tail), list->qty);

        for (int i = (start + 1); i < end; ++i) {
                tmp[i]         = xmalloc(sizeof **tmp);
                tmp[i]->data   = blist->lst[i];
                tmp[i]->prev   = tmp[i-1];
                tmp[i-1]->next = tmp[i];
        }

        if (at) {
                tmp[last]->next = at->next;
                at->next        = tmp[0];
                if (tmp[last]->next)
                        tmp[last]->next->prev = tmp[last];
        } else {
                tmp[last]->next = NULL;
        }

        if (!list->head)
                list->head = tmp[0];
        if (!list->tail || at == list->tail)
                list->tail = tmp[last];

        list->qty += (end - start);
        free(tmp);

        eprintf("NOW at is %p, head: %p, tail: %p, qty: %d\n",
                V(at), V(list->head), V(list->tail), list->qty);
}


void
ll_insert_blist_before(linked_list *list, ll_node *at, b_list *blist, int start, int end)
{
        eprintf("Start: %d, End: %d\n", start, end);
        start = (start >= 0) ? start : (start + (int)blist->qty + 1);
        end   = (end   >= 0) ? end   : (end   + (int)blist->qty + 1);

        assert(end > 0 && start >= 0 && start < end);
        assert(blist->qty > 0 && (unsigned)end <= blist->qty);

        if ((end - start) == 1) {
                ll_insert_before(list, at, blist->lst[start]);
                return;
        }

        ll_node **tmp = nmalloc(blist->qty, sizeof *tmp);
        tmp[0]        = xmalloc(sizeof **tmp);
        tmp[0]->data  = blist->lst[start];
        int last      = end - 1;

        eprintf("Start: %d, End: %d => at is %p, head: %p, tail: %p, qty: %d\n",
                start, end, V(at), V(list->head), V(list->tail), list->qty);

        for (int i = (start + 1); i < end; ++i) {
                tmp[i]         = xmalloc(sizeof **tmp);
                tmp[i]->data   = blist->lst[i];
                tmp[i]->prev   = tmp[i-1];
                tmp[i-1]->next = tmp[i];
        }

        tmp[last]->next = at;

        if (at) {
                tmp[0]->prev = at->prev;
                at->prev     = tmp[last];
                if (tmp[0]->prev)
                        tmp[0]->prev->next = tmp[0];
        } else {
                tmp[0]->prev = NULL;
        }

        if (!list->head || at == list->head)
                list->head = tmp[0];
        if (!list->tail)
                list->tail = tmp[last];

        list->qty += (end - start);
        free(tmp);

        eprintf("NOW at is %p, head: %p, tail: %p, qty: %d\n",
                V(at), V(list->head), V(list->tail), list->qty);
}


void
ll_delete_range(linked_list *list, ll_node *at, unsigned range)
{
        if (range == 0)
                return;

        eprintf("removing: at is %p, head: %p, tail: %p, qty: %d\n",
                V(at), V(list->head), V(list->tail), list->qty);

        ll_node *current = at;
        ll_node *next    = NULL;
        ll_node *prev    = (at) ? at->prev : NULL;
        
        bool replace_head = (at == list->head || !list->head);
        
        for (uint64_t i = 0; i < range && current; ++i) {
                next = current->next;
                free_data(current);
                free(current);
                current = next;
                --list->qty;
        }

        list->qty = (list->qty < 0) ? 0 : list->qty;

        if (replace_head)
                list->head = next;
        if (!next)
                list->tail = prev;

        if (prev)
                prev->next = next;
        if (next)
                next->prev = prev;


        eprintf("NOW: at is %p, head: %p, tail: %p, qty: %d\n",
                V(at), V(list->head), V(list->tail), list->qty);
}


/*============================================================================*/


bstring *
_ll_popat(linked_list *list, int index, uint8_t type)
{
        ll_node *node = ll_at(list, index);

        switch (type) {
        case DEL_ONLY:
                free_data(node);
                ll_delete_node(list, node);
                return NULL;
        case RET_ONLY:
                return node->data;
        case BOTH:
                ; bstring *data = node->data;
                ll_delete_node(list, node);
                return data;
        default:
                errx(1, "Unreachable!\n");
        }
}


ll_node *
ll_at(linked_list *list, int index)
{
        if (list->qty == 0)
                return NULL;
        if (index == 0)
                return list->head;
        if (index == (-1) || index == list->qty)
                return list->tail;

        if (index < 0)
                index += list->qty;
        if (index < 0 || index > list->qty)
                errx(1, "index: %d, qty %d", index, list->qty);

        ll_node *current;

        if (index < ((list->qty - 1) / 2)) {
                int x = 0;
                current = list->head;

                while (x++ != index)
                        current = current->next;
        } else {
                int x = list->qty - 1;
                current = list->tail;

                while (x-- != index)
                        current = current->prev;
        }

        return current;
}


void
ll_destroy(linked_list *list)
{
        if (list->qty == 1) {
                ll_node *current;
                if (list->tail)
                        current = list->tail;
                else
                        current = list->head;
                free_data(current);
                free(current);
        } else if (list->qty > 1) {
                ll_node *current = list->head;
                do {
                        ll_node *tmp = current;
                        current      = current->next;
                        free_data(tmp);
                        free(tmp);
                } while (current);
        }

        free(list);
}


void
ll_delete_node(linked_list *list, ll_node *node)
{
        assert(node != NULL);

        if (list->qty == 1) {
                list->head = list->tail = NULL;
        } else if (node == list->head) {
                list->head = node->next;
                list->head->prev = NULL;
        } else if (node == list->tail) {
                list->tail = node->prev;
                list->tail->next = NULL;
        } else {
                node->prev->next = node->next;
                node->next->prev = node->prev;
        }

        --list->qty;
        free(node);
}


/*============================================================================*/


bool
ll_verify_size(linked_list *list)
{
        int cnt = 0;
        LL_FOREACH_F (list, node)
                ++cnt;

        bool ret = (cnt == list->qty);

        if (!ret)
                list->qty = cnt;

        return ret;
}


/*============================================================================*/


ll_node *
ll_find_bstring(const linked_list *const list, const bstring *const find)
{
        LL_FOREACH_F (list, node)
                if (b_iseq(find, node->data))
                        return node;

        return NULL;
}


static void
free_data(ll_node *node)
{
        b_destroy(node->data);
}
