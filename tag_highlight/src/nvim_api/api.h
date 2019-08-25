#pragma once
#ifndef NVIM_API_API_H_
#define NVIM_API_API_H_

#include "Common.h"
#include "highlight.h"
#include "my_p99_common.h"

#include "contrib/p99/p99_defarg.h"
#include "contrib/p99/p99_futex.h"
#include "contrib/p99/p99_fifo.h"
#include "mpack/mpack.h"
#include "util/list.h"

#ifdef __cplusplus
extern "C" {
#endif

enum nvim_filetype_id {
        FT_NONE, FT_C, FT_CXX, FT_CSHARP, FT_GO, FT_JAVA,
        FT_JAVASCRIPT, FT_LISP, FT_PERL, FT_PHP, FT_PYTHON,
        FT_RUBY, FT_RUST, FT_SHELL, FT_VIM, FT_ZSH,
};
enum nvim_message_type    { MES_REQUEST, MES_RESPONSE, MES_NOTIFICATION, MES_ANY };
enum nvim_write_type      { NW_STANDARD, NW_ERROR, NW_ERROR_LN };
enum nvim_connection_type { NVIM_STDOUT, NVIM_SOCKET, NVIM_FILE };

typedef enum nvim_message_type    nvim_message_type;
typedef enum nvim_filetype_id     nvim_filetype_id;
typedef enum nvim_connection_type nvim_connection_type;

struct nvim_wait {
        int32_t             fd;
        int32_t             count;
        mpack_obj          *obj;
};


/*============================================================================*/
/* API Wrappers */

extern void           _nvim_write             (enum nvim_write_type type, const bstring *mes);
extern void           nvim_printf             (const char *__restrict fmt, ...) __aFMT(1, 2);
extern void           nvim_vprintf            (const char *__restrict fmt, va_list args) __aFMT(1, 0);
extern void           nvim_b_printf           (const bstring *fmt, ...);
extern retval_t       nvim_get_var_fmt        (mpack_expect_t expect, const char *fmt, ...) __aFMT(2, 3) __aWUR;
extern int            nvim_buf_add_highlight  (unsigned bufnum, int hl_id, const bstring *group, unsigned line, unsigned start, int end);
extern mpack_dict_t * nvim_get_hl_by_name     (const bstring *name, bool rgb) __aWUR;
extern mpack_dict_t * nvim_get_hl_by_id       (int hlid, bool rgb) __aWUR;
extern b_list       * nvim_buf_attach         (int bufnum);
extern void           nvim_buf_clear_highlight(unsigned bufnum, int hl_id, unsigned start, int end);
extern unsigned       nvim_buf_get_changedtick(int bufnum);
extern b_list       * nvim_buf_get_lines      (unsigned bufnum, int start, int end) __aWUR;
extern bstring      * nvim_buf_get_name       (int bufnum) __aWUR;
extern retval_t       nvim_buf_get_option     (int bufnum, const bstring *optname, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_buf_get_var        (int bufnum, const bstring *varname, mpack_expect_t expect) __aWUR;
extern unsigned       nvim_buf_line_count     (int bufnum);
extern void           nvim_call_atomic        (const struct mpack_arg_array *calls);
extern retval_t       nvim_call_function      (const bstring *function, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_call_function_args (const bstring *function, mpack_expect_t expect, const bstring *fmt, ...) __aWUR;
extern bool           nvim_command            (const bstring *cmd);
extern retval_t       nvim_command_output     (const bstring *cmd, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_eval               (const bstring *eval, mpack_expect_t expect) __aWUR;
extern void           nvim_get_api_info       (void);
extern int            nvim_get_current_buf    (void);
extern bstring      * nvim_get_current_line   (void) __aWUR;
extern retval_t       nvim_get_option         (const bstring *optname, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_get_var            (const bstring *varname, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_list_bufs          (void) __aWUR;
extern void           nvim_subscribe          (const bstring *event);
extern bool           nvim_set_var            (const bstring *varname, const bstring *fmt, ...);

extern void nvim_set_client_info(const bstring *name, unsigned major, unsigned minor, const bstring *dev,
                                 const bstring *type, const void *methods, const void *attributes);

extern bstring * _nvim_get_notification();

/* Convenience Macros */
#define nvim_out_write(MES) _nvim_write(NW_STANDARD, (MES))
#define nvim_err_write(MES) _nvim_write(NW_ERROR, (MES))

#if 0
#define echo(FMT_, ...)                                                                  \
        ((settings.verbose) ? nvim_printf(0, "tag_highlight: " FMT_ "\n", ##__VA_ARGS__) \
                            : NOP)                                                        
#endif


#define echo(...)                                                                                                       \
        do {                                                                                                            \
                if (settings.verbose) {                                                                                 \
                        P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                                                              \
                        (nvim_out_write(B("echo1 -- tag_highlight: " __VA_ARGS__ "\n")))                                 \
                        (nvim_printf("echo2 -- tag_highlight: " P99_CHS(0, __VA_ARGS__) "\n", P99_SKP(1, __VA_ARGS__))); \
                }                                                                                                       \
        } while (0)

#define ECHO(...)                                                                                                             \
        do {                                                                                                                  \
                if (settings.verbose) {                                                                                       \
                        P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                                                                    \
                        (nvim_out_write(B("ECHO1 -- tag_highlight: " __VA_ARGS__ "\n")))                                      \
                        (nvim_b_printf(B("ECHO2 -- tag_highlight: " P99_CHS(0, __VA_ARGS__) "\n"), P99_SKP(1, __VA_ARGS__))); \
                }                                                                                                             \
        } while (0)

#if 0
#define echo(...) P99_IF_EQ_1(P99_NARG(__VA_ARGS__))\
        (nvim_out_write(B("echo1-- tag_highlight: " __VA_ARGS__ "\n")))\
        (nvim_printf("echo2-- tag_highlight: " P99_CHS(0, __VA_ARGS__) "\n", P99_SKP(1, __VA_ARGS__)))

#define ECHO(FMT_, ...)                                                                       \
        ((settings.verbose) ? nvim_b_printf(0, B("tag_highlight: " FMT_ "\n"), ##__VA_ARGS__) \
                            : NOP)
#endif
#undef APRINTF

#define NVIM_GET_FUTEX_EXPECT(FD, CNT) (((unsigned)((uint8_t)(FD) << 030) | ((unsigned)(CNT) & 0x00FFFFFFu)) + 1u)

/*============================================================================*/
/* Misc helper functions */

/**
 * Request for Neovim to create an additional server socket/fifo, then connect
 * to it. Multiple connections can make multithreaded applications easier to
 * write safely.
 */
extern int _nvim_create_socket(void);
/* extern void _nvim_init(enum nvim_connection_type init_type, int init_fd); */
extern void _nvim_init(void) __attribute__((__constructor__));

extern int _nvim_get_tmpfile(bstring *restrict*restrict name, const bstring *restrict suffix);

P44_DECLARE_FIFO(_nvim_wait_node);
struct _nvim_wait_node {
        int              fd;
        unsigned         count;
        p99_futex        fut;
        mpack_obj       *obj;
        _nvim_wait_node_ptr p99_fifo;
};
P99_FIFO(_nvim_wait_node_ptr) _nvim_wait_queue;

/*============================================================================*/
extern int _nvim_api_read_fd;

/* 
 * Perl output: default arguments for Neovim functions.
 */
#define nvim_buf_add_highlight(...)         P99_CALL_DEFARG(nvim_buf_add_highlight, 6, __VA_ARGS__)
#define nvim_buf_add_highlight_defarg_1()   0
#define nvim_buf_add_highlight_defarg_2()   NULL
#define nvim_buf_add_highlight_defarg_3()   0
#define nvim_buf_add_highlight_defarg_4()   0
#define nvim_buf_add_highlight_defarg_5()   0

#define nvim_buf_get_lines(...)             P99_CALL_DEFARG(nvim_buf_get_lines, 3, __VA_ARGS__)
#define nvim_buf_get_lines_defarg_1()       0
#define nvim_buf_get_lines_defarg_2()       (-1)

#define nvim_set_client_info(...)           P99_CALL_DEFARG(nvim_set_client_info, 7, __VA_ARGS__)
#define nvim_set_client_info_defarg_3()     (B(""))
#define nvim_set_client_info_defarg_4()     (B("remote"))
#define nvim_set_client_info_defarg_5()     NULL
#define nvim_set_client_info_defarg_6()     NULL

#define _nvim_get_tmpfile(...)              P99_CALL_DEFARG(_nvim_get_tmpfile, 2, __VA_ARGS__)
#define _nvim_get_tmpfile_defarg_0()        NULL
#define _nvim_get_tmpfile_defarg_1()        NULL

/*============================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* api.h */
