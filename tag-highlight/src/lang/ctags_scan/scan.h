#ifndef SRC_LANG_CTAGS_SCAN_SCAN_H_
#define SRC_LANG_CTAGS_SCAN_SCAN_H_

#include "Common.h"
#include "highlight.h"
__BEGIN_DECLS

/* FROM NEOTAGS */
struct taglist {
        struct tag {
                bstring *b;
                int      kind;
        } **lst;

        unsigned qty;
        unsigned mlen;
};

extern bstring        *strip_comments(Buffer *bdata) __aWUR;
extern b_list         *tokenize      (Buffer *bdata, bstring *vimbuf) __aWUR;
extern struct taglist *process_tags  (Buffer const *bdata, b_list *toks) __aWUR;

__END_DECLS
#endif /* scan.h */
