// ReSharper disable CppInconsistentNaming
#ifndef __USE_ISOC99
# define __USE_ISOC99 1 //NOLINT
#endif
#ifndef __USE_ISOC11
# define __USE_ISOC11 1 //NOLINT
#endif

#include "Common.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
# define restrict __restrict
#endif

#ifdef DUMB_TEMPNAME_NUM_CHARS
# define NUM_RANDOM_CHARS DUMB_TEMPNAME_NUM_CHARS
#else
# define NUM_RANDOM_CHARS 16
#endif

#ifdef _WIN32
# define FILESEP_CHAR '\\'
# define FILESEP_STR  "\\"
# define CHAR_IS_FILESEP(ch) ((ch) == '\\' || (ch) == '/')
# ifndef PATH_MAX
#  if WDK_NTDDI_VERSION >= NTDDI_WIN10_RS1
#   define PATH_MAX 4096
#  else
#   define PATH_MAX MAX_PATH
#  endif
# endif
#else
# define FILESEP_CHAR '/'
# define FILESEP_STR  "/"
# define CHAR_IS_FILESEP(ch) ((ch) == '/')
#endif

#define RAND() cxx_random_engine_get_random_val()

static char *get_random_chars(char *buf);

/*======================================================================================*/

char *
braindead_tempname(_Notnull_   char       *restrict const buf,
                   _Notnull_   char const *restrict const dir,
                   _Maybenull_ char const *restrict const prefix,
                   _Maybenull_ char const *restrict const suffix)
{
      /* Microsoft's libc doesn't include stpcpy, and I can't bring myself to use strcat,
       * so this is about the best way I can think of to do this. Here's hoping the
       * compiler is smart enough to work out what's going on. */
      size_t len = strlen(dir);
      memcpy(buf, dir, len + 1);

      if (len > 0 && !CHAR_IS_FILESEP(buf[len - 1]))
            buf[len++] = FILESEP_CHAR;

      char *ptr = buf + len;

      if (prefix) {
            len = strlen(prefix);
            memcpy(ptr, prefix, len);
            ptr += len;
      }

      ptr = get_random_chars(ptr);

      if (suffix) {
            len = strlen(suffix);
            memcpy(ptr, suffix, len);
            ptr += len;
      }

      *ptr = '\0';
      return buf;
}

static char *
get_random_chars(char *buf)
{
      char *ptr = buf;

      for (int i = 0; i < NUM_RANDOM_CHARS; ++i) {
            uint32_t const tmp = RAND();

            if ((tmp & 0x0F) < 2)
                  *ptr++ = (char)((RAND() % 10) + '0');
            else
                  *ptr++ = (char)((RAND() % 26) + ((tmp & 1) ? 'A' : 'a'));
      }

      return ptr;
}
