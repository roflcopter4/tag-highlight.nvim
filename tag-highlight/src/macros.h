#ifndef THL_MACROS_H_
#define THL_MACROS_H_

#ifndef THL_HIGHLIGHT_H_
# include "highlight.h"
# error "Never include macros.h directly. It comes with highlight.h."
#endif

/*===========================================================================*/
/* cpp shenanigans */

__BEGIN_DECLS

ALWAYS_INLINE Buffer *
find_current_buffer(void)
{
        return find_buffer(nvim_get_current_buf());
}

ALWAYS_INLINE void __attribute__((__nonnull__))
nvim_buf_attach_bdata_wrap(const Buffer *const bdata)
{
        nvim_buf_attach(bdata->num);
}

__END_DECLS

#define update_highlight(...)       P99_CALL_DEFARG(update_highlight, 2, __VA_ARGS__)
#define update_highlight_defarg_0() (find_current_buffer())
#define update_highlight_defarg_1() (HIGHLIGHT_NORMAL)
#define clear_highlight(...)        P99_CALL_DEFARG(clear_highlight, 2, __VA_ARGS__)
#define clear_highlight_defarg_0()  (find_current_buffer())
#define clear_highlight_defarg_1()  (false)
#define destroy_buffer(...)         P99_CALL_DEFARG(destroy_buffer, 2, __VA_ARGS__)
#define destroy_buffer_defarg_1()   (DES_BUF_TALLOC_FREE | DES_BUF_DESTROY_NODE)

#define b_list_dump_nvim(LST) (b_list_dump_nvim)((LST), #LST)

#define nvim_buf_attach(buf_)                                \
        (                                                    \
            (_Generic((buf_),                                \
                      int:      nvim_buf_attach,             \
                      unsigned: nvim_buf_attach,             \
                      uint16_t: nvim_buf_attach,             \
                      Buffer *: nvim_buf_attach_bdata_wrap)  \
            )(buf_)                                          \
        )

#define echo(...)                                                                                                 \
        do {                                                                                                      \
                if (settings.verbose) {                                                                           \
                        P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                                                        \
                          (nvim_out_write(B("tag-highlight: " __VA_ARGS__ "\n")))                                 \
                          (nvim_printf("tag-highlight: " P99_CHS(0, __VA_ARGS__) "\n", P99_SKP(1, __VA_ARGS__))); \
                }                                                                                                 \
        } while (0)

#define ECHO(...)                                                                                                      \
        do {                                                                                                           \
                if (settings.verbose) {                                                                                \
                        P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                                                             \
                          (nvim_out_write(B("tag-highlight: " __VA_ARGS__ "\n")))                                      \
                          (nvim_b_printf(B("tag-highlight: " P99_CHS(0, __VA_ARGS__) "\n"), P99_SKP(1, __VA_ARGS__))); \
                }                                                                                                      \
        } while (0)

#define SHOUT(...)                                                                                \
        P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                                                        \
          (nvim_out_write(B("tag-highlight: " __VA_ARGS__ "\n")))                                 \
          (nvim_printf("tag-highlight: " P99_CHS(0, __VA_ARGS__) "\n", P99_SKP(1, __VA_ARGS__)));


#if 0
#define echo(...) \
        warn_(UTIL_ERRWARN_USE_NVIM_IO, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define ECHO(...)                                                      \
        warn_(UTIL_ERRWARN_USE_NVIM_IO | UTIL_ERRWARN_USE_BSTR_PRINTF, \
              __FILE__, __LINE__, __func__, __VA_ARGS__)

#define SHOUT(...)                                                                          \
        warn_(UTIL_ERRWARN_USE_NVIM_IO | UTIL_ERRWARN_FORCE, \
              __FILE__, __LINE__, __func__, __VA_ARGS__)
#endif

#endif /* macros.h */
