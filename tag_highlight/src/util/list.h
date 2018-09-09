#ifndef SRC_LIST_H_
#define SRC_LIST_H_

#include "bstring/bstring.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*======================================================================================================*/

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

/*======================================================================================================*/

#if 0
#define ll_insert_at(WHERE_, LIST_, IND_, DATA_) \
        (ll_insert_##WHERE((LIST_), ll_at((LIST_), (IND_)), (DATA_)))

#define ll_insert_blist_at(WHERE_, LIST_, IND_, DATA_, S_IND_, E_IND_) \
        (ll_insert_blist_##WHERE((LIST_), ll_at((LIST_), (IND_)), (DATA_), (S_IND_), (E_IND_)))
#endif


#define ll_insert_before_at(LIST_, IND_, DATA_) \
        (ll_insert_before((LIST_), ll_at((LIST_), (IND_)), (DATA_)))

#define ll_insert_after_at(LIST_, IND_, DATA_) \
        (ll_insert_after((LIST_), ll_at((LIST_), (IND_)), (DATA_)))

#define ll_insert_blist_before_at(LIST_, IND_, DATA_, S_IND_, E_IND_) \
        (ll_insert_blist_before((LIST_), ll_at((LIST_), (IND_)), (DATA_), (S_IND_), (E_IND_)))

#define ll_insert_blist_after_at(LIST_, IND_, DATA_, S_IND_, E_IND_) \
        (ll_insert_blist_after((LIST_), ll_at((LIST_), (IND_)), (DATA_), (S_IND_), (E_IND_)))

#define ll_delete_at(LIST, IND) \
        (ll_delete_node((LIST), ll_at((LIST), (IND))))

#define ll_delete_range_at(LIST_, IND_, RANGE_) \
        (ll_delete_range((LIST_), ll_at((LIST_), (IND_)), (RANGE_)))

#define LL_FOREACH_F(_LIST__, VAR_) \
        for (ll_node *VAR_ = (_LIST__)->head; (VAR_) != NULL; (VAR_) = (VAR_)->next)

#define LL_FOREACH_B(_LIST__, VAR_) \
        for (ll_node *VAR_ = (_LIST__)->tail; (VAR_) != NULL; (VAR_) = (VAR_)->prev)

/*======================================================================================================*/

extern linked_list *ll_make_new    (void);
extern ll_node     *ll_at          (linked_list *list, int index);
/* extern ll_node     *ll_at          (const linked_list *list, ll_node *ref, const int ref_ind, int index); */
extern void ll_prepend             (linked_list *list, bstring *data);
extern void ll_append              (linked_list *list, bstring *data);
extern void ll_delete_node         (linked_list *list, ll_node *node);
extern void ll_delete_range        (linked_list *list, ll_node *at, unsigned range);
extern void ll_destroy             (linked_list *list);
extern void ll_insert_after        (linked_list *list, ll_node *at, bstring *data);
extern void ll_insert_before       (linked_list *list, ll_node *at, bstring *data);
extern void ll_insert_blist_after  (linked_list *list, ll_node *at, b_list *blist, int start, int end);
extern void ll_insert_blist_before (linked_list *list, ll_node *at, b_list *blist, int start, int end);
extern bool ll_verify_size         (linked_list *list);

/* Depends on the data being composed of bstrings. */
extern bstring *ll_join(linked_list *list, int sepchar);

/*======================================================================================================*/
/* Generic list */

#if !defined(__GNUC__) && !defined(__attribute__)
#  define __attribute__(...)
#endif

typedef struct generic_list {
        unsigned qty;
        unsigned mlen;
        void **  lst;
} genlist;

typedef int (*genlist_copy_func)(void **dest, void *item);

extern genlist *genlist_create(void);
extern int      genlist_destroy(genlist *sl);
extern genlist *genlist_create_alloc(const unsigned msz);
extern int      genlist_alloc(genlist *sl, const unsigned msz);
extern int      genlist_append(genlist *listp, void *item);
extern int      genlist_remove_index(genlist *list, const unsigned index);
extern int      genlist_remove(genlist *listp, const void *obj);
extern genlist *genlist_copy(const genlist *list, const genlist_copy_func cpy);
/* genlist *genlist_copy(const genlist *list); */


typedef struct argument_vector {
        char   **lst;
        unsigned qty;
        unsigned mlen;
} str_vector;

extern struct argument_vector *argv_create(const unsigned len);
extern void argv_append (struct argument_vector *argv, const char *str, const bool cpy);
extern void argv_destroy(struct argument_vector *argv);
extern void argv_fmt    (struct argument_vector *argv, const char *const __restrict fmt, ...)
        __attribute__((__format__(printf, 2, 3)));

void argv_dump__(FILE *fp, const struct argument_vector *argv, const char *const listname);
void argv_dump_fd__(int fd, const struct argument_vector *argv, const char *const listname);

#define argv_dump(FP, ARGV)    (argv_dump__((FP), (ARGV), #ARGV))
#define argv_dump_fd(FD, ARGV) (argv_dump_fd__((FD), (ARGV), #ARGV))


#ifdef __cplusplus
}
#endif
/*======================================================================================================*/
#endif /* list.h */