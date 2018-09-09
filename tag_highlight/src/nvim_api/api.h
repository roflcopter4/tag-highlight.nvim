#ifndef SRC_API_H
#define SRC_API_H

#include "bstring/bstring.h"
#include "data.h"
#include "mpack/mpack.h"
#include "util/list.h"

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
enum message_types { MES_REQUEST, MES_RESPONSE, MES_NOTIFICATION, MES_ANY };
enum nvim_write_type { NW_STANDARD, NW_ERROR, NW_ERROR_LN };

struct nvim_wait {
        enum message_types mtype;
        int16_t            fd;
        int32_t            count;
        pthread_cond_t    *cond;
        mpack_obj         *obj;
};

struct nvim_buffer {
        struct {
                bstring full[1];
                bstring base[1];
                bstring path[1];
        } name;

        /* const struct nvim_filetype *ft; */
        linked_list *contents;
        uint32_t     ctick;
        uint16_t     num;
        uint16_t     flags;
        /* enum filetype_id id; */
};

/* struct nvim_filetype {                  */
/*         const bstring          name[1]; */
/*         const enum filetype_id id;      */
/* };                                      */


/*============================================================================*/
/* API Wrappers */
extern void       __nvim_write    (int fd, enum nvim_write_type type, const bstring *mes);
extern void       nvim_printf     (int fd, const char *__restrict fmt, ...) __attribute__((__format__(printf,2, 3)));
extern void       nvim_vprintf    (int fd, const char *__restrict fmt, va_list args);
extern void       nvim_b_printf   (int fd, const bstring *fmt, ...);
extern retval_t   nvim_get_var_fmt(int fd, mpack_expect_t expect, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

extern int        nvim_buf_add_highlight  (int fd, unsigned bufnum, int hl_id, const bstring *group, unsigned line, unsigned start, int end);
extern b_list   * nvim_buf_attach         (int fd, int bufnum);
extern void       nvim_buf_clear_highlight(int fd, unsigned bufnum, int hl_id, unsigned start, int end);
extern unsigned   nvim_buf_get_changedtick(int fd, int bufnum);
extern b_list   * nvim_buf_get_lines      (int fd, unsigned bufnum, int start, int end);
extern bstring  * nvim_buf_get_name       (int fd, int bufnum);
extern retval_t   nvim_buf_get_option     (int fd, int bufnum, const bstring *optname, mpack_expect_t expect);
extern retval_t   nvim_buf_get_var        (int fd, int bufnum, const bstring *varname, mpack_expect_t expect);
extern unsigned   nvim_buf_line_count     (int fd, int bufnum);
extern void       nvim_call_atomic        (int fd, const struct atomic_call_array *calls);
extern retval_t   nvim_call_function      (int fd, const bstring *function, mpack_expect_t expect);
extern retval_t   nvim_call_function_args (int fd, const bstring *function, mpack_expect_t expect, const bstring *fmt, ...);
extern bool       nvim_command            (int fd, const bstring *cmd);
extern retval_t   nvim_command_output     (int fd, const bstring *cmd, mpack_expect_t expect);
extern void       nvim_get_api_info       (int fd);
extern int        nvim_get_current_buf    (int fd);
extern bstring  * nvim_get_current_line   (int fd);
extern retval_t   nvim_get_option         (int fd, const bstring *optname, mpack_expect_t expect);
extern retval_t   nvim_get_var            (int fd, const bstring *varname, mpack_expect_t expect);
extern retval_t   nvim_list_bufs          (int fd);
extern void       nvim_subscribe          (int fd, const bstring *event);
extern bool       nvim_set_var            (int fd, const bstring *varname, const bstring *fmt, ...);

extern bstring * get_notification(int fd);

/* Convenience Macros */
#define nvim_out_write(FD, MES) __nvim_write((FD), NW_STANDARD, (MES))
#define nvim_err_write(FD, MES) __nvim_write((FD), NW_ERROR, (MES))
#define ECHO(FMT_, ...)                                                                       \
        ((settings.verbose) ? nvim_b_printf(0, B("tag_highlight: " FMT_ "\n"), ##__VA_ARGS__) \
                            : (void)0)

/*============================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* api.h */