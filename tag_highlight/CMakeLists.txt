cmake_minimum_required (VERSION 3.14.0)
project (Tag_Highlight_Nvim C)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)

if (MSVC)
    message(FATAL_ERROR "MSVC is not and will probably never be supported. Use MinGW")
endif()

###############################################################################
# Options

set (REQUIRED_LZMA_VERSION 5.3.1)
option(BUILD_DIST "\
Include the required alpha (and therefore uncommon) liblzma version as a static\
library, and use bundled BSD extensions instead of libbsd even if it is available.")
option(STATIC_LIBS_ONLY "Build with only static libraries.")
option(SANITIZE "Enable sanitizers")
option(USE_JEMALLOC "Use jemalloc")

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

macro(FIX_WINDOWS_PATHS _pathvar)
    string(REPLACE "\\" "/" ${_pathvar} "${${_pathvar}}")
endmacro()

if (NOT CMAKE_BUILD_TYPE)
    message("Setting build type to \"Release\"")
    set (CMAKE_BUILD_TYPE "Release")
endif()

if ((CMAKE_BUILD_TYPE STREQUAL "Debug")
    OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    set (DEBUG 1)
endif()

###############################################################################
# Includes and defines

if (MINGW)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include _mingw.h")

    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
            " -Wl,--allow-multiple-definition")

    include_directories("D:/msys64/mingw64/x86_64-w64-mingw32/include")
    add_definitions(-D__MINGW__)
    add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
else()
    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
            "-L/usr/local/lib -rdynamic")
endif()

add_definitions(-D_GNU_SOURCE -DHAVE_TOPCONFIG_H -DBSTR_USE_P99)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib/bstring")

set (CMAKE_REQUIRED_DEFINITIONS
    -D__USE_ISOC11
    -D_GNU_SOURCE
    -DHAVE_TOPCONFIG_H
)

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

if (NOT MINGW)
    # find_package (LibEv)
    # if (LIBEV_FOUND)
    #     set (HAVE_LIBEV 1)
    # endif()
endif ()

if (USE_JEMALLOC)
    find_package (Jemalloc REQUIRED)
endif()


###############################################################################
# Symbol and header checks

CHECK_SYMBOL_EXISTS (mkostemps      "stdlib.h"   HAVE_MKOSTEMPS)
CHECK_SYMBOL_EXISTS (reallocarray   "stdlib.h"   HAVE_REALLOCARRAY)
CHECK_SYMBOL_EXISTS (asprintf       "stdio.h"    HAVE_ASPRINTF)
CHECK_SYMBOL_EXISTS (open_memstream "stdio.h"    HAVE_OPEN_MEMSTREAM)
CHECK_SYMBOL_EXISTS (vasprintf      "stdio.h"    HAVE_VASPRINTF)
CHECK_SYMBOL_EXISTS (memrchr        "string.h"   HAVE_MEMRCHR)
CHECK_SYMBOL_EXISTS (strchrnul      "string.h"   HAVE_STRCHRNUL)
CHECK_SYMBOL_EXISTS (strdupa        "string.h"   HAVE_STRDUPA)
CHECK_SYMBOL_EXISTS (strlcat        "string.h"   HAVE_STRLCAT)
CHECK_SYMBOL_EXISTS (strlcpy        "string.h"   HAVE_STRLCPY)
CHECK_SYMBOL_EXISTS (strsep         "string.h"   HAVE_STRSEP)
CHECK_SYMBOL_EXISTS (strtonum       "string.h"   HAVE_STRTONUM)
CHECK_SYMBOL_EXISTS (stricmp        "string.h"   HAVE_STRICMP)
CHECK_SYMBOL_EXISTS (strcasecmp     "strings.h"  HAVE_STRCASECMP)
CHECK_SYMBOL_EXISTS (fork           "unistd.h"   HAVE_FORK)
CHECK_SYMBOL_EXISTS (pause          "unistd.h"   HAVE_PAUSE)
CHECK_SYMBOL_EXISTS (pipe2          "unistd.h"   HAVE_PIPE2)
CHECK_SYMBOL_EXISTS (err            "err.h"      HAVE_ERR)
CHECK_SYMBOL_EXISTS (clock_gettime  "time.h"     HAVE_CLOCK_GETTIME)
CHECK_SYMBOL_EXISTS (gettimeofday   "sys/time.h" HAVE_GETTIMEOFDAY)
CHECK_SYMBOL_EXISTS (posix_spawnp   "spawn.h"    HAVE_POSIX_SPAWNP)

CHECK_INCLUDE_FILE("execinfo.h"    HAVE_EXECINFO_H)
CHECK_INCLUDE_FILE("stdatomic.h"   HAVE_STDATOMIC_H)
CHECK_INCLUDE_FILE("stdnoreturn.h" HAVE_STDNORETURN_H)
CHECK_INCLUDE_FILE("threads.h"     HAVE_THREADS_H)

###############################################################################
# General flags

string(CONCAT CMAKE_C_FLAGS "${LIBCLANG_CFLAGS}" " ${CMAKE_C_FLAGS}")

string (CONCAT WARNS
    "-Wall -Wextra -Wpedantic"
    " -Werror=implicit"
    " -Werror=incompatible-pointer-types"
    " -Werror=pointer-to-int-cast"
    " -Werror=int-conversion"
    " -Werror=implicit-function-declaration"
    " -Werror=incompatible-pointer-types"
    " -Werror=format-extra-args"
)

if (NOT BUILD_DIST) 
    set (MARCH_SETTING "-march=native")
endif()

if (SANITIZE)
    set (SANIT "-fsanitize-address-use-after-scope -fsanitize=undefined")
    if ("${SANITIZE}" STREQUAL "thread")
        string (CONCAT SANIT "${SANIT} -fsanitize=thread")
    else ()
        string (CONCAT SANIT "${SANIT} -fsanitize=address")
    endif()
endif()

message(WARNING "Sanitize set to \"${SANIT}\" -- \"${SANITIZE}\"")

string (CONCAT BASE "${WARNS} ${MARCH_SETTING} ${SANIT}"
                    " -std=gnu17 -fdiagnostics-color=always")

################################################################################
# Compiler specific flags. Currently these override $CFLAGS.

string(FIND ${CMAKE_C_COMPILER} "clang" CLANG_STR_SEARCH)

if (NOT CLANG_STR_SEARCH EQUAL -1)

    # set (BASE "${BASE} -fprofile-instr-generate")
    # set (BASE "${BASE} -fprofile-instr-use=${CMAKE_CURRENT_BINARY_DIR}/code.profdata")
    # string(CONCAT CMAKE_C_FLAGS_DEBUG "${BASE} ${SANIT} -O0 -g3 -Wextra -Wpedantic -Wformat" " -Wall" # " -Weverything"
    #       " -Wno-sign-conversion -Wno-documentation-unknown-command"
    #       " -Wno-cast-qual -Wno-shorten-64-to-32 -Wno-unused-macros -Wno-switch-enum"
    #       " -Wno-disabled-macro-expansion -Wno-covered-switch-default -Wno-padded -Werror=enum-conversion"
    #       " -Wno-vla -Wno-missing-variable-declarations -Wno-gnu" " -Wno-format-nonliteral"
    # )
    # "-Rpass='.*'"
    # "-Rpass-missed='.*'" 
    # "-Rpass='inline'"
    # "-Rpass-missed='inline'" 
    # "-mllvm -inline-threshold=10000000"
    # "-mllvm -inlinehint-threshold=30000000"
    # "-Rpass-analysis='.*'"
    # )

    set (BASE "${BASE} -Wno-gnu-zero-variadic-macro-arguments")

    set (CMAKE_C_FLAGS_DEBUG
            "${BASE} -Og -g -Wextra -Wpedantic -Wformat -Wno-gnu-zero-variadic-macro-arguments")
    set (CMAKE_C_FLAGS_RELWITHDEBINFO
            "${BASE} -Og -g")
    set (CMAKE_C_FLAGS_RELEASE
        "${CMAKE_C_FLAGS_RELEASE} ${BASE}")

    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
           " -rtlib=compiler-rt -fuse-ld=lld")

else() # GCC

    string(CONCAT BASE "${BASE}"
        " -Wattributes"
        " -Wsuggest-attribute=pure -Wsuggest-attribute=cold -Wsuggest-attribute=malloc"
        " -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-attribute=format"
        " -Wold-style-definition -Wold-style-declaration -Werror=designated-init")

    set (CMAKE_C_FLAGS_DEBUG
            "${BASE} -Og -g3 -Wextra -Wpedantic -Wformat -Wno-switch-unreachable")
    set (CMAKE_C_FLAGS_RELWITHDEBINFO
            "${BASE} -Wextra -Wpedantic -Wno-switch-unreachable -Og -g")
    set (CMAKE_C_FLAGS_RELEASE
            "${CMAKE_C_FLAGS_RELEASE} ${BASE} -fwhole-program")

    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
            " -Wno-free-nonheap")
endif()


#####################################################################################################
# Misc

string (CONCAT CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${BASE} -Og -g3")
string (CONCAT CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG -flto -ffunction-sections")

if (NOT DEBUG)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
endif()

if (MINGW)
    FIX_WINDOWS_PATHS(CMAKE_C_FLAGS)
endif()

configure_file(cmake-config.h.in topconfig.h)
add_subdirectory(src)

# vim: tw=0
