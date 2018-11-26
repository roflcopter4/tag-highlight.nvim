# /src
set (tag_highlight_SOURCES
    lang/clang/clang.c
    lang/clang/index.c
    lang/clang/typeid.c
    lang/clang/util.c

    lang/ctags_scan/scan.c
    lang/ctags_scan/strip.c
    lang/ctags_scan/tok.c

    lang/golang/go.c

    nvim_api/api.c
    nvim_api/common.c
    nvim_api/misc.c

    bufdata.c
    ctags.c
    events.c
    globals.c
    lang/lang.c
    main.c
    update_commands.c
)

if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} util/archive_lzma.c)
endif()

add_executable(tag_highlight ${tag_highlight_SOURCES})
add_definitions(-D_BUILD_)

add_library(bstring STATIC
    contrib/bstring/additions.c
    contrib/bstring/b_list.c
    contrib/bstring/bstrlib.c
)

add_library(mpack-c STATIC 
    mpack/data.c
    mpack/decode.c
    mpack/encode.c
    mpack/mpack.c
    mpack/print.c
)

add_library(thl-util STATIC
    contrib/contrib.c
    util/archive_gzip.c
    util/archive_read.c
    util/archive_write.c
    util/find.c
    util/generic_list.c
    util/linked_list.c
    util/util.c
)

################################################################################
# Libraries

if (MINGW)
    target_link_libraries(tag_highlight -lstdc++)
else()
    target_link_libraries(tag_highlight
        -lrt -pthread -lunwind
        ${LIBEV_LIBRARIES}
    )
endif()

target_link_libraries(tag_highlight 
    # -latomic
    ${LIBCLANG_LIBRARY}
    ${LLVM_SYSTEM_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    mpack-c
    bstring
    thl-util
)

if (LZMA_SUPPORT)
    target_link_libraries(tag_highlight ${LIBLZMA_LIBRARIES})
endif()

if (USE_JEMALLOC)
   target_link_libraries(tag_highlight -ljemalloc -ldl)
endif()
