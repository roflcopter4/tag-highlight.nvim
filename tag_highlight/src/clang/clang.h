#ifndef SRC_CLANG_CLANG_H
#define SRC_CLANG_CLANG_H

#include "util/util.h"

#include "data.h"
#include "p99/p99_defarg.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif


/* extern void libclang_get_compilation_unit(struct bufdata *bdata); */
/* extern struct translationunit *libclang_get_compilation_unit(struct bufdata *bdata); */

//extern void libclang_get_hl_commands(struct bufdata *bdata);
//extern void libclang_update_line(struct bufdata *bdata, int first, int last);

/* enum libclang_update_type { LCUPDATE_NORMAL, LCUPDATE_FORCE }; */

extern pthread_cond_t libclang_cond;

extern noreturn void *libclang_threaded_highlight(void *vdata);

extern void launch_libclang_waiter(void);
extern void libclang_highlight(struct bufdata *bdata, int first, int last, bool force);
extern void destroy_clangdata(struct bufdata *bdata);


#define libclang_highlight(...) P99_CALL_DEFARG(libclang_highlight, 4, __VA_ARGS__)
#define libclang_highlight_defarg_1() (0)
#define libclang_highlight_defarg_2() (-1)
#define libclang_highlight_defarg_3() (false)


#ifdef __cplusplus
}
#endif
#endif /* clang.h */
