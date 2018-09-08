#ifndef SRC_CLANG_CLANG_H
#define SRC_CLANG_CLANG_H

#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif


/* extern void libclang_get_compilation_unit(struct bufdata *bdata); */
/* extern struct translationunit *libclang_get_compilation_unit(struct bufdata *bdata); */

//extern void libclang_get_hl_commands(struct bufdata *bdata);
//extern void libclang_update_line(struct bufdata *bdata, int first, int last);

extern void libclang_highlight(struct bufdata *bdata, int first, int last);
extern void destroy_clangdata(struct bufdata *bdata);

#ifdef __cplusplus
}
#endif
#endif /* clang.h */
