// ReSharper disable CppUnusedIncludeDirective
#pragma once
#ifndef THL_COMMON_H_
#define THL_COMMON_H_
#ifdef __cplusplus
extern "C" {
#endif
/*===========================================================================*/
#if defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
#  ifndef __MINGW__
#    define __MINGW__
#  endif
#  include "mingw_config.h"
//#  define __WINPTRHEAD_ENABLE_WRAP_API 1
#endif
#ifdef _MSC_VER
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  ifndef _CRT_NONSTDC_NO_WARNINGS
#    define _CRT_NONSTDC_NO_WARNINGS
#  endif
#endif
#if !(defined __GNUC__ || defined __clang__) && !defined __attribute__
#  define __attribute__(a)
#  error "SIGH"
#endif
#if defined(HAVE_TOPCONFIG_H)
#  include "topconfig.h"
#elif defined(HAVE_CONFIG_H)
#  include "config.h"
#else
#  define _GNU_SOURCE
#  define __USE_GNU
#endif
#if (defined(_WIN64) || defined(_WIN32)) && !defined(__CYGWIN__)
#  define _USE_DECLSPECS_FOR_SAL 1
#  ifdef __MINGW__
#    include <dirent.h>
#    include <sys/stat.h>
#    include <unistd.h>
#  else
typedef signed long long int ssize_t;
#  endif
#  include <Windows.h>
#  include <direct.h>
#  include <io.h>
#  include <winsock2.h>
#  include <pthread.h>
#  ifdef _MSC_VER
//#    pragma comment(lib, "pthreadVC3.lib")
#  endif
#  define PATHSEP     '\\'
#  define PATHSEP_STR "\\"
#  ifndef _UCRT
#    define at_quick_exit(a)
#    define quick_exit(a) _Exit(a)
#  endif
#  undef mkdir
#  define mkdir(PATH, MODE) mkdir(PATH)
typedef SOCKET socket_t;
#else
#  include <dirent.h>
#  include <libgen.h>
#  include <pthread.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  define PATHSEP     '/'
#  define PATHSEP_STR "/"
typedef int socket_t;
#endif
#if (defined(__MINGW32__) || defined(__MINGW64__)) && \
    (!defined(__MINGW__) || !defined(_WIN32))
#  error "Something really messed up here."
#endif
#if defined __cplusplus
#  include <ctime>
#elif defined(HAVE_TIME_H)
#  include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#  include <sys/time.h>
#endif

#ifndef HAVE_BASENAME
extern char *basename(char const *);
#endif
#ifndef HAVE_PROGRAM_INVOCATION_SHORT_NAME
extern char const *program_invocation_short_name;
#endif
#ifndef HAVE_PROGRAM_INVOCATION_NAME
extern char const *program_invocation_name;
#endif

#define SAFE_PATH_MAX (4096)

#ifndef __cplusplus
#  include <assert.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <inttypes.h>
#  include <stdarg.h>
#  include <stdbool.h>
#  include <stddef.h>
#  include <stdint.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#endif

#ifdef basename
#  undef basename
#  ifndef __cplusplus
extern char *basename(const char *) __THROW __nonnull((1));
#  endif
#endif

#include <talloc.h>

//#if __has_include("threads.h")
//#  include <threads.h>
//#elif __has_include("tinycthread.h")
//#  include <tinycthread.h>
//#else
//#  error "no threads"
//#endif

/* Guarentee that this typedef exists. */
typedef int error_t;

/*===========================================================================*/

#define MPACK_USE_P99   1
#define BSTR_USE_TALLOC 1

#ifdef __cplusplus
}
#  include "contrib/bstring/defines.h"
//#  include "bstring.h"
#  include "contrib/contrib.h"
extern "C" {
#else
#  include "bstring.h"
#  include "my_p99_common.h"

#  include "contrib/p99/p99.h"
#  include "contrib/p99/p99_compiler.h"
#  include "contrib/contrib.h"
#endif
#include "util/initializer_hack.h"

#define ALWAYS_INLINE p99_inline
#define INLINE        p99_inline
#define STATIC_INLINE static inline

extern char *HOME;

#ifdef basename
#  undef basename
#endif

/*===========================================================================*/
/* Some system/compiler specific config/setup */

#if !defined(thread_local) && !defined(__cplusplus)
#  if defined(_MSC_VER)
#    define thread_local __declspec(thread)
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#    define thread_local _Thread_local
#  elif defined(__GNUC__)
#    define thread_local __thread
#  else
#    define thread_local
#  endif
#endif

#if !defined(static_assert) && !defined(__cplusplus)
#  if defined _MSC_VER
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#    define static_assert(...) _Static_assert(__VA_ARGS__)
#  else
#    define
#    define static_assert(COND ((char [(COND) ? 1 : -1]){})
#  endif
#endif

#if defined(_MSC_VER)
#  define WEAK_SYMB __declspec(selectany)
#  define __aWUR
#  define restrict __restrict
#elif defined(__GNUC__)
#  if defined(__MINGW__)
#    define WEAK_SYMB __declspec(selectany)
#  else
#    define WEAK_SYMB __attribute__((__weak__))
#  endif
#  define __aWUR __attribute__((__warn_unused_result__))
#else
#  define WEAK_SYMB static
#  define __aWUR
#endif

#if defined __cplusplus && (__cplusplus >= 201700L || defined __TAG_HIGHLIGHT__)
# define UNUSED      [[maybe_unused]]
# define ND          [[nodiscard]]
# define NORETURN    [[noreturn]]
# define FALLTHROUGH [[fallthrough]]
#elif defined __GNUC__
# define UNUSED      __attribute__((__unused__))
# define ND          __attribute__((__warn_unused_result__))
# define NORETURN    __attribute__((__noreturn__))
# define FALLTHROUGH __attribute__((__fallthrough__))
#elif defined _MSC_VER
# define UNUSED      __pragma(warning(suppress : 4100 4101))
# define ND          _Check_return_
# define NORETURN    __declspec(noreturn)
# define FALLTHROUGH __fallthrough
#else
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define NORETURN _Noreturn
# endif
# define UNUSED
# define ND
# define FALLTHROUGH
#endif

#ifndef __BEGIN_DECLS
#  ifdef __cplusplus
#    define __BEGIN_DECLS extern "C" {
#  else
#    define __BEGIN_DECLS
#  endif
#endif
#ifndef __END_DECLS
#  ifdef __cplusplus
#    define __END_DECLS }
#  else
#    define __END_DECLS
#  endif
#endif

#ifdef _WIN32
#  ifdef realpath
#    undef realpath
#  endif
#  ifdef __MINGW__
#    ifndef WINPTHREAD_API
#      define WINPTHREAD_API __declspec(dllimport)
#    endif
extern void WINPTHREAD_API(pthread_exit)(void *res) __attribute__((__noreturn__));
#  endif
#  ifndef PATH_MAX
#    define PATH_MAX            _MAX_PATH
#  endif
#  define realpath(PATH, BUF) _fullpath((BUF), (PATH), _MAX_PATH)
#  define strcasecmp          _stricmp
#  define strncasecmp         _strnicmp
#endif
#ifdef HAVE_NANOSLEEP
#  define fsleep(VAL) nanosleep(MKTIMESPEC((double)(VAL)), NULL)
#else
#  define fsleep(VAL) Sleep((long long)((double)(VAL) * (1000.0L)))
#endif

#ifndef O_BINARY
#  define O_BINARY (0)
#endif
#ifndef O_DSYNC
#  define O_DSYNC (0)
#endif
#ifndef O_DIRECTORY
#  define O_DIRECTORY (0)
#endif
#ifndef O_CLOEXEC
#  define O_CLOEXEC (0)
#endif
#ifndef O_EXCL
#  define O_EXCL (0)
#endif

#ifndef __WORDSIZE
#  if UINTPTR_MAX == UINT64_MAX
#    define __WORDSIZE 64
#  elif UINTPTR_MAX == UINT32_MAX
#    define __WORDSIZE 32
#  else
#    error "Unable to determine word size."
#  endif
#endif

#ifndef SIZE_C
#  define SIZE_C(x)  P99_PASTE3(UINT, __WORDSIZE, _C)(x)
#  define SSIZE_C(x) P99_PASTE3(INT,  __WORDSIZE, _C)(x)
#endif

#ifdef __cplusplus
#  ifdef bool
#    undef bool
#  endif
#  ifdef true
#    undef true
#  endif
#  ifdef false
#    undef false
#  endif
#endif

/*===========================================================================*/
/* Generic Macros */

#define MAKE_PTHREAD_ATTR_DETATCHED(ATTR_)                                 \
      do {                                                                 \
            pthread_attr_init((ATTR_));                                    \
            pthread_attr_setdetachstate((ATTR_), PTHREAD_CREATE_DETACHED); \
      } while (0)

#define START_DETACHED_PTHREAD(...)                                         \
      do {                                                                  \
            pthread_t      m_pid_;                                          \
            pthread_attr_t m_attr_;                                         \
            pthread_attr_init(&m_attr_);                                    \
            pthread_attr_setdetachstate(&m_attr_, PTHREAD_CREATE_DETACHED); \
            pthread_create(&m_pid_, &m_attr_, __VA_ARGS__);                 \
      } while (0)

/* #define SHOUT(...)      warn_(false, true,  __VA_ARGS__) */

#define shout(...)                                                          \
      (fprintf(stderr, "tag-highlight: " __VA_ARGS__), fputc('\n', stderr), \
       fflush(stderr))
//#  define eprintf(...) shout(__VA_ARGS__)
#  define eprintf(...) fprintf(stderr, __VA_ARGS__)
#if 0
#ifdef DEBUG
#  define eprintf(...) shout(__VA_ARGS__)
#else
#  define eprintf(...) ((void)0)
#endif
#endif

/*===========================================================================*/

#include "util/util.h"
#define nalloca(NUM, SIZ) alloca(((size_t)(NUM)) * ((size_t)(SIZ)))

//#define DEBUG_LOGS

#ifdef USE_JEMALLOC
#  define JEMALLOC_MANGLE
#  include <jemalloc/jemalloc.h>
#endif

/*===========================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* Common.h */
// vim: ft=c
