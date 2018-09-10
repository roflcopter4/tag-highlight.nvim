#ifndef SRC_CLANG_LIBCLANG_H
#define SRC_CLANG_LIBCLANG_H
#ifdef __cplusplus
extern "C" {
#endif

#include "util/util.h"

#include "data.h"

/* #ifdef _BUILD_ */
#include <clang-c/CXCompilationDatabase.h> /* TAG: /usr/lib64/llvm/8/include */
#include <clang-c/CXString.h> /* TAG: /usr/lib64/llvm/8/include */
#include <clang-c/Index.h> /* TAG: /usr/lib64/llvm/8/include */
#if 0
/* #else */
#include "/usr/lib64/llvm/8/include/clang-c/CXCompilationDatabase.h"
#include "/usr/lib64/llvm/8/include/clang-c/CXString.h"
#include "/usr/lib64/llvm/8/include/clang-c/Index.h"
/* #endif */
#endif

#define TMPSIZ    (512)
#define CLD(s)    ((struct clangdata *)((s)->clangdata))
#define CS(CXSTR) (clang_getCString(CXSTR))

struct clangdata {
        b_list             *enumerators;
        str_vector         *argv;
        struct cmd_info    *info;
        CXIndex             idx;
        CXTranslationUnit   tu;
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
        int num;
        int kind;
        bstring *group;
};

#if 0
struct toklist {
        CXTranslationUnit *tu;
        CXToken           *cxtokens;
        CXCursor          *cxcursors;
        genlist           *tokdata; /* <- struct token */
};
#endif

struct token {
        CXCursor    cursor;
        CXType      cursortype;
        CXToken     token;
        CXTokenKind tokenkind;
        unsigned    line;
        unsigned    col1;
        unsigned    col2;
        unsigned    len;
        char        raw[];
};

extern const char *const clang_paths[];
extern const size_t      n_clang_paths;

extern const char *const idx_entity_kind_repr[];
extern const size_t      idx_entity_kind_num;

/*======================================================================================*/

#define INTERN __attribute__((visibility("hidden"))) extern

INTERN mpack_call_array *type_id(struct bufdata *        bdata,
                                 struct translationunit *stu,
                                 const b_list *          enumerators,
                                 struct cmd_info *       info,
                                 const int               line,
                                 const int               end);

INTERN void              tokvisitor(struct token *tok);
INTERN IndexerCallbacks *make_cb_struct(void);
INTERN void lc_index_file(struct bufdata *bdata);

INTERN void _free_cxstrings(CXString *str, ...);
#define free_cxstrings(...) _free_cxstrings(__VA_ARGS__, (CXString *)NULL)


/*======================================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* libclang.h */
