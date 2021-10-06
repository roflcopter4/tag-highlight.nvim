cmake_minimum_required (VERSION 3.14.0)
project (Tag-Highlight_Nvim C)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)
include (GNUInstallDirs)


###############################################################################
# Options

set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/..")
get_filename_component(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)

set (REQUIRED_LZMA_VERSION 5.3.1)
option(SANITIZE "Enable sanitizers" OFF)
option(USE_JEMALLOC "Use jemalloc" OFF)
option(MINGW_PREFIX "Absolute path (windows style) to your Mingw prefix (eg. C:\\msys64\\mingw64)")

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

macro(FIX_WINDOWS_PATHS _pathvar)
    string(REPLACE "\\" "/" ${_pathvar} "${${_pathvar}}")
endmacro()

if (NOT CMAKE_BUILD_TYPE)
    message("Setting build type to \"Release\"")
    set (CMAKE_BUILD_TYPE "Release")
endif()

if ((CMAKE_BUILD_TYPE STREQUAL "Debug") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    set (DEBUG 1)
endif()

# Only FreeBSD and friends work right now.
if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD|DragonFly")
    set (IS_BSD 1)
elseif (CMAKE_SYSTEM_NAME MATCHES "OpenBSD|NetBSD")
    message(FATAL_ERROR "Sorry friend, your BSD isn't supported. No hard feelings, I'm just lazy.")
endif()

if (MSVC OR "${CMAKE_C_SIMULATE_ID}" STREQUAL "MSVC")
    set (MSVC_LIKE YES)
endif()
if (MSVC AND NOT "${CMAKE_C_SIMULATE_ID}" STREQUAL "MSVC")
    set (TRUE_MSVC YES)
endif()

###############################################################################
# Includes and defines

if (MSVC)
    set (CMAKE_REQUIRED_DEFINITIONS -DHAVE_CONFIG_H)
    add_definitions(-DHAVE_CONFIG_H)
else()
    if (MINGW)
        add_definitions(-D__MINGW__=1 -D__USE_MINGW_ANSI_STDIO=1)
    else()
        string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
                "-L/usr/local/lib" "-L/usr/local/lib64" "-rdynamic")
    endif()
    set (CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} -D__USE_ISOC99
         -D__USE_ISOC11 -D__USE_ISOCXX11 -D_GNU_SOURCE -DHAVE_CONFIG_H)
    add_definitions(${CMAKE_REQUIRED_DEFINITIONS})
endif()

add_definitions(-DBSTR_USE_P99 -DBSTR_USE_TALLOC)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib/bstring")

if (UNIX)
    # Why some compilers don't include this by default is something I think I'll never understand.
    include_directories("/usr/local/include")
endif()

include("${PROJECT_SOURCE_DIR}/cmake/get_compiler_include_paths.cmake")
GET_COMPILER_PATHS(COMPILER_INCLUDE_DIRECTORIES)

if (MSVC OR "${CMAKE_C_SIMULATE_ID}" STREQUAL "MSVC")
    if (NOT MINGW_PREFIX)
        if (DEFINED ENV{MINGW_PREFIX})
            set (${MINGW_PREFIX} $ENV{MINGW_PREFIX})
        else()
            message(FATAL_ERROR "You must specify your MinGW prefix")
        endif ()
    endif()
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    #include_directories("${MINGW_PREFIX}/include"
    #                    "${MINGW_PREFIX}/x86_64-w64-mingw32/include")
endif()


###############################################################################
# Libraries

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package (ZLIB REQUIRED)
find_package (Threads REQUIRED)
find_package (LibClang REQUIRED)
find_package (LibLZMA)
# find_package (sodium)

if (NOT ZLIB_FOUND)
    pkg_check_modules(ZLIB zlib REQUIRED)
    set (ZLIB_LIBRARIES ${ZLIB_LINK_LIBRARIES})
endif()

string(JOIN " " CMAKE_C_FLAGS ${LIBCLANG_CFLAGS} ${ZLIB_CFLAGS} ${CMAKE_C_FLAGS})
string(JOIN " " CMAKE_EXE_LINKER_FLAGS ${LIBCLANG_LDFLAGS} ${ZLIB_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS})

if (LIBLZMA_FOUND)
    if (${LIBLZMA_VERSION_STRING} STRLESS ${REQUIRED_LZMA_VERSION})
        message(WARNING "\
liblzma version ${REQUIRED_LZMA_VERSION} or greater is required for XZ support. \
XZ support will be disabled for this build.")
    else()
        include_directories(${LIBLZMA_INCLUDE_DIRS})
        string(JOIN " " CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
        set (LZMA_SUPPORT 1)
    endif()
endif()

# find_package (Talloc)
if (NOT TALLOC_FOUND)
    include_directories("${PROJECT_SOURCE_DIR}/src/contrib/talloc")
endif()
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

if (USE_JEMALLOC)
    find_package (Jemalloc REQUIRED)
    include_directories (${JEMALLOC_INCLUDE_DIRS})
    string(JOIN " " CMAKE_EXE_LINKER_FLAGS ${JEMALLOC_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS})
endif()

if (sodium_FOUND)
    set (USE_LIBSODIUM 1)
    add_definitions(-DUSE_LIBSODIUM)
endif()


###############################################################################
# Symbol and header checks

CHECK_INCLUDE_FILE ("time.h"     HAVE_TIME_H)
CHECK_INCLUDE_FILE ("sys/time.h" HAVE_SYS_TIME_H)

CHECK_SYMBOL_EXISTS (err            "err.h"      HAVE_ERR)
CHECK_SYMBOL_EXISTS (posix_spawnp   "spawn.h"    HAVE_POSIX_SPAWNP)
CHECK_SYMBOL_EXISTS (aligned_alloc  "stdlib.h"   HAVE_ALIGNED_ALLOC)
CHECK_SYMBOL_EXISTS (arc4random     "stdlib.h"   HAVE_ARC4RANDOM)
CHECK_SYMBOL_EXISTS (mkostemps      "stdlib.h"   HAVE_MKOSTEMPS)
CHECK_SYMBOL_EXISTS (reallocarray   "stdlib.h"   HAVE_REALLOCARRAY)
CHECK_SYMBOL_EXISTS (strtonum       "stdlib.h"   HAVE_STRTONUM)
CHECK_SYMBOL_EXISTS (asprintf       "stdio.h"    HAVE_ASPRINTF)
CHECK_SYMBOL_EXISTS (dprintf        "stdio.h"    HAVE_DPRINTF)
CHECK_SYMBOL_EXISTS (open_memstream "stdio.h"    HAVE_OPEN_MEMSTREAM)
CHECK_SYMBOL_EXISTS (vasprintf      "stdio.h"    HAVE_VASPRINTF)
CHECK_SYMBOL_EXISTS (basename       "string.h"   HAVE_BASENAME)
CHECK_SYMBOL_EXISTS (memrchr        "string.h"   HAVE_MEMRCHR)
CHECK_SYMBOL_EXISTS (strchrnul      "string.h"   HAVE_STRCHRNUL)
CHECK_SYMBOL_EXISTS (strdupa        "string.h"   HAVE_STRDUPA)
CHECK_SYMBOL_EXISTS (stricmp        "string.h"   HAVE_STRICMP)
CHECK_SYMBOL_EXISTS (strlcat        "string.h"   HAVE_STRLCAT)
CHECK_SYMBOL_EXISTS (strlcpy        "string.h"   HAVE_STRLCPY)
CHECK_SYMBOL_EXISTS (strsep         "string.h"   HAVE_STRSEP)
CHECK_SYMBOL_EXISTS (strcasecmp     "strings.h"  HAVE_STRCASECMP)
CHECK_SYMBOL_EXISTS (fork           "unistd.h"   HAVE_FORK)
CHECK_SYMBOL_EXISTS (pause          "unistd.h"   HAVE_PAUSE)
CHECK_SYMBOL_EXISTS (pipe2          "unistd.h"   HAVE_PIPE2)
CHECK_SYMBOL_EXISTS (lrand48        "stdlib.h"   HAVE_LRAND48)

CHECK_SYMBOL_EXISTS (getauxval "sys/getauxv.h" HAVE_GETAUXVAL)
CHECK_SYMBOL_EXISTS (program_invocation_name "errno.h" HAVE_PROGRAM_INVOCATION_NAME)
CHECK_SYMBOL_EXISTS (program_invocation_short_name "errno.h" HAVE_PROGRAM_INVOCATION_SHORT_NAME)

if (HAVE_TIME_H)
    CHECK_SYMBOL_EXISTS (clock_gettime  "time.h"     HAVE_CLOCK_GETTIME)
    CHECK_SYMBOL_EXISTS (gettimeofday   "time.h"     HAVE_GETTIMEOFDAY)
    CHECK_SYMBOL_EXISTS (nanosleep      "time.h"     HAVE_NANOSLEEP)
elseif (HAVE_SYS_TIME_H)
    CHECK_SYMBOL_EXISTS (clock_gettime  "sys/time.h" HAVE_CLOCK_GETTIME)
    CHECK_SYMBOL_EXISTS (gettimeofday   "sys/time.h" HAVE_GETTIMEOFDAY)
    CHECK_SYMBOL_EXISTS (nanosleep      "sys/time.h" HAVE_NANOSLEEP)
else()
    CHECK_SYMBOL_EXISTS (clock_gettime  "pthread.h"  HAVE_CLOCK_GETTIME)
    CHECK_SYMBOL_EXISTS (gettimeofday   "pthread.h"  HAVE_GETTIMEOFDAY)
    CHECK_SYMBOL_EXISTS (nanosleep      "pthread.h"  HAVE_NANOSLEEP)
endif()

# CHECK_INCLUDE_FILE ("bsd/stdlib.h"  HAVE_BSD_STDLIB_H)
CHECK_INCLUDE_FILE ("execinfo.h"    HAVE_EXECINFO_H)
CHECK_INCLUDE_FILE ("stdatomic.h"   HAVE_STDATOMIC_H)
CHECK_INCLUDE_FILE ("stdnoreturn.h" HAVE_STDNORETURN_H)
CHECK_INCLUDE_FILE ("sys/auxv.h"    HAVE_SYS_AUXV_H)
CHECK_INCLUDE_FILE ("threads.h"     HAVE_THREADS_H)
CHECK_INCLUDE_FILE ("unistd.h"      HAVE_UNISTD_H)

# if (NOT TALLOC_FOUND AND NOT HAVE_ARC4RANDOM)
#      find_package(LibBSD)
# 
#      if (LibBSD_FOUND)
#          set (HAVE_ARC4RANDOM 1)
#          set (HAVE_BSD_BSD_H 1)
#          set (HAVE_LIBBSD 1)
#      endif()
# endif()

if (NOT USE_JEMALLOC AND NOT HAVE_ALIGNED_ALLOC)
    find_package (Jemalloc REQUIRED)
    include_directories (${JEMALLOC_INCLUDE_DIRS})
    string(JOIN " " CMAKE_EXE_LINKER_FLAGS ${JEMALLOC_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS})
endif()


###############################################################################
# General flags

string (JOIN " " WARNS
        -Wall -Wextra
        -Werror=format-extra-args
        -Werror=implicit
        -Werror=implicit-function-declaration
        -Werror=incompatible-pointer-types
        -Werror=incompatible-pointer-types
        -Werror=int-conversion
        -Werror=pointer-to-int-cast
        # -fpie -fPIC
)

if (NOT BUILD_DIST) 
    set (MARCH_SETTING "-mtune=native -march=native")
endif()

if (SANITIZE AND NOT MSVC)
    # set (SANIT "-fsanitize=undefined")
    # if ("${SANITIZE}" STREQUAL "thread")
    #     string (CONCAT SANIT "${SANIT} -fsanitize=thread")
    # elseif ("${SANITIZE}" STREQUAL "memory")
    #     string (CONCAT SANIT "${SANIT} -fsanitize=memory")
    # else ()
    #     string (CONCAT SANIT "${SANIT} -fsanitize=address -fsanitize-address-use-after-scope")
    # endif()
    set (SANIT "${SANITIZE}")
endif()

message("Sanitize set to \"${SANIT}\" -- \"${SANITIZE}\"")


################################################################################
# Compiler specific flags. Currently these override $CFLAGS.

if (MSVC)
    if (SANITIZE)
        message(WARNING "Can't possibly sanitize MSVC. Try nuking from orbit")
    endif()
else()
    if (SANITIZE)
        set (CFLAGS_DEBUG_COMMON ${CFLAGS_DEBUG_COMMON}
            -fno-omit-frame-pointer -fno-optimize-sibling-calls)
    endif()
    string (CONCAT BASE "${WARNS} ${MARCH_SETTING} ${SANIT}" " -mprefer-vector-width=512"
                        " -pipe -std=gnu11 -fdiagnostics-color=always")

    set (CFLAGS_DEBUG_COMMON          "-O0 -g3 -Wextra -Wpedantic -Wformat -D_FORTIFY_SOURCE=2 -fstack-protector")
    set (CFLAGS_RELWITHDEBINFO_COMMON "-O2 -g3 -D_FORTIFY_SOURCE=2 -Wextra -ftree-vectorize -Wextra")
    set (CFLAGS_RELEASE_COMMON        "-Ofast -ftree-vectorize -g0 -DNDEBUG -U_FORTIFY_SOURCE -fno-strict-aliasing")
endif()

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    if ("${CMAKE_C_SIMULATE_ID}" STREQUAL "MSVC")
        string (CONCAT BASE "/arch:AVX512 /clang:-march=native /std:gnu17 /W4 /Zc:preprocessor -fdiagnostics-color=always")
        
        set (CFLAGS_RELEASE_COMMON "/O2 /Ob2 /Oi /Ot /clang:-O3 /DNDEBUG /GL -flto")
        set (CFLAGS_RELWITHDEBINFO_COMMON "${CFLAGS_RELEASE_COMMON} -g")
        set (CFLAGS_DEBUG_COMMON   "/clang:-Og -g3 /ZI /RTC1 -Wextra -Wpedantic -Wformat -D_FORTIFY_SOURCE=2 -fstack-protector")
        if (${CMAKE_BUILD_TYPE} STREQUAL "Release" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
            string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
        else()
            string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG:FULL")
        endif ()
        
        string (JOIN " " BASE ${BASE} ${WARNS})
        
        find_program(LLVM_CPP "clang-cpp" REQUIRED)
        set (CMAKE_CPP "${LLVM_CPP}")

    else()

        # string (JOIN " " WARNS ${WARNS}
        #     -Wno-gnu -Wno-gnu-zero-variadic-macro-arguments 
        #     -Wno-gnu-statement-expression -Werror=return-type
        #     -Wno-sign-compare -Wno-dollar-in-identifier-extension
        # )
        string (CONCAT BASE "${BASE} ${WARNS} ${SANIT}"
        )
        
        if (CMAKE_BUILD_TYPE STREQUAL "Release")

            string(JOIN " " BASE ${BASE} ${WARNS}
                -g3 -fvectorize -fslp-vectorize -fintegrated-as -fintegrated-cc1 -fno-legacy-pass-manager
                # -fprofile-use=/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag-highlight
                # -fprofile-instr-generate -fdebug-info-for-profiling
            )

        endif ()
        set (LTO_STR "-flto=full -fwhole-program-vtables")

        string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
            " -mprefer-vector-width=512 -rtlib=compiler-rt --unwindlib=libgcc"
            " -fuse-ld=lld"
            " -rdynamic"
            " ${LTO_STR} -Wl,--lto-O3 -Wl,--lto-whole-program-visibility -Wl,-O3"
        )
    endif()
    
    find_program(LLVM_AR "llvm-ar" REQUIRED)
    find_program(LLVM_NM "llvm-nm" REQUIRED)
    find_program(LLVM_RANLIB "llvm-ranlib" REQUIRED)
    set (CMAKE_AR "${LLVM_AR}")
    set (CMAKE_NM "${LLVM_NM}")
    set (CMAKE_RANLIB "${LLVM_RANLIB}")

elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")

    string (JOIN " " WARNS ${WARNS}
         -Wsuggest-attribute=pure
         -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-attribute=format
         -Wattributes -Wold-style-definition -Wold-style-declaration 
    )
    string(JOIN " " BASE ${BASE} ${WARNS} -Werror=designated-init 
        -mvect-cost-model=unlimited
        -masm=intel -fverbose-asm
        -g3 -gdwarf-5
    )
    set (LTO_STR "-flto -fuse-linker-plugin -fno-fat-lto-objects")

    find_program(GCC_AR "gcc-ar" REQUIRED)
    find_program(GCC_NM "gcc-nm" REQUIRED)
    find_program(GCC_RANLIB "gcc-ranlib" REQUIRED)
    set (CMAKE_AR "${GCC_AR}")
    set (CMAKE_NM "${GCC_NM}")
    set (CMAKE_RANLIB "${GCC_RANLIB}")
    
    string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LTO_STR}"
           " -rdynamic"
    )
endif()


#####################################################################################################
# Misc

if (TRUE_MSVC)
    message (FATAL_ERROR "Sorry, this won't work out.")
elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    string(JOIN " " CMAKE_C_FLAGS_DEBUG
                    ${CMAKE_C_FLAGS_DEBUG} ${CFLAGS_DEBUG_COMMON} ${BASE})
    string(JOIN " " CMAKE_C_FLAGS_RELWITHDEBINFO 
                    ${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CFLAGS_RELWITHDEBINFO_COMMON} ${BASE} ${LTO_STR})
    string(JOIN " " CMAKE_C_FLAGS_RELEASE
                    ${CMAKE_C_FLAGS_RELEASE} ${CFLAGS_RELEASE_COMMON} ${BASE} ${LTO_STR})
    string(JOIN " " CMAKE_C_FLAGS_MINSIZEREL
                    ${CMAKE_C_FLAGS_MINSIZEREL} ${CFLAGS_RELEASE_COMMON} ${BASE} -Os -s)
endif()

if (MINGW)
    FIX_WINDOWS_PATHS(CMAKE_C_FLAGS)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-allow-multiple-definition")
endif()

configure_file(cmake-config.h.in config.h)
add_subdirectory(src)

# vim: tw=0
