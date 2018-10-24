cmake_minimum_required (VERSION 3.6.0)
project (Tag_Highlight_Nvim C)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)

set (REQUIRED_LZMA_VERSION 5.3.1)
option(BUILD_DIST "\
Include the required alpha (and therefore uncommon) liblzma version as a static\
library, and use bundled BSD extensions instead of libbsd even if it is available.")
option(STATIC_LIBS_ONLY "Build with only static libraries.")
option(SANITIZE "Enable sanitizers")

###############################################################################
# Libraries

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
# set(USE_JEMALLOC ON)

macro(FIX_WINDOWS_PATHS _pathvar)
    string(REPLACE "\\" "/" ${_pathvar} "${${_pathvar}}")
endmacro()

if (MINGW OR CMAKE_COMPILER_IS_MINGW)
    if (NOT MINGW)
        set (MINGW TRUE)
    endif()
    message("Is mingw")
elseif (MSYS)
    message("Is msys")
endif()

if (MSVC)
    message(FATAL_ERROR "MSVC is not and will probably never be supported. Use MinGW")
endif()

if (MINGW)
    include_directories("D:/msys64/mingw64/x86_64-w64-mingw32/include")
    add_definitions(-D__MINGW__)
    #include_directories(tag_highlight "${CMAKE_SOURCE_DIR}/win32_libs/include/pthread")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package (ZLIB REQUIRED)
find_package (Threads REQUIRED)
find_package (LibLZMA)
find_package (LibClang REQUIRED)
find_package (PCRE2 REQUIRED)
find_package (LibEv REQUIRED)

include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${PCRE2_INCLUDE_DIRS})
include_directories("${CMAKE_CURRENT_BINARY_DIR}")


string(CONCAT CMAKE_C_FLAGS "${LIBCLANG_CFLAGS}" " ${CMAKE_C_FLAGS}")
if (MINGW)
    FIX_WINDOWS_PATHS(CMAKE_C_FLAGS)
endif()

set (CMAKE_REQUIRED_DEFINITIONS -D__USE_MINGW_ANSI_STDIO=1 -D_GNU_SOURCE -DHAVE_TOPCONFIG_H -D__USE_ISOC11)

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

CHECK_SYMBOL_EXISTS (reallocarray "stdlib.h"   HAVE_REALLOCARRAY)
CHECK_SYMBOL_EXISTS (vasprintf    "stdio.h"    HAVE_VASPRINTF)
CHECK_SYMBOL_EXISTS (asprintf     "stdio.h"    HAVE_ASPRINTF)
CHECK_SYMBOL_EXISTS (strlcpy      "string.h"   HAVE_STRLCPY)
CHECK_SYMBOL_EXISTS (strlcat      "string.h"   HAVE_STRLCAT)
CHECK_SYMBOL_EXISTS (strtonum     "string.h"   HAVE_STRTONUM)
CHECK_SYMBOL_EXISTS (strsep       "string.h"   HAVE_STRSEP)
CHECK_SYMBOL_EXISTS (strdupa      "string.h"   HAVE_STRDUPA)
CHECK_SYMBOL_EXISTS (memrchr      "string.h"   HAVE_MEMRCHR)
CHECK_SYMBOL_EXISTS (strchrnul    "string.h"   HAVE_STRCHRNUL)
CHECK_SYMBOL_EXISTS (err          "err.h"      HAVE_ERR)
CHECK_SYMBOL_EXISTS (gettimeofday "sys/time.h" HAVE_GETTIMEOFDAY)
CHECK_SYMBOL_EXISTS (posix_spawnp "spawn.h"    HAVE_POSIX_SPAWNP)
CHECK_SYMBOL_EXISTS (pause        "unistd.h"   HAVE_PAUSE)
CHECK_SYMBOL_EXISTS (pipe2        "unistd.h"   HAVE_PIPE2)
CHECK_SYMBOL_EXISTS (fork         "unistd.h"   HAVE_FORK)
CHECK_SYMBOL_EXISTS (open_memstream "stdio.h"  HAVE_OPEN_MEMSTREAM)

CHECK_INCLUDE_FILE("complex.h"     HAVE_COMPLEX_H)
CHECK_INCLUDE_FILE("execinfo.h"    HAVE_EXECINFO_H)
CHECK_INCLUDE_FILE("stdatomic.h"   HAVE_STDATOMIC_H)
CHECK_INCLUDE_FILE("stdnoreturn.h" HAVE_STDNORETURN_H)
CHECK_INCLUDE_FILE("tgmath.h"      HAVE_TGMATH_H)
CHECK_INCLUDE_FILE("threads.h"     HAVE_THREADS_H)

string(FIND ${CMAKE_C_COMPILER} "clang" CLANG_STR_SEARCH)

###############################################################################
# Flags

if ((CMAKE_BUILD_TYPE STREQUAL "Debug") OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    set (DEBUG 1)
endif()

#####################################################################################################
if (MSVC)
    set (CMAKE_C_FLAGS_RELEASE "/MP /GS /TC /GL /W3 /Gm- /Ox /Ob2 /Zc:inline /fp:fast /Gd /Oi /MD /FC /Ot /arch:AVX2")
    
    set (CMAKE_BUILD_TYPE Release)
    set (CMAKE_C_FLAGS_DEBUG "/GS /TC /Wall /ZI /Gm- /Od /RTC1 /Gd /Oi /MDd /FC /MP /arch:AVX2 /DDEBUG")
    set (CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_RELEASE})
    set (CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELEASE})

#####################################################################################################
else()
    if (MINGW)
        add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
        set (WARNS "-Wall -Werror=implicit -Werror=incompatible-pointer-types")
        string(CONCAT WARNS "-Werror=pointer-to-int-cast -Werror=int-conversion -Werror=implicit-function-declaration -Werror=incompatible-pointer-types")
    else()
        string(CONCAT WARNS "-Wall -Werror=implicit -Werror=incompatible-pointer-types"
            " -Werror=pointer-to-int-cast -Werror=int-conversion -Werror=format-extra-args")
    endif()

    if (NOT BUILD_DIST) 
        set (MARCH_SETTING "-march=native")
    endif()


    if (SANITIZE)
        if ("${SANITIZE}" STREQUAL "thread")
            set (SANIT "-fsanitize=thread -fsanitize-address-use-after-scope -fsanitize=undefined")
        elseif ("${SANITIZE}" STREQUAL "address")
            set (SANIT "-fsanitize=address -fsanitize-address-use-after-scope -fsanitize=undefined")
        elseif ("${SANITIZE}" STREQUAL "ON" OR "${SANITIZE}" STREQUAL "On")
            set (SANIT "-fsanitize-address-use-after-scope -fsanitize=undefined")
        else ()
            message(FATAL_ERROR "Unrecognized SANITIZE setting!")
        endif()
    endif()

    set (BASE "${WARNS} ${MARCH_SETTING} ${SANIT} -std=gnu17 -fdiagnostics-color=always")

#####################################################################################################
    if (CLANG_STR_SEARCH EQUAL -1)
        string(CONCAT WARNS "${WARNS}" 
            " -Wsuggest-attribute=pure -Wsuggest-attribute=cold -Wsuggest-attribute=malloc -Wsuggest-attribute=const"
            " -Wsuggest-attribute=noreturn -Wsuggest-attribute=format" " -Wold-style-definition -Wold-style-declaration")
        set (CMAKE_C_FLAGS_DEBUG "${BASE} -O0 -g3 -Wextra -Wpedantic -Wformat -Wno-switch-unreachable")
        set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} -Wextra -Wpedantic -Wno-switch-unreachable -Og -g3")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        # set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} -Og -g3")
#----------------------------------------------------------------------------------------------------
    else() # CLANG
#----------------------------------------------------------------------------------------------------
        # set (BASE "${BASE} -fprofile-instr-generate")
        # set (BASE "${BASE} -fprofile-instr-use=${CMAKE_CURRENT_BINARY_DIR}/code.profdata")
        # set (SANIT "")
        set (SANIT "-fsanitize=undefined -fsanitize=null")

        string(CONCAT BASE "${BASE} ${SANIT}"
            " -Wno-incompatible-pointer-types-discards-qualifiers"
            " -Wno-error=incompatible-pointer-types-discards-qualifiers"
            )

        string(CONCAT CMAKE_C_FLAGS_DEBUG "${BASE} -O0 -g3 -Wextra -Wpedantic -Wformat" " -Wall" # " -Weverything"
              " -Wno-sign-conversion -Wno-documentation-unknown-command"
              " -Wno-cast-qual -Wno-shorten-64-to-32 -Wno-unused-macros -Wno-switch-enum"
              " -Wno-disabled-macro-expansion -Wno-covered-switch-default -Wno-padded -Werror=enum-conversion"
              " -Wno-vla -Wno-missing-variable-declarations -Wno-gnu" " -Wno-format-nonliteral"
               )
        set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} -Og -g3")
        string (JOIN " " CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE}"
            # "-Rpass='.*'"
            # "-Rpass-missed='.*'" 
            # "-Rpass='inline'"
            # "-Rpass-missed='inline'" 
            # "-mllvm -inline-threshold=10000000"
            # "-mllvm -inlinehint-threshold=30000000"
            # "-Rpass-analysis='.*'"
        )
        # string (CONCAT CMAKE_C_FLAGS_RELWITHDEBINFO
        #         "-Werror=enum-conversion -Wno-shorten-64-to-32 -Wno-sign-conversion")
        # 
        # set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    endif()
#####################################################################################################

    string (JOIN " " CMAKE_C_FLAGS_MINSIZEREL  "${CMAKE_C_FLAGS_MINSIZEREL}" "${BASE}" "-Os")
    string (JOIN " " CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" "${BASE}" "-g3 -Ofast -ftree-vectorize")

    if (MINGW)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")
    endif()
    
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
    set (CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "-s")
endif()

add_definitions(-D_GNU_SOURCE -DHAVE_TOPCONFIG_H)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib/jsmn")

include("${PROJECT_SOURCE_DIR}/cmake/get_compiler_include_paths.cmake")
GET_COMPILER_PATHS(COMPILER_INCLUDE_DIRECTORIES)

configure_file(cmake-config.h.in topconfig.h)
add_subdirectory(src)

# vim: tw=0
