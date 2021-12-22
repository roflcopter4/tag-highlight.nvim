
typedef int bstring;

#include "Common.h"

#include <cinttypes>
#include <cstdint>
#include <random>

extern "C" uint32_t
cxx_random_device_get_random_val(void)
{
      static std::random_device rd;
      return rd();
}

extern "C" uint32_t
cxx_random_engine_get_random_val(void)
{
      static std::default_random_engine rand_engine(cxx_random_device_get_random_val());
      return static_cast<uint32_t>(rand_engine());
}
