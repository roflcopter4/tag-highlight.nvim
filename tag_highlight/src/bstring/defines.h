#ifndef TOP_BSTRLIB_H
#  error Never include this file manually. Include "bstrlib.h".
#endif

#ifndef BSTRLIB_DEFINES_H
#define BSTRLIB_DEFINES_H

#if (__GNUC__ >= 4)
#  define BSTR_PUBLIC  __attribute__((__visibility__("default")))
#  define BSTR_PRIVATE __attribute__((__visibility__("hidden")))
#  define INLINE       __attribute__((__always_inline__)) static inline
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#else
#  define BSTR_PUBLIC
#  define BSTR_PRIVATE
#  define INLINE static inline
#endif

#if (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#  define BSTR_PRINTF(format, argument) __attribute__((__format__(__printf__, format, argument)))
#  define BSTR_UNUSED __attribute__((__unused__))
#else
#  define BSTR_PRINTF(format, argument)
#  define BSTR_UNUSED
#endif

#if !defined(__GNUC__)
#  define __attribute__(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char uchar;

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BSTR_ERR (-1)
#define BSTR_OK (0)
#define BSTR_BS_BUFF_LENGTH_GET (0)

#define BSTR_WRITE_ALLOWED 0x01u
#define BSTR_FREEABLE      0x02u
#define BSTR_DATA_FREEABLE 0x04u
#define BSTR_LIST_END      0x08u
#define BSTR_CLONE         0x10u

#define BSTR_STANDARD (BSTR_WRITE_ALLOWED | BSTR_FREEABLE | BSTR_DATA_FREEABLE)


#pragma pack(push, 1)
struct tagbstring {
        unsigned slen;
        unsigned mlen;
        uchar *data;
        uint8_t flags;
};
#pragma pack(pop)


struct bstring_list {
        unsigned qty;
        unsigned mlen;
        struct tagbstring **lst;
};

typedef struct tagbstring   bstring;
typedef struct bstring_list b_list;

#endif /* defines.h */
