#ifndef SRC_LANG_CLANG_INTERN_H_
#define SRC_LANG_CLANG_INTERN_H_

#include "lang/common.h"

#ifdef I
#  undef I
#endif

#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/CXString.h>
#include <clang-c/Index.h>

__BEGIN_DECLS
/*======================================================================================*/

#define TMPSIZ (SAFE_PATH_MAX + 1)
#define CLD(s) \
        _Generic(s, struct bufdata *: (struct clangdata *)((s)->clangdata), \
                    const struct bufdata *: (struct clangdata *)((s)->clangdata))
#define CS(CXSTR) (clang_getCString(CXSTR))

struct clangdata {
        b_list             *enumerators;
        str_vector         *argv;
        struct cmd_info    *info;
        CXIndex             idx;
        CXTranslationUnit   tu;
        CXFile              mainfile;
        char                tmp_name[TMPSIZ];
};

struct translationunit {
        bstring           *buf;
        genlist           *tokens;
        CXToken           *cxtokens;
        CXCursor          *cxcursors;
        CXTranslationUnit  tu;
        unsigned           num;
};

struct token {
        CXCursor    cursor;
        CXType      cursortype;
        CXToken     token;
        CXTokenKind tokenkind;
        unsigned    line, col1, col2, len;
        bstring     text;
        char        raw[];
};

struct resolved_range {
        unsigned line, start, end, offset;
        CXFile   file;
};

/*======================================================================================*/

#define INTERN __attribute__((__visibility__("hidden"))) extern

INTERN nvim_arg_array *type_id(struct bufdata *bdata, struct translationunit *stu);

INTERN IndexerCallbacks *make_cb_struct(void);
INTERN void              lc_index_file(struct bufdata *bdata, struct translationunit *stu);

INTERN bool resolve_range(CXSourceRange r, struct resolved_range *res);
INTERN void get_tmp_path(char *buf);

/*======================================================================================*/
/* P99 */

#include "my_p99_common.h"
#define P01_FREE_CXSTRING(STR) clang_disposeString(STR)
#define free_cxstrings(...) P99_BLOCK(P99_SEP(P01_FREE_CXSTRING, __VA_ARGS__);)


/*======================================================================================*/
__END_DECLS
#endif /* intern.h */
