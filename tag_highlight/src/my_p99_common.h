#ifndef MY_P99_COMMON_H_
#define MY_P99_COMMON_H_

#include "Common.h"

#include "bstring.h"
#include "contrib/p99/p99.h"
#include "contrib/p99/p99_defarg.h"
#include "contrib/p99/p99_for.h"

#define pthread_create(...)       P99_CALL_DEFARG(pthread_create, 4, __VA_ARGS__)
#define pthread_create_defarg_3() NULL
#define pthread_exit(...)       P99_CALL_DEFARG(pthread_exit, 1, __VA_ARGS__)
#define pthread_exit_defarg_0() NULL

#ifdef USE_P99_TRY
#include "contrib/p99/p99_try.h"

#define TRY                P99_TRY
#define CATCH(NAME)        P99_CATCH(int NAME) if (NAME)
#define CATCH_FINALLY(...) P99_CATCH(P99_IF_EMPTY(__VA_ARGS__)()(int __VA_ARGS__))
#define TRY_RETURN         P99_UNWIND_RETURN

#define FINALLY                                                         \
        P00_FINALLY                                                     \
        P00_BLK_BEFORE(p00_unw = 0)                                     \
        P00_BLK_AFTER(!p00_code || p00_code == -(INT_MAX)               \
            ? (void)((P00_JMP_BUF_FILE = 0), (P00_JMP_BUF_CONTEXT = 0)) \
            : P99_RETHROW)


/*
 * Probably best that the arguments to this macro not have side effects, given
 * that they are expanded no fewer than 10 times.
 */
#define THROW(...)                                                          \
        p00_jmp_throw((((P99_NARG(__VA_ARGS__) == 1 &&                      \
                         _Generic(P99_CHS(0, __VA_ARGS__),                  \
                                  const char *: (true),                     \
                                  char *:       (true),                     \
                                  default:      (false))                    \
                        ) || (P99_CHS(0, __VA_ARGS__)) == 0)                \
                       ? (-1)                                               \
                       : (_Generic(P99_CHS(0, __VA_ARGS__),                 \
                                   char *:       (-1),                      \
                                   const char *: (-1),                      \
                                   default:      (P99_CHS(0, __VA_ARGS__))) \
                         )                                                  \
                      ),                                                    \
                      p00_unwind_top,                                       \
                      P99_STRINGIFY(__LINE__),                              \
                      __func__,                                             \
                      (P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                   \
                        (_Generic(P99_CHS(0, __VA_ARGS__),                  \
                                   char *:       (P99_CHS(0, __VA_ARGS__)), \
                                   const char *: (P99_CHS(0, __VA_ARGS__)), \
                                   default:      (""))                      \
                        )                                                   \
                        (P99_CHS(1, __VA_ARGS__))                           \
                      )                                                     \
                     )


#define P99_IF_TYPE_ELSE(ITEM, TYPE, VAL, ELSE) \
        (_Generic((ITEM),                       \
                  TYPE : (VAL),                 \
                  const TYPE : (VAL),           \
                  volatile TYPE : (VAL),        \
                  default : (ELSE)))
#endif


#define P01_POINTLESS_MACRO(...) (__VA_ARGS__)
#define P01_ANDALL(MACRO, ...) P00_MAP_(P99_NARG(__VA_ARGS__), MACRO, (&&), __VA_ARGS__)
#define P01_ORALL(MACRO, ...)  P00_MAP_(P99_NARG(__VA_ARGS__), MACRO, (||), __VA_ARGS__)

#define P99_ANDALL(...) P01_ANDALL(P01_POINTLESS_MACRO, __VA_ARGS__)
#define P99_ORALL(...)  P01_ORALL(P01_POINTLESS_MACRO, __VA_ARGS__)

#define P01_EQ(WHAT, X, I)   (X) == (WHAT)
#define P99_EQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P01_EQ, __VA_ARGS__)

#define P01_STREQ(WHAT, X, I)   (strcmp((WHAT), (X)) == 0)
#define P99_STREQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P01_STREQ, __VA_ARGS__)

#define P01_B_ISEQ(WHAT, X, I)   b_iseq((WHAT), (X))
#define P99_B_ISEQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P01_B_ISEQ, __VA_ARGS__)

#define P01_B_ISEQ_LIT(WHAT, X, I)                                 \
        (b_iseq((WHAT), ((bstring[]){{.data  = (uchar *)("" X ""), \
                                      .slen  = (sizeof(X) - 1),    \
                                      .mlen  = 0,                  \
                                      .flags = 0}})))
#define P99_B_ISEQ_LIT_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P01_B_ISEQ_LIT, __VA_ARGS__)

#define STREQ(S1, S2)  (strcmp((S1), (S2)) == 0)
/* #define STREQ_ANY      P99_STREQ_ANY */
#define b_iseq_any     P99_B_ISEQ_ANY
#define b_iseq_lit_any P99_B_ISEQ_LIT_ANY

#define P99_DECLARE_FIFO(NAME)   \
        P99_DECLARE_STRUCT(NAME); \
        P99_POINTER_TYPE(NAME);   \
        P99_FIFO_DECLARE(NAME##_ptr)

#define P99_DECLARE_LIFO(NAME)   \
        P99_DECLARE_STRUCT(NAME); \
        P99_POINTER_TYPE(NAME);   \
        P99_LIFO_DECLARE(NAME##_ptr)

#define pipe2_throw(...) P99_THROW_CALL_NEG(pipe2, EINVAL, __VA_ARGS__)
#define dup2_throw(...)  P99_THROW_CALL_NEG(dup2, EINVAL, __VA_ARGS__)
#define execl_throw(...) P99_THROW_CALL_NEG(execl, EINVAL, __VA_ARGS__)

#define P01_FREE_BSTRING(BSTR) b_destroy(BSTR)
#define b_destroy_all(...) P99_BLOCK(P99_SEP(P01_FREE_BSTRING, __VA_ARGS__);)
#define P01_WRITEPROTECT_BSTRING(BSTR) b_writeprotect(BSTR)
#define b_writeprotect_all(...) P99_BLOCK(P99_SEP(P01_WRITEPROTECT_BSTRING, __VA_ARGS__);)
#define P01_WRITEALLOW_BSTRING(BSTR) b_writeallow(BSTR)
#define b_writeallow_all(...) P99_BLOCK(P99_SEP(P01_WRITEALLOW_BSTRING, __VA_ARGS__);)

#endif /* p99_common.h */
