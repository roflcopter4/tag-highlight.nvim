#ifndef SRC_HIGHLIGHT_H
#define SRC_HIGHLIGHT_H

#include "util/util.h"

/* #define DEFAULT_FD       (mainchan) */
/* #define BUFFER_ATTACH_FD (bufchan) */
#define DEFAULT_FD       (1)
#define BUFFER_ATTACH_FD (1)

#include "bstring/bstring.h"
#include "data.h"
#include "nvim_api/api.h"
#include "p99/p99_defarg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* extern int mainchan, bufchan; */

#define PKG "tag_highlight#"
#define nvim_get_var_pkg(FD__, VARNAME_, EXPECT_) \
        nvim_get_var((FD__), B(PKG VARNAME_), (EXPECT_))

enum update_taglist_opts {
        UPDATE_TAGLIST_NORMAL,
        UPDATE_TAGLIST_FORCE,
        UPDATE_TAGLIST_FORCE_LANGUAGE,
};

extern bool    run_ctags          (struct bufdata *bdata, enum update_taglist_opts opts);
extern int     get_initial_taglist(struct bufdata *bdata);
extern int     update_taglist     (struct bufdata *bdata, enum update_taglist_opts opts);
extern b_list *find_header_files  (struct bufdata *bdata);
extern void    update_highlight   (int bufnum, struct bufdata *bdata, bool force);
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

/* extern noreturn void *event_loop    (void *vdata); */
extern void launch_event_loop(void);
extern void get_initial_lines(struct bufdata *bdata);

#define update_highlight(...) P99_CALL_DEFARG(update_highlight, 3, __VA_ARGS__)
#define update_highlight_defarg_0() (-1)
#define update_highlight_defarg_1() NULL
#define update_highlight_defarg_2() false
#define clear_highlight(...) P99_CALL_DEFARG(clear_highlight, 2, __VA_ARGS__)
#define clear_highlight_defarg_o() (-1)
#define clear_highlight_defarg_1() NULL


#ifdef __cplusplus
}
#endif

#endif /* highlight.h */
