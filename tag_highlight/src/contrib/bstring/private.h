#ifndef BSTRLIB_PRIVATE_H
#define BSTRLIB_PRIVATE_H

#include "mingw_config.h"

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#ifdef __clang__
#  define __gnu_printf__ __printf__
#endif
#if defined(HAVE_CONFIG_H)
#  include "config.h"
#elif defined(HAVE_TOPCONFIG_H)
#  include "topconfig.h"
#endif
/* These warnings from MSVC++ are totally pointless. */
#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS
#  pragma warning(disable : 4668) // undefined macros in ifdefs
#  pragma warning(disable : 4820) // padding
#  pragma warning(disable : 4996) // stupid deprications
#  pragma warning(disable : 5045) // spectre
#endif

#ifdef USE_JEMALLOC
#  include <jemalloc/jemalloc.h>
#endif

#include "bstring.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#  ifndef PATH_MAX
#    define PATH_MAX _MAX_PATH
#  endif
#  ifdef __GNUC__
#    pragma GCC diagnostic ignored "-Wattributes"
#  endif
#  include <io.h>
#elif defined(__unix__) /* Last hope... */
#  include <unistd.h>
#else
#  error "Must include a header that declares write()"
#endif

#ifdef __GNUC__
#  define FUNC_NAME (__extension__ __PRETTY_FUNCTION__)
#  ifdef HAVE_EXECINFO_H
#    include <execinfo.h>
#  endif
#else
#  define FUNC_NAME (__func__)
#  define __attribute__(...)
#endif

#if (__GNUC__ >= 4)
#  define BSTR_PUBLIC  __attribute__((__visibility__("default")))
#  define BSTR_PRIVATE __attribute__((__visibility__("hidden")))
#  define PURE         __attribute__((__pure__))
#  define INLINE       extern inline __attribute__((__always_inline__, __gnu_inline__))
#else
#  define BSTR_PUBLIC
#  define BSTR_PRIVATE
#  define INLINE static inline
#  define PURE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
    typedef signed long long int ssize_t;
#endif

typedef unsigned int uint;


/*============================================================================*/

#ifdef HAVE_ERR
#  include <err.h>
#else
    __attribute__((__format__(gnu_printf, 2, 3)))
    static void _warn(bool print_err, const char *fmt, ...)
    {
            va_list ap;
            va_start(ap, fmt);
            char buf[8192];
            snprintf(buf, 8192, "%s\n", fmt);
            va_end(ap);

            /* if (print_err)
                    snprintf(buf, 8192, "%s: %s\n", fmt, strerror(errno));
            else
                    snprintf(buf, 8192, "%s\n", fmt); */
            /* vfprintf(stderr, buf, ap); */
            if (print_err)
                    perror(buf);
            else
                    fputs(buf, stderr);
#  ifdef _WIN32
            fflush(stderr);
#  endif
    }
#  define warn(...)       _warn(true, __VA_ARGS__)
#  define warnx(...)      _warn(false, __VA_ARGS__)
#  define err(EVAL, ...)  _warn(true, __VA_ARGS__), exit(EVAL)
#  define errx(EVAL, ...) _warn(false, __VA_ARGS__), exit(EVAL)
#endif

#ifdef _WIN32
__attribute__((__format__(__gnu_printf__, 2, 3)))
static inline int dprintf(int fd, char *fmt, ...)
{
	int fdx = _open_osfhandle(fd, 0);
	register int ret;
	FILE *fds;
	va_list ap;
	va_start(ap, fmt);
	fdx = dup(fdx);
	if((fds = fdopen(fdx, "w")) == NULL) return(-1);
	ret = vfprintf(fds, fmt, ap);
	fclose(fds);
	va_end(ap);
	return(ret);
}
#endif /* _WIN32 */


/* 
 * Because I'm that lazy.
 */
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))
#define psub(PTR1, PTR2) ((ptrdiff_t)(PTR1) - (ptrdiff_t)(PTR2))


/*============================================================================*/

/* #define X_ERROR */

#ifdef X_ERROR
#  define BSTR_INTERN_NULL_ACTION errx(1, "bstring -> runtime error (ptr)")
#  define BSTR_INTERN_INT_ACTION  errx(1, "bstring -> runtime error (int)")
#else
#  define BSTR_INTERN_INT_ACTION  return BSTR_ERR
#  define BSTR_INTERN_NULL_ACTION return NULL
#endif

#undef DEBUG

/* 
 * Debugging aids
 */
#ifdef HAVE_EXECINFO_H
#define FATAL_ERROR(...)                                                                   \
        do {                                                                               \
                void * arr[128];                                                           \
                size_t num = backtrace(arr, 128);                                          \
                char buf[8192];                                                            \
                snprintf(buf, 8192, __VA_ARGS__);                                          \
                                                                                           \
                warnx("Fatal error in func %s in bstrlib.c, line %d", FUNC_NAME, __LINE__); \
                fprintf(stderr, "%s\n", buf);                                              \
                fputs("STACKTRACE: \n", stderr);                                           \
                backtrace_symbols_fd(arr, num, 2);                                         \
                abort();                                                                   \
        } while (0)
#else
#define FATAL_ERROR(...) errx(1, __VA_ARGS__)
#endif

#define RUNTIME_ERROR() BSTR_INTERN_INT_ACTION
#define RETURN_NULL()   BSTR_INTERN_NULL_ACTION


/*============================================================================*/

#if 0
#define USE_XMALLOC

/* #ifdef USE_XMALLOC */
#if 0
#  define ALWAYS_INLINE __always_inline

__attribute__((nothrow, warn_unused_result, malloc, alloc_size(1)))
__attribute__((visibility("hidden")))
ALWAYS_INLINE void *
BSTR_malloc(const size_t size)
{
        void *tmp = malloc(size);
        if (tmp == NULL)
                err(100, "Malloc call failed - attempted %zu bytes", size);
        return tmp;
}

__attribute__((nothrow, warn_unused_result, malloc, alloc_size(1, 2)))
__attribute__((visibility("hidden")))
ALWAYS_INLINE  void *
BSTR_calloc(const size_t num, const size_t size)
{
        void *tmp = calloc(num, size);
        if (tmp == NULL)
                err(101, "Calloc call failed - attempted %zu bytes", size);
        return tmp;
}

#  define malloc BSTR_malloc
#  define calloc BSTR_calloc


#  ifdef HAVE_VASPRINTF
__attribute__((format(__gnu_printf__, 2, 0), nothrow, warn_unused_result, nonnull(1, 2)))
__attribute__((visibility("hidden")))
ALWAYS_INLINE int
BSTR_xvasprintf(char **ptr, const char *const restrict fmt, va_list va)
{
        int ret = vasprintf(ptr, fmt, va);
        if (ret == (-1)) {
                warn("Asprintf failed to allocate memory");
                abort();
        }
        return ret;
}

#  define xvasprintf BSTR_xvasprintf
#  endif

__attribute__((nothrow, warn_unused_result, alloc_size(2)))
__attribute__((visibility("hidden")))
ALWAYS_INLINE void *
BSTR_realloc(void *ptr, const size_t size)
{
        void *tmp = realloc(ptr, size);
        if (tmp == NULL)
                err(102, "Realloc call failed - attempted %zu bytes", size);
        return tmp;
}

#define realloc BSTR_realloc

#if defined(HAVE_REALLOCARRAY) && !defined(WITH_JEMALLOC)
__attribute__((nothrow, warn_unused_result, alloc_size(2, 3)))
__attribute__((visibility("hidden")))
ALWAYS_INLINE void *
BSTR_reallocarray(void *ptr, size_t num, size_t size)
{
        void *tmp = reallocarray(ptr, num, size);
        if (tmp == NULL)
                err(103, "Realloc call failed - attempted %zu bytes", size);
        return tmp;
}

#  define reallocarray              BSTR_reallocarray
#  define nmalloc(NUM_, SIZ_)        reallocarray(NULL, (NUM_), (SIZ_))
#  define nrealloc(PTR_, NUM_, SIZ_) reallocarray((PTR_), (NUM_), (SIZ_))
#else
#  define nmalloc(NUM_, SIZ_)        malloc(((size_t)(NUM_)) * ((size_t)(SIZ_)))
#  define nrealloc(PTR_, NUM_, SIZ_) realloc((PTR_), ((size_t)(NUM_)) * ((size_t)(SIZ_)))
#endif
#else
#  define malloc  malloc
#  define calloc  calloc
#  define realloc realloc

#  define nmalloc(NUM_, SIZ_)        malloc(((size_t)(NUM_)) * ((size_t)(SIZ_)))
#  define nrealloc(PTR_, NUM_, SIZ_) realloc((PTR_), ((size_t)(NUM_)) * ((size_t)(SIZ_)))

#  if defined(HAVE_REALLOCARRAY) && !defined(WITH_JEMALLOC)
#    define reallocarray reallocarray
#  else
#    define reallocarray nrealloc
#  endif
#  ifdef HAVE_VASPRINTF
#    define xvasprintf vasprintf
#  endif
#endif
#define free(PTR) free(PTR)
#endif

#define nmalloc(NUM_, SIZ_)        malloc(((size_t)(NUM_)) * ((size_t)(SIZ_)))
#define nrealloc(PTR_, NUM_, SIZ_) realloc((PTR_), ((size_t)(NUM_)) * ((size_t)(SIZ_)))
#define nalloca(NUM_, SIZ_) alloca(((size_t)(NUM_)) * ((size_t)(SIZ_)))

#if 0
/* 
 * These make the code neater and save the programmer from having to check for
 * NULL returns at the cost of crashing the program at any allocation failure. I
 * see this as a net benefit. Not many programs can meaninfully continue when no
 * memory is left on the system (a very rare occurance anyway).
 */
#ifdef USE_XMALLOC
__attribute__((__malloc__, __always_inline__))
static inline void *
malloc(const size_t size)
{
        void *tmp = malloc(size);
        if (tmp == NULL)
                FATAL_ERROR("Malloc call failed - attempted %zu bytes", size);
        return tmp;
}

__attribute__((__always_inline__))
static inline void *
calloc(const int num, const size_t size)
{
        void *tmp = calloc(num, size);
        if (tmp == NULL)
                FATAL_ERROR("Calloc call failed - attempted %zu bytes", size);
        return tmp;
}
#else
#  define malloc malloc
#  define calloc calloc
#endif

__attribute__((__always_inline__))
static inline void *
realloc(void *ptr, size_t size)
{
        void *tmp = realloc(ptr, size);
        if (!tmp)
                FATAL_ERROR("Realloc call failed - attempted %zu bytes", size);
        return tmp; 
}

#ifdef HAVE_VASPRINTF
#  ifdef USE_XMALLOC
__attribute__((__format__(__gnu_printf__, 2, 0), __always_inline__))
static inline int
xvasprintf(char **ptr, const char *const restrict fmt, va_list va)
{
        int ret = vasprintf(ptr, fmt, va);
        if (ret == (-1)) {
                warn("Asprintf failed to allocate memory");
                abort();
        }
        return ret;
}
#  else
#    define xvasprintf vasprintf
#  endif
#endif

#define free free
#endif

/*============================================================================*/


#define BS_BUFF_SZ (1024)

struct gen_b_list {
        bstring *bstr;
        b_list *bl;
};


/*============================================================================*/


/* bstrlib.c */
__attribute__((__const__)) BSTR_PRIVATE uint snapUpSize(uint i);


/*============================================================================*/


/*
 * There were some pretty horrifying if statements in the original
 * implementation. I've tried to make them at least somewhat saner with these
 * macros that at least explain what the checks are trying to accomplish.
 */
#define IS_NULL(BSTR)   (!(BSTR) || !(BSTR)->data)
#define INVALID(BSTR)   (IS_NULL(BSTR))
#define IS_CLONE(BSTR)  ((BSTR)->flags & BSTR_CLONE)
#define NO_WRITE(BSTR)  (!((BSTR)->flags & BSTR_WRITE_ALLOWED) || IS_CLONE(BSTR))
#define NO_ALLOC(BSTR)  (!((BSTR)->flags & BSTR_DATA_FREEABLE))
#define IS_STATIC(BSTR) (NO_WRITE(BSTR) && NO_ALLOC(BSTR))

#ifdef __cplusplus
}
#endif

#endif /* private.h */
