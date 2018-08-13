#ifndef SRC_GENERIC_LIST_H
#define SRC_GENERIC_LIST_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif


struct generic_list {
        unsigned qty;
        unsigned mlen;
        void **  lst;
};
typedef struct generic_list genlist;

typedef int (*genlist_copy_func)(void **dest, void *item);

genlist * genlist_create(void);
int genlist_destroy(genlist *sl);
genlist * genlist_create_alloc(const unsigned msz);
int genlist_alloc(genlist *sl, const unsigned msz);
int genlist_append(genlist **listp, void *item);
int genlist_remove(genlist *listp, unsigned index);

/* genlist *genlist_copy(const genlist *list); */
genlist * genlist_copy(const genlist *list, const genlist_copy_func cpy);


#ifdef __cplusplus
}
#endif

#endif /* generic_list.h */
