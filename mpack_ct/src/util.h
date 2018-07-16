#ifndef SRC_UTIL_H
#define SRC_UTIL_H
/*===========================================================================*/
#ifdef __cplusplus
    extern "C" {
#endif
#ifdef _MSC_VER /* Microsoft sure likes to complain... */
#  pragma warning(disable : 4996)
#  define _CRT_SECURE_NO_WARNINGS
#  define _CRT_NONSTDC_NO_WARNINGS
#  define __attribute__(...)
#endif
#ifdef HAVE_CONFIG_H
#  include "config.h"
#else  /* This just shuts up linters too lazy to include config.h */
#  define DEBUG
#  if defined(__GNUC__) || defined(__FreeBSD__)
#    define HAVE_ERR
#  endif
#  define VERSION "0.0.1"
#  define PACKAGE_STRING "idunno" VERSION
#  define _GNU_SOURCE
#endif
/*===========================================================================*/

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#define USE_XMALLOC

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bstring/bstrlib.h"

#include "data.h"

extern const char *HOME;

struct backups {
        char **lst;
        unsigned qty;
        unsigned max;
};


/*===========================================================================*/
/* Generic Macros */

#if (defined(_WIN64) || defined(_WIN32)) && !defined(HAVE_UNISTD_H)
#  define DOSISH
#  include <io.h>
#  include <Windows.h>
#  define strcasecmp  _stricmp
#  define strncasecmp _strnicmp
#  define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#  undef BUFSIZ
#  define BUFSIZ 8192
#  define PATHSEP '\\'
    extern char * basename(char *path);
#else
#  include <unistd.h>
#  define PATHSEP '/'
#endif
#ifndef O_BINARY
#  define O_BINARY 0
#endif

#define ARRSIZ(ARR_)     (sizeof(ARR_) / sizeof(*(ARR_)))
#define modulo(iA, iB)   (((iA) % (iB) + (iB)) % (iB))
#define stringify(VAR_)  #VAR_
#define nputs(STR_)      fputs((STR_), stdout)
#define eprintf(...)     fprintf(stderr, __VA_ARGS__)
#define xfree(PTR_)      (free(PTR_), (PTR_) = NULL)
#define SLS(STR_)        ("" STR_ ""), (sizeof(STR_) - 1)
#define PSUB(PTR1, PTR2) ((ptrdiff_t)(PTR1) - (ptrdiff_t)(PTR2))

#define MAX(IA_, IB_)                   \
        __extension__({                 \
                __auto_type ia = (IA_); \
                __auto_type ib = (IB_); \
                (ia > ib) ? ia : ib;    \
        })
#define MIN(IA_, IB_)                   \
        __extension__({                 \
                __auto_type ia = (IA_); \
                __auto_type ib = (IB_); \
                (ia < ib) ? ia : ib;    \
        })


#define ALWAYS_INLINE   __attribute__((__always_inline__)) static inline
#define UNUSED          __attribute__((__unused__))
#define aNORET          __attribute__((__noreturn__))
#define aWUR            __attribute__((__warn_unused_result__))
#define aMAL            __attribute__((__malloc__))
#define aALSZ(...)      __attribute__((__alloc_size__(__VA_ARGS__)))
#define aFMT(A1_, A2_)  __attribute__((__format__(printf, A1_, A2_)))

#define fsleep(VAL)                                                                                 \
        nanosleep(                                                                                  \
            (struct timespec[]){                                                                    \
                {(int64_t)(VAL),                                                                   \
                 (int64_t)(((long double)(VAL) - (long double)((int64_t)(VAL))) * 1000000000.0l)}}, \
            NULL)

#ifdef DEBUG
   void __warn(bool print_err, const char *fmt, ...) aFMT(2, 3);
   void __err(int status, bool print_err, const char *fmt, ...) aFMT(3, 4) aNORET;
#  define warn(...)       __warn(true, __VA_ARGS__)
#  define warnx(...)      __warn(false, __VA_ARGS__)
#  define SHOUT_(...)      __warn(false, __VA_ARGS__)
#  define err(EVAL, ...)  __err((EVAL), true, __VA_ARGS__)
#  define errx(EVAL, ...) __err((EVAL), false, __VA_ARGS__)
#  undef eprintf
#  define nvprintf warnx
#  define eprintf warnx
#  define echo    warnx
#else
#  undef eprintf
#  define warnx(...)
#  define nvprintf warnx
#  define eprintf warnx
#  define echo warnx
#  define warn warnx
#  define err(...) abort()
#  define errx(...) abort()
/* #  define SHOUT_(...) fprintf(stderr, __VA_ARGS__) */
#  define SHOUT_(...)
/* #  define fprintf(...) */
#endif

#if defined(__GNUC__)
#  if defined(__clang__) || defined(__cplusplus)
#    define FUNC_NAME (__extension__ __PRETTY_FUNCTION__)
#  else
     extern const char * __ret_func_name(const char *const function, size_t size);
#    define FUNC_NAME (__extension__(__ret_func_name(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__))))
#endif
#else
#  define FUNC_NAME (__func__)
#endif

#define ASSERT(CONDITION_, ...)  do { if (!(CONDITION_)) err(50, __VA_ARGS__); } while (0)
#define ASSERTX(CONDITION_, ...) do { if (!(CONDITION_)) errx(50, __VA_ARGS__); } while (0)
#define static_assert _Static_assert
#define thread_local  _Thread_local
#define noreturn      _Noreturn


/*===========================================================================*/
/* Generic Utility Functions */

#define xatoi(STR_)            __xatoi((STR_), false)
#define s_xatoi(STR_)          __xatoi((STR_), true)
#define free_all(...)          __free_all(__VA_ARGS__, NULL)

extern void    __free_all    (void *ptr, ...);
extern int64_t __xatoi       (const char *str, bool strict);
extern int     find_num_cpus (void);
extern FILE *  safe_fopen    (const char *filename, const char *mode) aWUR;
extern FILE *  safe_fopen_fmt(const char *fmt, const char *mode, ...) aWUR aFMT(1,3);
extern void    add_backup    (struct backups *list, void *item);
extern void    free_backups  (struct backups *list);
extern void *  xrealloc      (void *ptr, const size_t size)     aWUR aALSZ(2);

#ifdef USE_XMALLOC
   extern void *  xmalloc    (const size_t size)                aWUR aMAL aALSZ(1);
   extern void *  xcalloc    (const int num, const size_t size) aWUR aMAL aALSZ(1, 2);
#else
#  define xmalloc malloc
#  define xcalloc calloc
#endif

#define HAVE_REALLOCARRAY
#if defined(HAVE_REALLOCARRAY)
extern void * xreallocarray  (void *ptr, size_t num, size_t size) aWUR aALSZ(2, 3);
#  define nmalloc(NUM_, SIZ_)        xreallocarray(NULL, (NUM_), (SIZ_))
#  define nrealloc(PTR_, NUM_, SIZ_) xreallocarray((PTR_), (NUM_), (SIZ_))
#else
#  define nmalloc(NUM_, SIZ_)        xmalloc(((size_t)(NUM_)) * ((size_t)(SIZ_)))
#  define nrealloc(PTR_, NUM_, SIZ_) xrealloc((PTR_), ((size_t)(NUM_)) * ((size_t)(SIZ_)))
#endif

#define nalloca(NUM_, SIZ_) alloca(((size_t)(NUM_)) * ((size_t)(SIZ_)))
#define b_dump_list_nvim(LST_) __b_dump_list_nvim((LST_), #LST_)

extern void __b_dump_list_nvim(const b_list *list, const char *listname);


#ifdef __cplusplus
    }
#endif
/*===========================================================================*/
#endif /* util.h */
