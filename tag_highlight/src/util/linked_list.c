
#include "util/util.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util/list.h"

#define V(PTR) ((void *)(PTR))
#define MSG1()                                                                                  \
        do {                                                                                    \
                echo("Start: %d, End: %d, diff: %d => at is %p, head: %p, tail: %p, qty: %d\n", \
                     start, end, diff, V(at), V(list->head), V(list->tail), list->qty);         \
        } while (0)
#define MSG2()                                                             \
        do {                                                               \
                echo("NOW at is %p, head: %p, tail: %p, qty: %d\n", V(at), \
                     V(list->head), V(list->tail), list->qty);             \
        } while (0)

#define ASSERTIONS()                                                          \
        ASSERTX((start >= 0 && (end > 0 || start == end)),                    \
                "Start (%d), end (%d)", start, end);                          \
        ASSERTX(start <= end, "Start (%d) is not < end (%d)!", start, end);   \
        ASSERTX((blist->qty > 0), "blist->qty (%u) is not > 0!", blist->qty); \
        ASSERTX((unsigned)end <= blist->qty,                                  \
                "End (%d) is not <= blist->qty (%u)!", end, blist->qty)

ALWAYS_INLINE void free_data(ll_node *node);
ALWAYS_INLINE void resolve_negative_index(int *index, int base);

static pthread_mutex_t ll_mutex = PTHREAD_MUTEX_INITIALIZER;


linked_list *
ll_make_new(void)
{
        linked_list *list = xmalloc(sizeof *list);
        list->head        = NULL;
        list->tail        = NULL;
        list->qty         = 0;
        return list;
}


void
ll_prepend(linked_list *list, bstring *data)
{
        pthread_mutex_lock(&ll_mutex);
        assert(list);
        assert(data && data->data);
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
        pthread_mutex_unlock(&ll_mutex);
}


void
ll_append(linked_list *list, bstring *data)
{
        assert(list);
        assert(data && data->data);
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
        pthread_mutex_lock(&ll_mutex);
        assert(list);
        ll_node *node = xmalloc(sizeof *node);
        node->data    = data;
        node->prev    = at;

        if (at) {
                node->next = at->next;
                at->next   = node;
                if (node->next)
                        node->next->prev = node;
        } else
                node->next = NULL;

        if (!list->head)
                list->head = node;
        if (!list->tail || at == list->tail)
                list->tail = node;

        /* This shuts up clang's whining about a potential memory leak. */
        assert((at && at->next == node) || list->tail == node);
        ++list->qty;
        pthread_mutex_unlock(&ll_mutex);
}


void
ll_insert_before(linked_list *list, ll_node *at, bstring *data)
{
        pthread_mutex_lock(&ll_mutex);
        assert(list);
        ll_node *node = xmalloc(sizeof *node);
        node->data    = data;
        node->next    = at;

        if (at) {
                node->prev = at->prev;
                at->prev   = node;
                if (node->prev)
                        node->prev->next = node;
        } else
                node->prev = NULL;

        if (!list->head || at == list->head)
                list->head = node;
        if (!list->tail)
                list->tail = node;

        /* This shuts up clang's whining about a potential memory leak. */
        assert((at && at->prev == node) || list->head == node);
        ++list->qty;
        pthread_mutex_unlock(&ll_mutex);
}


/*============================================================================*/


static inline int
create_nodes(const int start, const int end, int i,
             linked_list *list, ll_node **tmp, const b_list *blist)
{
        for (int x = (start + 1); x < end; ++x, ++i) {
                assert((unsigned)i < blist->qty);
                assert(blist->lst[x]);
                tmp[i]         = xmalloc(sizeof **tmp);
                tmp[i]->data   = blist->lst[x];
                tmp[i]->prev   = tmp[i-1];
                tmp[i-1]->next = tmp[i];
                ++list->qty;
        }

        return i;
}


void
ll_insert_blist_after(linked_list *list, ll_node *at, b_list *blist, int start, int end)
{
        assert(list && blist && blist->lst);
        /* Resolve any negative indices. */
        resolve_negative_index(&start, (int)blist->qty);
        resolve_negative_index(&end,   (int)blist->qty);
        /* ASSERTIONS(); */

        const int diff = end - start;
        if (diff == 1) {
                ll_insert_after(list, at, blist->lst[start]);
                return;
        }
        pthread_mutex_lock(&ll_mutex);

        ll_node **tmp  = nmalloc(diff, sizeof *tmp);
        tmp[0]         = xmalloc(sizeof **tmp);
        tmp[0]->data   = blist->lst[start];
        tmp[0]->prev   = at;
        ++list->qty;
        const int last = create_nodes(start, end, 1, list, tmp, blist) - 1;

        if (at) {
                tmp[last]->next = at->next;
                at->next        = tmp[0];
                if (tmp[last]->next)
                        tmp[last]->next->prev = tmp[last];
        } else
                tmp[last]->next = NULL;

        if (!list->head)
                list->head = tmp[0];
        if (!list->tail || at == list->tail)
                list->tail = tmp[last];

        xfree(tmp);
        pthread_mutex_unlock(&ll_mutex);
}


void
ll_insert_blist_before(linked_list *list, ll_node *at, b_list *blist, int start, int end)
{
        assert(list && blist && blist->lst);
        /* Resolve any negative indices. */
        resolve_negative_index(&start, (int)blist->qty);
        resolve_negative_index(&end,   (int)blist->qty);
        /* ASSERTIONS(); */

        const int diff = end - start;
        if (diff == 1) {
                ll_insert_before(list, at, blist->lst[start]);
                return;
        }
        pthread_mutex_lock(&ll_mutex);

        ll_node **tmp   = nmalloc(diff, sizeof *tmp);
        tmp[0]          = xmalloc(sizeof **tmp);
        tmp[0]->data    = blist->lst[start];
        ++list->qty;
        const int last  = create_nodes(start, end, 1, list, tmp, blist) - 1;
        tmp[last]->next = at;

        if (at) {
                tmp[0]->prev = at->prev;
                at->prev     = tmp[last];
                if (tmp[0]->prev)
                        tmp[0]->prev->next = tmp[0];
        } else
                tmp[0]->prev = NULL;

        if (!list->head || at == list->head)
                list->head = tmp[0];
        if (!list->tail)
                list->tail = tmp[last];

        xfree(tmp);
        pthread_mutex_unlock(&ll_mutex);
}


void
ll_delete_range(linked_list *list, ll_node *at, const int range)
{
        assert(list);
        assert(list->qty >= range);

        if (range == 0)
                return;
        if (range == 1) {
                assert(at);
                ll_delete_node(list, at);
                return;
        }
        pthread_mutex_lock(&ll_mutex);

        ll_node *current        = at;
        ll_node *next           = NULL;
        ll_node *prev           = (at) ? at->prev : NULL;
        const bool replace_head = (at == list->head || !list->head);

        for (int i = 0; i < range && current; ++i) {
                next = current->next;
                free_data(current);
                xfree(current);
                current = next;
                --list->qty;
        }

        list->qty = (list->qty < 0) ? 0 : list->qty;

        if (replace_head)
                list->head = next;
        if (prev)
                prev->next = next;

        if (next)
                next->prev = prev;
        else
                list->tail = prev;

        pthread_mutex_unlock(&ll_mutex);
}


/*============================================================================*/


ll_node *
ll_at(linked_list *list, int index)
{
        if (!list || list->qty == 0)
                return NULL;
        if (index == 0)
                return list->head;
        if (index == (-1) || index == list->qty)
                return list->tail;
        if (index < 0)
                index += list->qty;
        if (index < 0 || index > list->qty) {
                warnx("Failed to find node: index: %d, qty %d",
                      index, list->qty);
                return NULL;
        }

        pthread_mutex_lock(&ll_mutex);
        ll_node *current;

        if (index < ((list->qty - 1) / 2)) {
                int x   = 0;
                current = list->head;

                while (x++ != index)
                        current = current->next;
        } else {
                int x   = list->qty - 1;
                current = list->tail;

                while (x-- != index)
                        current = current->prev;
        }

        pthread_mutex_unlock(&ll_mutex);
        return current;
}


void
ll_destroy(linked_list *list)
{
        if (!list || !list->head)
                return;
        pthread_mutex_lock(&ll_mutex);

        if (list->qty == 1) {
                ll_node *current;
                if (list->tail)
                        current = list->tail;
                else
                        current = list->head;
                free_data(current);
                xfree(current);
        } else if (list->qty > 1) {
                ll_node *current = list->head;
                do {
                        ll_node *tmp = current;
                        current      = current->next;
                        free_data(tmp);
                        xfree(tmp);
                } while (current);
        }

        pthread_mutex_unlock(&ll_mutex);
        xfree(list);
}


void
ll_delete_node(linked_list *list, ll_node *node)
{
        assert(node != NULL);
        pthread_mutex_lock(&ll_mutex);

        if (list->qty == 1) {
                list->head = list->tail = NULL;
        } else if (node == list->head) {
                list->head       = node->next;
                list->head->prev = NULL;
        } else if (node == list->tail) {
                list->tail       = node->prev;
                list->tail->next = NULL;
        } else {
                node->prev->next = node->next;
                node->next->prev = node->prev;
        }

        --list->qty;
        xfree(node);
        pthread_mutex_unlock(&ll_mutex);
}


/*============================================================================*/


bool
ll_verify_size(linked_list *list)
{
        pthread_mutex_lock(&ll_mutex);
        assert(list);
        int cnt = 0;
        LL_FOREACH_F (list, node)
                ++cnt;

        const bool ret = (cnt == list->qty);
        if (!ret)
                list->qty = cnt;

        pthread_mutex_unlock(&ll_mutex);
        return ret;
}


bstring *
ll_join(linked_list *list, const int sepchar)
{
        pthread_mutex_lock(&ll_mutex);
        const unsigned seplen = (sepchar) ? 1 : 0;
        unsigned          len = 0;

        LL_FOREACH_F (list, line)
                len += line->data->slen + seplen;

        bstring *joined = b_alloc_null(len);

        if (sepchar) {
                LL_FOREACH_F (list, line) {
                        b_concat(joined, line->data);
                        b_conchar(joined, sepchar);
                }
        } else {
                LL_FOREACH_F (list, line)
                        b_concat(joined, line->data);
        }

        pthread_mutex_unlock(&ll_mutex);
        return joined;
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

ALWAYS_INLINE void
free_data(ll_node *node)
{
        b_free(node->data);
}

ALWAYS_INLINE void
resolve_negative_index(int *index, const int base)
{
        *index = ((*index >= 0) ? *index : (*index + base + 1));
}
