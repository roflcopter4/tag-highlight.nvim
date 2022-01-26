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

    # contrib/bstring/additions.c
    # contrib/bstring/bstrlib.c
    # 
    # contrib/contrib.c
    # 
    # util/archive_gzip.c
    # util/archive_read.c
    # util/archive_write.c
    # util/find.c
    # util/generic_list.c
    # util/linked_list.c
    # util/nanosleep.c
    # util/util.c
)

set (tag-highlight_headers
    Common.h
    events.h
    highlight.h
    mingw_config.h
    my_p99_common.h

    mpack/intern.h
    mpack/mpack.h

    nvim_api/api.h
    nvim_api/intern.h
    nvim_api/read.h
    nvim_api/wait_node.h

    lang/clang/clang.h
    lang/clang/intern.h
    lang/ctags_scan/scan.h
    lang/lang.h

    contrib/bstring/additions.h
    contrib/bstring/bstring.h
    contrib/bstring/defines.h
    contrib/bstring/mingw_config.h
    contrib/bstring/private.h

    contrib/contrib.h

    util/archive.h
    util/find.h
    util/list.h
    util/util.h
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
)

if (LZMA_SUPPORT)
   set (tag-highlight_SOURCES ${tag-highlight_SOURCES} util/archive_lzma.c)
endif()

add_subdirectory(lang)

add_executable(tag-highlight ${tag-highlight_SOURCES})
add_library(thl-util OBJECT ${thl-util_SOURCES})


# set_property(TARGET tag-highlight APPEND PROPERTY INCLUDE_DIRECTORIES
#              ${LIBUV_INCLUDE_DIRS}
# )

################################################################################
# Libraries

target_link_libraries(tag-highlight
    ${CMAKE_THREAD_LIBS_INIT}
    #${ZLIB_LIBRARIES}
    ${LLVM_SYSTEM_LIBRARIES}
    ${LLVM_LIBRARIES}
    ${LIBCLANG_LIBRARY}
    #${LIBUV_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    #-LD:/new_msys64/clang64/lib
    #-levent_core -levent_extra -levent_openssl -levent
    #D:/vcpkg/installed/x64-windows/lib/tinycthread.lib
    -lz -pthread -lgc
    thl-util
    tag_highlight_lang
)

if (IS_BSD)
    target_link_libraries(tag-highlight -lstdthreads -lexecinfo)
elseif (MINGW OR MSVC)
    target_link_libraries(tag-highlight -lDbghelp)
endif ()

if (LIBEV_FOUND)
    target_link_libraries(tag-highlight ${LIBEV_LIBRARIES})
endif ()

if (LZMA_SUPPORT)
    # target_link_libraries(tag-highlight LibLZMA::LibLZMA)
    target_link_libraries(tag-highlight -llzma)
endif ()

if (USE_JEMALLOC)
    target_link_libraries(tag-highlight -ljemalloc)
endif()

if (TALLOC_FOUND)
    target_link_libraries(tag-highlight ${TALLOC_LIBRARIES})

    install(TARGETS tag-highlight
            RUNTIME DESTINATION "bin"
    )
elseif (FALSE)
    include_directories("${PROJECT_SOURCE_DIR}/src/contrib/talloc")

    # Unfortunately, apparently talloc *must* be a shared library
    # to work properly. What a pain.
    add_library(mytalloc SHARED "${PROJECT_SOURCE_DIR}/src/contrib/talloc/talloc.c")
                
    set (mytalloc_compile_options_common
        "-fno-lto"
        "-D_XOPEN_SOURCE_EXTENDED=1"
        "-D__STDC_WANT_LIB_EXT1__=1")

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
                "-fPIC"
                "-fstack-clash-protection"
                "-fstack-protector-strong"
                "-fvisibility=hidden"
                "-DSTATIC_talloc_MODULES=NULL"
                "-DSTATIC_talloc_MODULES_PROTO=extern void __talloc_dummy_module_proto(void)"
        )
    endif()
    
    target_link_options(mytalloc PRIVATE -fno-lto)
    
    if (USE_JEMALLOC)
        target_link_libraries(mytalloc -ljemalloc)
    endif()

    target_link_libraries(tag-highlight mytalloc)

    if (LibBSD_FOUND)
        target_link_libraries(tag-highlight ${LibBSD_LIBRARIES})
    endif()
    
    set_property(TARGET tag-highlight PROPERTY INSTALL_RPATH "\$ORIGIN")

    install(TARGETS tag-highlight mytalloc
            RUNTIME DESTINATION "bin"
            LIBRARY DESTINATION "bin"
    )
else ()
    include_directories("${PROJECT_SOURCE_DIR}/src/contrib/talloc")
    add_library(mytalloc OBJECT "${PROJECT_SOURCE_DIR}/src/contrib/talloc/talloc.c")
    
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

################################################################################

# add_subdirectory(json)

# get_target_property(_THE_FLAGS tag-highlight INCLUDE_DIRECTORIES)
# list(REMOVE_ITEM _THE_FLAGS /usr/include)
# set_target_properties(tag-highlight PROPERTIES INCLUDE_DIRECTORIES "${_THE_FLAGS}")
# 
# get_target_property(_THE_FLAGS thl-util INCLUDE_DIRECTORIES)
# list(REMOVE_ITEM _THE_FLAGS /usr/include)
# set_target_properties(thl-util PROPERTIES INCLUDE_DIRECTORIES "${_THE_FLAGS}")
# 
# get_target_property(_THE_FLAGS tag_highlight_lang INCLUDE_DIRECTORIES)
# list(REMOVE_ITEM _THE_FLAGS /usr/include)
# set_target_properties(tag_highlight_lang PROPERTIES INCLUDE_DIRECTORIES "${_THE_FLAGS}")
