#
# Language submodules
#

set (tag-highlight-lang_SOURCES
    clang/clang.c
    clang/index.c
    clang/typeid.c
    clang/util.c
    ctags_scan/scan.c
    ctags_scan/strip.c
    ctags_scan/tok.c
    golang/golang.c
    golang/pipe.c
    lang.c
)

add_subdirectory(golang)

add_library(tag_highlight_lang OBJECT
    ${tag-highlight-lang_SOURCES}
)
