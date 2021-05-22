#ifndef NVIM_API_API_H_
#define NVIM_API_API_H_

#include "Common.h"
#include "contrib/p99/p99_defarg.h"
#include "contrib/p99/p99_if.h"
#include "contrib/p99/p99_list.h"
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
        int32_t    fd;
        int32_t    count;
        mpack_obj *obj;
};


/*============================================================================*/
/* API Wrappers */

extern void           $nvim_write              (enum nvim_write_type type, bstring const *mes);
extern void           nvim_printf              (char const *restrict fmt, ...) __aFMT(1, 2);
extern void           nvim_vprintf             (char const *restrict fmt, va_list args) __aFMT(1, 0);
extern void           nvim_b_printf            (bstring const *fmt, ...);
extern mpack_retval   nvim_get_var_fmt         (mpack_expect_t expect, char const *fmt, ...) __aFMT(2, 3) __aWUR;
extern int            nvim_buf_add_highlight   (unsigned bufnum, int hl_id, bstring const *group, unsigned line, unsigned start, int end);
extern mpack_dict   * nvim_get_hl_by_name      (bstring const *name, bool rgb) __aWUR;
extern mpack_dict   * nvim_get_hl_by_id        (int hlid, bool rgb) __aWUR;
extern void           nvim_buf_attach          (int bufnum);
extern void           nvim_buf_clear_namespace (unsigned bufnum, int ns_id, unsigned start, int end, bool blocking);
extern unsigned       nvim_buf_get_changedtick (int bufnum);
extern b_list       * nvim_buf_get_lines       (unsigned bufnum, int start, int end) __aWUR;
extern bstring      * nvim_buf_get_name        (int bufnum) __aWUR;
extern mpack_retval   nvim_buf_get_option      (int bufnum, bstring const *optname, mpack_expect_t expect) __aWUR;
extern mpack_retval   nvim_buf_get_var         (int bufnum, bstring const *varname, mpack_expect_t expect) __aWUR;
extern unsigned       nvim_buf_line_count      (int bufnum);
extern void           nvim_call_atomic         (struct mpack_arg_array const *calls);
extern mpack_retval   nvim_call_function       (bstring const *function, mpack_expect_t expect) __aWUR;
extern mpack_retval   nvim_call_function_args  (bstring const *function, mpack_expect_t expect, bstring const *fmt, ...) __aWUR;
extern bool           nvim_command             (bstring const *cmd);
extern mpack_retval   nvim_command_output      (bstring const *cmd, mpack_expect_t expect) __aWUR;
extern mpack_retval   nvim_eval                (bstring const *eval, mpack_expect_t expect) __aWUR;
extern void           nvim_get_api_info        (void);
extern int            nvim_get_current_buf     (void);
extern bstring      * nvim_get_current_line    (void) __aWUR;
extern mpack_retval   nvim_get_option          (bstring const *optname, mpack_expect_t expect) __aWUR;
extern mpack_retval   nvim_get_var             (bstring const *varname, mpack_expect_t expect) __aWUR;
extern mpack_retval   nvim_list_bufs           (void) __aWUR;
extern void           nvim_subscribe           (bstring const *event);
extern bool           nvim_set_var             (bstring const *varname, bstring const *fmt, ...);
extern bool           nvim_set_option          (bstring const *optname, bstring const *value);

extern void nvim_set_client_info(bstring const *name, unsigned major, unsigned minor, bstring const *dev,
                                 bstring const *type, void const *methods, void const *attributes);

extern bstring * _nvim_get_notification();

__attribute__((__deprecated__("Removed from neovim's documentation")))
extern void nvim_buf_clear_highlight (unsigned bufnum, int hl_id, unsigned start, int end, bool blocking);

/*----------------------------------------------------------------------------*/

/* Convenience Macros */
#define nvim_out_write(MES) $nvim_write(NW_STANDARD, (MES))
#define nvim_err_write(MES) $nvim_write(NW_ERROR, (MES))
#define NVIM_GET_FUTEX_EXPECT(FD, CNT) (((unsigned)((uint8_t)(FD) << 030) | ((unsigned)(CNT) & 0x00FFFFFFu)) + 1u)

/*----------------------------------------------------------------------------*/
/* Misc helper functions */

/*
 * Request for Neovim to create an additional server socket/fifo, then connect
 * to it. Multiple connections can make multithreaded applications easier to
 * write safely.
 */
extern int  _nvim_create_socket(void);
extern void _nvim_init(void) __attribute__((__constructor__));
extern int  _nvim_get_tmpfile(bstring *restrict*restrict name, const bstring *restrict suffix);


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

#define nvim_buf_clear_highlight(...)       P99_CALL_DEFARG(nvim_buf_clear_highlight, 5, __VA_ARGS__)
#define nvim_buf_clear_highlight_defarg_4() false

/*============================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* api.h */
