#ifndef SRC_UTIL_H
#define SRC_UTIL_H
#pragma once

#if !defined THL_COMMON_H_
#  error "Must include Common.h first."
#endif
#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif
/***************************************************************************************/

/* Attribute aliases and junk like MIN, MAX, NOP, etc */

//#if !defined __GNUC__ && !defined __clang__ && !defined __attribute__
//#  define __attribute__(a)
//#endif
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
#  define MAXOF(IA, IB)  __extension__({auto_type ia=(IA); auto_type ib=(IB); (ia>ib)?ia:ib;})
#  define MINOF(IA, IB)  __extension__({auto_type ia=(IA); auto_type ib=(IB); (ia<ib)?ia:ib;})
#  define MODULO(IA, IB) __extension__({auto_type ia=(IA); auto_type ib=(IB); (ia % ib + ib) % ib;})
#else
#  ifdef _MSC_VER
#    define FUNC_NAME __FUNCTION__
#  else
#    define FUNC_NAME __func__
#  endif
#  define MAXOF(iA, iB)  (((iA) > (iB)) ? (iA) : (iB))
#  define MINOF(iA, iB)  (((iA) < (iB)) ? (iA) : (iB))
#  define MODULO(iA, iB) (((iA) % (iB) + (iB)) % (iB))
#endif

#define dump_alignof_help(t, ts, fn) __attribute__((__constructor__)) static void fn (void) { eprintf(ts " alignment is %zu\n", alignof(t)); }
#define dump_alignof(t) dump_alignof_help(t, #t, P99_UNIQ())
#define aligned_alloc_for(t) aligned_alloc(alignof(t), sizeof(t))

/*===========================================================================*/
/*
 * Timer structure
 */

struct timer {
        struct timespec tv1;
        struct timespec tv2;
};

#define STRUCT_TIMER_INITIALIZER {{0, 0}, {0, 0}}

#define USEC2SECOND (1000000LLU) /* 1,000,000 - one million */
#define NSEC2SECOND (1000000000LLU) /* 1,000,000,000 - one billion */

#define TDIFF(STV1, STV2)                                                    \
        (((double)((STV2).tv_usec - (STV1).tv_usec) / (double)USEC2SECOND) + \
         ((double)((STV2).tv_sec - (STV1).tv_sec)))

/* Taken from glibc */
#define TIMESPEC_ADD(a, b, result)                           \
      do {                                                   \
            (result)->tv_sec  = (a)->tv_sec + (b)->tv_sec;   \
            (result)->tv_nsec = (a)->tv_nsec + (b)->tv_nsec; \
            if ((result)->tv_nsec >= UINT64_C(1000000000)) { \
                  ++(result)->tv_sec;                        \
                  (result)->tv_nsec -= UINT64_C(1000000000); \
            }                                                \
      } while (0)

#define TIMESPEC_SUB(a, b, result)                           \
      do {                                                   \
            (result)->tv_sec  = (a)->tv_sec - (b)->tv_sec;   \
            (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
            if ((result)->tv_nsec < 0) {                     \
                  --(result)->tv_sec;                        \
                  (result)->tv_nsec += UINT64_C(1000000000); \
            }                                                \
      } while (0)

/* This mess is my own */
#define TIMESPEC2DOUBLE(STV) \
      ((double)((((double)(STV)->tv_sec)) + (((double)(STV)->tv_nsec) / (double)NSEC2SECOND)))

#define TIMER_START(T_) \
        ((void)timespec_get(&(T_)->tv1, TIME_UTC))

#define TIMER_START_BAR(T_)                          \
        do {                                         \
                TIMER_START(T_);                     \
                eprintf("----------------------\n"); \
        } while (0)

#define TIMER_REPORT(T_, FMT_, ...)                                        \
        do {                                                               \
                struct timespec tmp_;                                      \
                (void)timespec_get(&(T_)->tv2, TIME_UTC);                  \
                TIMESPEC_SUB(&(T_)->tv2, &(T_)->tv1, &tmp_);               \
                nvim_printf("Time for (" FMT_ "):  %.9fs\n", ##__VA_ARGS__,   \
                        /*(int)(65 - sizeof(FMT_)),*/ \
                    TIMESPEC2DOUBLE(&tmp_)); \
        } while (0)

#define TIMER_REPORT_RESTART(T, ...) do { TIMER_REPORT(T, __VA_ARGS__); TIMER_START(T); } while (0)

#define TIMESPEC_FROM_DOUBLE(FLT)                                          \
      { (uintmax_t)((double)(FLT)),                                        \
            (uintmax_t)(((double)((double)(FLT) -                          \
                                  (double)((uintmax_t)((double)(FLT))))) * \
                        (double)NSEC2SECOND) }

#define TIMESPEC_FROM_SECOND_FRACTION(seconds, numerator, denominator)               \
      {                                                                              \
            (uintmax_t)(seconds),                                                    \
            (uintmax_t)((denominator) == 0 ? UINTMAX_C(0)                            \
                                           : (((uintmax_t)(numerator)*NSEC2SECOND) / \
                                              (uintmax_t)(denominator)))             \
      }

#define MKTIMESPEC(s, n) ((struct timespec[1]){{s, n}})
#define NANOSLEEP(s, n)  nanosleep(MKTIMESPEC((s), (n)), NULL)


/*===========================================================================*/
/* Generic Utility Functions */

#ifndef __always_inline
#  define __always_inline extern __inline__ __attribute__((__always_inline__))
#endif
#ifdef noreturn
# error "I give up"
#endif

#ifndef NOP
#  define NOP ((void)0)
#endif

#define xatoi(STR_)        xatoi__((STR_), false)
#define s_xatoi(STR_)      xatoi__((STR_), true)
#define free_all(...)      free_all__(__VA_ARGS__, NULL)

#define ARRSIZ(ARR)        (sizeof(ARR) / sizeof((ARR)[0]))
#define LSLEN(STR)         ((size_t)(sizeof(STR) - 1llu))
#define PSUB(PTR1, PTR2)   ((ptrdiff_t)(PTR1) - (ptrdiff_t)(PTR2))
#define SLS(STR)           ("" STR ""), LSLEN(STR)
#define STRINGIFY_HLP(...) #__VA_ARGS__
#define STRINGIFY(...)     STRINGIFY_HLP(__VA_ARGS__)

/**
 * Silly convenience macro for assertions that should _always_ be checked regardless of
 * release type. Saves an if statement.
 */
#define ALWAYS_ASSERT(COND)                                                                           \
        (P99_UNLIKELY(COND)                                                                           \
                  ? NOP                                                                               \
                  : errx(1, "ERROR: Condition \"%s\" failed at (FILE: `%s', LINE: `%d', FUNC: `%s')", \
                         STRINGIFY(COND), __FILE__, __LINE__, FUNC_NAME))

#define ASSERT(COND, ...)   (P99_UNLIKELY(COND) ? NOP : err(50,  __VA_ARGS__))
#define ASSERTX(COND, ...)  (P99_UNLIKELY(COND) ? NOP : errx(50, __VA_ARGS__))

#define err(EVAL, ...)  err_ ((EVAL), true,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define errx(EVAL, ...) err_ ((EVAL), false,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define warn(...)       warn_(true,   true,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define warnx(...)      warn_(false,  true,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define warnd(...)      warn_(false,  false,  __FILE__, __LINE__, __func__, __VA_ARGS__)

extern void warn_(bool print_err, bool force, char const *restrict file, int line, char const *restrict func, char const *restrict fmt, ...) __aFMT(6, 7);
NORETURN extern void err_(int status, bool print_err, char const *restrict file, int line, char const *restrict func, char const *restrict fmt, ...) __aFMT(6, 7);

extern void     free_all__    (void *ptr, ...);
extern int64_t  xatoi__       (char const *str, bool strict);
extern unsigned find_num_cpus (void);
ND extern FILE *fopen_fmt     (char const *restrict mode, char const *restrict fmt, ...) __aNN(1, 2) __aFMT(2, 3);
ND extern FILE *safe_fopen    (char const *filename, char const *mode) __aNN(1, 2);
ND extern FILE *safe_fopen_fmt(char const *mode, char const *fmt, ...) __aNN(1, 2) __aFMT(2,3);
ND extern int   safe_open     (char const *filename, int flags, int mode) ;
ND extern int   safe_open_fmt (int flags, int mode, char const *fmt, ...) __aFMT(3, 4);
extern void     fd_set_open_flag(int fd, int flag);

#if 0 && defined DEBUG
extern int clock_nanosleep_for_(intmax_t seconds, intmax_t nanoseconds, char const *file, int line, char const *fn);
#  define clock_nanosleep_for(s, n) clock_nanosleep_for_((s), (n), __FILE__, __LINE__, __func__)
#else
extern int clock_nanosleep_for(intmax_t seconds, intmax_t nanoseconds);
#endif

/*
 * Sleep for (s + (i/d)) seconds.
 */
#define NANOSLEEP_FOR_SECOND_FRACTION(s, i, d) \
        clock_nanosleep_for((uintmax_t)(s), (uintmax_t)((NSEC2SECOND * (uintmax_t)(i)) / ((uintmax_t)(d))))

extern bstring *get_command_output(char const *command, char *const *argv, bstring *input, int *status) __aWUR __aNN(1, 2);
#ifdef _WIN32
extern int           win32_start_process_with_pipe(char const *exe, char *argv, HANDLE pipehandles[2], PROCESS_INFORMATION *pi);
extern bstring *     _win32_get_command_output(char *argv, bstring const *input, int *status);
NORETURN extern void win32_error_exit(int status, char const *msg, DWORD dw);

# define WIN32_ERROR_EXIT_HELPER(VAR, ...)    \
      (__extension__({                        \
            char VAR[2048];                   \
            snprintf(VAR, 2048, __VA_ARGS__); \
            VAR;                              \
      }))
# define WIN32_ERROR_EXIT(ST, ...) \
      win32_error_exit((ST), WIN32_ERROR_EXIT_HELPER(P99_UNIQ(), __VA_ARGS__), GetLastError())
#endif

/*=====================================================================================*/

#ifndef _Notnull_
# define _Notnull_
#endif
#ifndef _Maybenull_
# define _Maybenull_
#endif

extern char *
braindead_tempname(_Notnull_   char       *restrict buf,
                   _Notnull_   char const *restrict dir,
                   _Maybenull_ char const *restrict prefix,
                   _Maybenull_ char const *restrict suffix)
    __attribute__((__nonnull__(1, 2)));

extern uint32_t cxx_random_device_get_random_val(void) __aWUR;
extern uint32_t cxx_random_engine_get_random_val(void) __aWUR;

/* buf should be at least 320 bytes long */
extern char *util_format_int_to_binary(char *buf, uintmax_t val);

/*=====================================================================================*/

#ifdef __GNUC__
#  if !__has_builtin(__builtin_bswap16) || !__has_builtin(__builtin_bswap32) || !__has_builtin(__builtin_bswap64)
#    error "Compiler does not support __builtin_bswapXX. Get a newer version."
#  endif
#  define MY_BSWAP_16(n) __builtin_bswap16(n)
#  define MY_BSWAP_32(n) __builtin_bswap32(n)
#  define MY_BSWAP_64(n) __builtin_bswap64(n)
#elif defined _MSC_VER
#  define MY_BSWAP_16(n) _byteswap_ushort(n)
#  define MY_BSWAP_32(n) _byteswap_ulong(n)
#  define MY_BSWAP_64(n) _byteswap_uint64(n)
#else
#  error "Can't be bothered to support whatever whacky compiler you're using."
#endif

/***************************************************************************************/
#ifdef __cplusplus
}
#endif
#endif /* util.h */
