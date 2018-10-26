#ifndef SRC_LANG_COMMON_H_
#define SRC_LANG_COMMON_H_

#include "tag_highlight.h"

#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "my_p99_common.h"

__BEGIN_DECLS
/*======================================================================================*/

P99_DECLARE_STRUCT(cmd_info);
P99_DECLARE_STRUCT(line_data);
struct cmd_info {
        unsigned num;
        int      kind;
        bstring *group;
};

struct line_data {
        unsigned line;
        unsigned start;
        unsigned end;
};

extern void add_hl_call (nvim_arg_array *calls, int bufnum, int hl_id,
                         const bstring *group, const line_data *data);
extern void add_clr_call(nvim_arg_array *calls, int bufnum, int hl_id, int line, int end);

extern nvim_arg_array  *new_arg_array(void);
extern const bstring   *find_group   (struct filetype *ft, const cmd_info *info,
                                      unsigned num, const int ctags_kind);
extern struct cmd_info *getinfo      (struct bufdata *bdata);
extern void destroy_struct_info(cmd_info *info);

/*======================================================================================*/

extern FILE *cmd_log;

#define STRDUP(STR)                                                     \
        __extension__({                                                 \
                static const char strng_[]   = ("" STR "");             \
                char *            strng_cpy_ = xmalloc(sizeof(strng_)); \
                memcpy(strng_cpy_, strng_, sizeof(strng_));             \
                strng_cpy_;                                             \
        })

#define CTAGS_CLASS        'c'
#define CTAGS_ENUM         'g'
#define CTAGS_ENUMCONST    'e'
#define CTAGS_FUNCTION     'f'
#define CTAGS_GLOBALVAR    'v'
#define CTAGS_MEMBER       'm'
#define CTAGS_NAMESPACE    'n'
#define CTAGS_PACKAGE      'p'
#define CTAGS_PREPROC      'd'
#define CTAGS_STRUCT       's'
#define CTAGS_TYPE         't'
#define CTAGS_UNION        'u'
#define EXTENSION_TEMPLATE 'T'

/*======================================================================================*/
__END_DECLS
#endif /* common.h */
