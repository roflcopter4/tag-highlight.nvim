#ifndef MY_P99_COMMON_H
#define MY_P99_COMMON_H

/* #include "p99/p99.h" */
/* #include "p99/p99_args.h" */
/* #include "p99/p99_c99_throw.h" */
#include "p99/p99_block.h"
#include "p99/p99_defarg.h"
#include "p99/p99_for.h"
/* #include "p99/p99_posix_default.h" */
/* #include "p99/p99_new.h" */
/* #include "p99/p99_try.h" */

#define TRY      P99_TRY
#define TRYF     P99_TRY { P99_TRY
#define CATCH    P99_CATCH(int code) if (code > 0)
#define FINALLY  P99_FINALLY
#define FFINALLY } P99_FINALLY
#define ENDTRY   } P99_FINALLY

#define TRY1   TRY {
#define CATCH1 } CATCH

#define P44_ANDALL(MACRO, ...) P00_MAP_(P99_NARG(__VA_ARGS__), MACRO, (&&), __VA_ARGS__)
#define P44_ORALL(MACRO, ...) P00_MAP_(P99_NARG(__VA_ARGS__), MACRO, (||), __VA_ARGS__)

#define P04_EQ(WHAT, X, I) (X) == (WHAT)
#define P44_EQ_ANY(VAR, ...) P99_FOR(VAR, P99_NARG(__VA_ARGS__), P00_OR, P04_EQ, __VA_ARGS__)

#endif /* p99_common.h */
