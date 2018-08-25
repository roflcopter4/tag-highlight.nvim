cmake_minimum_required (VERSION 3.6.0)
project (tag_highlight C)
include (FindPkgConfig)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckIncludeFile)

set (REQUIRED_LZMA_VERSION 5.3.1)
option(BUILD_DIST "\
Include the required alpha (and therefore uncommon) liblzma version as a static\
library, and use bundled BSD extensions instead of libbsd even if it is available.")
option(STATIC_LIBS_ONLY "Build with only static libraries.")

###############################################################################
# Libraries

set(THREADS_PREFER_PTHREAD_FLAG ON)

if (MSVC OR MINGW)
    if (MSVC)
        set (USE_PTHREADS 1)
        include_directories("${CMAKE_SOURCE_DIR}/win32_libs/include/pthread")
        include_directories("${CMAKE_SOURCE_DIR}/win32_libs/include/msvc")
    elseif (MINGW)
        find_package (Threads REQUIRED)
        include_directories("${CMAKE_SOURCE_DIR}/win32_libs/include/msvc")
    endif()
    set (LZMA_SUPPORT 1)
    include_directories("${CMAKE_SOURCE_DIR}/win32_libs/include")
    message(WARNING "Automatically enabling threads and lzma for Windows")
else()
    find_package (ZLIB REQUIRED)
    find_package (Threads REQUIRED)
    find_package (LibLZMA)
endif()

set (CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE -DHAVE_CONFIG_H)

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

if (LIBLZMA_FOUND)
    if (${LIBLZMA_VERSION_STRING} STRLESS ${REQUIRED_LZMA_VERSION})
        message(WARNING "\
liblzma version ${REQUIRED_LZMA_VERSION} or greater is required for XZ support. \
XZ support will be disabled for this build.")
    else()
        set (LZMA_SUPPORT 1)
    endif()
endif()

if (THREADS_HAVE_PTHREAD_ARG)
    set (USE_PTHREADS 1)
endif()

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
        set (WARNS "-fdiagnostics-color=always -Wall -Werror=implicit -Werror=incompatible-pointer-types")
        string(CONCAT WARNS "-Werror=pointer-to-int-cast -Werror=int-conversion")
    else()
        set (WARNS "-fdiagnostics-color=always -Wall -Werror=implicit -Werror=incompatible-pointer-types")
        string(CONCAT WARNS "-Werror=pointer-to-int-cast -Werror=int-conversion -Werror=format-extra-args")
    endif()

    if (NOT BUILD_DIST) 
        set (MARCH_SETTING "-march=native")
    endif()
    set (SANIT "-fsanitize=address -fsanitize=leak -fsanitize-address-use-after-scope")
    set (BASE "${WARNS} ${MARCH_SETTING} -fdiagnostics-color=always")

#####################################################################################################
    if (CLANG_STR_SEARCH EQUAL -1)
        set (CMAKE_C_FLAGS_DEBUG "${BASE} -O0 -g3 -Wextra -Wpedantic -Wformat")
        set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} ${SANIT} -Og -g3")
        # set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} -O2 -g3")
#----------------------------------------------------------------------------------------------------
    else() # CLANG
#----------------------------------------------------------------------------------------------------
        # set (BASE "${BASE} -fprofile-instr-generate")
        # set (BASE "${BASE} -fprofile-instr-use=${CMAKE_CURRENT_BINARY_DIR}/code.profdata")
        set (SANIT "")
        # set (SANIT "-fsanitize=address -fsanitize=leak -fsanitize-address-use-after-scope")

        set (CMAKE_C_FLAGS_DEBUG "${BASE} -O0 -g3 -Wextra -Wpedantic -Wformat -Weverything")
        string(CONCAT CMAKE_C_FLAGS_DEBUG "-Wno-sign-conversion -Wno-documentation-unknown-command"
               "-Wno-cast-qual -Wno-shorten-64-to-32 -Wno-unused-macros -Wno-switch-enum"
               "-Wno-disabled-macro-expansion -Wno-covered-switch-default -Wno-padded -Werror=enum-conversion")
        set (CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE} ${SANIT} -O2 -g")
        string (CONCAT CMAKE_C_FLAGS_RELWITHDEBINFO
                "-Werror=enum-conversion -Wno-shorten-64-to-32 -Wno-sign-conversion")

        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -rtlib=compiler-rt")
    endif()
#####################################################################################################

    set (CMAKE_C_FLAGS_MINSIZEREL     "${BASE} -Os")
    set (CMAKE_C_FLAGS_RELEASE        "${BASE} -Ofast -ftree-vectorize")

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
    # set (CMAKE_EXE_LINKER_FLAGS_RELEASE "-static -s")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "-s")
    set (CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "-s")
endif()

add_definitions(-D_GNU_SOURCE -DHAVE_CONFIG_H)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/src/contrib/jsmn")
configure_file(cmake-config.h.in topconfig.h)
add_subdirectory(src)
