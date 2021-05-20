#include "Common.h"

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"

/*======================================================================================*/
/* Echo output functions (wrappers for nvim_out_write) */

void
(_nvim_write)(enum nvim_write_type const type, bstring const *mes)
{
        static const bstring fn_names[] = {
                bt_init("nvim_out_write"),
                bt_init("nvim_err_write"),
                bt_init("nvim_err_writeln"),
        };
        
        bstring const *func;

        switch (type) {
        case NW_STANDARD: func = &fn_names[0]; break;
        case NW_ERROR:    func = &fn_names[1]; break;
        case NW_ERROR_LN: func = &fn_names[2]; break;
        default:          errx(1, "Should be unreachable!");
        }

        mpack_obj *result = generic_call(false, func, B("s"), mes);
        assert(result == NULL);
}

void
nvim_printf(char const *const restrict fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *tmp = b_vformat(fmt, ap);
        va_end(ap);

        nvim_out_write(tmp);
        b_destroy(tmp);
}

void
(nvim_vprintf)(char const *const restrict fmt, va_list args)
{
        bstring *tmp = b_vformat(fmt, args);
        nvim_out_write(tmp);
        b_destroy(tmp);
}

void
nvim_b_printf(bstring const *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *tmp = _b_vsprintf(fmt, ap);
        va_end(ap);
        nvim_out_write(tmp);
        if (tmp)
                b_destroy(tmp);
}

/*--------------------------------------------------------------------------------------*/
/* Other wrappers */

mpack_retval
(nvim_get_var_fmt)(mpack_expect_t const expect, char const *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *varname = b_vformat(fmt, ap);
        va_end(ap);

        mpack_retval ret = nvim_get_var(varname, expect);
        b_destroy(varname);
        return ret;
}

/*--------------------------------------------------------------------------------------
 * /================\
 * |BUFFER FUNCTIONS|
 * \================/
 *--------------------------------------------------------------------------------------*/ 

mpack_retval
(nvim_list_bufs)(void)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, NULL);
        return m_expect_intern(result, E_MPACK_ARRAY);
}

int
(nvim_get_current_buf)(void)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, NULL);
        return (int)m_expect_intern(result, E_NUM).num;
}

bstring *
(nvim_get_current_line)(void)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, NULL);
        return m_expect_intern(result, E_STRING).ptr;
}

unsigned
(nvim_buf_line_count)(int const bufnum)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d"), bufnum);
        return (unsigned)m_expect_intern(result, E_NUM).num;
}

b_list *
(nvim_buf_get_lines)(unsigned const bufnum,
                     int      const start,
                     int      const end)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d,d,d,d"), bufnum, start, end, 0);
        return m_expect_intern(result, E_STRLIST).ptr;
}


bstring *
(nvim_buf_get_name)(int const bufnum)
{
        static const bstring fn = BS_FROMARR(__func__);
        char       fullname[PATH_MAX + 1];
        mpack_obj *result = generic_call(true, &fn, B("d"), bufnum);
        bstring   *ret    = m_expect_intern(result, E_STRING).ptr;
        b_assign_cstr(ret, realpath(BS(ret), fullname));
        return ret;
}

/*--------------------------------------------------------------------------------------
 * /================\
 * |GLOBAL FUNCTIONS|
 * \================/
 *--------------------------------------------------------------------------------------*/ 

/*--------------------------------------------------------------------------------------*/
/* Functions and commands */

bool
(nvim_command)(bstring const *cmd)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), cmd);
        mpack_obj *error  = mpack_index(result, 2);
        ALWAYS_ASSERT(error != NULL);
        bool const ret = mpack_type(error) == MPACK_NIL;

        if (mpack_type(error) == MPACK_ARRAY) {
                error = mpack_index(error, 1);
                ALWAYS_ASSERT(error != NULL && mpack_type(error) == MPACK_STRING);
                b_fwrite(stderr, error->str, B("\n"));
        }
        talloc_free(result);

        return ret;
}

mpack_retval
(nvim_command_output)(bstring const *cmd, mpack_expect_t const expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), cmd);
        return m_expect_intern(result, expect);
}

mpack_retval
(nvim_call_function)(bstring const *function, mpack_expect_t const expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s,[]"), function);
        return m_expect_intern(result, expect);
}

mpack_retval
(nvim_call_function_args)(bstring        const *function,
                          mpack_expect_t const  expect,
                          bstring        const *fmt,
                          ...)
{
        static const bstring fn  = bt_init("nvim_call_function");
        bstring       *buf = b_sprintf("s,[!%s]", fmt);
        va_list        ap;

        va_start(ap, fmt);
        mpack_obj *result = generic_call(true, &fn, buf, function, &ap);
        va_end(ap);

        b_destroy(buf);
        return m_expect_intern(result, expect);
}

mpack_retval
(nvim_eval)(bstring const *eval, mpack_expect_t const expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), eval);
        return m_expect_intern(result, expect);
}

/*--------------------------------------------------------------------------------------*/

mpack_retval
(nvim_get_var)(bstring const *varname, mpack_expect_t const expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), varname);
        return m_expect_intern(result, expect);
}

mpack_retval
(nvim_get_option)(bstring const *optname, mpack_expect_t const expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), optname);
        return m_expect_intern(result, expect);
}

mpack_retval
(nvim_buf_get_var)(int const             bufnum,
                   bstring        const *varname,
                   mpack_expect_t const  expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d,s"), bufnum, varname);
        return m_expect_intern(result, expect);
}

mpack_retval
(nvim_buf_get_option)(int const             bufnum,
                      bstring        const *optname,
                      mpack_expect_t const  expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d,s"), bufnum, optname);
        return m_expect_intern(result, expect);
}

unsigned
(nvim_buf_get_changedtick)(int const bufnum)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d"), bufnum);
        return (unsigned)m_expect_intern(result, E_NUM).num;
}

/*--------------------------------------------------------------------------------------*/

bool
nvim_set_var(bstring const *varname, bstring const *fmt, ...)
{
        va_list  ap;
        static const bstring fn  = BS_FROMARR(__func__);
        bstring       *tmp = b_sprintf("[s,!%s]", fmt);

        va_start(ap, fmt);
        mpack_obj *result = generic_call(true, &fn, tmp, varname, &ap);
        va_end(ap);

        b_destroy(tmp);
        return mpack_type(mpack_index(result, 2)) == MPACK_NIL;
}

/*--------------------------------------------------------------------------------------*/
/* Highlight related functions */

int
(nvim_buf_add_highlight)(unsigned const  bufnum,
                         int      const  hl_id,
                         bstring  const *group,
                         unsigned const  line,
                         unsigned const  start,
                         int      const  end)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("dd,s,ddd"), bufnum,
                                         hl_id, group, line, start, end);
        return (int)m_expect_intern(result, E_NUM).num;
}

void
(nvim_buf_clear_highlight)(unsigned const bufnum,
                           int      const hl_id,
                           unsigned const start,
                           int      const end,
                           bool     const blocking)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *obj = generic_call(blocking, &fn, B("dd,dd"), bufnum, hl_id, start, end);
        talloc_free(obj);
}

mpack_dict *
(nvim_get_hl_by_name)(bstring const *name, bool const rgb)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("sB"), name, rgb);
        return m_expect_intern(result, E_MPACK_DICT).ptr;
}

mpack_dict *
(nvim_get_hl_by_id)(int const hlid, bool const rgb)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("dB"), hlid, rgb);
        return m_expect_intern(result, E_MPACK_DICT).ptr;
}

/*--------------------------------------------------------------------------------------*/

void
(nvim_get_api_info)(void)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, NULL);
        talloc_free(result);
}

void
(nvim_subscribe)(bstring const *event)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), event);
        talloc_free(result);
}

void
(nvim_buf_attach)(int const bufnum)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d,B,[]"), bufnum, false);
        talloc_free(result);
}

void
(nvim_set_client_info)(bstring  const *name,
                       unsigned const  major,
                       unsigned const  minor,
                       bstring  const *dev,
                       bstring  const *type,
                       UNUSED void const *methods,
                       UNUSED void const *attributes)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(false, &fn, B("s; {s:d, s:d, s:s}; s; {}; {}"),
                                         name,
                                         B("major"), major,
                                         B("minor"), minor,
                                         B("dev"), dev,
                                         type);
        assert(result == NULL);
}

/*======================================================================================*/
/* The single most important api function gets its own section for no reason. */

void
(nvim_call_atomic)(mpack_arg_array const *calls)
{
#if 0 && defined DEBUG
        static bool first = true;
        if (first) {
                char tmp[PATH_MAX];
                snprintf(tmp, PATH_MAX, "%s/.tag-highlight_log/atomic.log", HOME);
                unlink(tmp);
                first = false;
        }
#endif
        
        static bstring  fn  = BS_FROMARR(__func__);
        bstring        *fmt = b_fromcstr_alloc(4096, "[d,d,s:[:");

        if (calls->qty) {
                b_sprintfa(fmt, "[ @[%n],", calls->fmt[0]);
                for (unsigned i = 1; i < calls->qty; ++i)
                        b_sprintfa(fmt, "[*%n],", calls->fmt[i]);
                b_catlit(fmt, " ]");
        }
        b_catlit(fmt, ":]]");

        int const  count = INC_COUNT();
        mpack_obj *pack  = mpack_encode_fmt(calls->qty, BS(fmt), MES_REQUEST,
                                            count, &fn, calls->args);
        b_destroy(fmt);
        mpack_obj *result = special_call(false, &fn, pack, count);
        assert(result == NULL);
}
