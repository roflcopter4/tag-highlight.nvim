# /src

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_definitions(-D_BUILD_)

set (tag-highlight_SOURCES

    # ${PROJECT_SOURCE_DIR}/src/contrib/talloc/talloc.c

    buffer.c
    ctags.c
    data.c
    event_loop.c
    event_handlers.c
    main.c
    update.c

    mpack/data.c
    mpack/decode.c
    mpack/encode.c
    mpack/mpack.c
    mpack/print.c

    nvim_api/api.c
    nvim_api/common.c
    nvim_api/misc.c


    Common.h
    events.h
    highlight.h
	macros.h
    mingw_config.h
    my_p99_common.h

    mpack/intern.h
    mpack/mpack.h

    nvim_api/api.h
    nvim_api/intern.h
    nvim_api/read.h
    nvim_api/wait_node.h
)

set (thl-util_SOURCES
    contrib/bstring/additions.c
    contrib/bstring/bstrlib.c

    contrib/contrib.c

    util/archive_gzip.c
    util/archive_read.c
    util/archive_write.c
    util/cxx_utils.cc
    util/err.c
    util/find.c
    util/format_binary.c
    util/generic_list.c
    util/linked_list.c
    util/nanosleep.c
    util/temp_name.c
    util/util.c

    contrib/bstring/additions.h
    contrib/bstring/bstring.h
    contrib/bstring/defines.h
    contrib/bstring/mingw_config.h
    contrib/bstring/private.h

    contrib/contrib.h

    util/archive.h
    util/find.h
    util/initializer_hack.h
    util/list.h
    util/util.h
)

set (p99_SOURCES
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
    contrib/p99/p00_pragmas.h
)

if (LZMA_SUPPORT)
   set (tag-highlight_SOURCES ${tag-highlight_SOURCES} util/archive_lzma.c)
endif()

if(NOT HAVE_THREADS_H)

    include_directories("${CMAKE_SOURCE_DIR}/contrib/tinycthread/source")
    message("FOO")
endif()

add_subdirectory(lang)

add_executable(tag-highlight
	${tag-highlight_SOURCES}
    ${p99_SOURCES}
)
add_library(thl-util OBJECT
	${thl-util_SOURCES}
)

if (NOT MINGW AND NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   target_precompile_headers(tag-highlight
       PRIVATE
       $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/Common.h>
   )
endif()

#####################################################################################################
# Extra flags, includes

list(JOIN GLib_PKG_CFLAGS " " _glib_cflags )
list(JOIN GLib_PKG_LIBS " " _glib_ldflags)
set_property(TARGET tag-highlight APPEND PROPERTY COMPILE_FLAGS "${_glib_cflags}")

# set_property(TARGET tag-highlight APPEND PROPERTY INCLUDE_DIRECTORIES
#              ${LIBUV_INCLUDE_DIRS}
# )

#if (MSVC)
#    set (_unistring_location "${CMAKE_SOURCE_DIR}/contrib/libunistring")
#    set_property(TARGET tag-highlight APPEND PROPERTY INCLUDE_DIRECTORIES
#        "${_unistring_location}/include")
#    target_link_libraries(tag-highlight
#        "${_unistring_location}/lib/libunistring.dll.a")
#else()
#    if (MINGW)
#    else()
#        set_property(TARGET tag-highlight APPEND PROPERTY LINK_FLAGS "-Wl,--as-needed")
#    endif()
#    target_link_libraries(tag-highlight -lunistring)
#    set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/pch.hh APPEND PROPERTY COMPILE_OPTIONS
#            -fpch-debuginfo -fpch-instantiate-templates -fpch-codegen -relocatable-pch
#    )
#endif()

if (MSVC AND NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # FIXME: This is clearly trash and needs to be re-written.

    if (NOT _VCPKG_INSTALLED_DIR)
        message(FATAL_ERROR "No vcpkg found")
    endif()

    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
        set (_WIN32_VCPKG_LIB_PATH "${_VCPKG_INSTALLED_DIR}/x64-windows/debug/lib")
        #target_link_libraries(tag-highlight ${TINYCTHREAD_LIBRARY_DEBUG})
    else()
        set (_WIN32_VCPKG_LIB_PATH "${_VCPKG_INSTALLED_DIR}/x64-windows/lib")
        #target_link_libraries(tag-highlight ${TINYCTHREAD_LIBRARY_RELEASE})
    endif()

endif()

################################################################################
# Libraries

target_link_libraries(tag-highlight
    ${CMAKE_THREAD_LIBS_INIT}
    ${ZLIB_LIBRARIES}
    ${LLVM_SYSTEM_LIBRARIES}
    ${LLVM_LIBRARIES}
    ${LIBCLANG_LIBRARY}
    #-lz
    thl-util
    tag_highlight_lang
)

if (MSVC)
    target_link_libraries(tag-highlight unofficial::libuv::libuv)
else ()
    target_link_libraries(tag-highlight ${LIBUV_LIBRARIES})
endif()

if (IS_BSD)
    target_link_libraries(tag-highlight -lstdthreads -lexecinfo)
elseif (MINGW)
    target_link_libraries(tag-highlight -lDbghelp ws2_32.lib)
elseif (MSVC)
    target_link_libraries(tag-highlight -lDbghelp pthreadVC3.lib ws2_32.lib)
endif ()
if(NOT HAVE_THREADS_H)
    add_library(tinycthread OBJECT "${CMAKE_SOURCE_DIR}/contrib/tinycthread/source/tinycthread.c")
    target_link_libraries(tag-highlight tinycthread)
endif()

if (LIBEV_FOUND)
    target_link_libraries(tag-highlight ${LIBEV_LIBRARIES})
endif ()
# if (LIBEVENT_FOUND)
    target_link_libraries(tag-highlight ${LIBEVENT_LIBRARIES})
# else()

    #include_directories("D:/OPT/Libevent2/include")
    #target_link_libraries(tag-highlight 
    #"D:/OPT/Libevent2/lib/libevent.a"
    #"D:/OPT/Libevent2/lib/libevent_core.a"
    #"D:/OPT/Libevent2/lib/libevent_extra.a"
    #"D:/OPT/Libevent2/lib/libevent_openssl.a"
    #"D:/OPT/Libevent2/lib/libevent_pthreads.a"
#)
# endif()

if (LZMA_SUPPORT)
    # target_link_libraries(tag-highlight LibLZMA::LibLZMA)
    target_link_libraries(tag-highlight -llzma)
endif ()

if (USE_JEMALLOC)
    target_link_libraries(tag-highlight -ljemalloc)
endif()

if (TALLOC_FOUND)
    target_link_libraries(tag-highlight ${TALLOC_LIBRARIES})
else ()
    include_directories("${PROJECT_SOURCE_DIR}/src/contrib/talloc")
    add_library(mytalloc OBJECT
		"${PROJECT_SOURCE_DIR}/src/contrib/talloc/talloc.c"
		"${PROJECT_SOURCE_DIR}/src/contrib/talloc/talloc.h"
    )

    set (mytalloc_compile_options_common
            # "-flto"
            "-D_XOPEN_SOURCE_EXTENDED=1"
            "-D__STDC_WANT_LIB_EXT1__=1"
        )

    if (MINGW OR MSVC_LIKE)
        target_compile_options(
            mytalloc
            PRIVATE
                ${mytalloc_compile_options_common}
                "-DBUILDING_DLL_=1"
        )
    elseif (NOT MSVC_LIKE)
        target_compile_options(
            mytalloc
            PRIVATE
                ${mytalloc_compile_options_common}
                "-fvisibility=hidden"
                "-DSTATIC_talloc_MODULES=NULL"
                "-DSTATIC_talloc_MODULES_PROTO=extern void __talloc_dummy_module_proto(void)"
        )
    endif()

    target_link_libraries(tag-highlight mytalloc)
endif()


install(TARGETS tag-highlight
    RUNTIME DESTINATION "bin"
)
