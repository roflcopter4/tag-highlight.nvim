cmake_minimum_required (VERSION 3.14.0)
project (Tag-Highlight_Nvim C CXX)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)
include (GNUInstallDirs)


###############################################################################
# Options

set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/..")
get_filename_component(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)

set (REQUIRED_LZMA_VERSION 5.3.0)
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

if (NOT CMAKE_C_STANDARD)
    set (CMAKE_C_STANDARD 17)
endif ()
if (NOT CMAKE_CXX_STANDARD)
    set (CMAKE_CXX_STANDARD 20)
endif ()

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

#find_package (ZLIB REQUIRED)
find_package (Threads REQUIRED)
find_package (LibClang REQUIRED)
find_package (LibUV REQUIRED)
find_package (Libevent REQUIRED)
#find_package (LibLZMA)

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
CHECK_SYMBOL_EXISTS (localtime_r    "time.h"     HAVE_LOCALTIME_R)
CHECK_SYMBOL_EXISTS (localtime_s    "time.h"     HAVE_LOCALTIME_S)

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

set (WARNS
        -Wall -Wextra
        -Werror=format-extra-args
        -Werror=implicit
        -Werror=implicit-function-declaration
        -Werror=incompatible-pointer-types
        -Werror=incompatible-pointer-types
        -Werror=int-conversion
        -Werror=pointer-to-int-cast
        -Werror=invalid-pch
)

if (NOT BUILD_DIST) 
    set (MARCH_SETTING "-mtune=native -march=native")
endif()

# set (SANIT -fsanitize=memory)
# if (SANITIZE)
#     set (SANIT -fsanitize-address-use-after-scope -fsanitize=undefined -fsanitize=bounds -fsanitize=bool)
#     if ("${SANITIZE}" STREQUAL "thread")
#         set (SANIT ${SANIT} -fsanitize=thread)
#     elseif ("${SANITIZE}" STREQUAL "memory")
#         set (SANIT -fsanitize=undefined -fsanitize=memory)
#     else ()
#         set (SANIT ${SANIT} -fsanitize=address)
#     endif()
# endif()

message(WARNING "Sanitize set to \"${SANIT}\" -- \"${SANITIZE}\"")

set (BASE ${WARNS} ${MARCH_SETTING} ${SANIT}
          -pipe -fdiagnostics-color=always -m64 -mprefer-vector-width=512)

set (CFLAGS_DEBUG_COMMON          -Og -g3 -Wextra -Wpedantic -Wformat -D_FORTIFY_SOURCE=2 -fstack-protector)
set (CFLAGS_RELWITHDEBINFO_COMMON -Og -g3 -D_FORTIFY_SOURCE=2 -Wextra -ftree-vectorize -Wextra)
set (CFLAGS_RELEASE_COMMON        -Ofast -ftree-vectorize -g -DNDEBUG -U_FORTIFY_SOURCE)

################################################################################
# Compiler specific flags. Currently these override $CFLAGS.

if (NOT MSVC)
    if (SANITIZE)
        set (CFLAGS_DEBUG_COMMON ${CFLAGS_DEBUG_COMMON}
            -fno-omit-frame-pointer -fno-optimize-sibling-calls)
    endif()
else()
    message(WARNING "Can't possibly sanitize MSVC. Try nuking from orbit")
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
        message(WARNING "Clang")
        set (WARNS
             -Werror=return-type 
             -Wno-gnu-zero-variadic-macro-arguments
             -Wno-gnu-statement-expression -Wno-fixed-enum-extension
             -Wno-sign-compare -Wno-dollar-in-identifier-extension -Wno-unused-parameter
        )
        set (BASE ${BASE} ${WARNS}
            -g3 -fintegrated-as -fintegrated-cc1 -fno-legacy-pass-manager
        )
        set (LTO_STR -flto=full)

        string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
                # -fuse-ld=gold
                -fuse-ld=lld
                ${LTO_STR}
        )
        if (NOT MINGW AND NOT MSYS)
            string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
                -stdlib=libstdc++
                -rtlib=compiler-rt 
                --unwindlib=libgcc
                #-rtlib=libgcc
                #--unwindlib=libgcc
                -Wl,--as-needed
                -Wl,--lto-O3
                -Wl,--lto-whole-program-visibility
            )
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
        else ()
            string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
                -stdlib=libstdc++
                #-rtlib=compiler-rt --unwindlib=libunwind
                -rtlib=libgcc --unwindlib=libgcc
            )
            set (BASE ${BASE} -U_FORTIFY_SOURCE -fno-stack-protector)
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
        endif ()


        set (__EXTRA_C_CXX_RELEASE_FLAGS -fvectorize -fslp-vectorize -mprefer-vector-width=512)

        find_program(LLVM_AR "llvm-ar" REQUIRED)
        find_program(LLVM_NM "llvm-nm" REQUIRED)
        find_program(LLVM_RANLIB "llvm-ranlib" REQUIRED)
        set (CMAKE_AR "${LLVM_AR}")
        set (CMAKE_NM "${LLVM_NM}")
        set (CMAKE_RANLIB "${LLVM_RANLIB}")

    endif()

elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")

    set (WARNS
        -Wsuggest-attribute=pure -Wsuggest-attribute=cold
        -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-attribute=format
        -Wattributes
    )
    set (BASE ${BASE} ${WARNS} -Werror=designated-init 
        -fvect-cost-model=unlimited
        -masm=intel -fverbose-asm
        -g3
    )
    if (NOT WIN32)
        set (BASE ${BASE} -gdwarf-5)
    endif()
    set (LTO_STR -flto -fuse-linker-plugin -fno-fat-lto-objects)

    string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
        -fuse-ld=bfd
        ${LTO_STR}
        -Wl,--as-needed
    )

    find_program(GCC_AR "gcc-ar" REQUIRED)
    find_program(GCC_NM "gcc-nm" REQUIRED)
    find_program(GCC_RANLIB "gcc-ranlib" REQUIRED)
    set (CMAKE_AR "${GCC_AR}")
    set (CMAKE_NM "${GCC_NM}")
    set (CMAKE_RANLIB "${GCC_RANLIB}")

elseif (MSVC)

   # Allow use of deprecated function names in MSVC (read/write)
   add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)

endif()

#####################################################################################################
# Misc

if (MSVC)

else()
    list(APPEND CMAKE_C_FLAGS_DEBUG ${CFLAGS_DEBUG_COMMON} ${BASE} ${LTO_STR} ${CMAKE_C_FLAGS})
    list(APPEND CMAKE_C_FLAGS_MINSIZEREL ${CFLAGS_RELEASE_COMMON} ${BASE} -Os -s ${LTO_STR} ${CMAKE_C_FLAGS})
    list(APPEND CMAKE_C_FLAGS_RELEASE
        ${CMAKE_C_FLAGS_RELEASE} ${CFLAGS_RELEASE_COMMON}
        ${BASE} ${LTO_STR} ${CMAKE_C_FLAGS} ${__EXTRA_C_CXX_RELEASE_FLAGS})
    list(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO
        ${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CFLAGS_RELWITHDEBINFO_COMMON}
        ${BASE} ${LTO_STR} ${CMAKE_C_FLAGS} ${__EXTRA_C_CXX_RELEASE_FLAGS})

    string(JOIN " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" "-std=gnu2x")
    string(JOIN " " CMAKE_CXX_FLAGS "-std=gnu++${CMAKE_CXX_STANDARD}" "${CMAKE_CXX_FLAGS}")

    set(CMAKE_CXX_FLAGS_DEBUG          ${CMAKE_C_FLAGS_DEBUG}          ${CMAKE_CXX_FLAGS})
    set(CMAKE_CXX_FLAGS_RELEASE        ${CMAKE_C_FLAGS_RELEASE}        ${CMAKE_CXX_FLAGS})
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_FLAGS})
    set(CMAKE_CXX_FLAGS_MINSIZEREL     ${CMAKE_C_FLAGS_MINSIZEREL}     ${CMAKE_CXX_FLAGS})

    string(TOUPPER ${CMAKE_BUILD_TYPE} _upper_CMAKE_BUILD_TYPE)
    set (ALL_THE_C_FLAGS   ${CMAKE_C_FLAGS_${_upper_CMAKE_BUILD_TYPE}})
    set (ALL_THE_CXX_FLAGS ${CMAKE_CXX_FLAGS_${_upper_CMAKE_BUILD_TYPE}})

    list(JOIN CMAKE_C_FLAGS_DEBUG            " " CMAKE_C_FLAGS_DEBUG            )
    list(JOIN CMAKE_C_FLAGS_RELEASE          " " CMAKE_C_FLAGS_RELEASE          )
    list(JOIN CMAKE_C_FLAGS_RELWITHDEBINFO   " " CMAKE_C_FLAGS_RELWITHDEBINFO   )
    list(JOIN CMAKE_C_FLAGS_MINSIZEREL       " " CMAKE_C_FLAGS_MINSIZEREL       )
    list(JOIN CMAKE_CXX_FLAGS_DEBUG          " " CMAKE_CXX_FLAGS_DEBUG          )
    list(JOIN CMAKE_CXX_FLAGS_RELEASE        " " CMAKE_CXX_FLAGS_RELEASE        )
    list(JOIN CMAKE_CXX_FLAGS_RELWITHDEBINFO " " CMAKE_CXX_FLAGS_RELWITHDEBINFO )
    list(JOIN CMAKE_CXX_FLAGS_MINSIZEREL     " " CMAKE_CXX_FLAGS_MINSIZEREL     )
endif()

if (MINGW)
    FIX_WINDOWS_PATHS(CMAKE_C_FLAGS)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-allow-multiple-definition")
endif()

configure_file(cmake-config.h.in config.h)
add_subdirectory(src)

# vim: tw=0
