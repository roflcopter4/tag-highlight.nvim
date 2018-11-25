#ifndef LANG_CLANG_CLANG_H_
#define LANG_CLANG_CLANG_H_

#include "Common.h"
#include "highlight.h"

#ifdef __cplusplus
extern "C" {
#endif


/* extern void libclang_get_compilation_unit(Buffer *bdata); */
/* extern struct translationunit *libclang_get_compilation_unit(Buffer *bdata); */

//extern void libclang_get_hl_commands(Buffer *bdata);
//extern void libclang_update_line(Buffer *bdata, int first, int last);

/* enum libclang_update_type { LCUPDATE_NORMAL, LCUPDATE_FORCE }; */

extern noreturn void *libclang_threaded_highlight(void *vdata);

extern void launch_libclang_waiter(void);
extern void libclang_highlight(Buffer *bdata, int first, int last, int force);
extern void destroy_clangdata(Buffer *bdata);


#define libclang_highlight(...) P99_CALL_DEFARG(libclang_highlight, 4, __VA_ARGS__)
#define libclang_highlight_defarg_1() (0)
#define libclang_highlight_defarg_2() (-1)
#define libclang_highlight_defarg_3() (HIGHLIGHT_NORMAL)


#ifdef __cplusplus
}
#endif
#endif /* clang.h */
