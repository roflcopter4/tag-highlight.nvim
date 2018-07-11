#ifndef PRIVATE_H
#define PRIVATE_H

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
/* These warnings from MSVC++ are totally pointless. */
#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include "bstrlib.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <execinfo.h>

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#elif defined(_WIN32)
#  include <io.h>
#elif defined(__unix__) /* Last hope... */
#  include <unistd.h>
#else
#  error "Must include a header that declares write()"
#endif

#ifndef __GNUC__
#  define __attribute__((...))
#endif

#ifdef HAVE_ERR
#  include <err.h>
#else
    __attribute__((__format__(printf, 2, 3)))
    static void _warn(bool print_err, const char *fmt, ...)
    {
            va_list ap;
            va_start(ap, fmt);
            char buf[8192];
            if (print_err)
                    snprintf(buf, 8192, "%s: %s\n", fmt, strerror(errno));
            else
                    snprintf(buf, 8192, "%s\n", fmt);
            vfprintf(stderr, buf, ap);
            va_end(ap);
    }
#  define warn(...)       _warn(true, __VA_ARGS__)
#  define warnx(...)      _warn(false, __VA_ARGS__)
#  define err(EVAL, ...)  _warn(true, __VA_ARGS__), exit(EVAL)
#  define errx(EVAL, ...) _warn(false, __VA_ARGS__), exit(EVAL)
#endif


/* 
 * Because I'm that lazy.
 */
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))


/*
 * Just a length safe wrapper for memmove.
 */
#define b_BlockCopy(D, buf, blen)                    \
        do {                                         \
                if ((blen) > 0)                      \
                        memmove((D), (buf), (blen)); \
        } while (0);


#ifdef __GNUC__
#  define FUNC_NAME (__extension__ __PRETTY_FUNCTION__)
#else
#  define FUNC_NAME (__func__)
#endif


/* #define DEBUG */

/* 
 * Debugging aids
 */
#if defined(DEBUG)
#  define RUNTIME_ERROR()                                                  \
        do {                                                               \
                void *arr[128];                                            \
                size_t num     = backtrace(arr, 128);                      \
                char **strings = backtrace_symbols(arr, num);              \
                                                                           \
                warnx("Runtime error in func %s in bstrlib.c, line %d\n"   \
                      "STACKTRACE: ", FUNC_NAME, __LINE__);                \
                for (unsigned i = 0; i < num; ++i)                         \
                        fprintf(stderr, "  -  %s\n", strings[i]);          \
                                                                           \
                free(strings);                                             \
                return BSTR_ERR;                                           \
        } while (0)
#  define RETURN_NULL()                                                  \
        do {                                                             \
                void *arr[128];                                          \
                size_t num     = backtrace(arr, 128);                    \
                char **strings = backtrace_symbols(arr, num);            \
                                                                         \
                warnx("Null return in func %s in bstrlib.c, line %d\n"   \
                      "STACKTRACE: ", FUNC_NAME, __LINE__);              \
                for (unsigned i = 0; i < num; ++i)                       \
                        fprintf(stderr, "  -  %s\n", strings[i]);        \
                                                                         \
                free(strings);                                           \
                return NULL;                                             \
        } while (0)
#  define ALLOCATION_ERROR(RETVAL)                                         \
        do {                                                               \
                warnx("Allocation error in func %s in bstrlib.c, line %d", \
                      FUNC_NAME,  __LINE__);                               \
                return (RETVAL);                                           \
        } while (0)
#elif defined(X_ERROR)
#  define RUNTIME_ERROR() \
        errx(1, "Runtime error at file %s, line %d", __FILE__, __LINE__)
#  define RETURN_NULL() \
        errx(1, "Null return at file %s, line %d", __FILE__, __LINE__)
#  define ALLOCATION_ERROR(RETVAL) \
        err(1, "Allocation error at file %s, line %d", __FILE__, __LINE__);
#else
#  define RUNTIME_ERROR()          return BSTR_ERR
#  define RETURN_NULL()            return NULL
/* #  define ALLOCATION_ERROR(RETVAL) return (RETVAL) */
#  define ALLOCATION_ERROR(RETVAL) abort();
#endif


#if defined(__GNUC__) && !defined(HAVE_VASPRINTF)
#  define HAVE_VASPRINTF
#endif
#define USE_XMALLOC

/* 
 * These make the code neater and save the programmer from having to check for
 * NULL returns at the cost of crashing the program at any allocation failure. I
 * see this as a net benefit. Not many programs can meaninfully continue when no
 * memory is left on the system (a very rare occurance anyway).
 */
#ifdef USE_XMALLOC
static inline void *
xmalloc(size_t size)
{
        void *tmp = malloc(size);
        if (!tmp) {
                warn("Failed to allocate %zu bytes", size);
                abort();
        }
        return tmp; 
}
#else
#  define xmalloc malloc
#endif

static inline void *
xrealloc(void *ptr, size_t size)
{
        void *tmp = realloc(ptr, size);
        if (!tmp) {
                warn("Failed to reallocate %zu bytes", size);
                abort();
        }
        return tmp; 
}

#ifdef HAVE_VASPRINTF
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
#endif


#endif /* private.h */
