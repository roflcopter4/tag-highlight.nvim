#ifndef SRC_FIND_H
#define SRC_FIND_H

/* #include "util/util.h" */

#include "bstring/bstring.h"

enum find_flags {
        FIND_LITERAL,
        FIND_SPLIT,
        FIND_SHORTEST,
        FIND_FIRST,
};

extern void    * find_file(const char *path, const char *search, const enum find_flags flags);


#endif /* find.h */
