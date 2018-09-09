#include "util/util.h"
#include <dirent.h>

#include "nvim_api/api.h"
#include "intern.h"
#include "mpack/mpack.h"

/*======================================================================================*/

void
__nvim_write(int fd, const enum nvim_write_type type, const bstring *mes)
{
        pthread_mutex_lock(&mpack_main_mutex);
        CHECK_DEF_FD(fd);
        bstring *func;
        switch (type) {
        case NW_STANDARD: func = B("nvim_out_write");   break;
        case NW_ERROR:    func = B("nvim_err_write");   break;
        case NW_ERROR_LN: func = B("nvim_err_writeln"); break;
        default:          errx(1, "Should be unreachable!");
        }

        const int  count = INC_COUNT(fd);
        mpack_obj *pack  = encode_fmt(0, "[d,d,s:[s]]", MES_REQUEST, count, func, mes);
        write_and_clean(fd, pack, func);
        /* mpack_obj *tmp  = decode_stream(fd, MES_RESPONSE); */
        mpack_obj *tmp = await_package(fd, count, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);

        mpack_print_object(mpack_log, tmp);
        mpack_destroy(tmp);
}

void
nvim_printf(const int fd, const char *const __restrict fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *tmp = b_vformat(fmt, ap);
        va_end(ap);

        nvim_out_write(fd, tmp);
        b_destroy(tmp);
}

void
nvim_vprintf(const int fd, const char *const __restrict fmt, va_list args)
{
        bstring *tmp = b_vformat(fmt, args);
        nvim_out_write(fd, tmp);
        b_destroy(tmp);
}

void
nvim_b_printf(const int fd, const bstring *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *tmp = b_vsprintf(fmt, ap);
        va_end(ap);
        nvim_out_write(fd, tmp);
        b_free(tmp);
}

/*--------------------------------------------------------------------------------------*/

#define VALUE (m_index(result, 3))


b_list *
nvim_buf_attach(int fd, const int bufnum)
{
        static bstring fn = BS_FROMARR(__func__);
#if 0
        pthread_mutex_lock(&mpack_main_mutex);

        CHECK_DEF_FD(fd);
        mpack_obj *pack = encode_fmt(0, "[d,d,s:[d,B,[]]:]", MES_REQUEST,
                                     INC_COUNT(fd), &fn, bufnum, false);
        write_and_clean(fd, pack, &fn);

        /* Don't wait for the response, it will be handled in the main thread. */
        pthread_mutex_unlock(&mpack_main_mutex);
#endif
        mpack_obj *result = generic_call(&fd, &fn, B("d,B,[]"), bufnum, false);
        /* mpack_destroy(result); */
        PRINT_AND_DESTROY(result);
        return NULL;
}

/*--------------------------------------------------------------------------------------*/

retval_t
nvim_list_bufs(int fd)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, NULL);
        return m_expect_intern(result, E_MPACK_ARRAY);
}

int
nvim_get_current_buf(int fd)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, NULL);
        return (int)m_expect_intern(result, E_NUM).num;
}

bstring *
nvim_get_current_line(int fd)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, NULL);
        return m_expect_intern(result, E_STRING).ptr;
}

unsigned
nvim_buf_line_count(int fd, const int bufnum)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("d"), bufnum);
        return (unsigned)m_expect_intern(result, E_NUM).num;
}

b_list *
nvim_buf_get_lines(int fd, const unsigned bufnum, const int start, const int end)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("d,d,d,d"), bufnum, start, end, 0);
        return m_expect_intern(result, E_STRLIST).ptr;
}


bstring *
nvim_buf_get_name(int fd, const int bufnum)
{
        static bstring fn = BS_FROMARR(__func__);
        char       fullname[PATH_MAX + 1];
        mpack_obj *result = generic_call(&fd, &fn, B("d"), bufnum);
        bstring   *ret    = m_expect_intern(result, E_STRING).ptr;
        b_assign_cstr(ret, realpath(BS(ret), fullname));
        return ret;
}

/*--------------------------------------------------------------------------------------*/

bool
nvim_command(int fd, const bstring *cmd)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("s"), cmd);
        const bool ret    = mpack_type(m_index(result, 2)) == MPACK_NIL;

        if (mpack_type(result->data.arr->items[2]) == MPACK_ARRAY) {
                bstring *errmsg = result->DAI[2]->DAI[1]->data.str;
                b_fputs(stderr, errmsg, B("\n"));
        }
        mpack_destroy(result);

        return ret;
}

retval_t
nvim_command_output(int fd, const bstring *cmd, const mpack_expect_t expect)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("s"), cmd);
        return m_expect_intern(result, expect);
}

retval_t
nvim_call_function(int fd, const bstring *function, const mpack_expect_t expect)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("s,[]"), function);
        return m_expect_intern(result, expect);
}

retval_t
nvim_call_function_args(int fd, const bstring *function, const mpack_expect_t expect,
                        const bstring *fmt, ...)
{
        static bstring fn  = bt_init("nvim_call_function");
        bstring       *buf = b_sprintf(B("s,[!%s]"), fmt);
        va_list        ap;

        va_start(ap, fmt);
        mpack_obj *result = generic_call(&fd, &fn, buf, function, &ap);
        va_end(ap);

        b_free(buf);
        return m_expect_intern(result, expect);
}

/*--------------------------------------------------------------------------------------*/

retval_t
nvim_get_var(int fd, const bstring *varname, const mpack_expect_t expect)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("s"), varname);
        return m_expect_intern(result, expect);
}

retval_t
nvim_get_option(int fd, const bstring *optname, const mpack_expect_t expect)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("s"), optname);
        return m_expect_intern(result, expect);
}

retval_t
nvim_buf_get_var(int fd, const int bufnum, const bstring *varname, const mpack_expect_t expect)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("d,s"), bufnum, varname);
        return m_expect_intern(result, expect);
}

retval_t
nvim_buf_get_option(int fd, const int bufnum, const bstring *optname, const mpack_expect_t expect)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("d,s"), bufnum, optname);
        return m_expect_intern(result, expect);
}

unsigned
nvim_buf_get_changedtick(int fd, const int bufnum)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("d"), bufnum);
        return (unsigned)m_expect_intern(result, E_NUM).num;
}

/*--------------------------------------------------------------------------------------*/

bool
nvim_set_var(int fd, const bstring *varname, const bstring *fmt, ...)
{
        va_list  ap;
        static bstring fn  = BS_FROMARR(__func__);
        bstring       *tmp = b_sprintf(B("[s,!%s]"), fmt);

        va_start(ap, fmt);
        mpack_obj *result = generic_call(&fd, &fn, tmp, varname, &ap);
        va_end(ap);

        b_free(tmp);
        return mpack_type(m_index(result, 2)) == MPACK_NIL;
}

/*--------------------------------------------------------------------------------------*/

int
nvim_buf_add_highlight(      int       fd,
                       const unsigned  bufnum,
                       const int       hl_id,
                       const bstring  *group,
                       const unsigned  line,
                       const unsigned  start,
                       const int       end)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("dd,s,ddd"), bufnum,
                                         hl_id, group, line, start, end);
        return (int)m_expect_intern(result, E_NUM).num;
}

void
nvim_buf_clear_highlight(      int      fd,
                         const unsigned bufnum,
                         const int      hl_id,
                         const unsigned start,
                         const int      end)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("dd,dd"), bufnum, hl_id, start, end);
        PRINT_AND_DESTROY(result);
}

/*--------------------------------------------------------------------------------------*/

void
nvim_get_api_info(int fd)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, NULL);
        PRINT_AND_DESTROY(result);
}

void
nvim_subscribe(int fd, const bstring *event)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("s"), event);
        PRINT_AND_DESTROY(result);
}

void
nvim_call_atomic(int fd, const struct atomic_call_array *calls)
{
        CHECK_DEF_FD(fd);
        static bstring fn             = BS_FROMARR(__func__);
        union atomic_call_args **args = calls->args;
        bstring                 *fmt  = b_fromcstr_alloc(4096, "[d,d,s:[:");

        if (calls->qty) {
                b_sprintfa(fmt, B("[ @[%n],"), calls->fmt[0]);

                for (unsigned i = 1; i < calls->qty; ++i)
                        b_sprintfa(fmt, B("[*%n],"), calls->fmt[i]);

                b_catlit(fmt, " ]");
        }
        b_catlit(fmt, ":]]");

        pthread_mutex_lock(&mpack_main_mutex);
        const int  count = INC_COUNT(fd);
        mpack_obj *pack  = encode_fmt(calls->qty, BS(fmt), MES_REQUEST,
                                      count, &fn, args);
        write_and_clean(fd, pack, &fn);

        /* mpack_obj *result = decode_stream(fd, MES_RESPONSE); */
        mpack_obj *result = await_package(fd, count, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        PRINT_AND_DESTROY(result);
        b_destroy(fmt);
}

/*======================================================================================*/

retval_t
nvim_get_var_fmt(const int fd, const mpack_expect_t expect, const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *varname = b_vformat(fmt, ap);
        va_end(ap);

        retval_t ret = nvim_get_var(fd, varname, expect);
        b_destroy(varname);
        return ret;
}