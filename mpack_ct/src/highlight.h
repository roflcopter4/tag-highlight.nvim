#ifndef SRC_HIGHLIGHT_H
#define SRC_HIGHLIGHT_H

/* #include "util.h" */

#include "bstring/bstrlib.h"
#include "data.h"


extern bool run_ctags(struct bufdata *bdata, struct top_dir *topdir);
extern bool check_gzfile(struct bufdata *bdata);
extern void get_initial_taglist(struct bufdata *bdata, struct top_dir *topdir);
extern bool update_taglist(struct bufdata *bdata);

extern int getlines(b_list *tags, enum comp_type_e comptype, const bstring *filename);

extern void update_highlight(const int bufnum, struct bufdata *bdata);
extern void clear_highlight(const int bufnum, struct bufdata *bdata);


/* FROM NEOTAGS */
struct taglist {
        struct tag {
                bstring *b;
                int      kind;
        } **lst;

        unsigned qty;
        unsigned mlen;
};

extern bstring * strip_comments(struct bufdata *bdata);
extern b_list  * tokenize(struct bufdata *bdata, bstring *vimbuf);
extern struct taglist * findemtagers(struct bufdata *bdata, b_list *toks) __attribute__((warn_unused_result));


extern int my_highlight(const int bufnum, struct bufdata *bdata);
extern void my_parser(const int bufnum, struct bufdata *bdata);


#endif /* highlight.h */
