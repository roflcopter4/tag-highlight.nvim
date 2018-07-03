#ifndef SRC_TAGS_H
#define SRC_TAGS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bstring/bstrlib.h"


extern bool run_ctags(int bufnum);
extern int getlines(b_list *tags, const bstring *comptype, const bstring *filename);


#endif /* tags.h */
