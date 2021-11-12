#ifndef SRC_CONTRIB_CONTRIB_H
#define SRC_CONTRIB_CONTRIB_H

#include "Common.h"

#ifdef __cplusplus
#  define restrict __restrict
   extern "C" {
#endif


#ifndef HAVE_STRSEP
   extern char *strsep(char **stringp, const char *delim);
#endif
#ifndef HAVE_STRLCPY
   extern size_t strlcpy(char *restrict dst, const char *restrict src, size_t dst_size);
#endif
#ifndef HAVE_STRLCAT
   extern size_t strlcat(char *restrict dst, const char *restrict src, size_t dst_size);
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
#ifndef HAVE_DPRINTF
   extern int dprintf(const SOCKET fd, const char *restrict fmt, ...);
#endif


#ifdef __cplusplus
   }
#endif
#endif /* contrib.h */
