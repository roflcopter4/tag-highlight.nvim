#ifndef SRC_LINKED_LIST_H
#define SRC_LINKED_LIST_H

#include "bstring/bstrlib.h"
#include <stdbool.h>
#include <stdint.h>

/*============================================================================*/

typedef struct linked_list_s linked_list;
typedef struct ll_node_s     ll_node;


struct linked_list_s {
        ll_node *head;
        ll_node *tail;
        int qty;
};

struct ll_node_s {
        bstring *data;
        ll_node *prev;
        ll_node *next;
};

enum ll_pop_type {
        DEL_ONLY,
        RET_ONLY,
        BOTH,
};


/*============================================================================*/


#define ll_pop(LIST)        _ll_popat((LIST), -1, BOTH)
#define ll_dequeue(LIST)    _ll_popat((LIST), 0, BOTH)
#define ll_pop_at(LIST,IND) _ll_popat((LIST), (IND), BOTH)
#define ll_get(LIST,IND)    _ll_popat((LIST), (IND), RET_ONLY)
#define ll_remove(LIST,IND) _ll_popat((LIST), (IND), DEL_ONLY)

#define ll_insert_at(WHERE, LIST, IND, DATA)                                   \
        (ll_insert_##WHERE((LIST), (ll_at((LIST), (IND))), (DATA)))

#define ll_insert_blist_at(WHERE, LIST, IND, DATA, S_IND_, E_IND_)             \
        (ll_insert_blist_##WHERE((LIST), (ll_at((LIST), (IND))), (DATA),       \
                                 (S_IND_), (E_IND_)))

#define ll_delete_at(LIST, IND) \
        (ll_delete_node((LIST), (ll_at((LIST), (IND)))))

#define ll_delete_range_at(LIST, IND, RANGE) \
        (ll_delete_range((LIST), (ll_at((LIST), (IND))), (RANGE)))

#define LL_FOREACH_F(_LIST__, VAR_)                                                \
        for (ll_node *VAR_ = (_LIST__)->head; (VAR_) != NULL; (VAR_) = (VAR_)->next)

#define LL_FOREACH_B(_LIST__, VAR_)                                                \
        for (ll_node *VAR_ = (_LIST__)->tail; (VAR_) != NULL; (VAR_) = (VAR_)->prev)


/*============================================================================*/


extern linked_list * ll_make_new(void);
extern ll_node     * ll_at      (linked_list *list, int index);
extern bstring     * _ll_popat  (linked_list *list, int index, uint8_t type);

extern void ll_add                (linked_list *list, bstring *data);
extern void ll_append             (linked_list *list, bstring *data);
extern void ll_delete_node        (linked_list *list, ll_node *node);
extern void ll_delete_range       (linked_list *list, ll_node *at, unsigned range);
extern void ll_destroy            (linked_list *list);
extern void ll_insert_after       (linked_list *list, ll_node *at, bstring *data);
extern void ll_insert_before      (linked_list *list, ll_node *at, bstring *data);
extern void ll_insert_blist_after (linked_list *list, ll_node *at, b_list *blist, int start, int end);
extern void ll_insert_blist_before(linked_list *list, ll_node *at, b_list *blist, int start, int end);
extern bool ll_verify_size        (linked_list *list);


/*============================================================================*/
#endif /* linked_list.h */
