# /src
set (tag_highlight_SOURCES

    archive/archive_util_gzip.c
    archive/readfile.c
    archive/writefile.c

    bstring/bstrlib.c
    bstring/additions.c
    bstring/b_list.c
    
    contrib/contrib.c
    contrib/jsmn/jsmn.c
    json.c

    modified_neotags/neotags.c
    modified_neotags/strip.c
    modified_neotags/tok.c

    generic_list.c
    buf_data.c
    globals.c
    ctags.c
    events.c
    linked_list.c
    main.c
    mpack.c
    mpack_decode.c
    mpack_encode.c
    mpack_print.c
    parse.c
    update_commands.c
    util.c
)
file (GLOB tag_highlight_GLOB *.h)

if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} archive/archive_util_xz.c)
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
    target_link_libraries(tag_highlight -lunwind)
#   target_link_libraries(tag_highlight -lpcre2-8)

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
endif()
