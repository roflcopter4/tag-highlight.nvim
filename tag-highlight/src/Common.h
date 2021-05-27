#ifndef THL_COMMON_H_
#define THL_COMMON_H_
#ifdef __cplusplus
extern "C" {
#endif
/*===========================================================================*/
#if defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
#    ifndef __MINGW__
#      define __MINGW__
#    endif
#  include "mingw_config.h"
#endif
#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_NONSTDC_NO_WARNINGS
#endif
#ifndef __GNUC__
#  define __attribute__(a)
#endif
#if defined(HAVE_TOPCONFIG_H)
#  include "topconfig.h"
#elif defined(HAVE_CONFIG_H)
#  include "config.h"
#else
#  define _GNU_SOURCE
#  define __USE_GNU
#endif
#ifdef USE_JEMALLOC
#  include <jemalloc/jemalloc.h>
#endif
#if (defined(_WIN64) || defined(_WIN32)) && !defined(__CYGWIN__)
#  define DOSISH
#  define WIN32_LEAN_AND_MEAN
#  ifdef __MINGW__
#    include <dirent.h>
#    include <unistd.h>
/* #    pragma GCC diagnostic ignored "-Wattributes" */
#  else
typedef signed long long int ssize_t;
#  endif
#  include <winsock2.h>
#  include <windows.h>
#  include <io.h>
#  include <direct.h>
#  include <pthread.h>
#  define PATHSEP '\\'
#  define __CLEANUP_C
#  define at_quick_exit(a)
#  define quick_exit(a) _Exit(a)
#  undef mkdir
#  define mkdir(PATH, MODE) mkdir(PATH)
#  ifndef O_CLOEXEC
#    define O_CLOEXEC 0
#  endif
extern char *basename(char *path);
#else
#  include <libgen.h>
#  include <pthread.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  define PATHSEP '/'
#endif
#if (defined(__MINGW32__) || defined(__MINGW64__)) && (!defined(__MINGW__) || !defined(DOSISH))
#  error "Something really messed up here."
#endif
#if defined(HAVE_TIME_H)
#  include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#  include <sys/time.h>
#endif
#ifndef __GNU_LIBRARY__
extern const char *program_invocation_short_name;
extern const char *program_invocation_name;
#endif

#define SAFE_PATH_MAX (4096)

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_STDNORETURN_H
#  include <stdnoreturn.h>
#endif

#include <talloc.h>

/* Guarentee that this typedef exists. */
typedef int error_t;

/*===========================================================================*/

#define MPACK_USE_P99   1
#define BSTR_USE_TALLOC 1

#include "bstring.h"
#include "my_p99_common.h"

#include "contrib/p99/p99.h"
#include "contrib/p99/p99_compiler.h"

/* #define ALWAYS_INLINE __attribute__((__gnu_inline__, __always_inline__)) extern inline  */
/* #define STATIC_INLINE __attribute__((__gnu_inline__, __always_inline__)) static inline  */
/* #define INLINE        __attribute__((__gnu_inline__)) extern inline */

#define ALWAYS_INLINE p99_inline
#define INLINE        p99_inline
#define STATIC_INLINE static inline

#include "contrib/contrib.h"

extern char *HOME;

/*===========================================================================*/
/* Some system/compiler specific config/setup */

#ifndef noreturn
#  if defined(_MSC_VER)
#    define noreturn __declspec(noreturn)
#  elif defined(__GNUC__)
#    define noreturn __attribute__((__noreturn__))
#  elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#    define noreturn _Noreturn
#  else
#    define noreturn
#  endif
#endif

#ifndef thread_local
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

#if !defined(__cplusplus) && !defined(static_assert)
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#    define static_assert _Static_assert
#  else
#    define static_assert(...)
#  endif
#endif

#if defined(_MSC_VER)
#  define UNUSED    __pragma(warning(suppress: 4100 4101))
#  define WEAK_SYMB __declspec(selectany)
#  define __aWUR    _Check_return_
#elif defined(__GNUC__)
#  define UNUSED    __attribute__((__unused__))
#  if defined(__MINGW__)
#    define WEAK_SYMB __declspec(selectany)
#  else
#    define WEAK_SYMB __attribute__((__weak__))
#  endif
#  define __aWUR      __attribute__((__warn_unused_result__))
#else
#  define WEAK_SYMB static
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

#ifdef DOSISH
#  ifdef realpath
#    undef realpath
#  endif
#  ifdef __MINGW__
#    ifndef WINPTHREAD_API
#      define WINPTHREAD_API __declspec(dllimport)
#    endif
extern void WINPTHREAD_API (pthread_exit)(void *res) __attribute__((__noreturn__));
#  endif
#  define realpath(PATH, BUF) _fullpath((BUF), (PATH), _MAX_PATH)
#  define strcasecmp   _stricmp
#  define strncasecmp  _strnicmp
#endif
#ifdef HAVE_NANOSLEEP //defined(__MINGW__) || !defined(DOSISH)
#  define fsleep(VAL)  nanosleep(MKTIMESPEC((double)(VAL)), NULL)
#else
#  define fsleep(VAL)  Sleep((long long)((double)(VAL) * (1000.0L)))
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
  #if __WORDSIZE == 64
  #  define SIZE_C  UINT64_C
  #  define SSIZE_C INT64_C
  #elif __WORDSIZE == 32
  #  define SIZE_C  UINT32_C
  #  define SSIZE_C INT32_C
  #else
  #  error "Unable to determine word size."
  #endif
#endif

/*===========================================================================*/
/* Attribute aliases and junk like MIN, MAX, NOP, etc */

#define __aMAL       __attribute__((__malloc__))
#define __aALSZ(...) __attribute__((__alloc_size__(__VA_ARGS__)))
#define __aNNA       __attribute__((__nonnull__))
#define __aNN(...)   __attribute__((__nonnull__(__VA_ARGS__)))
#define __aNT        __attribute__((__nothrow__))

#ifdef __clang__
#  define __aFMT(A1, A2) __attribute__((__format__(__printf__, A1, A2)))
#else
#  define __aFMT(A1, A2) __attribute__((__format__(__gnu_printf__, A1, A2)))
#endif

#if defined(__GNUC__)
#  if defined(__clang__) || defined(__cplusplus)
#    define FUNC_NAME (__extension__ __PRETTY_FUNCTION__)
#  else
     extern const char *ret_func_name__(const char *function, size_t size);
#    define FUNC_NAME \
        (__extension__(ret_func_name__(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__))))
#  endif
#  define auto_type      __extension__ __auto_type
#  define Auto           __extension__ __auto_type
#  define MAX(IA, IB)    __extension__({auto_type ia=(IA); auto_type ib=(IB); (ia>ib)?ia:ib;})
#  define MIN(IA, IB)    __extension__({auto_type ia=(IA); auto_type ib=(IB); (ia<ib)?ia:ib;})
#  define MODULO(IA, IB) __extension__({auto_type ia=(IA); auto_type ib=(IB); (ia % ib + ib) % ib;})
#else
#  define FUNC_NAME      (__func__)
#  define MAX(iA, iB)    (((iA) > (iB)) ? (iA) : (iB))
#  define MIN(iA, iB)    (((iA) < (iB)) ? (iA) : (iB))
#  define MODULO(iA, iB) (((iA) % (iB) + (iB)) % (iB))
#endif

/*===========================================================================*/
/* Generic Macros */

#define MAKE_PTHREAD_ATTR_DETATCHED(ATTR_)                                     \
        do {                                                                   \
                pthread_attr_init((ATTR_));                                    \
                pthread_attr_setdetachstate((ATTR_), PTHREAD_CREATE_DETACHED); \
        } while (0)

#define START_DETACHED_PTHREAD(...)                                             \
        do {                                                                    \
                pthread_t       m_pid_;                                         \
                pthread_attr_t m_attr_;                                         \
                pthread_attr_init(&m_attr_);                                    \
                pthread_attr_setdetachstate(&m_attr_, PTHREAD_CREATE_DETACHED); \
                pthread_create(&m_pid_, &m_attr_, __VA_ARGS__);                 \
        } while (0)

/* #define SHOUT(...)      warn_(false, true,  __VA_ARGS__) */

#define shout(...) (fprintf(stderr, "tag-highlight: " __VA_ARGS__), fflush(stderr))
#ifdef DEBUG
#  define eprintf(...) shout(__VA_ARGS__)
#else
#  define eprintf(...) ((void)0)
#endif

/*===========================================================================*/

#include "util/util.h"
#define nalloca(NUM, SIZ) alloca(((size_t)(NUM)) * ((size_t)(SIZ)))

/*===========================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* Common.h */
