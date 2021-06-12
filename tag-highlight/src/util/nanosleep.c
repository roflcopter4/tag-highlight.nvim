#include "Common.h"
#include "util.h"

#if 0 && defined DEBUG
int
clock_nanosleep_for_(intmax_t const seconds, intmax_t const nanoseconds, char const *file, int line, char const *fn)
{
        struct timespec cur, add, res;
        clock_gettime(CLOCK_MONOTONIC, &cur);
        add = (struct timespec){.tv_sec = seconds, .tv_nsec = nanoseconds};
        TIMESPEC_ADD(&cur, &add, &res);

        warn_(false, true, file, line, fn, "Sleeping from %f to %f, ie %fs",
              TIMESPEC2DOUBLE(&cur), TIMESPEC2DOUBLE(&res), TIMESPECDIFF(&cur, &res));

        return clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &res, NULL);
}
#else
int
clock_nanosleep_for(intmax_t const seconds, intmax_t const nanoseconds)
{
        struct timespec cur, add, res;
        clock_gettime(CLOCK_MONOTONIC, &cur);
        add = (struct timespec){.tv_sec = seconds, .tv_nsec = nanoseconds};
        TIMESPEC_ADD(&cur, &add, &res);

        return clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &res, NULL);
}
#endif
