#ifndef SRC_CONTRIB_CONTRIB_H
#define SRC_CONTRIB_CONTRIB_H

#ifdef __cplusplus
   extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#ifdef HAVE_TOPCONFIG_H
#  include "topconfig.h"
#endif

/* #if (defined(_WIN64) || defined(_WIN32)) && !defined(__CYGWIN__) */
   /* char *strsep(char **stringp, const char *delim); */
/* #endif */

#ifndef HAVE_STRSEP
   extern char *strsep(char **stringp, const char *delim);
#endif
#ifndef HAVE_STRLCPY
   extern size_t strlcpy(char * __restrict dst, const char * __restrict src, size_t dst_size);
#endif
#ifndef HAVE_STRLCAT
   extern size_t strlcat(char * __restrict dst, const char * __restrict src, size_t dst_size);
#endif
#ifndef HAVE_STRTONUM
   extern int64_t strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp);
#endif
#ifndef HAVE_MEMRCHR
   extern void *memrchr(const void *str, int ch, size_t n);
#endif
#ifndef HAVE_STRCHRNUL
   extern char *strchrnul(const char *ptr, int ch);
#endif

#ifdef DOSISH
#  include <WinSock2.h>
extern int dprintf(const SOCKET fd, const char *__restrict fmt, ...) __aFMT(2, 3);
#endif

#ifndef HAVE_GETTIMEOFDAY
#  if 0 && defined(_WIN32) || defined(__MINGW32__)
#    define WIN32_LEAN_AND_MEAN
//#  include <WinSock2.h>
#    include <Windows.h>
   
     struct timeval {
             int64_t tv_sec;
             int64_t tv_usec;
     };
   
     struct timezone {
             int tz_minuteswest;
             int tz_dsttime;
     };
     extern int gettimeofday(struct timeval * tp, struct timezone * tzp);
#  endif
#endif 

//#if defined(HAVE_GETTIMEOFDAY) && defined(_WIN32) && defined(__GNUC__)
////#  define gettimeofday(...) mingw_gettimeofday(__VA_ARGS__)
//#   include <unistd.h>
//     struct timeval {
//             int64_t tv_sec;
//             int64_t tv_usec;
//     };
//   
//     struct timezone {
//             int tz_minuteswest;
//             int tz_dsttime;
//     };
//     extern int gettimeofday(struct timeval * tp, struct timezone * tzp);
//#endif

#ifdef __cplusplus
   }
#endif

#endif /* contrib.h */
