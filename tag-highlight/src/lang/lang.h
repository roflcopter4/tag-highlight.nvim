#ifndef SRC_LANG_LANG_H_
#define SRC_LANG_LANG_H_

#include "Common.h"
#include "highlight.h"
#include "my_p99_common.h"
#include "util/util.h"

__BEGIN_DECLS
/*======================================================================================*/

P99_DECLARE_STRUCT(line_data);
struct line_data {
      alignas(16)
        unsigned line;
        unsigned start;
        unsigned end;
};

extern void add_hl_call (mpack_arg_array *calls, int bufnum, int hl_id,
                         const bstring *group, const line_data *data);
extern void add_clr_call(mpack_arg_array *calls, int bufnum, int hl_id, int line, int end);

extern mpack_arg_array *new_arg_array(void);
extern const bstring *find_group(struct filetype *ft, int ctags_kind)
        __attribute__((__pure__, __warn_unused_result__));

/*======================================================================================*/

extern FILE *cmd_log;

#define CTAGS_CLASS                 'c'
#define CTAGS_PREPROC               'd'
#define CTAGS_ENUMCONST             'e'
#define CTAGS_FUNCTION              'f'
#define CTAGS_ENUM                  'g'
#define CTAGS_MEMBER                'm'
#define CTAGS_NAMESPACE             'n'
#define CTAGS_PACKAGE               'p'
#define CTAGS_STRUCT                's'
#define CTAGS_TYPE                  't'
#define CTAGS_UNION                 'u'
#define CTAGS_GLOBALVAR             'v'
#define EXTENSION_CONSTANT          'C'
#define EXTENSION_OVERLOADED_DECL   'D'
#define EXTENSION_METHOD            'M'
#define EXTENSION_OVERLOADEDOP      'O'
#define EXTENSION_TEMPLATE          'T'
#define EXTENSION_TYPE_KEYWORD      'K'
#define EXTENSION_I_DONT_KNOW       '_'
#define EXTENSION_TEMPLATE_NONTYPE_PARAM 'N'

#define EXTENSION_TEMPLATE_TYPE_PARAM EXTENSION_OVERLOADEDOP


/*======================================================================================*/
__END_DECLS
#endif /* lang.h */
