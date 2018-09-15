#include "clang.h"
#include "intern.h"

#ifndef ARRSIZ
#  define ARRSIZ(ARR) (sizeof(ARR) / sizeof((ARR)[0]))
#endif

#if 0
const char *const tagargs[TAGARGS_SIZE] = {
    "-DHAVE_CONFIG_H",
    "-D_GNU_SOURCE",
    "-std=gnu11",
    "-I/usr/include",
    "-I/usr/lib64/clang/8.0.0/include",
    "-I/usr/lib64/llvm/8/include",
    "-I/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include",
    "-I/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include-fixed",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/build",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/libclang",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/contrib/jsmn",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/bstring",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/archive",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/mpack",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/util",
};

const char *const CX_token_spelling[5] = {
  "Punctuation",
  "Keyword",
  "Identifier",
  "Literal",
  "Comment",
};
#endif

const char *const clang_paths[] = {
//    //"-std=gnu++17", "-xc++",
//    ///* "-isystem", */ "-I", "/usr/include/c++/v1",
//    /* "-isystem", */ "-isystem/usr/lib64/clang/8.0.0/include",
//    ///* "-isystem", */ "-I", "/usr/lib64/llvm/8/include/x86_64-pc-linux-gnu/clang/Config/config.h",
//    /* "-isystem", */ "-isystem/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include",
//    /* "-isystem", */ "-isystem/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include-fixed",
//    /* "-isystem", "/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include/g++-v8", */
        "-stdlib=libstdc++"
};

const char *const gcc_sys_dirs[] = GCC_ALL_INCLUDE_DIRECTORIES;

const size_t n_clang_paths  = ARRSIZ(clang_paths);
const size_t n_gcc_sys_dirs = ARRSIZ(gcc_sys_dirs);


const char *const idx_entity_kind_repr[] = {
    "Unexposed",
    "Typedef",
    "Function",
    "Variable",
    "Field",
    "EnumConstant",
    "ObjCClass",
    "ObjCProtocol",
    "ObjCCategory",
    "ObjCInstanceMethod",
    "ObjCClassMethod",
    "ObjCProperty",
    "ObjCIvar",
    "Enum",
    "Struct",
    "Union",
    "CXXClass",
    "CXXNamespace",
    "CXXNamespaceAlias",
    "CXXStaticVariable",
    "CXXStaticMethod",
    "CXXInstanceMethod",
    "CXXConstructor",
    "CXXDestructor",
    "CXXConversionFunction",
    "CXXTypeAlias",
    "CXXInterface",
};
const size_t idx_entity_kind_num = ARRSIZ(idx_entity_kind_repr);


int cur_ctick;
pthread_cond_t libclang_cond = PTHREAD_COND_INITIALIZER;
