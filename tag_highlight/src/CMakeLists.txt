# /src
set (tag_highlight_SOURCES
    tagscan/scan.c
    tagscan/strip.c
    tagscan/tok.c

    clang/clang.c
    clang/const.c
    clang/typeid.c
    clang/threaded.c
    clang/index.c
    clang/util.c
    clang/clang.h
    clang/intern.h

    nvim_api/api.c
    nvim_api/common.c
    nvim_api/misc.c
    nvim_api/api.h
    nvim_api/intern.h

    buf_data.c
    ctags.c
    events.c
    globals.c
    json.c
    main.c
    scan_headers.c
    update_commands.c
    data.h
    highlight.h
)
if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} util/archive_lzma.c)
endif()

add_executable(tag_highlight ${tag_highlight_SOURCES})

add_library(bstring OBJECT
    bstring/additions.c
    bstring/b_list.c
    bstring/bstrlib.c
    bstring/additions.h
    bstring/bstring.h
    bstring/defines.h
    bstring/private.h
)
add_library(mpack OBJECT
    mpack/decode.c
    mpack/encode.c
    mpack/mpack.c
    mpack/print.c
    mpack/data.c
    mpack/intern.h
    mpack/mpack.h
)
add_library(util OBJECT
    util/archive_gzip.c
    util/archive_read.c
    util/archive_write.c
    util/find.c
    util/generic_list.c
    util/linked_list.c
    util/util.c
    util/archive.h
    util/find.h
    util/list.h
    util/util.h
)
add_library(contrib OBJECT
    contrib/contrib.c
    contrib/contrib.h
    contrib/jsmn/jsmn.c
    contrib/jsmn/jsmn.h
)

target_link_libraries(tag_highlight util mpack bstring contrib)

add_definitions(-D_BUILD_)

if (MSVC)
    include_directories(tag_highlight "${CMAKE_SOURCE_DIR}/win32_libs/include")
    target_link_libraries(tag_highlight
        "${CMAKE_SOURCE_DIR}/win32_libs/lib/libz.lib"
        "${CMAKE_SOURCE_DIR}/win32_libs/lib/liblzma.lib"
        "${CMAKE_SOURCE_DIR}/win32_libs/lib/pthreadVC2.lib")
    
elseif(MINGW)
    target_link_libraries(tag_highlight -lz -pthread
        "${CMAKE_SOURCE_DIR}/win32_libs/lib/liblzma.lib")

else()
    include_directories(${ZLIB_INCLUDE_DIRS})
    target_link_libraries(tag_highlight -pthread)
    target_link_libraries(tag_highlight ${ZLIB_LIBRARIES})
    target_link_libraries(tag_highlight -lm -latomic)

    string(CONCAT CMAKE_C_FLAGS "${LIBCLANG_CFLAGS}" " ${CMAKE_C_FLAGS}")
    target_link_libraries(tag_highlight ${LIBCLANG_LIBRARIES})

    # target_link_libraries(tag_highlight -lpcre2-8)

    if (USE_JEMALLOC)
        target_link_libraries(tag_highlight /opt/jemalloc/lib/libjemalloc.so.2 -ldl)
    endif()
    if (LZMA_SUPPORT)
        include_directories(${LIBLZMA_INCLUDE_DIRS})
        target_link_libraries(tag_highlight ${LIBLZMA_LIBRARIES})
    endif()
    if (CLANG_STR_SEARCH)
        target_link_libraries(tag_highlight -lunwind)
    endif()
endif()
