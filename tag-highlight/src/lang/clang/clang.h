#ifndef LANG_CLANG_CLANG_H_
#define LANG_CLANG_CLANG_H_

#include "Common.h"
#include "highlight.h"

#include "contrib/p99/p99_defarg.h"

__BEGIN_DECLS
/*===========================================================================*/

extern void libclang_highlight(Buffer *bdata, int first, int last, int type);
extern void launch_libclang_waiter(void);
extern void destroy_clangdata(Buffer *bdata);
extern noreturn void *highlight_c_pthread_wrapper(void *vdata);


#define libclang_highlight(...) P99_CALL_DEFARG(libclang_highlight, 4, __VA_ARGS__)
#define libclang_highlight_defarg_1() (0)
#define libclang_highlight_defarg_2() (-1)
#define libclang_highlight_defarg_3() (HIGHLIGHT_NORMAL)

/*===========================================================================*/
__END_DECLS
#endif /* clang.h */
