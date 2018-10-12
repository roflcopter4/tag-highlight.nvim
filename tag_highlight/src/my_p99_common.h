#ifndef MY_P99_COMMON_H_
#define MY_P99_COMMON_H_

/* #include "p99/p99.h" */
/* #include "p99/p99_args.h" */
/* #include "p99/p99_c99_throw.h" */
#include "p99/p99_block.h"
#include "p99/p99_defarg.h"
#include "p99/p99_for.h"
/* #include "p99/p99_posix_default.h" */
/* #include "p99/p99_new.h" */
/* #include "p99/p99_try.h" */

#define pthread_create(...) P99_CALL_DEFARG(pthread_create, 4, __VA_ARGS__)
#define pthread_create_defarg_3() NULL

#define pthread_exit(...) P99_CALL_DEFARG(pthread_exit, 1, __VA_ARGS__)
#define pthread_exit_defarg_0() NULL

/* #define p99_futex_wakeup(...) P99_CALL_DEFARG(p99_futex_wakeup, 3, __VA_ARGS__) */
/* #define p99_futex_wakeup_defarg_2() (P99_FUTEX_MAX_WAITERS) */

#define TRY                P99_TRY
#define CATCH(NAME)        P99_CATCH(int NAME) if (NAME)
#define CATCH_FINALLY(...) P99_CATCH(P99_IF_EMPTY(__VA_ARGS__)()(int __VA_ARGS__))
#define FINALLY            P99_FINALLY

#define THROW(...)                                                              \
        p00_jmp_throw((((P99_NARG(__VA_ARGS__) == 1 &&                          \
                         _Generic(P99_CHS(0, __VA_ARGS__),                      \
                                  const char *: (true),                         \
                                  char *:       (true),                         \
                                  default:      (false))                        \
                        ) || (P99_CHS(0, __VA_ARGS__)) == 0)                    \
                        ? (-1)                                                  \
                        : (_Generic(P99_CHS(0, __VA_ARGS__),                    \
                                    char *:       (-1),                         \
                                    const char *: (-1),                         \
                                    default:      (P99_CHS(0, __VA_ARGS__))))), \
                      p00_unwind_top,                                           \
                      P99_STRINGIFY(__LINE__),                                  \
                      __func__,                                                 \
                      (P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                       \
                        ((_Generic(P99_CHS(0, __VA_ARGS__),                     \
                                   char *:       (P99_CHS(0, __VA_ARGS__)),     \
                                   const char *: (P99_CHS(0, __VA_ARGS__)),     \
                                   default:      ("throw"))))                   \
                        (P99_CHS(1, __VA_ARGS__))))


#define P44_IF_TYPE_ELSE(ITEM, TYPE, VAL, ELSE) \
        (_Generic((ITEM),                       \
                  TYPE : (VAL),                 \
                  const TYPE : (VAL),           \
                  volatile TYPE : (VAL),        \
                  default : (ELSE)))

#define update_highlight(...)                                                                                  \
        (update_highlight)(P99_IF_EQ_3(P99_NARG(__VA_ARGS__))                                                  \
                (__VA_ARGS__)                                                                                  \
                ((P44_IF_TYPE_ELSE(P99_CHS(0, __VA_ARGS__), struct bufdata *, (-1), P99_CHS(0, __VA_ARGS__))), \
                 (P44_IF_TYPE_ELSE(P99_CHS(0, __VA_ARGS__), struct bufdata *, P99_CHS(0, __VA_ARGS__), NULL)), \
                 (P99_IF_EQ_2(P99_NARG(__VA_ARGS__))                                                           \
                        (P99_CHS(1, __VA_ARGS__))                                                              \
                        (false)))                                                                              \
        )

#define clear_highlight(...)                                                                                       \
        (clear_highlight)(P99_IF_EMPTY(__VA_ARGS__)                                                                \
                (nvim_get_current_buf(), NULL)                                                                     \
                (P99_IF_EQ_2(P99_NARG(__VA_ARGS__))                                                                \
                   (__VA_ARGS__)                                                                                   \
                   ((P44_IF_TYPE_ELSE(P99_CHS(0, __VA_ARGS__), struct bufdata *, (-1), P99_CHS(0, __VA_ARGS__))),  \
                    (P44_IF_TYPE_ELSE(P99_CHS(0, __VA_ARGS__), struct bufdata *, P99_CHS(0, __VA_ARGS__), NULL)))) \
        )


#define P44_ANDALL(MACRO, ...) P00_MAP_(P99_NARG(__VA_ARGS__), MACRO, (&&), __VA_ARGS__)
#define P44_ORALL(MACRO, ...) P00_MAP_(P99_NARG(__VA_ARGS__), MACRO, (||), __VA_ARGS__)

#define P04_EQ(WHAT, X, I) (X) == (WHAT)
#define P44_EQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P04_EQ, __VA_ARGS__)

#define P04_STREQ(WHAT, X, I) (strcmp((WHAT), (X)) == 0)
#define P44_STREQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P04_STREQ, __VA_ARGS__)

#define P44_DECLARE_FIFO(NAME)   \
        P99_DECLARE_STRUCT(NAME); \
        P99_POINTER_TYPE(NAME);   \
        P99_FIFO_DECLARE(NAME##_ptr)

#define P44_DECLARE_LIFO(NAME)   \
        P99_DECLARE_STRUCT(NAME); \
        P99_POINTER_TYPE(NAME);   \
        P99_LIFO_DECLARE(NAME##_ptr)

#endif /* p99_common.h */
