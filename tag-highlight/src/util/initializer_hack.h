#pragma once
#ifndef HGUARD__CONTRIB__INITIALIZER_HACK_H_
#define HGUARD__CONTRIB__INITIALIZER_HACK_H_

// Initializer/finalizer sample for MSVC and GCC/Clang.
// 2010-2016 Joe Lowe. Released into the public domain.

/*
 * Edited somewhat by some idiot called RoflCopter4.
 * Now _requires_ a conforment C pre-processor and the
 * common extension macro `__COUNTER__'.
 */

/*--------------------------------------------------------------------------------------*/
#if defined(__GNUC__) || defined(__clang__)
# ifdef __attribute__
#  error "__attribute__ was defined?"
# endif
# define HELP_INITIALIZER_HACK_1_(FN, UNIQ) \
      __attribute__((__constructor__)) static void FN##_##UNIQ(void)
# define HELP_INITIALIZER_HACK_N_1_(FN, N, UNIQ) \
      __attribute__((__constructor__(N))) static void FN##_##UNIQ(void)

/*--------------------------------------------------------------------------------------*/
#elif defined __cplusplus

# define HELP_INITIALIZER_HACK_2_(FN, ST)       \
      static void FN(void);                     \
      struct ST##t_ { ST##t_(void) { FN(); } }; \
      static ST##t_ ST;                         \
      static void FN(void)

# define HELP_INITIALIZER_HACK_1_(FN, UNIQ) \
      HELP_INITIALIZER_HACK_2_(FN##_##UNIQ, FN##_cxx_run_at_program_init_hack_##UNIQ)

/*--------------------------------------------------------------------------------------*/
#elif defined(_MSC_VER)
# pragma section(".CRT$XCU", read)
# define HELP_INITIALIZER_HACK_2_(fn, p)                         \
      static void fn(void);                                      \
      __declspec(allocate(".CRT$XCU")) void (*fn##_)(void) = fn; \
      __pragma(comment(linker, "/include:" p #fn "_"))           \
      static void fn(void)

# ifdef _WIN64
#  define HELP_INITIALIZER_HACK_1_(FN, UNIQ) \
      HELP_INITIALIZER_HACK_2_(FN##_##UNIQ, "")
# else
#  define HELP_INITIALIZER_HACK_1_(FN, UNIQ) \
      HELP_INITIALIZER_HACK_2_(FN##_##UNIQ, "_")
# endif

/*--------------------------------------------------------------------------------------*/
#else
# error "Are you using tcc or something? Sorry, your oddball compiler isn't supported."
#endif

#define HELP_INITIALIZER_HACK_02_(FN, CNT, LN) HELP_INITIALIZER_HACK_1_(FN, CNT ## _ ## LN)
#define HELP_INITIALIZER_HACK_01_(FN, CNT, LN) HELP_INITIALIZER_HACK_02_(FN, CNT, LN)
#define INITIALIZER_HACK(FN)                   HELP_INITIALIZER_HACK_01_(FN, __COUNTER__, __LINE__)

#if (defined(__GNUC__) || defined(__clang__)) // && !defined(__clang__)) || (defined(__clang__) && !defined(_WIN32))
# define HELP_INITIALIZER_HACK_N_02_(FN, N, CNT, LN) HELP_INITIALIZER_HACK_N_1_(FN, N, CNT ## _ ## LN)
# define HELP_INITIALIZER_HACK_N_01_(FN, N, CNT, LN) HELP_INITIALIZER_HACK_N_02_(FN, N, CNT, LN)
# define INITIALIZER_HACK_N(FN, N)                   HELP_INITIALIZER_HACK_N_01_(FN, N, __COUNTER__, __LINE__)
#else
# define INITIALIZER_HACK_N(FN, N) HELP_INITIALIZER_HACK_01_(FN, __COUNTER__, __LINE__)
#endif

#endif // initializer_hack.h
// vim: ft=cpp
