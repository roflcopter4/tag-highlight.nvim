# /src

add_definitions(-D_BUILD_)

set (tag_highlight_SOURCES
    buffer.c
    ctags.c
    data.c
    event_loop.c
    event_handlers.c
    message_autocmd.c
    main.c
    update.c
)

if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} util/archive_lzma.c)
endif()
# if (NOT TALLOC_FOUND)
#     include_directories("${PROJECT_SOURCE_DIR}/src/contrib/talloc")
#     set (tag_highlight_SOURCES ${tag_highlight_SOURCES} contrib/talloc/talloc.c)
# endif ()

add_executable(tag_highlight ${tag_highlight_SOURCES})

add_library(bstring OBJECT
    contrib/bstring/additions.c
    contrib/bstring/bstrlib.c
)

add_library(lang OBJECT
    lang/clang/clang.c
    lang/clang/index.c
    lang/clang/typeid.c
    lang/clang/util.c
    lang/ctags_scan/scan.c
    lang/ctags_scan/strip.c
    lang/ctags_scan/tok.c
    lang/golang/go.c
    lang/lang.c
)

add_library(nvim-api OBJECT
    nvim_api/api.c
    nvim_api/common.c
    nvim_api/misc.c
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

if (NOT CLANG_STR_SEARCH EQUAL -1)
    # target_link_libraries(tag_highlight
    #     -lunwind
    #     -lc++
    # )
endif ()

if (MINGW)
    # For some reason clang wants libstdc++ linked in on Windows.
    target_link_libraries(tag_highlight
        -lstdc++
    )
else ()
    #target_link_libraries(tag_highlight
    #)
    if (LIBEV_FOUND)
        target_link_libraries(tag_highlight
            ${LIBEV_LIBRARIES}
        )
    endif ()
endif ()

target_link_libraries(tag_highlight
    ${LIBCLANG_LIBRARY}
    ${LLVM_SYSTEM_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    bstring
    lang
    nvim-api
    mpack-c
    thl-util
)
if (LIBBSD_FOUND)
    target_link_libraries(tag_highlight "${LIBBSD_LIBRARIES}")
endif()


if (TALLOC_FOUND)
    target_link_libraries(tag_highlight
        ${TALLOC_LIBRARIES}
    )
else()
    include_directories("${PROJECT_SOURCE_DIR}/src/contrib/talloc")
    add_library(my_talloc SHARED
        "${PROJECT_SOURCE_DIR}/src/contrib/talloc/talloc.c"
    )

    target_compile_options(
        my_talloc
        PUBLIC
            "-fPIC"
            "-D_XOPEN_SOURCE_EXTENDED=1"
            "-D__STDC_WANT_LIB_EXT1__=1"
            "-fstack-protector-strong"
            "-fstack-clash-protection"
            "-fvisibility=hidden"
            "-DSTATIC_talloc_MODULES=NULL"
            "-DSTATIC_talloc_MODULES_PROTO=extern void __talloc_dummy_module_proto(void)"
    )



    target_link_libraries(tag_highlight my_talloc)

    if (LIBBSD_FOUND)
        target_link_libraries(tag_highlight "${LIBBSD_LINK_LIBRARIES}")
    endif()
endif()

if (LZMA_SUPPORT)
    target_link_libraries(tag_highlight
        LibLZMA::LibLZMA
        # ${LIBLZMA_LIBRARIES}
    )
endif ()

if (USE_JEMALLOC STREQUAL "YES")
    target_link_libraries(tag_highlight ${JEMALLOC_LIBRARY})
endif()
