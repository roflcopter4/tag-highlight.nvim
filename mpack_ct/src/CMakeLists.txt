# /src
set (tag_highlight_SOURCES
     archive/archive_util_gzip.c
     archive/archive_util_xz.c
     archive/getlines.c
     archive/writefile.c
     bstring/bstrlib.c
     contrib/bsd_funcs.c
     modified_neotags/main.c
     modified_neotags/strip.c
     modified_neotags/tok.c
     buf_data.c
     constants.c
     ctags.c
     events.c
     linked_list.c
     main.c
     mpack.c
     mpack_decode.c
     mpack_encode.c
     mpack_print.c
     update.c
     util.c
)

# if (LZMA_SUPPORT)
#     set (tag_highlight_SOURCES ${tag_highlight_SOURCES} archive_util_xz.c)
# endif()
# if (NOT HAVE_STRLCPY) 
#     set (tag_highlight_SOURCES ${tag_highlight_SOURCES} bsd_funcs.c)
# endif()

add_executable(tag_highlight ${tag_highlight_SOURCES})

# target_link_libraries(tag_highlight ${tag_highlight_MSGPACK_LIBRARIES})

if (ISWIN)
    include_directories(tag_highlight "${CMAKE_SOURCE_DIR}/contrib/include")
    target_link_libraries(tag_highlight
        "${CMAKE_SOURCE_DIR}/contrib/libz.lib"
        "${CMAKE_SOURCE_DIR}/contrib/liblzma.lib"
        "${CMAKE_SOURCE_DIR}/contrib/libpthread.lib")
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

    if(THREADS_HAVE_PTHREAD_ARG)
        target_link_libraries(tag_highlight -pthread)
    endif()

    # include_directories(${LibArchive_INCLUDE_DIRS})
    # target_link_libraries(tag_highlight ${LibArchive_LIBRARY})

    include_directories(${ZLIB_INCLUDE_DIRS})
    target_link_libraries(tag_highlight ${ZLIB_LIBRARIES})

    if (ISCLANG)
        target_link_libraries (tag_highlight -fuse-ld=lld)
    endif()

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
endif()
