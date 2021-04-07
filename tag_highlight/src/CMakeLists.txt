# /src

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_definitions(-D_BUILD_)

set (tag_highlight_SOURCES
    buffer.c
    ctags.c
    data.c
    event_loop.c
    event_handlers.c
    message_autocmd.c
    main.c
    update.c
)

if (LZMA_SUPPORT)
   set (tag_highlight_SOURCES ${tag_highlight_SOURCES} util/archive_lzma.c)
endif()
# if (NOT TALLOC_FOUND)
#     include_directories("${PROJECT_SOURCE_DIR}/src/contrib/talloc")
#     set (tag_highlight_SOURCES ${tag_highlight_SOURCES} contrib/talloc/talloc.c)
# endif ()

add_executable(tag_highlight ${tag_highlight_SOURCES})

add_library(bstring OBJECT
    contrib/bstring/additions.c
    contrib/bstring/bstrlib.c
)

add_library(lang OBJECT
    lang/clang/clang.c
    lang/clang/index.c
    lang/clang/typeid.c
    lang/clang/util.c
    lang/ctags_scan/scan.c
    lang/ctags_scan/strip.c
    lang/ctags_scan/tok.c
    lang/golang/go.c
    lang/lang.c
)

add_library(nvim-api OBJECT
    nvim_api/api.c
    nvim_api/common.c
    nvim_api/misc.c
)

add_library(mpack-c OBJECT 
    mpack/data.c
    mpack/decode.c
    mpack/encode.c
    mpack/mpack.c
    mpack/print.c
)

add_library(thl-util OBJECT
    contrib/contrib.c
    util/archive_gzip.c
    util/archive_read.c
    util/archive_write.c
    util/find.c
    util/generic_list.c
    util/linked_list.c
    util/util.c
)

################################################################################
# Libraries

target_link_libraries(tag_highlight
    ${LIBCLANG_LIBRARY}
    ${LLVM_SYSTEM_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    bstring
    lang
    nvim-api
    mpack-c
    thl-util
)

if (MINGW)
    # For some reason clang wants libstdc++ linked in on Windows.
    target_link_libraries(tag_highlight -lstdc++)
endif ()

if (LIBEV_FOUND)
    target_link_libraries(tag_highlight ${LIBEV_LIBRARIES})
endif ()

if (LIBBSD_FOUND)
    target_link_libraries(tag_highlight ${LIBBSD_LIBRARIES})
endif()

if (LZMA_SUPPORT)
    target_link_libraries(tag_highlight LibLZMA::LibLZMA)
endif ()

if (USE_JEMALLOC STREQUAL "YES")
    target_link_libraries(tag_highlight ${JEMALLOC_LIBRARY})
endif()

if (TALLOC_FOUND)
    target_link_libraries(tag_highlight ${TALLOC_LIBRARIES})

    install(TARGETS tag_highlight
            RUNTIME DESTINATION "bin"
    )
else()
    include_directories("${PROJECT_SOURCE_DIR}/src/contrib/talloc")

    # Unfortunately, apparently talloc *must* be a shared library to work
    # properly. What a pain.
    add_library(mytalloc SHARED
                "${PROJECT_SOURCE_DIR}/src/contrib/talloc/talloc.c")
                
    set (mytalloc_compile_options_common
        "-g"
        "-fPIC"
        "-fno-lto"
        "-D_XOPEN_SOURCE_EXTENDED=1"
        "-D__STDC_WANT_LIB_EXT1__=1"
        "-fstack-clash-protection")

    if (WIN32)
        target_compile_options(
            mytalloc
            PUBLIC
                mytalloc_compile_options_common
                "-DBUILDING_DLL_=1"
        )
    else()
        target_compile_options(
            mytalloc
            PUBLIC
                mytalloc_compile_options_common
                "-fstack-protector-strong"
                "-fvisibility=hidden"
                "-DSTATIC_talloc_MODULES=NULL"
                "-DSTATIC_talloc_MODULES_PROTO=extern void __talloc_dummy_module_proto(void)"
        )
    endif()
    
    if (CC_IS_CLANG)
        target_link_options(mytalloc PRIVATE -fno-lto)
    else()
        target_link_options(mytalloc PRIVATE -fno-lto -fno-whole-program)
    endif()

    target_link_libraries(tag_highlight mytalloc)

    if (LIBBSD_FOUND)
        target_link_libraries(tag_highlight ${LIBBSD_LINK_LIBRARIES})
    endif()
    
    set_property(TARGET tag_highlight PROPERTY INSTALL_RPATH "\$ORIGIN")

    install(TARGETS tag_highlight mytalloc
            RUNTIME DESTINATION "bin"
            LIBRARY DESTINATION "bin"
    )

endif()

