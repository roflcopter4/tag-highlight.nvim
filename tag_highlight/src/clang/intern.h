/* #ifndef SRC_CLANG_INTERN_H */
/* #define SRC_CLANG_INTERN_H */
#ifdef __cplusplus
extern "C" {
#endif

#include "tag_highlight.h"

#include "data.h"

#ifdef I
#  undef I
#endif

#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/CXString.h>
#include <clang-c/Index.h>

#define TMPSIZ    (512)
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

struct cmd_info {
        unsigned num;
        int      kind;
        bstring *group;
};

struct token {
        CXCursor    cursor;
        CXType      cursortype;
        CXToken     token;
        CXTokenKind tokenkind;
        unsigned    line, col1, col2, len;
        char        raw[];
};

struct resolved_range {
        unsigned line, start, end, offset;
        CXFile   file;
};

extern const char *const clang_paths[];
extern const size_t      n_clang_paths;

extern const char *const idx_entity_kind_repr[];
extern const size_t      idx_entity_kind_num;

/*======================================================================================*/

#define INTERN __attribute__((__visibility__("hidden"))) extern

INTERN nvim_call_array *type_id(struct bufdata         *bdata,
                                struct translationunit *stu,
                                const b_list           *enumerators,
                                struct cmd_info        *info,
                                const int               line,
                                const int               end,
                                const bool              clear_first);

INTERN void              tokvisitor(struct token *tok);
INTERN IndexerCallbacks *make_cb_struct(void);
INTERN void              lc_index_file(struct bufdata *bdata);

INTERN bool resolve_range(CXSourceRange r, struct resolved_range *res);
/* INTERN void locate_extent(bstring *dest, struct bufdata *bdata, const struct resolved_range *res); */

/*======================================================================================*/
/* P99 */

#include "my_p99_common.h"
#define P01_FREE_CXSTRING(STR) clang_disposeString(STR)
#define free_cxstrings(...) P99_BLOCK(P99_SEP(P01_FREE_CXSTRING, __VA_ARGS__);)


/*======================================================================================*/
#ifdef __cplusplus
}
#endif
/* #endif [> intern.h <] */
