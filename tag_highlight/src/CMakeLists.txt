# /src
set (tag_highlight_SOURCES
    bstring/bstrlib.c
    bstring/additions.c
    bstring/b_list.c
    
    contrib/contrib.c
    contrib/jsmn/jsmn.c
    json.c

    modified_neotags/neotags.c
    modified_neotags/strip.c
    modified_neotags/tok.c

    mpack/decode.c
    mpack/encode.c
    mpack/mpack.c
    mpack/print.c

    util/archive_gzip.c
    util/archive_read.c
    util/archive_write.c
    util/find.c
    util/generic_list.c
    util/linked_list.c
    util/util.c

    api.c
    buf_data.c
    ctags.c
    events.c
    globals.c
    loop.c
    main.c
    parse.c
    scan_headers.c
    update_commands.c
)
file (GLOB tag_highlight_GLOB *.h)

if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} util/archive_lzma.c)
endif()

add_executable(tag_highlight 
    ${tag_highlight_GLOB}
    ${tag_highlight_SOURCES}
)

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
    # set (JEMALLOC_STUFF /opt/jemalloc/lib/libjemalloc.so.2 -lm -lstdc++ -lpthread -ldl)
    if (LZMA_SUPPORT)
        include_directories(${LIBLZMA_INCLUDE_DIRS})
        if (BUILD_DIST OR STATIC_LIBS_ONLY)
            add_library(neo_LZMA_STATIC STATIC IMPORTED)
            set_property(TARGET neo_LZMA_STATIC PROPERTY IMPORTED_LOCATION /usr/lib64/liblzma.a)
            target_link_libraries(tag_highlight neo_LZMA_STATIC)
        else()
            target_link_libraries(tag_highlight ${LIBLZMA_LIBRARIES})
        endif()
    endif()
    
    include_directories(${ZLIB_INCLUDE_DIRS})
    target_link_libraries(tag_highlight -pthread)
    target_link_libraries(tag_highlight ${ZLIB_LIBRARIES})
    target_link_libraries(tag_highlight ${JEMALLOC_STUFF})

    if (CLANG_STR_SEARCH)
        target_link_libraries(tag_highlight -lunwind)
    endif()
#   target_link_libraries(tag_highlight -lpcre2-8)

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
endif()
