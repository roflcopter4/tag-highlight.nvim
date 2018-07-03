#ifndef SRC_HIGHLIGHT_H
#define SRC_HIGHLIGHT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bstring/bstrlib.h"
#include "data.h"


extern bool run_ctags(int bufnum, struct bufdata *bdata);
extern bool check_gzfile(struct bufdata *bdata);

extern int getlines(b_list *tags, const bstring *comptype, const bstring *filename);


#endif /* highlight.h */
