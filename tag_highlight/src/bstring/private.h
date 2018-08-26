#ifndef BSTRLIB_PRIVATE_H
#define BSTRLIB_PRIVATE_H

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#ifdef HAVE_CONFIG_H
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

#include "bstring.h"

//#include "unused.h"

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
#elif defined(_WIN32)
#  define PATH_MAX _MAX_PATH
#  include <io.h>
#elif defined(__unix__) /* Last hope... */
#  include <unistd.h>
#else
#  error "Must include a header that declares write()"
#endif

#ifdef __GNUC__
#  define FUNC_NAME (__extension__ __PRETTY_FUNCTION__)
#  if !defined(_WIN32) && !defined(__cygwin__)
#    include <execinfo.h>
#  endif
#else
#  define FUNC_NAME (__func__)
#  define __attribute__(...)
#endif

#if (__GNUC__ >= 4)
#  define BSTR_PUBLIC  __attribute__((__visibility__("default")))
#  define BSTR_PRIVATE __attribute__((__visibility__("hidden")))
#  define INLINE       __attribute__((__always_inline__)) static inline
#  define PURE         __attribute__((__pure__))
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
    __attribute__((__format__(printf, 2, 3)))
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


/* 
 * Because I'm that lazy.
 */
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))
#define psub(PTR1, PTR2) ((ptrdiff_t)(PTR1) - (ptrdiff_t)(PTR2))


/*============================================================================*/

/* #define X_ERROR */

#ifdef X_ERROR
#  define BSTR_INTERN_NULL_ACTION abort();
#  define BSTR_INTERN_INT_ACTION abort();
#else
#  define BSTR_INTERN_NULL_ACTION return NULL;
#  define BSTR_INTERN_INT_ACTION  return BSTR_ERR;
#endif

#undef DEBUG

/* 
 * Debugging aids
 */
/* #if defined(DEBUG)
#  ifdef _WIN32
    #define FATAL_ERROR(...)        \
        do {                        \
                warnx(__VA_ARGS__); \
                abort();            \
        } while (0)
#    define RUNTIME_ERROR()                                               \
        do {                                                              \
                warnx("Runtime error in func %s in bstrlib.c, line %d\n", \
                      FUNC_NAME, __LINE__);                               \
                return BSTR_ERR;                                          \
        } while (0)
#    define RETURN_NULL()                                               \
        do {                                                            \
                warnx("Null return in func %s in bstrlib.c, line %d\n", \
                      FUNC_NAME, __LINE__);                             \
                return NULL;                                            \
        } while (0)
#  else */
#if 0
#    define FATAL_ERROR(...)                                                                \
        do {                                                                                \
                void * arr[128];                                                            \
                size_t num     = backtrace(arr, 128);                                       \
                char **strings = backtrace_symbols(arr, num);                               \
                char   buf[8192];                                                           \
                snprintf(buf, 8192, __VA_ARGS__);                                           \
                                                                                            \
                warn("Fatal error in func %s in bstrlib.c, line %d", FUNC_NAME, __LINE__);  \
                fprintf(stderr, "%s\n", buf);                                               \
                fputs("STACKTRACE: \n", stderr);                                            \
                    for (unsigned i_ = 0; i_ < num; ++i_)                                   \
                        fprintf(stderr, "  -  %s\n", strings[i_]);                          \
                                                                                            \
                free(strings);                                                              \
                abort();                                                                    \
        } while (0)
#endif
#    define FATAL_ERROR(...)                                                                \
        do {                                                                                \
                void * arr[128];                                                            \
                size_t num     = backtrace(arr, 128);                                       \
                /* char **strings = backtrace_symbols(arr, num); */                               \
                char   buf[8192];                                                           \
                snprintf(buf, 8192, __VA_ARGS__);                                           \
                                                                                            \
                warn("Fatal error in func %s in bstrlib.c, line %d", FUNC_NAME, __LINE__);  \
                fprintf(stderr, "%s\n", buf);                                               \
                fputs("STACKTRACE: \n", stderr);                                            \
                backtrace_symbols_fd(arr, num, 2); \
                    /* for (unsigned i_ = 0; i_ < num; ++i_) */                                   \
                        /* fprintf(stderr, "  -  %s\n", strings[i_]); */                          \
                                                                                            \
                /* free(strings); */                                                              \
                abort();                                                                    \
        } while (0)
#if 0
#    define RUNTIME_ERROR()                                                \
        do {                                                               \
                void * arr[128];                                           \
                size_t num     = backtrace(arr, 128);                      \
                char **strings = backtrace_symbols(arr, num);              \
                                                                           \
                warnx("Runtime error in func %s in bstrlib.c, line %d\n"   \
                      "STACKTRACE: ",                                      \
                      FUNC_NAME, __LINE__);                                \
                for (unsigned i_ = 0; i_ < num; ++i_)                      \
                        fprintf(stderr, "  -  %s\n", strings[i_]);         \
                                                                           \
                free(strings);                                             \
                BSTR_INTERN_INT_ACTION                                     \
        } while (0)
#    define RETURN_NULL()                                                \
        do {                                                             \
                void * arr[128];                                         \
                size_t num     = backtrace(arr, 128);                    \
                char **strings = backtrace_symbols(arr, num);            \
                                                                         \
                warnx("Null return in func %s in bstrlib.c, line %d\n"   \
                      "STACKTRACE: ",                                    \
                      FUNC_NAME, __LINE__);                              \
                for (unsigned i_ = 0; i_ < num; ++i_)                    \
                        fprintf(stderr, "  -  %s\n", strings[i_]);       \
                                                                         \
                free(strings);                                           \
                BSTR_INTERN_NULL_ACTION                                  \
        } while (0)
#endif

/* #  endif
#  define ALLOCATION_ERROR(RETVAL)                                         \
        do {                                                               \
                warnx("Allocation error in func %s in bstrlib.c, line %d", \
                      FUNC_NAME,  __LINE__);                               \
                return (RETVAL);                                           \
        } while (0)
#elif defined(X_ERROR)
#  define FATAL_ERROR(...) \
        errx(1, __VA_ARGS__)
#  define RUNTIME_ERROR() \
        errx(1, "Runtime error at file %s, line %d", __FILE__, __LINE__)
#  define RETURN_NULL() \
        errx(1, "Null return at file %s, line %d", __FILE__, __LINE__)
#  define ALLOCATION_ERROR(RETVAL) \
        err(1, "Allocation error at file %s, line %d", __FILE__, __LINE__)
#else
#  define FATAL_ERROR(...)         errx(1, __VA_ARGS__)
#  define RUNTIME_ERROR()          return BSTR_ERR
#  define RETURN_NULL()            return NULL
#  define ALLOCATION_ERROR(RETVAL) abort();
#endif */

#  define RUNTIME_ERROR()          return BSTR_ERR
#  define RETURN_NULL()            return NULL


/*============================================================================*/


//#if defined(__GNUC__) && !defined(HAVE_VASPRINTF)
//#  define HAVE_VASPRINTF
//#endif
#define USE_XMALLOC
/* #include <jemalloc/jemalloc.h> */

/* 
 * These make the code neater and save the programmer from having to check for
 * NULL returns at the cost of crashing the program at any allocation failure. I
 * see this as a net benefit. Not many programs can meaninfully continue when no
 * memory is left on the system (a very rare occurance anyway).
 */
#ifdef USE_XMALLOC
static inline void *
xmalloc(const size_t size)
{
        void *tmp = malloc(size);
        if (tmp == NULL)
                FATAL_ERROR("Malloc call failed - attempted %zu bytes", size);
                /* err(100, "Malloc call failed - attempted %zu bytes", size); */
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
                FATAL_ERROR("Realloc call failed - attempted %zu bytes", size);
                /* warn("Failed to reallocate %zu bytes", size);
                abort(); */
        }
        return tmp; 
}

#ifdef HAVE_VASPRINTF
#  ifdef USE_XMALLOC
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

/*============================================================================*/


#define BS_BUFF_SZ (1024)

#ifndef BSTRLIB_AGGRESSIVE_MEMORY_FOR_SPEED_TRADEOFF
#  define LONG_LOG_BITS_QTY (3)
#  define LONG_BITS_QTY (1 << LONG_LOG_BITS_QTY)
#  define LONG_TYPE uchar
#  define CFCLEN ((1 << CHAR_BIT) / LONG_BITS_QTY)
   struct char_field {
           LONG_TYPE content[CFCLEN];
   };
#  define testInCharField(cf, c)                  \
       ((cf)->content[(c) >> LONG_LOG_BITS_QTY] & \
        ((1ll) << ((c) & (LONG_BITS_QTY - 1))))

#  define setInCharField(cf, idx)                                  \
       do {                                                        \
               int c = (uint)(idx);                                \
               (cf)->content[c >> LONG_LOG_BITS_QTY] |=            \
                   (LONG_TYPE)(1llu << (c & (LONG_BITS_QTY - 1))); \
       } while (0)
#else
#  define CFCLEN (1 << CHAR_BIT)
   struct charField {
           uchar content[CFCLEN];
   };
#  define testInCharField(cf, c)  ((cf)->content[(uchar)(c)])
#  define setInCharField(cf, idx) (cf)->content[(uint)(idx)] = ~0
#endif


struct gen_b_list {
        bstring *bstr;
        b_list *bl;
};


/*============================================================================*/


/* bstrlib.c */
BSTR_PRIVATE uint snapUpSize(uint i);

/* char_fields.c */
BSTR_PRIVATE int build_char_field(struct char_field *cf, const bstring *bstr);
BSTR_PRIVATE void invert_char_field(struct char_field *cf);
BSTR_PRIVATE int b_inchrCF(const uchar *data, const uint len, const uint pos, const struct char_field *cf);
BSTR_PRIVATE int b_inchrrCF(const uchar *data, const uint pos, const struct char_field *cf);

/* b_list.c */
BSTR_PRIVATE int b_scb(void *parm, const uint ofs, const uint len);


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
