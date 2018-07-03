# /src
set (mytags_SOURCES
     archive/archive_util_gzip.c
     archive/archive_util_xz.c
     archive/getlines.c
     bstring/bstrlib.c
     buf_data.c
     constants.c
     contrib/bsd_funcs.c
     events.c
     linked_list.c
     ctags.c
     main.c
     mpack.c
     mpack_encode.c
     mpack_obj_decode.c
     mpack_print.c
     mpack_stream_decode.c
     util.c
)

# if (LZMA_SUPPORT)
#     set (mytags_SOURCES ${mytags_SOURCES} archive_util_xz.c)
# endif()
# if (NOT HAVE_STRLCPY) 
#     set (mytags_SOURCES ${mytags_SOURCES} bsd_funcs.c)
# endif()

add_executable(mytags ${mytags_SOURCES})

# target_link_libraries(mytags ${mytags_MSGPACK_LIBRARIES})

if (ISWIN)
    include_directories(mytags "${CMAKE_SOURCE_DIR}/contrib/include")
    target_link_libraries(mytags
        "${CMAKE_SOURCE_DIR}/contrib/libz.lib"
        "${CMAKE_SOURCE_DIR}/contrib/liblzma.lib"
        "${CMAKE_SOURCE_DIR}/contrib/libpthread.lib")
else()
    if (LZMA_SUPPORT)
        include_directories(${LIBLZMA_INCLUDE_DIRS})
        if (BUILD_DIST OR STATIC_LIBS_ONLY)
            add_library(neo_LZMA_STATIC STATIC IMPORTED)
            set_property(TARGET neo_LZMA_STATIC PROPERTY IMPORTED_LOCATION /usr/lib64/liblzma.a)
            target_link_libraries(mytags neo_LZMA_STATIC)
        else()
            target_link_libraries(mytags ${LIBLZMA_LIBRARIES})
        endif()
    endif()

    if(THREADS_HAVE_PTHREAD_ARG)
        target_link_libraries(mytags -pthread)
    endif()

    # include_directories(${LibArchive_INCLUDE_DIRS})
    # target_link_libraries(mytags ${LibArchive_LIBRARY})

    include_directories(${ZLIB_INCLUDE_DIRS})
    target_link_libraries(mytags ${ZLIB_LIBRARIES})
endif()
