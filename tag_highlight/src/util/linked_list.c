
#include "Common.h"
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

STATIC_INLINE void resolve_negative_index(int *index, int base);

#ifdef NDEBUG
#  undef assert
#  define assert ALWAYS_ASSERT
#endif

linked_list *
(ll_make_new)(void *talloc_ctx)
{
        /* assert(free_data); */
        linked_list *list = talloc(talloc_ctx, linked_list);
        list->head        = NULL;
        list->tail        = NULL;
        list->qty         = 0;
        //list->intern      = talloc(list, pthread_rwlock_t);

        /* talloc_set_destructor(list, ll_destroy); */

#if 0
        linked_list *list = malloc(sizeof *list);
        list->head        = NULL;
        list->tail        = NULL;
        list->qty         = 0;
        list->free_data   = free_data;
        list->intern      = malloc(sizeof(pthread_rwlock_t));
#endif

#if 0
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
#ifdef PTHREAD_RWLOCK_PREFER_WRITER_NP
        pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NP);
#endif
        /* pthread_rwlock_init((pthread_rwlock_t *)list->intern, &attr); */
#endif
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&list->lock, &attr);

        return list;
}


void
ll_prepend(linked_list *list, void *data)
{
        pthread_mutex_lock(&list->lock);
        assert(list);
        assert(data);
        ll_node *node = talloc(list, ll_node);

        if (list->head)
                list->head->prev = node;
        if (!list->tail)
                list->tail = node;

        node->data = talloc_move(node, &data);
        node->prev = NULL;
        node->next = list->head;
        list->head = node;
        ++list->qty;
        pthread_mutex_unlock(&list->lock);
}


void
ll_append(linked_list *list, void *data)
{
        pthread_mutex_lock(&list->lock);
        assert(list);
        assert(data);
        ll_node *node = talloc(list, ll_node);

        if (list->tail)
                list->tail->next = node;
        if (!list->head)
                list->head = node;

        node->data = talloc_move(node, &data);
        node->prev = list->tail;
        node->next = NULL;
        list->tail = node;
        ++list->qty;
        pthread_mutex_unlock(&list->lock);
}


/*============================================================================*/


void
ll_insert_after(linked_list *list, ll_node *at, void *data)
{
        pthread_mutex_lock(&list->lock);
        assert(list);
        ll_node *node = talloc(list, ll_node);
        node->data    = talloc_move(node, &data);
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
        ALWAYS_ASSERT((at && at->next == node) || list->tail == node);
        ++list->qty;
        pthread_mutex_unlock(&list->lock);
}


void
ll_insert_before(linked_list *list, ll_node *at, void *data)
{
        pthread_mutex_lock(&list->lock);
        assert(list);
        ll_node *node = talloc(list, ll_node);
        node->data    = talloc_move(node, &data);
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
        ALWAYS_ASSERT((at && at->prev == node) || list->head == node);
        ++list->qty;
        pthread_mutex_unlock(&list->lock);
}


/*============================================================================*/


static inline int
create_nodes(int const start, int const end, int i,
             linked_list *list, ll_node **narr, b_list *blist)
{
        for (int x = (start + 1); x < end; ++x, ++i)
        {
                assert((unsigned)i < blist->qty);
                assert(blist->lst[x] != NULL);

                narr[i]         = talloc(list, ll_node);
                narr[i]->data   = talloc_move(narr[i], &blist->lst[x]);
                narr[i]->prev   = narr[i-1];
                narr[i-1]->next = narr[i];
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
        ASSERTIONS();

        int const diff = end - start;
        if (diff == 1) {
                ll_insert_after(list, at, blist->lst[start]);
                return;
        }

        pthread_mutex_lock(&list->lock);

        ll_node **narr = talloc_array(NULL, ll_node *, diff);
        narr[0]        = talloc(list, ll_node);
        narr[0]->data  = talloc_move(narr[0], &blist->lst[start]);
        narr[0]->prev  = at;
        ++list->qty;

        int const last = create_nodes(start, end, 1, list, narr, blist) - 1;

        if (at) {
                narr[last]->next = at->next;
                at->next         = narr[0];
                if (narr[last]->next)
                        narr[last]->next->prev = narr[last];
        } else {
                narr[last]->next = NULL;
        }

        if (!list->head)
                list->head = narr[0];
        if (!list->tail || at == list->tail)
                list->tail = narr[last];

        talloc_free(narr);
        pthread_mutex_unlock(&list->lock);
}


void
ll_insert_blist_before(linked_list *list, ll_node *at, b_list *blist, int start, int end)
{
        assert(list && blist && blist->lst);
        /* Resolve any negative indices. */
        resolve_negative_index(&start, (int)blist->qty);
        resolve_negative_index(&end,   (int)blist->qty);
        ASSERTIONS();

        int const diff = end - start;
        if (diff == 1) {
                ll_insert_before(list, at, blist->lst[start]);
                return;
        }

        pthread_mutex_lock(&list->lock);

        ll_node **narr = talloc_array(NULL, ll_node *, diff);
        narr[0]        = talloc(list, ll_node);
        narr[0]->data  = talloc_move(narr[0], &blist->lst[start]);
        ++list->qty;

        int const last   = create_nodes(start, end, 1, list, narr, blist) - 1;
        narr[last]->next = at;

        if (at) {
                narr[0]->prev = at->prev;
                at->prev      = narr[last];
                if (narr[0]->prev)
                        narr[0]->prev->next = narr[0];
        } else {
                narr[0]->prev = NULL;
        }

        if (!list->head || at == list->head)
                list->head = narr[0];
        if (!list->tail)
                list->tail = narr[last];

        talloc_free(narr);
        pthread_mutex_unlock(&list->lock);
}


void
ll_delete_range(linked_list *list, ll_node *at, int const range)
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
        pthread_mutex_lock(&list->lock);

        ll_node *current      = at;
        ll_node *next         = NULL;
        ll_node *prev         = (at) ? at->prev : NULL;
        bool     replace_head = (at == list->head || !list->head);

        for (int i = 0; i < range && current; ++i) {
                next = current->next;
                talloc_free(current);
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

        pthread_mutex_unlock(&list->lock);
}


/*============================================================================*/


ll_node *
ll_at(linked_list *list, int index)
{
        ll_node *current = NULL;
        if (!list)
                return NULL;
        pthread_mutex_lock(&list->lock);

        if (list->qty == 0)
                goto ret;
        if (index == 0) {
                current = list->head;
                goto ret;
        }
        if (index == (-1) || index == list->qty) {
                current = list->tail;
                goto ret;
        }
        if (index < 0)
                index += list->qty;
        if (index < 0 || index > list->qty) {
                eprintf("Failed to find node: index: %d, qty %d",
                        index, list->qty);
                goto ret;
        }


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

ret:
        pthread_mutex_unlock(&list->lock);
        return current;
}


int
ll_destroy(linked_list *list)
{
        talloc_free(list);
#if 0
        if (!list)
                return 0;
        pthread_mutex_lock(&list->lock);

#if 0
        pthread_rwlock_t *lock = talloc_move(NULL, &list->intern);
        pthread_mutex_lock(lock);
        talloc_free(list);
        pthread_mutex_unlock(lock);
        talloc_free(lock);

#endif
        if (list->qty == 1) {
                eprintf("Freeing 1 thing\n");
                ll_node *current = (list->tail) ? list->tail : list->head;
                talloc_free(current);
        } else if (list->qty > 1) {
                eprintf("Freeing %u things\n", list->qty);
                ll_node *current = list->head;
                do {
                        ll_node *tmp = current;
                        current      = current->next;
                        talloc_free(tmp);
                } while (current);
                eprintf("weee all done\n");
        }

        pthread_mutex_unlock(&list->lock);
        talloc_free(list->intern);
        talloc_free(list);
#endif

        return 0;
}


void
ll_delete_node(linked_list *list, ll_node *node)
{
        assert(node != NULL);
        pthread_mutex_lock(&list->lock);

        if (list->qty == 1) {
                list->head = list->tail = NULL;
        } else if (node == list->head) {
                list->head = node->next;
                if (list->head)
                        list->head->prev = NULL;
        } else if (node == list->tail) {
                list->tail = node->prev;
                if (list->tail)
                        list->tail->next = NULL;
        } else {
                node->prev->next = node->next;
                node->next->prev = node->prev;
        }

        --list->qty;
        talloc_free(node);
        pthread_mutex_unlock(&list->lock);
}


/*============================================================================*/

void *
ll_pop(linked_list *list)
{
        assert(list);
        void *ret = NULL;
        pthread_mutex_lock(&list->lock);

        if (list->tail) {
                ll_node *tmp = list->tail;
                list->tail   = list->tail->prev;
                ret          = tmp->data;
                talloc_steal(NULL, ret);
                talloc_free(tmp);

                if (list->tail)
                        list->tail->next = NULL;
                else
                        list->head = NULL;
        }

        pthread_mutex_unlock(&list->lock);
        return ret;
}

void *
ll_dequeue(linked_list *list)
{
        assert(list);
        void *ret = NULL;
        pthread_mutex_lock(&list->lock);

        if (list->head) {
                ll_node *tmp = list->head;
                list->head   = list->head->next;
                ret          = tmp->data;
                talloc_steal(NULL, ret);
                talloc_free(tmp);

                if (list->head)
                        list->head->next = NULL;
                else
                        list->tail = NULL;
        }

        pthread_mutex_unlock(&list->lock);
        return ret;
}

/*============================================================================*/

bool
ll_verify_size(linked_list *list)
{
        pthread_mutex_lock(&list->lock);
        assert(list);
        int cnt = 0;
        LL_FOREACH_F (list, node)
                ++cnt;

        bool const ret = (cnt == list->qty);
        if (!ret)
                list->qty = cnt;

        pthread_mutex_unlock(&list->lock);
        return ret;
}


bstring *
ll_join_bstrings(linked_list *list, int const sepchar)
{
        pthread_mutex_lock(&list->lock);
        unsigned const seplen = (sepchar) ? 1 : 0;
        unsigned       len    = 0;

        LL_FOREACH_F (list, line)
                len += ((bstring *)(line->data))->slen + seplen;

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

        pthread_mutex_unlock(&list->lock);
        return joined;
}

STATIC_INLINE void
resolve_negative_index(int *index, int const base)
{
        *index = ((*index >= 0) ? *index : (*index + base + 1));
}
