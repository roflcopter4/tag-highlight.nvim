#ifndef SRC_LIBCLANG_LIBCLANG_H
#define SRC_LIBCLANG_LIBCLANG_H

#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif


/* extern void libclang_get_compilation_unit(struct bufdata *bdata); */
/* extern struct translationunit *libclang_get_compilation_unit(struct bufdata *bdata); */

extern void libclang_get_hl_commands(struct bufdata *bdata);
extern void libclang_update_line(struct bufdata *bdata, int first, int last);
extern void destroy_clangdata(void *data);


#ifdef __cplusplus
}
#endif
#endif /* libclang.h */
