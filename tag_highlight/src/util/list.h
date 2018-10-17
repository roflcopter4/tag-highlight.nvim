#ifndef SRC_LIST_H_
#define SRC_LIST_H_

#include "bstring/bstring.h"
#include <pthread.h>
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
        /* pthread_rwlock_t lock; */
        pthread_mutex_t lock;
};

struct ll_node_s {
        bstring *data;
        ll_node *prev;
        ll_node *next;
};

#define ll_insert_before_at(LIST, IND, DATA) \
        ll_insert_before((LIST), ll_at((LIST), (IND)), (DATA))

#define ll_insert_after_at(LIST, IND, DATA) \
        ll_insert_after((LIST), ll_at((LIST), (IND)), (DATA))

#define ll_insert_blist_before_at(LIST, IND, DATA, S_IND, E_IND) \
        ll_insert_blist_before((LIST), ll_at((LIST), (IND)), (DATA), (S_IND), (E_IND))

#define ll_insert_blist_after_at(LIST, IND, DATA, S_IND, E_IND) \
        ll_insert_blist_after((LIST), ll_at((LIST), (IND)), (DATA), (S_IND), (E_IND))

#define ll_delete_at(LIST, IND) \
        ll_delete_node((LIST), ll_at((LIST), (IND)))

#define ll_delete_range_at(LIST, IND, RANGE) \
        ll_delete_range((LIST), ll_at((LIST), (IND)), (RANGE))

#define LL_FOREACH_F(LIST, VAR) \
        for (ll_node * VAR = (LIST)->head; (VAR) != NULL; (VAR) = (VAR)->next)

#define LL_FOREACH_B(LIST, VAR) \
        for (ll_node * VAR = (LIST)->tail; (VAR) != NULL; (VAR) = (VAR)->prev)

extern linked_list *ll_make_new    (void);
extern ll_node     *ll_at          (linked_list *list, int index);
extern void ll_prepend             (linked_list *list, bstring *data);
extern void ll_append              (linked_list *list, bstring *data);
extern void ll_delete_node         (linked_list *list, ll_node *node);
extern void ll_delete_range        (linked_list *list, ll_node *at, int range);
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
        void **  lst;
        unsigned qty;
        unsigned mlen;
        /* pthread_rwlock_t lock; */
        pthread_mutex_t mut;
} genlist;

typedef int (*genlist_copy_func)(void **dest, void *item);

extern genlist *genlist_create(void);
extern int      genlist_destroy(genlist *sl);
extern genlist *genlist_create_alloc(const unsigned msz);
extern int      genlist_alloc(genlist *sl, const unsigned msz);
extern int      genlist_append(genlist *listp, void *item, bool copy, size_t size);
extern int      genlist_remove_index(genlist *list, const unsigned index);
extern int      genlist_remove(genlist *listp, const void *obj);
extern genlist *genlist_copy(genlist *list, const genlist_copy_func cpy);

extern void    *genlist_pop(genlist *list);
extern void    *genlist_dequeue(genlist *list);
/* genlist *genlist_copy(const genlist *list); */

#define genlist_append(...) P99_CALL_DEFARG(genlist_append, 4, __VA_ARGS__)
#define genlist_append_defarg_2() (false)
#define genlist_append_defarg_3() (0llu)

/*======================================================================================================*/
/* Simple char * list. */

typedef struct argument_vector {
        char   **lst;
        unsigned qty;
        unsigned mlen;
} str_vector;

extern struct argument_vector *argv_create(const unsigned len);
extern void argv_append (struct argument_vector *argv, const char *str, const bool cpy);
extern void argv_destroy(struct argument_vector *argv);
extern void argv_fmt    (struct argument_vector *argv, const char *const __restrict fmt, ...)
        __attribute__((__format__(__gnu_printf__, 2, 3)));

void argv_dump__(FILE *fp, const struct argument_vector *argv, const char *listname, const char *, int);
void argv_dump_fd__(int fd, const struct argument_vector *argv, const char *listname, const char *, int);

#define argv_dump(FP, ARGV)    (argv_dump__((FP), (ARGV), #ARGV, __FILE__, __LINE__))
#define argv_dump_fd(FD, ARGV) (argv_dump_fd__((FD), (ARGV), #ARGV, __FILE__, __LINE__))


#ifdef __cplusplus
}
#endif
/*======================================================================================================*/
#endif /* list.h */
