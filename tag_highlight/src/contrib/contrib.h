#ifndef SRC_CONTRIB_CONTRIB_H
#define SRC_CONTRIB_CONTRIB_H

#include "Common.h"

#ifdef __cplusplus
   extern "C" {
#endif

#ifndef HAVE_STRSEP
   extern char *strsep(char **stringp, const char *delim);
#endif
#ifndef HAVE_BSD_STDLIB_H
#  ifndef HAVE_STRLCPY
     extern size_t strlcpy(char *restrict dst, const char *restrict src, size_t dst_size);
#  endif
#  ifndef HAVE_STRLCAT
     extern size_t strlcat(char *restrict dst, const char *restrict src, size_t dst_size);
#  endif
#  ifndef HAVE_STRTONUM
     extern int64_t strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp);
#  endif
#endif
#ifndef HAVE_MEMRCHR
   extern void *memrchr(const void *str, int ch, size_t n);
#endif
#ifndef HAVE_STRCHRNUL
   extern char *strchrnul(const char *ptr, int ch);
#endif

#ifdef DOSISH
#  include <WinSock2.h>
extern int dprintf(const SOCKET fd, const char *restrict fmt, ...);
#endif

INLINE size_t
thl_strlcpy(char         *const restrict dst,
            char   const *const restrict src,
            size_t const                 dst_size)
{
        const size_t src_size = strlen(src);

        if (dst_size) {
                memcpy(dst, src, dst_size);
                if (dst_size < src_size)
                        dst[dst_size - 1] = '\0';
        }

        return src_size;
}

/* size_t my_strlcpy(char *restrict dst, const char *restrict src, size_t dst_size); */
#define my_strlcpy thl_strlcpy


#ifdef __cplusplus
   }
#endif
#endif /* contrib.h */
