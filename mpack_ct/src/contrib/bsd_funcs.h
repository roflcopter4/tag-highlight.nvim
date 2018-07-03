#ifndef BSD_FUNCS_H
#define BSD_FUNCS_H

#ifdef __cplusplus
   extern "C" {
#endif

#include <string.h>
//#ifdef _MSC_VER
//#  define __restrict ____restrict
//#endif

#if (defined(_WIN64) || defined(_WIN32)) && !defined(__CYGWIN__)
   char *strsep(char **stringp, const char *delim);
#endif

size_t    mytags_strlcpy(char * __restrict dst, const char * __restrict src, size_t dst_size);
size_t    mytags_strlcat(char * __restrict dst, const char * __restrict src, size_t dst_size);
long long mytags_strtonum(const char *numstr, long long minval, long long maxval, const char **errstrp);

/* This is just to avoid symbol conflicts on systems with native versions of
 * these functions. A binary release may compile the bundled versions regardless
 * of whether they are needed. */
#define strlcpy  mytags_strlcpy
#define strlcat  mytags_strlcat
#define strtonum mytags_strtonum

#ifdef __cplusplus
   }
#endif

#endif /* bsd_funcs.h */
