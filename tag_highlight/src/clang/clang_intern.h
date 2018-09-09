#ifndef SRC_CLANG_LIBCLANG_H
#define SRC_CLANG_LIBCLANG_H

#include "util/util.h"

#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#define TAGARGS_SIZE (16)
extern const char *const tagargs[TAGARGS_SIZE];
extern const char *const CX_token_spelling[5];
extern const char *const clang_paths[];
extern const size_t      n_clang_paths;

/*======================================================================================*/

#if 0
__attribute__((visibility("hidden"))) extern void typeswitch(struct bufdata *bdata,
                                                             struct translationunit *stu,
                                                             const b_list *enumerators,
                                                             struct cmd_info **info);
__attribute__((visibility("hidden"))) extern void typeswitch_2(struct bufdata *bdata,
                                                               struct translationunit *stu,
                                                               const b_list *enumerators,
                                                               struct cmd_info **info,
                                                               int line,
                                                               int end);
#endif
/* __attribute__((visibility("hidden"))) */ extern void type_id(struct bufdata *bdata,
                                                          struct translationunit *stu,
                                                          const b_list *enumerators,
                                                          struct cmd_info *info,
                                                          const int        line,
                                                          const int        end);

///* __attribute__((visibility("hidden"))) */ extern void tokvisitor(struct token *tok);
void tokvisitor(struct token *tok);

#ifdef __cplusplus
}
#endif
#endif /* libclang.h */
