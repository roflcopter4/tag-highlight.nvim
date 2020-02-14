# /src

add_definitions(-D_BUILD_)

set (tag_highlight_SOURCES
    lang/clang/clang.c
    lang/clang/index.c
    lang/clang/typeid.c
    lang/clang/util.c
    lang/ctags_scan/scan.c
    lang/ctags_scan/strip.c
    lang/ctags_scan/tok.c
    lang/golang/go.c
    lang/lang.c

    nvim_api/api.c
    nvim_api/common.c
    nvim_api/misc.c

    buffer.c
    ctags.c
    data.c
    events.c
    event_autocmd.c
    main.c
    update.c
)

if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} util/archive_lzma.c)
endif()

add_executable(tag_highlight ${tag_highlight_SOURCES})

add_library(bstring OBJECT
    contrib/bstring/src/additions.c
    contrib/bstring/src/bstrlib.c
)

add_library(mpack-c OBJECT 
    mpack/data.c
    mpack/decode.c
    mpack/encode.c
    mpack/mpack.c
    mpack/print.c
)

add_library(thl-util OBJECT
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
    # For some reason clang wants libstdc++ linked in on Windows.
    target_link_libraries(tag_highlight
        -lstdc++
    )
else()
    target_link_libraries(tag_highlight
        ${LIBEV_LIBRARIES}
    )
endif()

target_link_libraries(tag_highlight 
    ${LIBCLANG_LIBRARY}
    ${LLVM_SYSTEM_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    mpack-c
    bstring
    thl-util
)

if (LZMA_SUPPORT)
    target_link_libraries(tag_highlight
        LibLZMA::LibLZMA
        # ${LIBLZMA_LIBRARIES}
    )
endif()

if (USE_JEMALLOC STREQUAL "YES")
    target_link_libraries(tag_highlight ${JEMALLOC_LIBRARY})
endif()
