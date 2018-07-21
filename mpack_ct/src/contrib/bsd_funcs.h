#ifndef BSD_FUNCS_H
#define BSD_FUNCS_H

#ifdef __cplusplus
   extern "C" {
#endif

#include <stdint.h>
#include <string.h>

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

#if 0
/* This is just to avoid symbol conflicts on systems with native versions of
 * these functions. A binary release may compile the bundled versions regardless
 * of whether they are needed. */
#define strlcpy  mytags_strlcpy
#define strlcat  mytags_strlcat
#define strtonum mytags_strtonum
#endif

#ifdef __cplusplus
   }
#endif

#endif /* bsd_funcs.h */
