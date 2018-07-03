#ifndef SRC_MYTAGS_H
#define SRC_MYTAGS_H
/*===========================================================================*/
#ifdef __cplusplus
    extern "C" {
#endif
#ifdef _MSC_VER /* Microsoft sure likes to complain... */
#   pragma warning(disable : 4996)
#   define _CRT_SECURE_NO_WARNINGS
#   define _CRT_NONSTDC_NO_WARNINGS
#   define __attribute__(...)
#endif
#ifdef HAVE_CONFIG_H
#   include "config.h"
#else  /* This just shuts up linters too lazy to include config.h */
#   if defined(__GNUC__) || defined(__FreeBSD__)
#      define HAVE_ERR
#   endif
#   define VERSION "0.0.1"
#   define PACKAGE_STRING "idunno" VERSION
#   define _GNU_SOURCE
#endif
/*===========================================================================*/

#include "bstring/bstrlib.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data.h"

extern int mesfd;

struct backups {
        char **lst;
        unsigned qty;
        unsigned max;
};


/*===========================================================================*/
/* Generic Macros */

#if (defined(_WIN64) || defined(_WIN32)) && !defined(HAVE_UNISTD_H)
#   define DOSISH
#   include <io.h>
#   include <Windows.h>
#   define strcasecmp  _stricmp
#   define strncasecmp _strnicmp
#   define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#   define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#   undef BUFSIZ
#   define BUFSIZ 8192
#   define PATHSEP '\\'
    extern char * basename(char *path);
#else
#   include <unistd.h>
#   define PATHSEP '/'
#endif

#define ARRSIZ(ARR_)    (sizeof(ARR_) / sizeof(*(ARR_)))
#define modulo(A, B)    (((A) % (B) + (B)) % (B))
#define stringify(VAR_) #VAR_
#define nputs(STR_)     fputs((STR_), stdout)
#define eprintf(...)    fprintf(stderr, __VA_ARGS__)
#define xfree(PTR_)     (free(PTR_), (PTR_) = NULL)
#define UNUSED          __attribute__((__unused__))
#define aWUR            __attribute__((__warn_unused_result__))
#define aMAL            __attribute__((__malloc__))
#define aALSZ(...)      __attribute__((__alloc_size__(__VA_ARGS__)))
#define aFMT(A1_, A2_)  __attribute__((__format__(printf, A1_, A2_)))

#define B(LIT_CSTR_)    b_tmp(LIT_CSTR_)

#ifdef HAVE_ERR
#   undef HAVE_ERR
#endif
/* #   include <err.h> */
/* #else */
    void _warn(bool print_err, const char *fmt, ...) aFMT(2, 3);
#   define warn(...)       _warn(true, __VA_ARGS__)
#   define warnx(...)      _warn(false, __VA_ARGS__)
#   define err(EVAL, ...)  _warn(true, __VA_ARGS__), abort()
#   define errx(EVAL, ...) _warn(false, __VA_ARGS__), abort()
/* #   define err(EVAL, ...)  _warn(true, __VA_ARGS__), exit(EVAL) */
/* #   define errx(EVAL, ...) _warn(false, __VA_ARGS__), exit(EVAL) */
/* #endif */

/*===========================================================================*/
/* Generic Utility Functions */

#define xatoi(STR_)            __xatoi((STR_), false)
#define s_xatoi(STR_)          __xatoi((STR_), true)
#define free_all(...)          __free_all(__VA_ARGS__, NULL)
#define b_dump_list(FP_, LST_) __b_dump_list((FP_), (LST_), #LST_)
#define b_add_to_list(LST_, BSTR) __b_add_to_list((&(LST_)), (BSTR))

extern void    __b_dump_list (FILE *fp, const b_list *list, const char *listname);
extern void    __free_all    (void *ptr, ...);
extern int64_t __xatoi       (const char *str, bool strict);
extern int     find_num_cpus (void);
extern void    __b_add_to_list (b_list **list, bstring *bstr);
extern void *  xmalloc       (const size_t size)                aWUR aMAL aALSZ(1);
extern void *  xcalloc       (const int num, const size_t size) aWUR aMAL aALSZ(1, 2);
extern void *  xrealloc      (void *ptr, const size_t size)     aWUR aALSZ(2);
extern FILE *  safe_fopen    (const char *filename, const char *mode) aWUR;
extern void    add_backup    (struct backups *list, void *item);
extern void    free_backups  (struct backups *list);

#define HAVE_REALLOCARRAY
#if defined(HAVE_REALLOCARRAY)
extern void * xreallocarray  (void *ptr, size_t num, size_t size) aWUR aALSZ(2, 3);
#   define nmalloc(NUM_, SIZ_)        xreallocarray(NULL, (NUM_), (SIZ_))
#   define nrealloc(PTR_, NUM_, SIZ_) xreallocarray((PTR_), (NUM_), (SIZ_))
#else
#   define nmalloc(NUM_, SIZ_)        xmalloc(((size_t)(NUM_)) * ((size_t)(SIZ_)))
#   define nrealloc(PTR_, NUM_, SIZ_) xrealloc((PTR_), ((size_t)(NUM_)) * ((size_t)(SIZ_)))
#endif


#ifdef __cplusplus
    }
#endif
/*===========================================================================*/
#endif /* mytags.h */
