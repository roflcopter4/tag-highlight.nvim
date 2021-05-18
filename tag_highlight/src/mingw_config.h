#ifndef SRC_MINGW_CONFIG_H_
#define SRC_MINGW_CONFIG_H_
# ifdef __MINGW__

# define __USE_MINGW_ANSI_STDIO 1
# include <stdio.h>
# define P99_WANT_THREADS 1

#define pipe(fds) _pipe((fds), 8192, 0)
# define asprintf(buf, ...)       __mingw_asprintf((buf), __VA_ARGS__)
# define fprintf(strm, ...)       __mingw_fprintf((strm), __VA_ARGS__)
# define printf(...)              __mingw_printf(__VA_ARGS__)
# define snprintf(buf, siz, ...)  __mingw_snprintf((buf), (siz), __VA_ARGS__)
# define sprintf(buf, ...)        __mingw_sprintf((buf), __VA_ARGS__)
# define vasprintf(buf, ...)      __mingw_vasprintf((buf), __VA_ARGS__)
# define vfprintf(strm, ...)      __mingw_vfprintf((strm), __VA_ARGS__)
# define vsnprintf(buf, siz, ...) __mingw_vsnprintf((buf), (siz), __VA_ARGS__)
# define vsprintf(strm, ...)      __mingw_vsprintf((strm), __VA_ARGS__)

# endif
#endif /* mingw_config.h */
