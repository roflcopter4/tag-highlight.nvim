#ifndef LANG_CLANG_INTERN_H_
#define LANG_CLANG_INTERN_H_

#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/CXString.h>
#include <clang-c/Index.h>

#include "Common.h"
#include "highlight.h"
#include "lang/lang.h"

__BEGIN_DECLS
/*======================================================================================*/

#define TMPSIZ    (SAFE_PATH_MAX + 1)
#define CS(CXSTR) (clang_getCString(CXSTR))
#define CLD(s)                                                         \
        _Generic((s),                                                  \
                 Buffer *:       (struct clangdata *)((s)->clangdata), \
                 const Buffer *: (struct clangdata *)((s)->clangdata)  \
        )

typedef struct clangdata       clangdata_t;
typedef struct translationunit translationunit_t;
typedef struct token           token_t;
typedef struct resolved_range  resolved_range_t;

struct clangdata {
        Buffer             *bdata;
        str_vector         *argv;
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
        CXIndex            idx;
        unsigned           num;
        nvim_filetype_id   ftid;
};


struct token {
        CXCursor    cursor;
        CXType      cursortype;
        CXToken     token;
        CXTokenKind tokenkind;
        unsigned    line, col1, col2, offset, len;
        bstring     text;
        char        raw[];
};

struct resolved_range {
        unsigned line, start, end, offset1, offset2, len;
        CXFile   file;
};

/*--------------------------------------------------------------------------------------*/

#define INTERN __attribute__((__visibility__("hidden"))) extern

INTERN mpack_arg_array  *create_nvim_calls(Buffer *bdata, translationunit_t *stu);
INTERN IndexerCallbacks *make_cb_struct(void);

INTERN void lc_index_file(Buffer *bdata, translationunit_t *stu, mpack_arg_array *calls);
INTERN bool resolve_range(CXSourceRange r, resolved_range_t *res);
INTERN void get_tmp_path(char *buf);

#undef INTERN

/*--------------------------------------------------------------------------------------*/
/* P99 */

#include "contrib/p99/p99_block.h"
#include "contrib/p99/p99_for.h"
#define P01_FREE_CXSTRING(STR) clang_disposeString(STR)
#define free_cxstrings(...) P99_BLOCK(P99_SEP(P01_FREE_CXSTRING, __VA_ARGS__);)

/*======================================================================================*/
__END_DECLS
#endif /* intern.h */
