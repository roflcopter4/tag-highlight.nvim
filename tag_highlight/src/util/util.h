#ifndef SRC_UTIL_H
#define SRC_UTIL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "tag_highlight.h"

/*======================================================================================*/

/* 
 * Timer structure
 */
#ifdef DOSISH
struct timer {
        struct timeval tv1, tv2;
};
#  define TIMER_START(T_) (gettimeofday(&(T_).tv1, NULL))
#  define TIMER_START_BAR(T_)                    \
        do {                                     \
                gettimeofday(&(T_).tv1, NULL);   \
                SHOUT("----------------------"); \
        } while (0)
#  define TIMER_REPORT(T_, MSG_)                                            \
        do {                                                                \
                gettimeofday(&(T_).tv2, NULL);                              \
                SHOUT("Time for \"%s\": % *fs", (MSG_),                    \
                      (int)(30 - sizeof(MSG_)), TDIFF((T_).tv1, (T_).tv2)); \
        } while (0)

#else // NOT DOSISH

struct timer {
        struct timespec tv1, tv2;
};
#  define TIMER_START(T_) (clock_gettime(CLOCK_REALTIME, &(T_).tv1))
#  define TIMER_START_BAR(T_)                             \
        do {                                              \
                clock_gettime(CLOCK_REALTIME, &(T_).tv1); \
                SHOUT("----------------------");          \
        } while (0)
#  define TIMER_REPORT(T_, MSG_)                                               \
        do {                                                                   \
                clock_gettime(CLOCK_REALTIME, &(T_).tv2);                      \
                SHOUT("Time for \"%s\": % *.9fs", (MSG_),                     \
                      (int)(35 - sizeof(MSG_)), SPECDIFF((T_).tv1, (T_).tv2)); \
        } while (0)
#endif
#define TIMER_REPORT_RESTART(T, MSG) do { TIMER_REPORT(T, MSG); TIMER_START(T); } while (0)

/*======================================================================================*/

WEAK_SYMB const double USEC2SECOND = 1000000.0;
WEAK_SYMB const double NSEC2SECOND = 1000000000.0;

#define MKTIMESPEC(FLT) &(struct timespec){ \
          (int64_t)(FLT),                   \
          (int64_t)(((double)((FLT) - (double)((int64_t)(FLT)))) * NSEC2SECOND)}

#define TDIFF(STV1, STV2)                                                 \
        (((double)((STV2).tv_usec - (STV1).tv_usec) / USEC2SECOND) + \
         ((double)((STV2).tv_sec - (STV1).tv_sec)))

#define SPECDIFF(STV1, STV2)                                              \
        (((double)((STV2).tv_nsec - (STV1).tv_nsec) / NSEC2SECOND) + \
         ((double)((STV2).tv_sec - (STV1).tv_sec)))

#define TSPEC2DOUBLE(STV) \
        ((double)((((double)(STV).tv_sec)) + (((double)(STV).tv_nsec) / NSEC2SECOND)))

/*===========================================================================*/
/* Generic Utility Functions */

#define xatoi(STR_)          __xatoi((STR_), false)
#define s_xatoi(STR_)        __xatoi((STR_), true)
#define free_all(...)        __free_all(__VA_ARGS__, NULL)

extern void     __free_all    (void *ptr, ...);
extern int64_t  __xatoi       (const char *str, bool strict);
extern unsigned find_num_cpus (void);
extern FILE *   safe_fopen    (const char *filename, const char *mode) aWUR aNNA;
extern FILE *   safe_fopen_fmt(const char *fmt, const char *mode, ...) aWUR aFMT(1,3);
extern int      safe_open     (const char *filename, int flags, int mode) aWUR;
extern int      safe_open_fmt (const char *fmt, int flags, int mode, ...) aWUR aFMT(1, 4);
extern void     add_backup    (struct backups *list, void *item) aNNA;
extern void     free_backups  (struct backups *list);


#endif /* util.h */
