cmake_minimum_required (VERSION 3.20.0)
project (Tag-Highlight_Nvim VERSION 0.1.0 LANGUAGES C CXX)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)
include (GNUInstallDirs)


###############################################################################
# Options

option(SANITIZE "Enable sanitizers" OFF)
option(USE_JEMALLOC "Use jemalloc" OFF)
option(MINGW_PREFIX "Absolute path (windows style) to your Mingw prefix (eg. C:\\msys64\\mingw64)")
option(VCPKG_ROOT "Directory in which VCPKG resides (Windows only)" "")
option(WITH_AVX512 "" OFF)

###############################################################################
# JUNK

set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/..")
get_filename_component(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)

set (REQUIRED_LZMA_VERSION 5.3.0)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

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

if (NOT CMAKE_BUILD_TYPE)
    message("Setting build type to \"Release\"")
    set (CMAKE_BUILD_TYPE "Release")
endif()

if (MSVC)
    set (CMAKE_REQUIRED_DEFINITIONS -DHAVE_CONFIG_H)
else()
    set (CMAKE_REQUIRED_DEFINITIONS -D__USE_ISOC99 -D__USE_ISOC11
         -D__USE_ISOCXX11 -D_GNU_SOURCE -DHAVE_CONFIG_H)
    if (MINGW)
        list(APPEND CMAKE_REQUIRED_DEFINITIONS -D__MINGW__ -D__USE_MINGW_ANSI_STDIO=1)
    endif()
endif()
add_definitions(${CMAKE_REQUIRED_DEFINITIONS})

if (NOT CMAKE_BUILD_TYPE)
    message("Setting build type to \"Release\"")
    set (CMAKE_BUILD_TYPE "Release")
endif()

if ((CMAKE_BUILD_TYPE STREQUAL "Debug") OR
    (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    set (DEBUG 1)
endif()

macro(FIX_WINDOWS_PATHS _pathvar)
    string(REPLACE "\\" "/" ${_pathvar} "${${_pathvar}}")
endmacro()

###############################################################################
# Includes

add_definitions(-DBSTR_USE_P99 -DBSTR_USE_TALLOC)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib/bstring")

if (UNIX)
    # Why some compilers don't include this by default is something I 
    # think I'll never understand.
    include_directories("/usr/local/include")
endif()

include("${PROJECT_SOURCE_DIR}/cmake/get_compiler_include_paths.cmake")
GET_COMPILER_PATHS(COMPILER_INCLUDE_DIRECTORIES)

if (MSVC OR "${CMAKE_C_SIMULATE_ID}" STREQUAL "MSVC")
    if (NOT MINGW_PREFIX)
        #if (DEFINED ENV{MINGW_PREFIX})
        #    set (${MINGW_PREFIX} $ENV{MINGW_PREFIX})
        #else()
        #    #message(FATAL_ERROR "You must specify your MinGW prefix")
        #endif ()
    endif()
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    #include_directories("${MINGW_PREFIX}/include"
    #                    "${MINGW_PREFIX}/x86_64-w64-mingw32/include")
endif()
if (NOT MINGW)
    if (WIN32)
        message ("YUP")
        if (NOT VCPKG_ROOT)
            message(FATAL_ERROR "Must provide a definition for VCPKG_ROOT (ie the directory with vcpkg.exe")
        endif()
        message( "INSTALLED DIR IS \"${VCPKG_ROOT}\"" )
        list(APPEND CMAKE_MODULE_PATH "${VCPKG_ROOT}/scripts/buildsystems")
        
        if (DEBUG)
            set (_WIN32_VCPKG_LIB_PATH "${_VCPKG_INSTALLED_DIR}/x64-windows/debug/lib")
        else()
            set (_WIN32_VCPKG_LIB_PATH "${_VCPKG_INSTALLED_DIR}/x64-windows/lib")
            link_directories("${VCPKG_ROOT}/installed/x64-windows/lib")
        endif()

        include_directories("${VCPKG_ROOT}/installed/x64-windows/include")
        
        #set (VCPKG_ENABLED "YES")
		#set (VcpkgEnabled CACHE BOOL TRUE)
        include (vcpkg)
    else()
        # list (APPEND CMAKE_MODULE_PATH "/usr/lib/cmake" "/usr/share/cmake" "/usr/local/share/cmake" "/usr/local/lib/cmake")
        # list (APPEND CMAKE_PREFIX_PATH "/usr/lib/cmake" "/usr/share/cmake" "/usr/local/share/cmake" "/usr/local/lib/cmake")
    endif()
endif()

###############################################################################
# Libraries

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package (ZLIB)
find_package (Threads REQUIRED)
find_package (LibClang REQUIRED)
find_package (Libevent)
find_package (LibUV)

if (MSVC)
    find_package(pthreads REQUIRED)
    find_package(unofficial-libuv CONFIG REQUIRED)
else ()
    find_package (GLib REQUIRED)
    find_package (LibUV REQUIRED)
endif()

if (NOT LIBEVENT_FOUND)
    pkg_check_modules(LIBEVENT_BASE libevent REQUIRED)
    pkg_check_modules(LIBEVENT_CORE libevent_core REQUIRED)
    pkg_check_modules(LIBEVENT_EXTRA libevent_extra REQUIRED)
    pkg_check_modules(LIBEVENT_PTHREADS libevent_pthreads)
    set (LIBEVENT_LIBRARIES
        ${LIBEVENT_BASE_LDFLAGS} ${LIBEVENT_CORE_LDFLAGS}
        ${LIBEVENT_EXTRA_LDFLAGS} ${LIBEVENT_PTHREADS_LDFLAGS})
    include_directories(
        ${LIBEVENT_BASE_CFLAGS_I} ${LIBEVENT_CORE_CFLAGS_I}
        ${LIBEVENT_EXTRA_CFLAGS_I} ${LIBEVENT_PTHREADS_CFLAGS_I})
endif()

if (NOT ZLIB_FOUND)
    pkg_check_modules(ZLIB zlib REQUIRED)
    set (ZLIB_LIBRARIES ${ZLIB_LINK_LIBRARIES})
endif()


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
    string(JOIN " " CMAKE_EXE_LINKER_FLAGS 
           ${JEMALLOC_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS})
endif()

if (WIN32)
    foreach( v $ENV{PKG_CONFIG_PATH} )
        string(REPLACE "/" "\\" _temp_pkgconfig_path "${_temp_pkgconfig_path}")
        list(APPEND _temp_pkgconfig_path "${v}")
    endforeach()
    string(REPLACE ";" ":" _temp_pkgconfig_path "${_temp_pkgconfig_path}")
    set(ENV{PKG_CONFIG_PATH} "${_temp_pkgconfig_path}")
    unset(_temp_pkgconfig_path)
endif()

if (MSVC)
    execute_process(COMMAND pkgconf.exe --cflags glib-2.0
                    RESULT_VARIABLE _my_pkgconfig_retval
                    OUTPUT_VARIABLE GLib_PKG_CFLAGS
                    ERROR_VARIABLE _my_pkgconfig_error
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)

    if (${_my_pkgconfig_retval} GREATER 0)
        message(FATAL_ERROR "Failed to locate glib: ${_my_pkgconfig_error}")
    endif()
    execute_process(COMMAND pkgconf.exe --libs glib-2.0
                    OUTPUT_VARIABLE GLib_PKG_LIBS
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
else ()
    find_package (GLib REQUIRED)
endif()

# Add flags...
string(JOIN " " CMAKE_C_FLAGS
    ${LIBCLANG_CFLAGS} ${ZLIB_CFLAGS} ${CMAKE_C_FLAGS})
string(JOIN " " CMAKE_CXX_FLAGS
    ${LIBCLANG_CFLAGS} ${ZLIB_CFLAGS} ${CMAKE_CXX_FLAGS})
string(JOIN " " CMAKE_EXE_LINKER_FLAGS
    ${LIBCLANG_LDFLAGS} ${ZLIB_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS})

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

###############################################################################
# Symbol and header checks

CHECK_INCLUDE_FILE ("time.h"     HAVE_TIME_H)
CHECK_INCLUDE_FILE ("sys/time.h" HAVE_SYS_TIME_H)

CHECK_SYMBOL_EXISTS (aligned_alloc  "stdlib.h"      HAVE_ALIGNED_ALLOC)
CHECK_SYMBOL_EXISTS (arc4random     "stdlib.h"      HAVE_ARC4RANDOM)
CHECK_SYMBOL_EXISTS (asprintf       "stdio.h"       HAVE_ASPRINTF)
CHECK_SYMBOL_EXISTS (basename       "string.h"      HAVE_BASENAME)
CHECK_SYMBOL_EXISTS (dprintf        "stdio.h"       HAVE_DPRINTF)
CHECK_SYMBOL_EXISTS (err            "err.h"         HAVE_ERR)
CHECK_SYMBOL_EXISTS (fork           "unistd.h"      HAVE_FORK)
CHECK_SYMBOL_EXISTS (getauxval      "sys/getauxv.h" HAVE_GETAUXVAL)
CHECK_SYMBOL_EXISTS (localtime_r    "time.h"        HAVE_LOCALTIME_R)
CHECK_SYMBOL_EXISTS (localtime_s    "time.h"        HAVE_LOCALTIME_S)
CHECK_SYMBOL_EXISTS (memrchr        "string.h"      HAVE_MEMRCHR)
CHECK_SYMBOL_EXISTS (mkdtemp        "stdlib.h"      HAVE_MKDTEMP)
CHECK_SYMBOL_EXISTS (mkostemps      "stdlib.h"      HAVE_MKOSTEMPS)
CHECK_SYMBOL_EXISTS (mkstemp        "stdlib.h"      HAVE_MKSTEMP)
CHECK_SYMBOL_EXISTS (nanosleep      "time.h"        HAVE_NANOSLEEP)
CHECK_SYMBOL_EXISTS (open_memstream "stdio.h"       HAVE_OPEN_MEMSTREAM)
CHECK_SYMBOL_EXISTS (pause          "unistd.h"      HAVE_PAUSE)
CHECK_SYMBOL_EXISTS (pipe2          "unistd.h"      HAVE_PIPE2)
CHECK_SYMBOL_EXISTS (posix_spawnp   "spawn.h"       HAVE_POSIX_SPAWNP)
CHECK_SYMBOL_EXISTS (reallocarray   "stdlib.h"      HAVE_REALLOCARRAY)
CHECK_SYMBOL_EXISTS (socketpair     "sys/socket.h"  HAVE_SOCKETPAIR)
CHECK_SYMBOL_EXISTS (strcasecmp     "strings.h"     HAVE_STRCASECMP)
CHECK_SYMBOL_EXISTS (strchrnul      "string.h"      HAVE_STRCHRNUL)
CHECK_SYMBOL_EXISTS (strcpy_s       "string.h"      HAVE_STRCPY_S)
CHECK_SYMBOL_EXISTS (strdupa        "string.h"      HAVE_STRDUPA)
CHECK_SYMBOL_EXISTS (strerror_r     "string.h"      HAVE_STRERROR_R)
CHECK_SYMBOL_EXISTS (strerror_s     "string.h"      HAVE_STRERROR_S)
CHECK_SYMBOL_EXISTS (stricmp        "string.h"      HAVE_STRICMP)
CHECK_SYMBOL_EXISTS (strlcat        "string.h"      HAVE_STRLCAT)
CHECK_SYMBOL_EXISTS (strlcpy        "string.h"      HAVE_STRLCPY)
CHECK_SYMBOL_EXISTS (strsep         "string.h"      HAVE_STRSEP)
CHECK_SYMBOL_EXISTS (strtonum       "stdlib.h"      HAVE_STRTONUM)
CHECK_SYMBOL_EXISTS (timespec_get   "time.h"        HAVE_TIMESPEC_GET)
CHECK_SYMBOL_EXISTS (vasprintf      "stdio.h"       HAVE_VASPRINTF)

CHECK_SYMBOL_EXISTS (program_invocation_name       "errno.h" HAVE_PROGRAM_INVOCATION_NAME)
CHECK_SYMBOL_EXISTS (program_invocation_short_name "errno.h" HAVE_PROGRAM_INVOCATION_SHORT_NAME)

if (WIN32)
    CHECK_SYMBOL_EXISTS (nanosleep "pthread.h"  HAVE_NANOSLEEP)
else()
    CHECK_SYMBOL_EXISTS (nanosleep "time.h"     HAVE_NANOSLEEP)
endif()

CHECK_INCLUDE_FILE ("execinfo.h"    HAVE_EXECINFO_H)
CHECK_INCLUDE_FILE ("stdatomic.h"   HAVE_STDATOMIC_H)
CHECK_INCLUDE_FILE ("stdnoreturn.h" HAVE_STDNORETURN_H)
CHECK_INCLUDE_FILE ("sys/auxv.h"    HAVE_SYS_AUXV_H)
CHECK_INCLUDE_FILE ("threads.h"     HAVE_THREADS_H)
CHECK_INCLUDE_FILE ("unistd.h"      HAVE_UNISTD_H)

if (NOT USE_JEMALLOC AND NOT HAVE_ALIGNED_ALLOC)
    find_package (Jemalloc REQUIRED)
    include_directories (${JEMALLOC_INCLUDE_DIRS})
    string(JOIN " " CMAKE_EXE_LINKER_FLAGS 
           ${JEMALLOC_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS})
endif()
if (NOT HAVE_THREADS_H)
    if (WIN32)
        find_package (tinycthread)
    else()
        message(WARNING
            "C11 threads library is required. I'm going to assume you have "
            "tinycthreads installed because I'm too lazy to write anything "
            "to search for it. If you do not, this will not go well."
        )
    endif()
endif()

###############################################################################
# General flags

set (WARNS
        -Wall -Wextra
        -Werror=format-extra-args
        -Wimplicit-fallthrough
        -Werror=invalid-pch
)
set (C_ONLY_WARNS
        -Werror=implicit
        -Werror=implicit-function-declaration
        -Werror=incompatible-pointer-types
        -Werror=int-conversion
        -Werror=pointer-to-int-cast
)

if (NOT BUILD_DIST)
    set (MARCH_SETTING "-mtune=native -march=native")
endif()
if (WITH_AVX512)
    set (MARCH_SETTING "${MARCH_SETTING} -mprefer-vector-width=512")
endif()

if (SANITIZE)
    set (SANIT -fsanitize=undefined -fsanitize=bounds -fsanitize=bool)
    if ("${SANITIZE}" STREQUAL "thread")
        set (SANIT ${SANIT} -fsanitize=thread)
    elseif ("${SANITIZE}" STREQUAL "memory")
        set (SANIT -fsanitize=undefined -fsanitize=memory)
    else ()
        set (SANIT ${SANIT} -fsanitize=address -fsanitize-address-use-after-scope)
    endif()
endif()

set (BASE ${WARNS} ${MARCH_SETTING} ${SANIT}
          -pipe -fdiagnostics-color=always
)

set (CFLAGS_DEBUG_COMMON
     -Og -g -UNDEBUG -D_FORTIFY_SOURCE=2
     -Wextra -Wpedantic -Wformat -U_FORTIFY_SOURCE
)
set (CFLAGS_RELWITHDEBINFO_COMMON
     -O2 -g -D_FORTIFY_SOURCE=2
     -Wextra -ftree-vectorize -Wextra -U_FORTIFY_SOURCE
)
set (CFLAGS_RELEASE_COMMON
     -Ofast -ftree-vectorize -g3
     -DNDEBUG -U_FORTIFY_SOURCE
)


################################################################################
# Compiler specific flags. Currently these override $CFLAGS.

#-----------------------------------------------------------------------------------------
#-- CLANG --
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    #-------------------------------------------------------------------------------------
    #-- CLANG PRETENDING TO BE MSVC --
    if ("${CMAKE_C_SIMULATE_ID}" STREQUAL "MSVC")
        set (BASE
             "-W4" "-Zc:preprocessor"
             "-clang:-march=native"
             "-clang:-fdiagnostics-color=always"
             "-clang:-fansi-escape-codes"
             "-ID:/VCPKG/installed/x64-windows/include"
        )

        set (CFLAGS_RELEASE_COMMON 
            -O2 -Ob2 -Oi -Ot "-clang:-O3" -DNDEBUG -GL -flto
        )
        set (CFLAGS_DEBUG_COMMON 
            "-clang:-Og" "-clang:-g3" -ZI -RTC1 -Wextra -Wpedantic
            -Wformat -D_FORTIFY_SOURCE=2 -fno-stack-protector
        )
        set (CFLAGS_RELWITHDEBINFO_COMMON ${CFLAGS_RELEASE_COMMON})
        set (CFLAGS_MINSIZEREL_COMMON ${CFLAGS_RELEASE_COMMON})
        
        if (DEBUG)
            string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -DEBUG:FULL")
        else()
            string(CONCAT CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -LTCG")
        endif ()
        if (NOT BUILD_DIST)
            set (BASE ${BASE} "-clang:-march=native")
        endif ()

        #find_program(LLVM_CPP "clang-cpp" REQUIRED)
        #set (CMAKE_CPP "${LLVM_CPP}")
        
        message(WARNING "Clang is pretending to be msvc. Ugh.")

    #-----------------------------------------------------------------------------------
    #-- CLANG PRETENDING TO BE GCC --
    else()
        set (WARNS ${WARNS}
            #-Weverything
            -Wno-c++98-compat -Wno-c++98-compat-pedantic
            -Wno-zero-as-null-pointer-constant
            -Wno-disabled-macro-expansion -Wno-reserved-macro-identifier -Wno-unused-macros
            -Wno-weak-vtables
            -Wno-shorten-64-to-32
            -Wno-ctad-maybe-unsupported
            -Wno-missing-prototypes
        )
        set (WARNS ${WARNS}
            -Wno-gnu -Wno-gnu-zero-variadic-macro-arguments
            -Wno-gnu-statement-expression
            -Werror=return-type
            -Werror=inline-namespace-reopened-noninline
        )
    
        if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
            set (LTO_STR 
                -flto=full
            )

            set (LTO_LINK_STR ${LTO_STR} -g3
                -Wl,-Xlink=-mllvm:-march=native
                -Wl,-Xlink=-mllvm:--aggressive-ext-opt
                -Wl,-Xlink=-mllvm:--extra-vectorizer-passes
                -Wl,-Xlink=-mllvm:--exhaustive-register-search
                -Wl,-Xlink=-mllvm:--enable-unsafe-fp-math
                -Wl,-Xlink=-mllvm:--optimize-regalloc
                -Wl,-Xlink=-mllvm:--scalar-evolution-use-expensive-range-sharpening
                -Wl,-Xlink=-mllvm:--slp-max-vf=0
                -Wl,-Xlink=-mllvm:--slp-vectorize-hor
                -Wl,-Xlink=-mllvm:--whole-program-visibility
                -Wl,-Xlink=-mllvm:--x86-indirect-branch-tracking
                -Wl,-Xlink=-mllvm:--interleave-loops
                -Wl,-Xlink=-mllvm:--ir-outliner
                -Wl,-Xlink=-mllvm:--enable-post-misched
                -Wl,-Xlink=-mllvm:--enable-nontrivial-unswitch
                -Wl,-Xlink=-mllvm:--enable-nonnull-arg-prop
            )
        else()
            set (LTO_STR -flto=full -fwhole-program-vtables 
                         -fvirtual-function-elimination -fsplit-lto-unit)
            set (LTO_LINK_STR -Wl,--lto-whole-program-visibility ${LTO_STR})
        endif()
    
        if (WIN32 OR MINGW OR MSYS)
            if (FALSE)
                set (BASE ${BASE} -fansi-escape-codes -target x86_64-w64-windows -fc++-abi=microsoft)
                string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
                        ${CLANG_STDLIB} ${LTO_STR}
                        -target x86_64-w64-windows -fc++-abi=microsoft
                )
            else()
                set (CLANG_STDLIB -stdlib=libc++)
                set (BASE ${BASE} -fansi-escape-codes)
                string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
                        ${CLANG_STDLIB}
                        ${LTO_LINK_STR}
                        -fuse-ld=lld
                        -rtlib=compiler-rt
                        --unwindlib=libunwind
                )
            endif()
            message("FOO")
        else()
            set (CLANG_STDLIB -stdlib=libc++)
            # set (CLANG_STDLIB -stdlib=libstdc++)
            string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
                    -fuse-ld=lld
                    -rtlib=compiler-rt
                    -unwindlib=libgcc
                    -rdynamic
                    ${CLANG_STDLIB}
                    ${LTO_LINK_STR}
            )
        endif()

        set (BASE ${BASE} ${WARNS} #-fPIC -fpie -masm=intel
             -fintegrated-as -fintegrated-cc1 -fno-legacy-pass-manager
             -fslp-vectorize
        )
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti ${CLANG_STDLIB} -std=gnu++20")
        
    endif() # Any clang now

        find_program(LLVM_AR "llvm-ar" REQUIRED)
        find_program(LLVM_NM "llvm-nm" REQUIRED)
        find_program(LLVM_RANLIB "llvm-ranlib" REQUIRED)
        set (CMAKE_AR "${LLVM_AR}")
        set (CMAKE_NM "${LLVM_NM}")
        set (CMAKE_RANLIB "${LLVM_RANLIB}")

#-----------------------------------------------------------------------------------------
#-- GCC --
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    set (WARNS ${WARNS}
         -Wsuggest-attribute=noreturn -Wsuggest-attribute=format
         -Wsuggest-attribute=const -Wsuggest-attribute=pure
         -Wsuggest-attribute=cold -Wsuggest-attribute=malloc
         -Wattributes -fdiagnostics-show-option
    )
    set (BASE ${BASE} ${WARNS} -fPIC -fpie -g3 -gdwarf-5)
    set (LTO_STR "-flto -fuse-linker-plugin")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -std=gnu++20")

    string (JOIN " " CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}"
        #-rdynamic
        -fuse-ld=bfd -Wl,-O3
        ${LTO_STR}
    )
    
    find_program(GCC_AR "gcc-ar" REQUIRED)
    find_program(GCC_NM "gcc-nm" REQUIRED)
    find_program(GCC_RANLIB "gcc-ranlib" REQUIRED)
    set (CMAKE_AR "${GCC_AR}")
    set (CMAKE_NM "${GCC_NM}")
    set (CMAKE_RANLIB "${GCC_RANLIB}")

#-----------------------------------------------------------------------------------------
#-- MSVC --
elseif (MSVC)

    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    if (NOT BUILD_DIST)
        set (BASE ${BASE} "/arch:AVX512")
    endif ()

endif()


#####################################################################################################
# Misc

#if (MSVC)
#    string(JOIN " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" "/clang:-std=gnu17" /clang:-xc)
#    string(JOIN " " CMAKE_CXX_FLAGS "-std=gnu++${CMAKE_CXX_STANDARD}" "${CMAKE_CXX_FLAGS}")
#else()
    list(APPEND CMAKE_C_FLAGS_DEBUG ${CFLAGS_DEBUG_COMMON} ${BASE} ${LTO_STR} ${CMAKE_C_FLAGS})
    list(APPEND CMAKE_C_FLAGS_MINSIZEREL ${CFLAGS_RELEASE_COMMON} ${BASE} -Os -s ${LTO_STR} ${CMAKE_C_FLAGS})
    list(APPEND CMAKE_C_FLAGS_RELEASE
        ${CMAKE_C_FLAGS_RELEASE} ${CFLAGS_RELEASE_COMMON}
        ${BASE} ${LTO_STR} ${CMAKE_C_FLAGS} ${__EXTRA_C_CXX_RELEASE_FLAGS})
    list(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO
        ${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CFLAGS_RELWITHDEBINFO_COMMON}
        ${BASE} ${LTO_STR} ${CMAKE_C_FLAGS} ${__EXTRA_C_CXX_RELEASE_FLAGS})

    #string(JOIN " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" "-std=gnu2x")
    #string(JOIN " " CMAKE_CXX_FLAGS "-std=gnu++${CMAKE_CXX_STANDARD}" "${CMAKE_CXX_FLAGS}")

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

    list(JOIN C_ONLY_WARNS " " C_ONLY_WARNS)
    string(JOIN " " CMAKE_C_FLAGS "${C_ONLY_WARNS} ${CMAKE_C_FLAGS}")
#endif()

if (MINGW)
    FIX_WINDOWS_PATHS(CMAKE_C_FLAGS)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-allow-multiple-definition")
endif()

configure_file(cmake-config.h.in config.h)
add_subdirectory(src)

# vim: tw=0
