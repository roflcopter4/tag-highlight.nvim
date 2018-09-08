#ifndef SRC_HIGHLIGHT_H
#define SRC_HIGHLIGHT_H

#include "util/util.h"

#include "bstring/bstring.h"
#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif


#define PKG "tag_highlight#"
#define nvim_get_var_pkg(FD__, VARNAME_, EXPECT_) \
        nvim_get_var((FD__), B(PKG VARNAME_), (EXPECT_))

extern bool    run_ctags          (struct bufdata *bdata, int force);
extern int     get_initial_taglist(struct bufdata *bdata);
extern int     update_taglist     (struct bufdata *bdata, int force);
extern b_list *find_header_files  (struct bufdata *bdata);
extern void    update_highlight   (int bufnum, struct bufdata *bdata);
extern void    clear_highlight    (int bufnum, struct bufdata *bdata);


/* FROM NEOTAGS */
struct taglist {
        struct tag {
                bstring *b;
                int      kind;
        } **lst;

        unsigned qty;
        unsigned mlen;
};

extern bstring        *strip_comments(struct bufdata *bdata);
extern b_list         *tokenize      (struct bufdata *bdata, bstring *vimbuf);
extern struct taglist *process_tags  (struct bufdata *bdata, b_list *toks) aWUR;

extern b_list *parse_json(const bstring *json_path, const bstring *filename, b_list *includes);

extern int  my_highlight(int bufnum, struct bufdata *bdata);
extern void my_parser   (int bufnum, struct bufdata *bdata);

extern void *event_loop    (void *vdata);
extern void  get_init_lines(struct bufdata *bdata);


#ifdef __cplusplus
}
#endif

#endif /* highlight.h */
