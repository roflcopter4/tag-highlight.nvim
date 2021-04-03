cmake_minimum_required (VERSION 3.14.0)
project (Tag_Highlight_Nvim C)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)

if (MSVC)
    message(FATAL_ERROR
            "MSVC is not and will probably never be supported. Use MinGW")
endif()

###############################################################################
# Options

set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/..")
get_filename_component(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)

set (REQUIRED_LZMA_VERSION 5.3.1)
option(SANITIZE "Enable sanitizers" OFF)
option(USE_JEMALLOC "Use jemalloc" OFF)
option(USE_LIBEV "Use libev for the event loop (if available)" "TRY")

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

macro(FIX_WINDOWS_PATHS _pathvar)
    string(REPLACE "\\" "/" ${_pathvar} "${${_pathvar}}")
endmacro()

if (NOT CMAKE_BUILD_TYPE)
    message("Setting build type to \"Release\"")
    set (CMAKE_BUILD_TYPE "Release")
endif()

if ((CMAKE_BUILD_TYPE STREQUAL "Debug") OR (CMAKE_BUILD_TYPE STREQUAL
                                            "RelWithDebInfo"))
    set (DEBUG 1)
endif()

###############################################################################
# Includes and defines

if (MINGW)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include _mingw.h")

    string(JOIN " " CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS}
           "-Wl,--allow-multiple-definition")

    add_definitions(-D__MINGW__)
    add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
else()
    string(JOIN " " CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS}
           -L/usr/local/lib -rdynamic)
endif()

add_definitions(-D_GNU_SOURCE -DHAVE_CONFIG_H -DBSTR_USE_P99 -DBSTR_USE_TALLOC)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib/bstring")

set (CMAKE_REQUIRED_DEFINITIONS -D__USE_ISOC11 -D_GNU_SOURCE -DHAVE_CONFIG_H)

include("${PROJECT_SOURCE_DIR}/cmake/get_compiler_include_paths.cmake")
GET_COMPILER_PATHS(COMPILER_INCLUDE_DIRECTORIES)

###############################################################################
# Libraries

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package (ZLIB REQUIRED)
find_package (Threads REQUIRED)
find_package (LibClang REQUIRED)
find_package (LibLZMA)

if (LIBLZMA_FOUND)
    if (${LIBLZMA_VERSION_STRING} STRLESS ${REQUIRED_LZMA_VERSION})
        message(WARNING "\
liblzma version ${REQUIRED_LZMA_VERSION} or greater is required for XZ support. \
XZ support will be disabled for this build.")
    else()
        include_directories(${LIBLZMA_INCLUDE_DIRS})
        set (LZMA_SUPPORT 1)
    endif()
endif()

if (MINGW)
    # Nil I guess
else ()
    find_package (Talloc)
    if (USE_LIBEV)
        if ("${USE_LIBEV}" STREQUAL "TRY")
            find_package (LibEV)
        else()
            find_package (LibEv REQUIRED)
        endif()
        if (LIBEV_FOUND)
            set (HAVE_LIBEV 1)
        endif()
    endif()
endif()

if (USE_JEMALLOC)
    find_package (Jemalloc REQUIRED)
endif()


###############################################################################
# Symbol and header checks

check_symbol_exists (err            "err.h"      HAVE_ERR)
CHECK_SYMBOL_EXISTS (posix_spawnp   "spawn.h"    HAVE_POSIX_SPAWNP)
CHECK_SYMBOL_EXISTS (arc4random     "stdlib.h"   HAVE_ARC4RANDOM)
CHECK_SYMBOL_EXISTS (mkostemps      "stdlib.h"   HAVE_MKOSTEMPS)
CHECK_SYMBOL_EXISTS (reallocarray   "stdlib.h"   HAVE_REALLOCARRAY)
CHECK_SYMBOL_EXISTS (asprintf       "stdio.h"    HAVE_ASPRINTF)
CHECK_SYMBOL_EXISTS (open_memstream "stdio.h"    HAVE_OPEN_MEMSTREAM)
CHECK_SYMBOL_EXISTS (vasprintf      "stdio.h"    HAVE_VASPRINTF)
CHECK_SYMBOL_EXISTS (memrchr        "string.h"   HAVE_MEMRCHR)
CHECK_SYMBOL_EXISTS (strchrnul      "string.h"   HAVE_STRCHRNUL)
CHECK_SYMBOL_EXISTS (strdupa        "string.h"   HAVE_STRDUPA)
CHECK_SYMBOL_EXISTS (stricmp        "string.h"   HAVE_STRICMP)
CHECK_SYMBOL_EXISTS (strlcat        "string.h"   HAVE_STRLCAT)
CHECK_SYMBOL_EXISTS (strlcpy        "string.h"   HAVE_STRLCPY)
CHECK_SYMBOL_EXISTS (strsep         "string.h"   HAVE_STRSEP)
CHECK_SYMBOL_EXISTS (strtonum       "string.h"   HAVE_STRTONUM)
CHECK_SYMBOL_EXISTS (strcasecmp     "strings.h"  HAVE_STRCASECMP)
CHECK_SYMBOL_EXISTS (gettimeofday   "sys/time.h" HAVE_GETTIMEOFDAY)
CHECK_SYMBOL_EXISTS (clock_gettime  "time.h"     HAVE_CLOCK_GETTIME)
CHECK_SYMBOL_EXISTS (fork           "unistd.h"   HAVE_FORK)
CHECK_SYMBOL_EXISTS (pause          "unistd.h"   HAVE_PAUSE)
CHECK_SYMBOL_EXISTS (pipe2          "unistd.h"   HAVE_PIPE2)

CHECK_INCLUDE_FILE ("bsd/stdlib.h"  HAVE_BSD_STDLIB_H)
CHECK_INCLUDE_FILE ("execinfo.h"    HAVE_EXECINFO_H)
CHECK_INCLUDE_FILE ("stdatomic.h"   HAVE_STDATOMIC_H)
CHECK_INCLUDE_FILE ("stdnoreturn.h" HAVE_STDNORETURN_H)
CHECK_INCLUDE_FILE ("sys/auxv.h"    HAVE_SYS_AUXV_H)
CHECK_INCLUDE_FILE ("threads.h"     HAVE_THREADS_H)

if (NOT TALLOC_FOUND AND NOT HAVE_ARC4RANDOM)
    pkg_check_modules(LIBBSD libbsd)
    if (NOT LIBBSD_FOUND)
        message(FATAL_ERROR "libbsd is required")
    endif()
endif()

###############################################################################
# General flags

string(JOIN " " CMAKE_C_FLAGS ${LIBCLANG_CFLAGS} ${CMAKE_C_FLAGS})

string (JOIN " " WARNS
        -Wall -Wextra
        -Werror=format-extra-args
        -Werror=implicit
        -Werror=implicit-function-declaration
        -Werror=incompatible-pointer-types
        -Werror=incompatible-pointer-types
        -Werror=int-conversion
        -Werror=pointer-to-int-cast
)

if (NOT BUILD_DIST) 
    set (MARCH_SETTING "-march=native")
endif()

if (SANITIZE)
    set (SANIT "-fsanitize-address-use-after-scope -fsanitize=undefined")
    string(CONCAT SANITIZER_LOG_PLACE $ENV{HOME} "/sanit/asan.log")
    if ("${SANITIZE}" STREQUAL "thread")
        string (CONCAT SANIT "${SANIT} -fsanitize=thread")
    else ()
        string (CONCAT SANIT "${SANIT} -fsanitize=address")
    endif()
endif()

message(WARNING "Sanitize set to \"${SANIT}\" -- \"${SANITIZE}\"")

string (CONCAT BASE "${WARNS} ${MARCH_SETTING} ${SANIT}"
                    " -pipe -std=gnu17 -fdiagnostics-color=always")

################################################################################
# Compiler specific flags. Currently these override $CFLAGS.

string(FIND ${CMAKE_C_COMPILER} "clang" CLANG_STR_SEARCH)
if (NOT CLANG_STR_SEARCH EQUAL -1)
    set (CC_IS_CLANG 1)
endif()

set (CFLAGS_DEBUG_COMMON          "-Og -g3 -Wextra -Wpedantic -Wformat")
set (CFLAGS_RELWITHDEBINFO_COMMON "-O2 -g  -Wextra -ftree-vectorize -Wextra")
set (CFLAGS_RELEASE_COMMON        "-O3 -g0 -DNDEBUG -ffunction-sections")

if (SANITIZE)
    string (JOIN " " CFLAGS_DEBUG_COMMON ${CFLAGS_DEBUG_COMMON}
            -fno-omit-frame-pointer -fno-optimize-sibling-calls)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()

if (CC_IS_CLANG)
    string (JOIN " " WARNS ${WARNS}
         -Wno-gnu -Wno-gnu-zero-variadic-macro-arguments 
         -Wno-gnu-statement-expression -Werror=return-type
         -Wno-sign-compare
    )
    string (CONCAT BASE "${BASE} ${WARNS}")
    set (LTO_STR "-flto=full")

    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
        " -fuse-ld=lld -flto=full")

else() # GCC

    string (JOIN " " WARNS ${WARNS}
         -Wsuggest-attribute=pure -Wsuggest-attribute=cold -Wsuggest-attribute=malloc
         -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-attribute=format
         -Wattributes -Wold-style-definition -Wold-style-declaration 
    )
    string(JOIN " " BASE ${BASE} ${WARNS} -Werror=designated-init)
    set (LTO_STR "-flto")

    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto -fwhole-program")
endif()

#####################################################################################################
# Misc

string(JOIN " " CMAKE_C_FLAGS_DEBUG
                ${CMAKE_C_FLAGS_DEBUG} ${CFLAGS_DEBUG_COMMON} ${BASE})
string(JOIN " " CMAKE_C_FLAGS_RELWITHDEBINFO 
                ${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CFLAGS_RELWITHDEBINFO_COMMON} ${BASE})
string(JOIN " " CMAKE_C_FLAGS_RELEASE
                ${CMAKE_C_FLAGS_RELEASE} ${CFLAGS_RELEASE_COMMON} ${BASE} ${LTO_STR})
string(JOIN " " CMAKE_C_FLAGS_MINSIZEREL
                ${CMAKE_C_FLAGS_MINSIZEREL} ${CFLAGS_RELEASE_COMMON} ${BASE} -Os -s)

if (MINGW)
    FIX_WINDOWS_PATHS(CMAKE_C_FLAGS)
endif()

configure_file(cmake-config.h.in config.h)
add_subdirectory(src)

# vim: tw=0
