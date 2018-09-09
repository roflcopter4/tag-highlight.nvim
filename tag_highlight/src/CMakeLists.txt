# /src
set (tag_highlight_SOURCES
    contrib/contrib.c
    contrib/jsmn/jsmn.c

    tagscan/scan.c
    tagscan/strip.c
    tagscan/tok.c

    clang/cl2.c
    clang/const.c
    clang/typeid.c
    clang/threaded.c
    clang/clang.h
    clang/clang_intern.h

    nvim_api/api.c
    nvim_api/common.c
    nvim_api/api.h
    nvim_api/intern.h

    buf_data.c
    ctags.c
    events.c
    globals.c
    json.c
    loop.c
    main.c
    parse.c
    scan_headers.c
    update_commands.c
    data.h
    highlight.h
)
if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} util/archive_lzma.c)
endif()

add_executable(tag_highlight ${tag_highlight_SOURCES})

add_library(bstring STATIC
    bstring/additions.c
    bstring/b_list.c
    bstring/bstrlib.c
    bstring/additions.h
    bstring/bstring.h
    bstring/defines.h
    bstring/private.h
)
add_library(mpack STATIC
    mpack/decode.c
    mpack/encode.c
    mpack/mpack.c
    mpack/print.c
    mpack/data.c
    mpack/intern.h
    mpack/mpack.h
)
add_library(util STATIC
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
add_library(all_libs SHARED
    nvim_api/api.c
    nvim_api/common.c
    bstring/bstrlib.c
    bstring/additions.c
    bstring/b_list.c
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
)

target_link_libraries(tag_highlight util mpack bstring)

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
    # set (JEMALLOC_STUFF /opt/jemalloc/lib/libjemalloc.so.2 -lm -lstdc++ -lpthread -ldl)
    # target_link_libraries(tag_highlight "-L/mnt/arch/backup/clang-dev/lib" /mnt/arch/backup/clang-dev/lib/libclang.so.8svn -lstdc++)

    # target_link_libraries(tag_highlight -static "${myCLANGLIST}" -lstdc++)
    # target_link_libraries(tag_highlight -Wl,-Bdynamic /mnt/arch/backup/clang-dev/lib/libclang.so -lunwind)
    # target_link_libraries(tag_highlight -static "-L${CMAKE_SOURCE_DIR}" -lclang ${myCLANGLIST} -Wl,-Bdynamic -lstdc++ -lunwind -lm -ldl -lrt -ldl -lcurses -lxml2)

    # target_link_libraries(tag_highlight -static)
    include_directories(${ZLIB_INCLUDE_DIRS})
    target_link_libraries(tag_highlight -pthread)
    target_link_libraries(tag_highlight ${ZLIB_LIBRARIES})

    # set(CMAKE_C_FLAGS "-I/mnt/arch/backup/clang-dev/include")
    # set(CMAKE_C_FLAGS "${LIBCLANG_CFLAGS} ${CMAKE_C_FLAGS}")
    # target_link_libraries(tag_highlight ${LIBCLANG_LIBRARIES})
    # target_link_libraries(tag_highlight -static /mnt/arch/backup/clang-dev/lib/libclang.a -lunwind ${myCLANGLIST} -Wl,-Bdynamic -lstdc++ -lunwind -lm -ldl -lrt -ldl -lcurses -lxml2)
    # target_link_libraries(tag_highlight "-L/opt/DCLANG/lib" -Wl,-rpath=/opt/DCLANG/lib /opt/DCLANG/lib/libclang.so.8svn -lstdc++)

    if (LZMA_SUPPORT)
        include_directories(${LIBLZMA_INCLUDE_DIRS})
        target_link_libraries(tag_highlight ${LIBLZMA_LIBRARIES})
    endif()
    if (CLANG_STR_SEARCH)
        target_link_libraries(tag_highlight -lunwind)
    endif()

    # target_link_libraries(tag_highlight -lpcre2-8)

    include_directories(/opt/more_clang/include)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic -Wl,-rpath=/opt/more_clang/lib -L/opt/more_clang/lib")
    target_link_libraries(tag_highlight /opt/more_clang/lib/libclang.so.8svn)
endif()
