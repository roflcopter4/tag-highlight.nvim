#ifndef SRC_HIGHLIGHT_H
#define SRC_HIGHLIGHT_H

#include "tag_highlight.h"

/* #define DEFAULT_FD       (mainchan) */
/* #define BUFFER_ATTACH_FD (bufchan) */
#define DEFAULT_FD       (1)
#define BUFFER_ATTACH_FD (0)

#include "bstring/bstring.h"
#include "data.h"
#include "highlight.h"
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

enum { HIGHLIGHT_NORMAL, HIGHLIGHT_UPDATE, HIGHLIGHT_REDO };

extern bool    run_ctags          (struct bufdata *bdata, enum update_taglist_opts opts);
extern int     get_initial_taglist(struct bufdata *bdata);
extern int     update_taglist     (struct bufdata *bdata, enum update_taglist_opts opts);
extern b_list *find_header_files  (struct bufdata *bdata);
extern void    update_highlight   (struct bufdata *bdata, int type);
extern void    clear_highlight    (struct bufdata *bdata);


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

static inline struct bufdata *find_current_buffer(void) {
        return find_buffer(nvim_get_current_buf());
}

#define update_highlight(...) P99_CALL_DEFARG(update_highlight, 2, __VA_ARGS__)
#define update_highlight_defarg_0() (find_current_buffer())
#define update_highlight_defarg_1() (UPDATE_TAGLIST_NORMAL)
#define clear_highlight(...) P99_CALL_DEFARG(clear_highlight, 1, __VA_ARGS__)
#define clear_highlight_defarg_0() (find_current_buffer())

#define b_list_dump_nvim(LST_) _b_list_dump_nvim((LST_), #LST_)
extern void _b_list_dump_nvim(const b_list *list, const char *listname);

#ifdef __cplusplus
}
#endif

#endif /* highlight.h */
