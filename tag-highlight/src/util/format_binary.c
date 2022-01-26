#include "Common.h"
#include "util.h"

#if __has_include("x86intrin.h")
#include <x86intrin.h>
#else
#include <intrin.h>
#endif

static char const *nibbles[16] = {
    "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
    "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111",
};


static unsigned
nearest_power_of_two(uintmax_t i)
{
      if (i < 2) {
            i = 1;
      } else {
            uint64_t j = i;
            j |= (j >> 1);
            j |= (j >> 2);
            j |= (j >> 4);
            j |= (j >> 8);
            j |= (j >> 16);
            j |= (j >> UINT64_C(32));

            if (++j >= i)
                  i = j;
      }

      return i;
}

static unsigned
find_number_of_nibbles_needed(uintmax_t const val)
{
      unsigned num_nibbles;

      if (val < 16) {
            num_nibbles = 0;
      } else {
            unsigned const nearest_pow = nearest_power_of_two(val);
            double const   num_bits    = floor(log2((double)nearest_pow));
            num_nibbles                = (unsigned)floor(num_bits / 4.0);

            if (num_nibbles > 0 && (_lzcnt_u32(val) % 4) == 0)
                  --num_nibbles;
      }

      return num_nibbles;
}

char *
util_format_int_to_binary(char *buf, uintmax_t const val)
{
      unsigned const num_nibbles = find_number_of_nibbles_needed(val);
      char          *ptr         = buf;

      for (int i = (int)num_nibbles; i >= 0; --i) {
            uintmax_t const mask = UINTMAX_C(0xF) << (i * 4);
            char const     *nib  = nibbles[(val & mask) >> (i * 4)];
            memcpy(ptr, nib, 4);
            ptr += 4;
            *ptr++ = (i & 1) ? '_' : '\'';
      }

      *--ptr = '\0';
      return buf;
}
