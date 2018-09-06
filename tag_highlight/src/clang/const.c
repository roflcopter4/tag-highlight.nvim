#include "clang.h"
#include "libclang.h"

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

const char *const clang_paths[3] = {
    "-I/usr/lib64/clang/8.0.0/include",
    "-I/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include",
    "-I/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include-fixed",
};