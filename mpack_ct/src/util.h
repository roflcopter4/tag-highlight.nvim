#ifndef SRC_UTIL_H
#define SRC_UTIL_H
/*===========================================================================*/
#ifdef __cplusplus
    extern "C" {
#endif
#ifndef DEBUG
#  define DEBUG
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
#ifdef HAVE_CONFIG_H
#  include "topconfig.h"
#else  /* This just shuts up linters too lazy to include config.h */
#  define DEBUG
#  if defined(__GNUC__) || defined(__FreeBSD__)
#    define HAVE_ERR
#  endif
#  define VERSION "0.0.1"
#  define PACKAGE_STRING "idunno" VERSION
#  define _GNU_SOURCE
#endif
#if (defined(_WIN64) || defined(_WIN32))
#  define DOSISH
#  define WIN32_LEAN_AND_MEAN
//#  include <Winsock2.h>
#  include <io.h>
#  include <Windows.h>
#  include <dirent.h>
#  define strcasecmp  _stricmp
#  define strncasecmp _strnicmp
#  define realpath(PATH_, BUF_) _fullpath((BUF_),(PATH_),_MAX_PATH)
//#  define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
//#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#  undef BUFSIZ
#  define BUFSIZ 8192
#  define PATHSEP '\\'
#  define MAX(iA, iB) (((iA) > (iB)) ? (iA) : (iB))
    extern char * basename(char *path);
    typedef signed long long int ssize_t;
#else
#  include <unistd.h>
#  define PATHSEP '/'
#  define MAX(IA_, IB_)                 \
        __extension__({                 \
                __auto_type ia = (IA_); \
                __auto_type ib = (IB_); \
                (ia > ib) ? ia : ib;    \
        })
#  define MIN(IA_, IB_)                 \
        __extension__({                 \
                __auto_type ia = (IA_); \
                __auto_type ib = (IB_); \
                (ia < ib) ? ia : ib;    \
        })


#endif
/*===========================================================================*/

#if defined(__GNUC__) && !defined(DOSISH) && !defined(_GNU_SOURCE)
#  define _GNU_SOURCE
#endif
#define USE_XMALLOC

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bstring/bstrlib.h"
#include "contrib/contrib.h"
#include "data.h"

extern const char *HOME;

struct backups {
        char **lst;
        unsigned qty;
        unsigned max;
};


/*===========================================================================*/
/* Generic Macros */


#ifndef O_BINARY
#  define O_BINARY 00
#endif
#ifndef O_DSYNC
#  define O_DSYNC 00
#endif
#ifndef O_DIRECTORY
#  define O_DIRECTORY 00
#endif

#define ARRSIZ(ARR_)     (sizeof(ARR_) / sizeof(*(ARR_)))
#define modulo(iA, iB)   (((iA) % (iB) + (iB)) % (iB))
#define stringify(VAR_)  #VAR_
#define nputs(STR_)      fputs((STR_), stdout)
#define eprintf(...)     fprintf(stderr, __VA_ARGS__)
#define xfree(PTR_)      (free(PTR_), (PTR_) = NULL)
#define SLS(STR_)        ("" STR_ ""), (sizeof(STR_) - 1)
#define PSUB(PTR1, PTR2) ((ptrdiff_t)(PTR1) - (ptrdiff_t)(PTR2))

#define MAKE_PTHREAD_ATTR_DETATCHED(ATTR_)                                     \
        do {                                                                   \
                pthread_attr_init((ATTR_));                                    \
                pthread_attr_setdetachstate((ATTR_), PTHREAD_CREATE_DETACHED); \
        } while (0)

#define ALWAYS_INLINE   __attribute__((__always_inline__)) static inline
#define UNUSED          __attribute__((__unused__))
#define aWUR            __attribute__((__warn_unused_result__))
#define aMAL            __attribute__((__malloc__))
#define aALSZ(...)      __attribute__((__alloc_size__(__VA_ARGS__)))
#define aFMT(A1_, A2_)  __attribute__((__format__(printf, A1_, A2_)))

#ifdef DOSISH
#  define NORETURN __declspec(noreturn)
#  define fsleep(VAL) Sleep((VAL) * 1000)
#else
#  define NORETURN __attribute__((__noreturn__))
#  define fsleep(VAL)                                                                                 \
        nanosleep(                                                                                  \
            (struct timespec[]){                                                                    \
                {(int64_t)(VAL),                                                                    \
                 (int64_t)(((long double)(VAL) - (long double)((int64_t)(VAL))) * 1000000000.0L)}}, \
            NULL)
#endif

void __warn(bool print_err, const char *fmt, ...) aFMT(2, 3);
NORETURN void __err(int status, bool print_err, const char *fmt, ...) aFMT(3, 4);

#define err(EVAL, ...)  __err((EVAL), true, __VA_ARGS__)
#define errx(EVAL, ...) __err((EVAL), false, __VA_ARGS__)

#ifdef DEBUG
#  define warn(...)       __warn(true, __VA_ARGS__)
#  define warnx(...)      __warn(false, __VA_ARGS__)
#  define SHOUT(...)      __warn(false, __VA_ARGS__)
#  define nvprintf warnx
#  define echo    warnx
#else
#  undef eprintf
#  define warn(...)
#  define warnx(...)
#  define nvprintf(...)
#  define eprintf(...)
#  define echo(...)
#  define SHOUT(...) __warn(false, __VA_ARGS__)
/* #  define SHOUT(...) */
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
extern int     safe_open     (const char *filename, int flags, int mode) aWUR;
extern int     safe_open_fmt (const char *fmt, int flags, int mode, ...) aWUR aFMT(1, 4);
extern void    add_backup    (struct backups *list, void *item);
extern void    free_backups  (struct backups *list);
/* extern void *  xrealloc      (void *ptr, size_t size) aWUR aALSZ(2); */
extern void *  xrealloc_     (void *ptr, size_t size, const bstring *caller, int lineno) aWUR aALSZ(2);

#ifdef USE_XMALLOC
   /* extern void *  xmalloc    (size_t size)          aWUR aMAL aALSZ(1);
   extern void *  xcalloc    (int num, size_t size) aWUR aMAL aALSZ(1, 2); */
   extern void *  xmalloc_   (size_t size, const bstring *caller, int lineno)          aWUR aMAL aALSZ(1);
   extern void *  xcalloc_   (int num, size_t size, const bstring *caller, int lineno) aWUR aMAL aALSZ(1, 2);
#  define xmalloc(NBYTES_) xmalloc_((NBYTES_), btp_fromarray(__func__), __LINE__)
#  define xcalloc(ISIZE_, INUM_) xcalloc_((ISIZE_), (INUM_), btp_fromarray(__func__), __LINE__)
#  define xrealloc(PTR_, NBYTES_) xrealloc_((PTR_), (NBYTES_), btp_fromarray(__func__), __LINE__)
#else
#  define xmalloc malloc
#  define xcalloc calloc
#endif

#ifdef HAVE_REALLOCARRAY
/* extern void * xreallocarray  (void *ptr, size_t num, size_t size) aWUR aALSZ(2, 3); */
extern void * xreallocarray_ (void *ptr, size_t num, size_t size, const bstring *caller, int lineno) aWUR aALSZ(2, 3);
#  define nmalloc(NUM_, SIZ_)        xreallocarray_(NULL, (NUM_), (SIZ_), btp_fromarray(__func__), __LINE__)
#  define nrealloc(PTR_, NUM_, SIZ_) xreallocarray_((PTR_), (NUM_), (SIZ_), btp_fromarray(__func__), __LINE__)
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
