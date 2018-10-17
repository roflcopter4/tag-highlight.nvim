# /src
set (tag_highlight_SOURCES
    scan_tags/scan.c
    scan_tags/strip.c
    scan_tags/tok.c

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
    nvim_api/read.h

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
    my_p99_common.h
    tag_highlight.h

    regex.c

    bstring/additions.c
    bstring/b_list.c
    bstring/bstrlib.c
    bstring/additions.h
    bstring/bstring.h
    bstring/defines.h
    bstring/private.h

    mpack/decode.c
    mpack/encode.c
    mpack/mpack.c
    mpack/print.c
    mpack/data.c
    mpack/intern.h
    mpack/mpack.h

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

    contrib/contrib.c
    contrib/contrib.h
    contrib/jsmn/jsmn.c
    contrib/jsmn/jsmn.h
    
    p99/p99.h
    p99/p99_args.h
    p99/p99_arith.h
    p99/p99_atomic.h
    p99/p99_atomic_arm.h
    p99/p99_atomic_flag.h
    p99/p99_atomic_gcc.h
    p99/p99_atomic_gcc_atomic.h
    p99/p99_atomic_gcc_sync.h
    p99/p99_atomic_x86.h
    p99/p99_bitset.h
    p99/p99_block.h
    p99/p99_c99.h
    p99/p99_c99_default.h
    p99/p99_c99_throw.h
    p99/p99_callback.h
    p99/p99_checkargs.h
    p99/p99_choice.h
    p99/p99_classification.h
    p99/p99_clib.h
    p99/p99_cm.h
    p99/p99_compiler.h
    p99/p99_constraint.h
    p99/p99_count.h
    p99/p99_defarg.h
    p99/p99_double.h
    p99/p99_enum.h
    p99/p99_errno.h
    p99/p99_fifo.h
    p99/p99_for.h
    p99/p99_futex.h
    p99/p99_futex_c11.h
    p99/p99_futex_linux.h
    p99/p99_generated.h
    p99/p99_generic.h
    p99/p99_getopt.h
    p99/p99_hook.h
    p99/p99_id.h
    p99/p99_if.h
    p99/p99_init.h
    p99/p99_int.h
    p99/p99_iterator.h
    p99/p99_libc.h
    p99/p99_lifo.h
    p99/p99_list.h
    p99/p99_logical.h
    p99/p99_map.h
    p99/p99_new.h
    p99/p99_notifier.h
    p99/p99_paste.h
    p99/p99_posix_default.h
    p99/p99_qsort.h
    p99/p99_rand.h
    p99/p99_rwl.h
    p99/p99_str.h
    p99/p99_swap.h
    p99/p99_threads.h
    p99/p99_threads_posix.h
    p99/p99_tp.h
    p99/p99_try.h
    p99/p99_tss.h
    p99/p99_type.h
    p99/p99_typenames.h
    p99/p99_uchar.h
    p99/p99_uf.h
)
if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} util/archive_lzma.c)
endif()

add_executable(tag_highlight ${tag_highlight_SOURCES})
add_definitions(-D_BUILD_)

if (MSVC)
    include_directories(tag_highlight "${CMAKE_SOURCE_DIR}/win32_libs/include")
    target_link_libraries(tag_highlight
        "${CMAKE_SOURCE_DIR}/win32_libs/lib/libz.lib"
        "${CMAKE_SOURCE_DIR}/win32_libs/lib/liblzma.lib"
        "${CMAKE_SOURCE_DIR}/win32_libs/lib/pthreadVC2.lib")
else()
    if (MINGW)
        target_link_libraries(tag_highlight -lstdc++)
    else()
        target_link_libraries(tag_highlight -lrt -pthread)
    endif()
    
    target_link_libraries(tag_highlight 
        -lm -latomic
        #"D:/msys64/mingw64/lib/libz.a"
        #"D:/msys64/mingw64/lib/libclang.a"
        #"D:/msys64/mingw64/lib/liblzma.a"
        #"D:/msys64/mingw64/lib/libcpre2-8.a"
        ${LIBCLANG_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${PCRE2_LIBRARIES}
    )
    if (LZMA_SUPPORT)
        target_link_libraries(tag_highlight ${LIBLZMA_LIBRARIES})
    endif()
    
    #if (USE_JEMALLOC)
    #    target_link_libraries(tag_highlight /opt/jemalloc/lib/libjemalloc.so.2 -ldl)
    #endif()
    #if (CLANG_STR_SEARCH)
    #    target_link_libraries(tag_highlight -lunwind)
    #endif()
    #set(THL_GO_DIR "${PROJECT_SOURCE_DIR}/src/lang/golang/pkg/gccgo_linux_amd64_fPIC")
    #target_link_libraries(tag_highlight "-Wl,-rpath=${THL_GO_DIR} -L${THL_GO_DIR} -ltag_highlight")
endif()
