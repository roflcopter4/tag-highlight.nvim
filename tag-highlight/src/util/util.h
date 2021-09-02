#ifndef SRC_UTIL_H
#define SRC_UTIL_H

#if !defined THL_COMMON_H_
#  error "Must include Common.h first."
#endif
#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif
/*======================================================================================*/

/* 
 * Timer structure
 */
#if defined __MINGW__

struct timer {
        struct timespec tv1, tv2;
};

#  define TIMER_START(T_)        ((void)0)
#  define TIMER_START_BAR(T_)    ((void)0)
#  define TIMER_REPORT(T_, MSG_) ((void)0)

#elif defined HAVE_CLOCK_GETTIME

struct timer {
      alignas(32)
        struct timespec tv1;
      alignas(16)
        struct timespec tv2;
};

#  define TIMER_START(T_) \
        (clock_gettime(CLOCK_REALTIME, &(T_)->tv1))
#  define TIMER_START_BAR(T_)                    \
        do {                                     \
                TIMER_START(T_);                 \
                shout("----------------------"); \
        } while (0)
#  define TIMER_REPORT(T_, MSG_)                                                      \
        do {                                                                          \
                clock_gettime(CLOCK_REALTIME, &(T_)->tv2);                            \
                echo("Time for \"%s\": % *.9fs", (MSG_),                              \
                     (int)(35 - sizeof(MSG_)), TIMESPECDIFF(&(T_)->tv1, &(T_)->tv2)); \
        } while (0)

#else
        
struct timer {
        struct timeval tv1, tv2;
};

#  define TIMER_START(T_) (gettimeofday(&(T_)->tv1, NULL))
#  define TIMER_START_BAR(T_)                    \
        do {                                     \
                gettimeofday(&(T_)->tv1, NULL);  \
                echo("----------------------");  \
        } while (0)
#  define TIMER_REPORT(T_, MSG_)                                              \
        do {                                                                  \
                gettimeofday(&(T_)->tv2, NULL);                               \
                echo("Time for \"%s\": % *fs", (MSG_),                        \
                      (int)(30 - sizeof(MSG_)), TDIFF((T_)->tv1, (T_)->tv2)); \
        } while (0)
#endif

#if defined __MINGW__
#  define TIMER_REPORT_RESTART(T, MSG) ((void)0)
#else
#  define TIMER_REPORT_RESTART(T, MSG) do { TIMER_REPORT(T, MSG); TIMER_START(T); } while (0)
#endif

#define STRUCT_TIMER_INITIALIZER {{0, 0}, {0, 0}}

#define STRDUP(STR)                                                     \
        __extension__({                                                 \
                static const char strng_[]   = ("" STR "");             \
                char *            strng_cpy_ = malloc(sizeof(strng_));  \
                memcpy(strng_cpy_, strng_, sizeof(strng_));             \
                strng_cpy_;                                             \
        })

/*======================================================================================*/

#define USEC2SECOND (1000000LLU) /* 1,000,000 - one million */
#define NSEC2SECOND (1000000000LLU) /* 1,000,000,000 - one billion */

#if 0
#define MKTIMESPEC(FLT) ((struct timespec[]){{ \
          (int64_t)(FLT),                    \
          (int64_t)(((double)((FLT) - (double)((int64_t)(FLT)))) * (double)NSEC2SECOND)}})
#endif

#define TDIFF(STV1, STV2)                                            \
        (((double)((STV2).tv_usec - (STV1).tv_usec) / (double)USEC2SECOND) + \
         ((double)((STV2).tv_sec - (STV1).tv_sec)))

/* Taken from glibc */
#define TIMESPEC_ADD(a, b, result)                               \
        do {                                                     \
                (result)->tv_sec  = (a)->tv_sec + (b)->tv_sec;   \
                (result)->tv_nsec = (a)->tv_nsec + (b)->tv_nsec; \
                if ((result)->tv_nsec >= 1000000000) {           \
                        ++(result)->tv_sec;                      \
                        (result)->tv_nsec -= 1000000000;         \
                }                                                \
        } while (0)

#define TIMESPECDIFF(STV1, STV2)                                         \
        (((double)((STV2)->tv_nsec - (STV1)->tv_nsec) / (double)NSEC2SECOND) + \
         ((double)((STV2)->tv_sec - (STV1)->tv_sec)))

#define TIMESPEC2DOUBLE(STV) \
        ((double)((((double)(STV)->tv_sec)) + (((double)(STV)->tv_nsec) / (double)NSEC2SECOND)))

#define DOUBLE2TIMESPEC(FLT) ((struct timespec[]){{ \
          (int64_t)(FLT),                           \
          (int64_t)(((double)((FLT) - (double)((int64_t)(FLT)))) * (double)NSEC2SECOND)}})
#if 0
#define DOUBLE2TIMESPEC(STV, FLT)            \
        ((STV)->tv_sec  = (int64_t)(FLT), \
         (STV)->tv_nsec = (int64_t)(((double)((FLT) - (double)((int64_t)(FLT)))) * (double)NSEC2SECOND))
#endif

#define MKTIMESPEC(s, n) ((struct timespec[]){{s, n}})
#define NANOSLEEP(s, n) nanosleep(MKTIMESPEC((s), (n)), NULL)

/*===========================================================================*/
/* Generic Utility Functions */

#ifndef __always_inline
#  define __always_inline extern __inline__ __attribute__((__always_inline__))
/* #  undef __always_inline */
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

#define ASSERT(COND, ...)   ((!!(COND)) ? NOP : err(50,  __VA_ARGS__))
#define ASSERTX(COND, ...)  ((!!(COND)) ? NOP : errx(50, __VA_ARGS__))
#define DIE_UNLESS(COND)    ((!!(COND)) ? NOP : err(55, "%s", (#COND)))
#define DIE_UNLESSX(COND)   ((!!(COND)) ? NOP : errx(55, "%s", (#COND)))

#define ALWAYS_ASSERT(COND)                                                                           \
        (!!(COND) ? NOP                                                                               \
                  : errx(1, "ERROR: Condition \"%s\" failed at (FILE: `%s', LINE: `%d', FUNC: `%s')", \
                         STRINGIFY(COND), __FILE__, __LINE__, FUNC_NAME))

#define err(EVAL, ...)  err_ ((EVAL), true,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define errx(EVAL, ...) err_ ((EVAL), false,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define warn(...)       warn_(true,   true,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define warnx(...)      warn_(false,  true,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define warnd(...)      warn_(false,  false,  __FILE__, __LINE__, __func__, __VA_ARGS__)

extern void warn_(bool print_err, bool force, char const *restrict file, int line, char const *restrict func, char const *restrict fmt, ...) __aFMT(6, 7);
extern noreturn void err_(int status, bool print_err, char const *restrict file, int line, char const *restrict func, char const *restrict fmt, ...) __aFMT(6, 7);


extern void     free_all__    (void *ptr, ...);
extern int64_t  xatoi__       (const char *str, bool strict);
extern unsigned find_num_cpus (void);
extern FILE *   safe_fopen    (const char *filename, const char *mode) __aWUR __aNNA;
extern FILE *   safe_fopen_fmt(const char *mode, const char *fmt, ...) __aWUR __aFMT(2,3);
extern int      safe_open     (const char *filename, int flags, int mode) __aWUR;
extern int      safe_open_fmt (int flags, int mode, const char *fmt, ...) __aWUR __aFMT(3, 4);
extern void     fd_set_open_flag(int fd, int flag);

#if 0 && defined DEBUG
extern int clock_nanosleep_for_(intmax_t seconds, intmax_t nanoseconds, char const *file, int line, char const *fn);
#  define clock_nanosleep_for(s, n) clock_nanosleep_for_((s), (n), __FILE__, __LINE__, __func__)
#else
extern int clock_nanosleep_for(intmax_t seconds, intmax_t nanoseconds);
#endif

#define NANOSLEEP_FOR_SECOND_FRACTION(i, d) \
        clock_nanosleep_for((uintmax_t)(i), (uintmax_t)(NSEC2SECOND / ((uintmax_t)(d))))

extern bstring *get_command_output(const char *command, char *const *argv, bstring *input, int *status);
#ifdef DOSISH
extern int win32_start_process_with_pipe(char const *exe, char *argv, HANDLE pipehandles[2], PROCESS_INFORMATION *pi);
extern bstring *_win32_get_command_output(char *argv, bstring *input, int *status);
extern noreturn void win32_error_exit(int status, const char *msg, DWORD dw);

#define WIN32_ERROR_EXIT_HELPER(VAR, ...)     \
      (__extension__({                        \
            char VAR[2048];                   \
            snprintf(VAR, 2048, __VA_ARGS__); \
            VAR;                              \
      }))
#define WIN32_ERROR_EXIT(ST, ...) \
      win32_error_exit((ST), WIN32_ERROR_EXIT_HELPER(P99_UNIQ(), __VA_ARGS__), GetLastError())
#endif

#ifdef __cplusplus
}
#endif
#endif /* util.h */
