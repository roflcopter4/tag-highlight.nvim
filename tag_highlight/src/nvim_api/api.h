#ifndef SRC_API_H
#define SRC_API_H

#include "tag_highlight.h"

#include "data.h"
#include "mpack/mpack.h"
#include "p99/p99_defarg.h"
#include "p99/p99_futex.h"
#include "util/list.h"

__BEGIN_DECLS

enum nvim_filetype_id {
        FT_NONE, FT_C, FT_CPP, FT_CSHARP, FT_GO, FT_JAVA,
        FT_JAVASCRIPT, FT_LISP, FT_PERL, FT_PHP, FT_PYTHON,
        FT_RUBY, FT_RUST, FT_SHELL, FT_VIM, FT_ZSH,
};
enum nvim_message_type    { MES_REQUEST, MES_RESPONSE, MES_NOTIFICATION, MES_ANY };
enum nvim_write_type      { NW_STANDARD, NW_ERROR, NW_ERROR_LN };
enum nvim_connection_type { NVIM_STDOUT, NVIM_SOCKET, NVIM_FILE };

#ifndef __cplusplus
typedef enum   nvim_message_type    nvim_message_type;
typedef enum   nvim_filetype_id     nvim_filetype_id;
typedef enum   nvim_connection_type nvim_connection_type;
#endif
typedef struct nvim_arg_array nvim_arg_array;
typedef union  nvim_argument  nvim_argument;

struct nvim_arg_array {
        char    **fmt;
        union nvim_argument {
                bool     boolean;
                int64_t  num;
                uint64_t uint;
                bstring *str;
                char    *c_str;
        } **args;

        uint32_t qty;
        uint32_t mlen;
};

struct nvim_wait {
        int32_t             fd;
        int32_t             count;
        mpack_obj          *obj;
};


/*============================================================================*/
/* API Wrappers */

extern void           _nvim_write             (int fd, enum nvim_write_type type, const bstring *mes);
extern void           nvim_printf             (int fd, const char *__restrict fmt, ...) __aFMT(2, 3);
extern void           nvim_vprintf            (int fd, const char *__restrict fmt, va_list args) __aFMT(2, 0);
extern void           nvim_b_printf           (int fd, const bstring *fmt, ...);
extern retval_t       nvim_get_var_fmt        (int fd, mpack_expect_t expect, const char *fmt, ...) __aFMT(3, 4) __aWUR;
extern int            nvim_buf_add_highlight  (int fd, unsigned bufnum, int hl_id, const bstring *group, unsigned line, unsigned start, int end);
extern mpack_dict_t * nvim_get_hl_by_name     (int fd, const bstring *name, bool rgb) __aWUR;
extern mpack_dict_t * nvim_get_hl_by_id       (int fd, int hlid, bool rgb) __aWUR;
extern b_list       * nvim_buf_attach         (int fd, int bufnum);
extern void           nvim_buf_clear_highlight(int fd, unsigned bufnum, int hl_id, unsigned start, int end);
extern unsigned       nvim_buf_get_changedtick(int fd, int bufnum);
extern b_list       * nvim_buf_get_lines      (int fd, unsigned bufnum, int start, int end) __aWUR;
extern bstring      * nvim_buf_get_name       (int fd, int bufnum) __aWUR;
extern retval_t       nvim_buf_get_option     (int fd, int bufnum, const bstring *optname, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_buf_get_var        (int fd, int bufnum, const bstring *varname, mpack_expect_t expect) __aWUR;
extern unsigned       nvim_buf_line_count     (int fd, int bufnum);
extern void           nvim_call_atomic        (int fd, const struct nvim_arg_array *calls);
extern retval_t       nvim_call_function      (int fd, const bstring *function, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_call_function_args (int fd, const bstring *function, mpack_expect_t expect, const bstring *fmt, ...) __aWUR;
extern bool           nvim_command            (int fd, const bstring *cmd);
extern retval_t       nvim_command_output     (int fd, const bstring *cmd, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_eval               (int fd, const bstring *eval, mpack_expect_t expect) __aWUR;
extern void           nvim_get_api_info       (int fd);
extern int            nvim_get_current_buf    (int fd);
extern bstring      * nvim_get_current_line   (int fd) __aWUR;
extern retval_t       nvim_get_option         (int fd, const bstring *optname, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_get_var            (int fd, const bstring *varname, mpack_expect_t expect) __aWUR;
extern retval_t       nvim_list_bufs          (int fd) __aWUR;
extern void           nvim_subscribe          (int fd, const bstring *event);
extern bool           nvim_set_var            (int fd, const bstring *varname, const bstring *fmt, ...);

extern void nvim_set_client_info(int fd, const bstring *name, unsigned major, unsigned minor, const bstring *dev,
                                 const bstring *type, const void *methods, const void *attributes);

extern bstring * _nvim_get_notification(int fd);
extern void      _nvim_destroy_arg_array(struct nvim_arg_array *calls);

/* Convenience Macros */
#define nvim_out_write(FD, MES) _nvim_write((FD), NW_STANDARD, (MES))
#define nvim_err_write(FD, MES) _nvim_write((FD), NW_ERROR, (MES))
#define ECHO(FMT_, ...)                                                                       \
        ((settings.verbose) ? nvim_b_printf(0, B("tag_highlight: " FMT_ "\n"), ##__VA_ARGS__) \
                            : (void)0)
#undef APRINTF

#define NVIM_GET_FUTEX_EXPECT(FD, CNT) (((unsigned)((uint8_t)(FD) << 030) | ((unsigned)(CNT) & 0x00FFFFFFu)) + 1u)

/*============================================================================*/
/* Misc helper functions */

/**
 * Request for Neovim to create an additional server socket/fifo, then connect
 * to it. Multiple connections can make multithreaded applications easier to
 * write safely.
 */
extern int _nvim_create_socket(int mes_fd);
/* extern void _nvim_init(enum nvim_connection_type init_type, int init_fd); */
extern void _nvim_init(void) __attribute__((__constructor__));

extern int _nvim_get_tmpfile(const int fd, bstring **name, const bstring *suffix);

/* 
 * Perl output: default arguments for Neovim functions.
 */
#define _nvim_write(...) P99_CALL_DEFARG(_nvim_write, 3, __VA_ARGS__)
#define _nvim_write_defarg_0() (0)
#define nvim_vprintf(...) P99_CALL_DEFARG(nvim_vprintf, 3, __VA_ARGS__)
#define nvim_vprintf_defarg_0() (0)
#define nvim_buf_add_highlight(...) P99_CALL_DEFARG(nvim_buf_add_highlight, 7, __VA_ARGS__)
#define nvim_buf_add_highlight_defarg_0() (0)
#define nvim_buf_add_highlight_defarg_2() (0)
#define nvim_buf_add_highlight_defarg_3() NULL
#define nvim_buf_add_highlight_defarg_4() (0)
#define nvim_buf_add_highlight_defarg_5() (0)
#define nvim_buf_add_highlight_defarg_6() (0)
#define nvim_get_hl_by_name(...) P99_CALL_DEFARG(nvim_get_hl_by_name, 3, __VA_ARGS__)
#define nvim_get_hl_by_name_defarg_0() (0)
#define nvim_get_hl_by_id(...) P99_CALL_DEFARG(nvim_get_hl_by_id, 3, __VA_ARGS__)
#define nvim_get_hl_by_id_defarg_0() (0)
#define nvim_buf_attach(...) P99_CALL_DEFARG(nvim_buf_attach, 2, __VA_ARGS__)
#define nvim_buf_attach_defarg_0() (0)
#define nvim_buf_clear_highlight(...) P99_CALL_DEFARG(nvim_buf_clear_highlight, 5, __VA_ARGS__)
#define nvim_buf_clear_highlight_defarg_0() (0)
/* #define nvim_buf_clear_highlight_defarg_1() nvim_get_current_buf()
#define nvim_buf_clear_highlight_defarg_2() nvim_get_current_buf() */
#define nvim_buf_get_changedtick(...) P99_CALL_DEFARG(nvim_buf_get_changedtick, 2, __VA_ARGS__)
#define nvim_buf_get_changedtick_defarg_0() (0)
#define nvim_buf_get_lines(...) P99_CALL_DEFARG(nvim_buf_get_lines, 4, __VA_ARGS__)
#define nvim_buf_get_lines_defarg_0() (0)
#define nvim_buf_get_lines_defarg_2() (0)
#define nvim_buf_get_lines_defarg_3() (-1)
#define nvim_buf_get_name(...) P99_CALL_DEFARG(nvim_buf_get_name, 2, __VA_ARGS__)
#define nvim_buf_get_name_defarg_0() (0)
#define nvim_buf_get_option(...) P99_CALL_DEFARG(nvim_buf_get_option, 4, __VA_ARGS__)
#define nvim_buf_get_option_defarg_0() (0)
#define nvim_buf_get_var(...) P99_CALL_DEFARG(nvim_buf_get_var, 4, __VA_ARGS__)
#define nvim_buf_get_var_defarg_0() (0)
#define nvim_buf_line_count(...) P99_CALL_DEFARG(nvim_buf_line_count, 2, __VA_ARGS__)
#define nvim_buf_line_count_defarg_0() (0)
#define nvim_call_atomic(...) P99_CALL_DEFARG(nvim_call_atomic, 2, __VA_ARGS__)
#define nvim_call_atomic_defarg_0() (0)
#define nvim_call_function(...) P99_CALL_DEFARG(nvim_call_function, 3, __VA_ARGS__)
#define nvim_call_function_defarg_0() (0)
#define nvim_command(...) P99_CALL_DEFARG(nvim_command, 2, __VA_ARGS__)
#define nvim_command_defarg_0() (0)
#define nvim_command_output(...) P99_CALL_DEFARG(nvim_command_output, 3, __VA_ARGS__)
#define nvim_command_output_defarg_0() (0)
#define nvim_eval(...) P99_CALL_DEFARG(nvim_eval, 3, __VA_ARGS__)
#define nvim_eval_defarg_0() (0)
#define nvim_get_api_info(...) P99_CALL_DEFARG(nvim_get_api_info, 1, __VA_ARGS__)
#define nvim_get_api_info_defarg_0() (0)
#define nvim_get_current_buf(...) P99_CALL_DEFARG(nvim_get_current_buf, 1, __VA_ARGS__)
#define nvim_get_current_buf_defarg_0() (0)
#define nvim_get_current_line(...) P99_CALL_DEFARG(nvim_get_current_line, 1, __VA_ARGS__)
#define nvim_get_current_line_defarg_0() (0)
#define nvim_get_option(...) P99_CALL_DEFARG(nvim_get_option, 3, __VA_ARGS__)
#define nvim_get_option_defarg_0() (0)
#define nvim_get_var(...) P99_CALL_DEFARG(nvim_get_var, 3, __VA_ARGS__)
#define nvim_get_var_defarg_0() (0)
#define nvim_list_bufs(...) P99_CALL_DEFARG(nvim_list_bufs, 1, __VA_ARGS__)
#define nvim_list_bufs_defarg_0() (0)
#define nvim_subscribe(...) P99_CALL_DEFARG(nvim_subscribe, 2, __VA_ARGS__)
#define nvim_subscribe_defarg_0() (0)

#define nvim_set_client_info(...) P99_CALL_DEFARG(nvim_set_client_info, 8, __VA_ARGS__)
#define nvim_set_client_info_defarg_0() (0)
#define nvim_set_client_info_defarg_4() (B(""))
#define nvim_set_client_info_defarg_5() (B("remote"))
#define nvim_set_client_info_defarg_6() NULL
#define nvim_set_client_info_defarg_7() NULL

#define nvim_get_var_fmt(...) (nvim_get_var_fmt)(0, __VA_ARGS__)
#define _nvim_get_tmpfile(...) P99_CALL_DEFARG(_nvim_get_tmpfile, 3, __VA_ARGS__)
#define _nvim_get_tmpfile_defarg_0() (0)
#define _nvim_get_tmpfile_defarg_1() NULL
#define _nvim_get_tmpfile_defarg_2() NULL


/*============================================================================*/
__END_DECLS
#endif /* api.h */
