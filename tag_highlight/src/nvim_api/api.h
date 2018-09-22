#ifndef SRC_API_H
#define SRC_API_H

#include "bstring/bstring.h"
#include "data.h"
#include "mpack/mpack.h"
#include "p99/p99_defarg.h"
#include "p99/p99_futex.h"
#include "util/list.h"

#define APRINTF(a,b) __attribute__((__format__(__printf__, a, b)))

#ifndef __GNUC__
#  define __attribute__((...))
#endif
#ifdef __cplusplus
extern "C" {
#endif

extern genlist *wait_list;

enum filetype_id {
        FT_NONE, FT_C, FT_CPP, FT_CSHARP, FT_GO, FT_JAVA,
        FT_JAVASCRIPT, FT_LISP, FT_PERL, FT_PHP, FT_PYTHON,
        FT_RUBY, FT_RUST, FT_SHELL, FT_VIM, FT_ZSH,
};
enum message_types   { MES_REQUEST, MES_RESPONSE, MES_NOTIFICATION, MES_ANY };
enum nvim_write_type { NW_STANDARD, NW_ERROR, NW_ERROR_LN };
enum nvim_connection_type { NVIM_STDOUT, NVIM_SOCKET, NVIM_FILE };

typedef struct atomic_call_array nvim_call_array;
typedef union  atomic_call_args  nvim_call_arg;

struct atomic_call_array {
        char    **fmt;
        union atomic_call_args {
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
        enum message_types mtype;
        int16_t            fd;
        int32_t            count;
        /* pthread_cond_t     cond; */
        p99_futex          fut;
        mpack_obj         *obj;
};


/*============================================================================*/
/* API Wrappers */

extern void           _nvim_write             (int fd, enum nvim_write_type type, const bstring *mes);
extern void           nvim_printf             (int fd, const char *__restrict fmt, ...) APRINTF(2, 3);
extern void           nvim_vprintf            (int fd, const char *__restrict fmt, va_list args) APRINTF(2, 0);
extern void           nvim_b_printf           (int fd, const bstring *fmt, ...);
extern retval_t       nvim_get_var_fmt        (int fd, mpack_expect_t expect, const char *fmt, ...) APRINTF(3, 4);
extern int            nvim_buf_add_highlight  (int fd, unsigned bufnum, int hl_id, const bstring *group, unsigned line, unsigned start, int end);
extern mpack_dict_t * nvim_get_hl_by_name     (int fd, const bstring *name, bool rgb);
extern mpack_dict_t * nvim_get_hl_by_id       (int fd, int hlid, bool rgb);
extern b_list       * nvim_buf_attach         (int fd, int bufnum);
extern void           nvim_buf_clear_highlight(int fd, unsigned bufnum, int hl_id, unsigned start, int end);
extern unsigned       nvim_buf_get_changedtick(int fd, int bufnum);
extern b_list       * nvim_buf_get_lines      (int fd, unsigned bufnum, int start, int end);
extern bstring      * nvim_buf_get_name       (int fd, int bufnum);
extern retval_t       nvim_buf_get_option     (int fd, int bufnum, const bstring *optname, mpack_expect_t expect);
extern retval_t       nvim_buf_get_var        (int fd, int bufnum, const bstring *varname, mpack_expect_t expect);
extern unsigned       nvim_buf_line_count     (int fd, int bufnum);
extern void           nvim_call_atomic        (int fd, const struct atomic_call_array *calls);
extern retval_t       nvim_call_function      (int fd, const bstring *function, mpack_expect_t expect);
extern retval_t       nvim_call_function_args (int fd, const bstring *function, mpack_expect_t expect, const bstring *fmt, ...);
extern bool           nvim_command            (int fd, const bstring *cmd);
extern retval_t       nvim_command_output     (int fd, const bstring *cmd, mpack_expect_t expect);
extern void           nvim_get_api_info       (int fd);
extern int            nvim_get_current_buf    (int fd);
extern bstring      * nvim_get_current_line   (int fd);
extern retval_t       nvim_get_option         (int fd, const bstring *optname, mpack_expect_t expect);
extern retval_t       nvim_get_var            (int fd, const bstring *varname, mpack_expect_t expect);
extern retval_t       nvim_list_bufs          (int fd);
extern void           nvim_subscribe          (int fd, const bstring *event);
extern bool           nvim_set_var            (int fd, const bstring *varname, const bstring *fmt, ...);

extern bstring * get_notification  (int fd);
extern void      destroy_call_array(struct atomic_call_array *calls);

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

#if 0
#define _nvim_init(...) P99_CALL_DEFARG(_nvim_init, 2, __VA_ARGS__)
#ifdef DOSISH
#  define _nvim_init_defarg_0() (NVIM_FILE)
#else
#  define _nvim_init_defarg_0() (NVIM_SOCKET)
#endif
#define _nvim_init_defarg_1() (1)
#endif


/*============================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* api.h */
