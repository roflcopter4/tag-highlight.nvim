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
#ifdef _MSC_VER /* Microsoft sure likes to complain... */
#  pragma warning(disable : 4668) // undefined macros in ifdefs
#  pragma warning(disable : 4820) // padding
#  pragma warning(disable : 4996) // stupid deprications
#  pragma warning(disable : 5045) // spectre
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_NONSTDC_NO_WARNINGS
#endif
#ifndef __GNUC__
#  define __attribute__(...)
#endif
#if defined(HAVE_TOPCONFIG_H)
#  include "topconfig.h"
#elif defined(HAVE_CONFIG_H)
#  include "config.h"
#endif
#ifdef USE_JEMALLOC
#  include <jemalloc/jemalloc.h>
#endif
#if (defined(_WIN64) || defined(_WIN32)) && !defined(__CYGWIN__)
#  define DOSISH
#  define WIN32_LEAN_AND_MEAN
#  ifdef __MINGW__
#    include <dirent.h>
#    include <sys/time.h>
#    include <unistd.h>
#    pragma GCC diagnostic ignored "-Wattributes"
#  else
typedef signed long long int ssize_t;
#  endif
#  include <WinSock2.h>
#  include <Windows.h>
#  include <io.h>
#  include <direct.h>
#  include <pthread.h>
#  define PATHSEP '\\'
#  define __CLEANUP_C
#  define at_quick_exit(a)
#  define quick_exit(a) _Exit(a)
#  undef mkdir
#  define mkdir(PATH, MODE) mkdir(PATH)
extern char * basename(char *path);
#else
#  include <pthread.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/time.h>
#  include <unistd.h>
#  define PATHSEP '/'
#endif
#if (defined(__MINGW32__) || defined(__MINGW64__)) && (!defined(__MINGW__) || !defined(DOSISH))
#  error "Something really messed up here."
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
#ifdef HAVE_THREADS_H
#  include <threads.h>
#endif

/* Apperently some lunatic working on glibc decided it would be a good idea to
 * define `I' to the imaginary unit. As nice as that sounds, that's just about
 * the stupidest thing I have ever seen in anything resembling a system header.
 * It breaks things left, right, and centre. */
#ifdef I
#  undef I
#endif

/*===========================================================================*/

#define USE_XMALLOC
#define MPACK_USE_P99

#include "contrib/bstring/bstring.h"
#include "contrib/contrib.h"

extern char *HOME;

/*===========================================================================*/
/* Generic Macros */

#ifndef O_BINARY
#  define O_BINARY (0)
#endif
#ifndef O_DSYNC
#  define O_DSYNC (0)
#endif
#ifndef O_DIRECTORY
#  define O_DIRECTORY (0)
#endif

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

#define ARRSIZ(ARR)        (sizeof(ARR) / sizeof((ARR)[0]))
#define ASSERT(COND, ...)  ((!!(COND)) ? ((void)0) : err(50,  __VA_ARGS__))
#define ASSERTX(COND, ...) ((!!(COND)) ? ((void)0) : errx(50, __VA_ARGS__))
#define DIE_UNLESS(COND)   ((!!(COND)) ? ((void)0) : err(55, "%s", (#COND)))
#define DIE_UNLESSX(COND)  ((!!(COND)) ? ((void)0) : errx(55, "%s", (#COND)))
#define LSLEN(STR)         ((size_t)(sizeof(STR) - 1llu))
#define MODULO(iA, iB)     (((iA) % (iB) + (iB)) % (iB))
#define PSUB(PTR1, PTR2)   ((ptrdiff_t)(PTR1) - (ptrdiff_t)(PTR2))
#define SLS(STR)           ("" STR ""), LSLEN(STR)
#define STRINGIFY(VAR)     #VAR

#define __aMAL       __attribute__((__malloc__))
#define __aALSZ(...) __attribute__((__alloc_size__(__VA_ARGS__)))
#define __aNNA       __attribute__((__nonnull__))
#define __aNN(...)   __attribute__((__nonnull__(__VA_ARGS__)))
#define __aNT        __attribute__((__nothrow__))

#ifndef __always_inline
#  define __always_inline extern __inline__ __attribute__((__always_inline__))
#endif
#define ALWAYS_INLINE __always_inline
#define STATIC_INLINE __attribute__((__always_inline__)) static inline

#ifdef __clang__
#  define __aFMT(A1, A2) __attribute__((__format__(__printf__, A1, A2)))
#else
#  define __aFMT(A1, A2) __attribute__((__format__(__gnu_printf__, A1, A2)))
#endif

#if defined(__GNUC__)
#  if defined(__clang__) || defined(__cplusplus)
#    define FUNC_NAME (__extension__ __PRETTY_FUNCTION__)
#  else
extern const char * ret_func_name__(const char *const function, size_t size);
#    define FUNC_NAME \
        (__extension__(ret_func_name__(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__))))
#  endif
#  define auto_type   __extension__ __auto_type
#  define Auto        __extension__ __auto_type
#  define MAX(IA, IB) __extension__({auto_type ia=(IA); auto_type ib=(IB); (ia>ib)?ia:ib;})
#  define MIN(IA, IB) __extension__({auto_type ia=(IA); auto_type ib=(IB); (ia<ib)?ia:ib;})
#else
#  define FUNC_NAME   (__func__)
#  define MAX(iA, iB) (((iA) > (iB)) ? (iA) : (iB))
#  define MIN(iA, iB) (((iA) < (iB)) ? (iA) : (iB))
#endif

/*===========================================================================*/

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

/*===========================================================================*/

#ifdef realpath
#  undef realpath
#endif
#ifdef DOSISH
#  ifdef __MINGW__
extern void WINPTHREAD_API (pthread_exit)(void *res) __attribute__((__noreturn__));
#  endif
#  define realpath(PATH, BUF) _fullpath((BUF), (PATH), _MAX_PATH)
#  define strcasecmp   _stricmp
#  define strncasecmp  _strnicmp
#  define eprintf(...) (fprintf(stderr, "tag_highlight: " __VA_ARGS__), fflush(stderr))
#else
#  define eprintf(...) fprintf(stderr, "tag_highlight: " __VA_ARGS__)
#endif
#if defined(__MINGW__) || !defined(DOSISH)
#  define fsleep(VAL)  nanosleep(MKTIMESPEC((double)(VAL)), NULL)
#else
#  define fsleep(VAL)  Sleep((long long)((double)(VAL) * (1000.0L)))
#endif

/*===========================================================================*/

void          warn_(bool print_err, const char *fmt, ...) __aFMT(2, 3);
noreturn void err_ (int status, bool print_err, const char *fmt, ...) __aFMT(3, 4);

#define err(EVAL, ...)  err_((EVAL), true, __VA_ARGS__)
#define errx(EVAL, ...) err_((EVAL), false, __VA_ARGS__)
/* #define warn(...)       warn_(true, __VA_ARGS__) */
/* #define warnx(...)      warn_(false, __VA_ARGS__) */
#define warn  eprintf
#define warnx eprintf

#define echo warnx
#ifdef DEBUG
#  define SHOUT(...) warn_(false, __VA_ARGS__)
#  define echo       warnx
#else
#  undef eprintf
#  define eprintf(...)
#  define SHOUT(...)  warn_(false, __VA_ARGS__)
#endif

/*===========================================================================*/

#include "util/util.h"

#ifdef USE_XMALLOC
__aNT __aWUR __aMAL __aALSZ(1) ALWAYS_INLINE void *
xmalloc(const size_t size)
{
        void *tmp = malloc(size);
        if (tmp == NULL)
                err(100, "Malloc call failed - attempted %zu bytes", size);
        return tmp;
}

__aNT __aWUR __aMAL __aALSZ(1, 2) ALWAYS_INLINE  void *
xcalloc(const size_t num, const size_t size)
{
        void *tmp = calloc(num, size);
        if (tmp == NULL)
                err(101, "Calloc call failed - attempted %zu bytes", size);
        return tmp;
}
#else
#  define xmalloc malloc
#  define xcalloc calloc
#endif

__aNT __aWUR __aALSZ(2) ALWAYS_INLINE void *
xrealloc(void *ptr, const size_t size)
{
        void *tmp = realloc(ptr, size);
        if (tmp == NULL)
                err(102, "Realloc call failed - attempted %zu bytes", size);
        return tmp;
}

#if defined(HAVE_REALLOCARRAY) && !defined(WITH_JEMALLOC)
__aNT __aWUR __aALSZ(2, 3) ALWAYS_INLINE void *
xreallocarray(void *ptr, size_t num, size_t size)
{
        void *tmp = reallocarray(ptr, num, size);
        if (tmp == NULL)
                err(103, "Realloc call failed - attempted %zu bytes", size);
        return tmp;
}
#  define nmalloc(NUM_, SIZ_)        xreallocarray(NULL, (NUM_), (SIZ_))
#  define nrealloc(PTR_, NUM_, SIZ_) xreallocarray((PTR_), (NUM_), (SIZ_))
#else
#  define nmalloc(NUM_, SIZ_)        xmalloc(((size_t)(NUM_)) * ((size_t)(SIZ_)))
#  define nrealloc(PTR_, NUM_, SIZ_) xrealloc((PTR_), ((size_t)(NUM_)) * ((size_t)(SIZ_)))
#endif
#define xfree(PTR) free(PTR)
#define nalloca(NUM_, SIZ_) alloca(((size_t)(NUM_)) * ((size_t)(SIZ_)))

/*===========================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* Common.h */
