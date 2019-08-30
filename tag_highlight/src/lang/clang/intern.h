#ifndef LANG_CLANG_INTERN_H_
#define LANG_CLANG_INTERN_H_

#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/CXString.h>
#include <clang-c/Index.h>

#include "Common.h"
#include "highlight.h"
#include "lang/lang.h"

#ifdef __cplusplus
extern "C" {
#endif
/*======================================================================================*/

#define TMPSIZ (SAFE_PATH_MAX + 1)
#define CLD(s) \
        _Generic(s, Buffer *: (struct clangdata *)((s)->clangdata), \
                    const Buffer *: (struct clangdata *)((s)->clangdata))
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
        unsigned line, start, end, offset1, offset2, len;
        CXFile   file;
};

/*======================================================================================*/

#define INTERN __attribute__((__visibility__("hidden"))) extern

INTERN mpack_arg_array *create_nvim_calls(Buffer *bdata, struct translationunit *stu);

INTERN IndexerCallbacks *make_cb_struct(void);
INTERN void              lc_index_file(Buffer *bdata, struct translationunit *stu, mpack_arg_array *calls);

INTERN bool resolve_range(CXSourceRange r, struct resolved_range *res);
INTERN void get_tmp_path(char *buf);

/*======================================================================================*/
/* P99 */

#include "my_p99_common.h"
#define P01_FREE_CXSTRING(STR) clang_disposeString(STR)
#define free_cxstrings(...) P99_BLOCK(P99_SEP(P01_FREE_CXSTRING, __VA_ARGS__);)

/*======================================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* intern.h */
