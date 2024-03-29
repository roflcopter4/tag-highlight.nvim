#ifndef SRC_LIST_H_
#define SRC_LIST_H_

#include "bstring.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

//#if !defined __GNUC__ && !defined __attribute__
//#  define __attribute__(...)
//#endif
#ifndef LLAwur
#  define LLAwur __attribute__((__warn_unused_result__))
#endif
#ifndef LLAfmt
#  ifdef __clang__
#    define LLAfmt(A1, A2) __attribute__((__format__(__printf__, A1, A2)))
#  else
#    define LLAfmt(A1, A2) __attribute__((__format__(__gnu_printf__, A1, A2)))
#  endif
#endif
#ifndef LLAnn
#  define LLAnn(...) __attribute__((__nonnull__(__VA_ARGS__)))
#  define LLAnna     __attribute__((__nonnull__))
#endif
#if defined __MINGW__ || defined __MINGW32__ || defined __MINGW64__
#  define LLDECL extern
#  define LLINTERN extern /* That's defininitely not confusing at all... */
#else
#  define LLDECL __attribute__((__visibility__("default"))) extern
#  define LLINTERN __attribute__((__visibility__("hidden"))) extern
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*======================================================================================================*/

typedef struct linked_list_s linked_list;
typedef struct ll_node_s     ll_node;

struct linked_list_s {
        void            *intern;
        ll_node         *head;
        ll_node         *tail;
        int              qty;
        pthread_mutex_t  lock;
};

struct ll_node_s {
        void    *data;
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

#define ll_push    ll_append
#define ll_enqueue ll_prepend
#define ll_create  ll_make_new

LLDECL linked_list *ll_make_new       (void *talloc_ctx) LLAwur __attribute__((__malloc__));
LLDECL ll_node     *ll_at             (linked_list *list, int index)  LLAnn(1);
LLDECL void    ll_prepend             (linked_list *list, void *data) LLAnn(1, 2);
LLDECL void    ll_append              (linked_list *list, void *data) LLAnn(1, 2);
LLDECL void    ll_delete_node         (linked_list *list, ll_node *node) LLAnn(1, 2);
LLDECL void    ll_delete_range        (linked_list *list, ll_node *at, int range) LLAnn(1);
LLDECL int     ll_destroy             (linked_list *list);
LLDECL void    ll_insert_after        (linked_list *list, ll_node *at, void *data) LLAnn(1);
LLDECL void    ll_insert_before       (linked_list *list, ll_node *at, void *data) LLAnn(1);
LLDECL void    ll_insert_blist_after  (linked_list *list, ll_node *at, b_list *blist, int start, int end) LLAnn(1, 3);
LLDECL void    ll_insert_blist_before (linked_list *list, ll_node *at, b_list *blist, int start, int end) LLAnn(1, 3);
LLDECL bool    ll_verify_size         (linked_list *list) LLAnna;
LLDECL void    *ll_pop                (linked_list *list) LLAwur LLAnna;
LLDECL void    *ll_dequeue            (linked_list *list) LLAwur LLAnna;
LLDECL bstring *ll_join_bstrings      (linked_list *list, int sepchar) LLAwur LLAnna;

#define ll_make_new(...) P99_CALL_DEFARG(ll_make_new, 1, __VA_ARGS__)
#define ll_make_new_defarg_0() NULL

/*======================================================================================================*/
/* Generic list */

typedef struct generic_list genlist;
struct generic_list {
        void          **lst;
        unsigned        qty;
        unsigned        mlen;
        pthread_mutex_t mut;
};

typedef int (*genlist_copy_func)(void **dest, void *item);

LLDECL genlist *genlist_create       (void *talloc_ctx) LLAwur;
LLDECL genlist *genlist_create_alloc (void *talloc_ctx, unsigned msz) LLAwur;
LLDECL int      genlist_destroy      (genlist *list);
LLDECL int      genlist_alloc        (genlist *list, unsigned msz);
LLDECL int      genlist_append       (genlist *list, void *item) LLAnn(2);
LLDECL int      genlist_remove_index (genlist *list, unsigned index);
LLDECL int      genlist_remove       (genlist *list, const void *obj);
LLDECL void    *genlist_pop          (genlist *list) LLAwur;
LLDECL void    *genlist_dequeue      (genlist *list) LLAwur;
LLDECL genlist *genlist_copy         (genlist *list, genlist_copy_func cpy) LLAwur;

#define GENLIST_FOREACH(LIST, TYPE, VAR, ...)               \
        GENLIST_FOREACH_EXPLICIT_(LIST, TYPE, VAR,          \
                                  P99_IF_EMPTY(__VA_ARGS__) \
                                        (P99_UNIQ(cnt))     \
                                        (__VA_ARGS__),      \
                                  P99_UNIQ(genlist_b))

#define GENLIST_FOREACH_EXPLICIT_(LIST, TYPE, VAR, CTR, BL)                      \
        /*NOLINTNEXTLINE(bugprone-macro-parentheses)*/                           \
        for (unsigned CTR, BL = true; (BL) && (LIST) != NULL; (BL) = false)      \
                for (TYPE VAR = ((LIST)->lst[((CTR) = 0)]);                      \
                     (CTR) < (LIST)->qty && (((VAR) = (LIST)->lst[(CTR)]) || 1); \
                     ++(CTR))

/*======================================================================================================*/
/* Simple char * list. */

typedef struct argument_vector_st str_vector;
struct argument_vector_st {
        char   **lst;
        unsigned qty;
        unsigned mlen;
        pthread_mutex_t lock;
};

LLDECL str_vector *argv_create      (unsigned len) LLAwur;
LLDECL void        argv_append      (str_vector *argv, const char *str, bool cpy) LLAnn(1);
LLDECL void        argv_pop         (str_vector *argv) LLAnn(1);
LLDECL void        argv_destroy     (str_vector *argv);
LLDECL void        argv_append_fmt  (str_vector *argv, const char *__restrict fmt, ...) LLAfmt(2, 3) LLAnn(1, 2);
LLDECL void        argv_append_null (str_vector *argv) LLAnn(1);

LLINTERN void argv_dump__   (FILE *fp, str_vector *argv, const char *listname, const char *, int) LLAnna;
LLINTERN void argv_dump_fd__(int fd, str_vector *argv, const char *listname, const char *, int)   LLAnna;

#define argv_dump(FP, ARGV)    (argv_dump__((FP), (ARGV), #ARGV, __FILE__, __LINE__))
#define argv_dump_fd(FD, ARGV) (argv_dump_fd__((FD), (ARGV), #ARGV, __FILE__, __LINE__))

#undef LLDECL
#undef LLINTERN
#undef LLAwur
#undef LLAfmt
#undef LLAnn
#undef LLAnna

/*======================================================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* list.h */
