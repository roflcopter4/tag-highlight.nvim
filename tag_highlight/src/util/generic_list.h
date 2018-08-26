#ifndef SRC_GENERIC_LIST_H
#define SRC_GENERIC_LIST_H
#  ifdef __cplusplus
   extern "C" {
#  endif

#include "util/util.h"

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


struct argument_vector {
        char   **lst;
        unsigned qty;
        unsigned mlen;
};

extern struct argument_vector *argv_create(const unsigned len);
extern void argv_append (struct argument_vector *argv, const char *str, const bool cpy);
extern void argv_destroy(struct argument_vector *argv);
extern void argv_fmt    (struct argument_vector *argv, const char *const __restrict fmt, ...)
        __attribute__((__format__(printf, 2, 3)));


#  ifdef __cplusplus
   }
#  endif
#endif /* generic_list.h */
