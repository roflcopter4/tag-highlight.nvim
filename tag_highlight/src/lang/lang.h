#pragma once
#ifndef SRC_LANG_LANG_H_
#define SRC_LANG_LANG_H_

#include "Common.h"
#include "highlight.h"
#include "my_p99_common.h"
#include "util/util.h"

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

extern void add_hl_call (mpack_arg_array *calls, int bufnum, int hl_id,
                         const bstring *group, const line_data *data);
extern void add_clr_call(mpack_arg_array *calls, int bufnum, int hl_id, int line, int end);

extern mpack_arg_array  *new_arg_array(void);
extern const bstring   *find_group   (struct filetype *ft, const cmd_info *info,
                                      unsigned num, const int ctags_kind);
extern struct cmd_info *getinfo      (Buffer *bdata);
extern void destroy_struct_info(cmd_info *info);

/*======================================================================================*/

extern FILE *cmd_log;

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
#define EXTENSION_CONSTANT 'c'

/*======================================================================================*/
__END_DECLS
#endif /* lang.h */
