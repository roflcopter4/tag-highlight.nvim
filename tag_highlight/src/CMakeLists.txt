# /src
set (tag_highlight_SOURCES
    scan_tags/scan.c
    scan_tags/strip.c
    scan_tags/tok.c

    lang/clang/clang.c
    lang/clang/const.c
    lang/clang/typeid.c
    lang/clang/threaded.c
    lang/clang/index.c
    lang/clang/util.c
    lang/clang/clang.h
    lang/clang/intern.h

    nvim_api/api.c
    nvim_api/common.c
    nvim_api/misc.c
    nvim_api/mark2.c
    nvim_api/api.h
    nvim_api/intern.h
    nvim_api/read.h

    buf_data.c
    ctags.c
    events.c
    globals.c
    main.c
    update_commands.c
    data.h
    highlight.h
    my_p99_common.h
    tag_highlight.h

    go_crap.c
    regex.c
    lang/common.c
    lang/common.h

    contrib/bstring/additions.c
    contrib/bstring/b_list.c
    contrib/bstring/bstrlib.c
    contrib/bstring/additions.h
    contrib/bstring/bstring.h
    contrib/bstring/defines.h
    contrib/bstring/private.h

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
    
    contrib/p99/p99.h
    contrib/p99/p99_args.h
    contrib/p99/p99_arith.h
    contrib/p99/p99_atomic.h
    contrib/p99/p99_atomic_arm.h
    contrib/p99/p99_atomic_flag.h
    contrib/p99/p99_atomic_gcc.h
    contrib/p99/p99_atomic_gcc_atomic.h
    contrib/p99/p99_atomic_gcc_sync.h
    contrib/p99/p99_atomic_x86.h
    contrib/p99/p99_bitset.h
    contrib/p99/p99_block.h
    contrib/p99/p99_c99.h
    contrib/p99/p99_c99_default.h
    contrib/p99/p99_c99_throw.h
    contrib/p99/p99_callback.h
    contrib/p99/p99_checkargs.h
    contrib/p99/p99_choice.h
    contrib/p99/p99_classification.h
    contrib/p99/p99_clib.h
    contrib/p99/p99_cm.h
    contrib/p99/p99_compiler.h
    contrib/p99/p99_constraint.h
    contrib/p99/p99_count.h
    contrib/p99/p99_defarg.h
    contrib/p99/p99_double.h
    contrib/p99/p99_enum.h
    contrib/p99/p99_errno.h
    contrib/p99/p99_fifo.h
    contrib/p99/p99_for.h
    contrib/p99/p99_futex.h
    contrib/p99/p99_futex_c11.h
    contrib/p99/p99_futex_linux.h
    contrib/p99/p99_generated.h
    contrib/p99/p99_generic.h
    contrib/p99/p99_getopt.h
    contrib/p99/p99_hook.h
    contrib/p99/p99_id.h
    contrib/p99/p99_if.h
    contrib/p99/p99_init.h
    contrib/p99/p99_int.h
    contrib/p99/p99_iterator.h
    contrib/p99/p99_libc.h
    contrib/p99/p99_lifo.h
    contrib/p99/p99_list.h
    contrib/p99/p99_logical.h
    contrib/p99/p99_map.h
    contrib/p99/p99_new.h
    contrib/p99/p99_notifier.h
    contrib/p99/p99_paste.h
    contrib/p99/p99_posix_default.h
    contrib/p99/p99_qsort.h
    contrib/p99/p99_rand.h
    contrib/p99/p99_rwl.h
    contrib/p99/p99_str.h
    contrib/p99/p99_swap.h
    contrib/p99/p99_threads.h
    contrib/p99/p99_threads_posix.h
    contrib/p99/p99_tp.h
    contrib/p99/p99_try.h
    contrib/p99/p99_tss.h
    contrib/p99/p99_type.h
    contrib/p99/p99_typenames.h
    contrib/p99/p99_uchar.h
    contrib/p99/p99_uf.h
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
        ${LIBEV_LIBRARIES}
    )

    # target_link_libraries(tag_highlight
        # "-L${CMAKE_CURRENT_SOURCE_DIR}/lang/golang/pkg/gccgo_linux_amd64_fPIC"
        # "-Wl,-rpath=${CMAKE_CURRENT_SOURCE_DIR}/lang/golang/pkg/gccgo_linux_amd64_fPIC"
        # "libtag_highlight_go.so"
        # "-L${CMAKE_CURRENT_SOURCE_DIR}/lang/golang/pkg/linux_amd64_shared"
        # "-Wl,-rpath=${CMAKE_CURRENT_SOURCE_DIR}/lang/golang/pkg/linux_amd64_shared"
        # "${CMAKE_SOURCE_DIR}/src/lang/golang/pkg/linux_amd64_shared/tag_highlight_go.a"
    # )

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
