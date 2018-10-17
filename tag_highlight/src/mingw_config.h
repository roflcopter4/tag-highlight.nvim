#ifndef TOP_MINGW_CONFIG_
#define TOP_MINGW_CONFIG_
#ifdef __MINGW__

#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>

#define asprintf(buf, ...)       __mingw_asprintf((buf), __VA_ARGS__)
#define fprintf(strm, ...)       __mingw_fprintf((strm), __VA_ARGS__)
#define printf(...)              __mingw_printf(__VA_ARGS__)
#define snprintf(buf, siz, ...)  __mingw_snprintf((buf), (siz), __VA_ARGS__)
#define sprintf(buf, ...)        __mingw_sprintf((buf), __VA_ARGS__)
#define vasprintf(buf, ...)      __mingw_vasprintf((buf), __VA_ARGS__)
#define vfprintf(strm, ...)      __mingw_fprintf((strm), __VA_ARGS__)
#define vsnprintf(buf, siz, ...) __mingw_vsnprintf((buf), (siz), __VA_ARGS__)
#define vsprintf(strm, ...)      __mingw_vsprintf((strm), __VA_ARGS__)

#ifdef __clang__
#  define __gnu_printf__ __printf__
#endif

#endif
#endif /* mingw_config.h */
