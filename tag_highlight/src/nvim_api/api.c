#include "Common.h"

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"

/*======================================================================================*/
/* Echo output functions (wrappers for nvim_out_write) */

void
(_nvim_write)(const enum nvim_write_type type, const bstring *mes)
{
        static const bstring fn_names[] = {
                bt_init("nvim_out_write"),
                bt_init("nvim_err_write"),
                bt_init("nvim_err_writeln"),
        };
        
        const bstring *func;

        switch (type) {
        case NW_STANDARD: func = &fn_names[0]; break;
        case NW_ERROR:    func = &fn_names[1]; break;
        case NW_ERROR_LN: func = &fn_names[2]; break;
        default:          errx(1, "Should be unreachable!");
        }

        /* const int  count = INC_COUNT(); */
        /* mpack_obj *pack  = mpack_encode_fmt(0, "[d,d,s,[s]]", MES_REQUEST, count, func, mes); */

        generic_call(false, func, B("s"), mes);

        /* (void)write_and_clean(pack, count, func, false); */

        /* struct gencall *gc = malloc(sizeof *gc); */
        /* *gc    = (struct gencall){pack, func, count}; */
        /* START_DETACHED_PTHREAD(generic_call_threaded, gc); */

        /* mpack_obj *tmp   = write_and_clean(pack, count, func, false); */
        /* mpack_print_object(mpack_log, tmp);                           */
        /* mpack_destroy_object(tmp);                                    */
}

void
nvim_printf(const char *const restrict fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *tmp = b_vformat(fmt, ap);
        va_end(ap);

        nvim_out_write(tmp);
        b_destroy(tmp);
}

void
(nvim_vprintf)(const char *const restrict fmt, va_list args)
{
        bstring *tmp = b_vformat(fmt, args);
        nvim_out_write(tmp);
        b_destroy(tmp);
}

void
nvim_b_printf(const bstring *fmt, ...)
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

retval_t
(nvim_get_var_fmt)(const mpack_expect_t expect, const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *varname = b_vformat(fmt, ap);
        va_end(ap);

        retval_t ret = nvim_get_var(varname, expect);
        b_destroy(varname);
        return ret;
}

/*--------------------------------------------------------------------------------------
 * /================\
 * |BUFFER FUNCTIONS|
 * \================/
 *--------------------------------------------------------------------------------------*/ 

retval_t
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
(nvim_buf_line_count)(const int bufnum)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d"), bufnum);
        return (unsigned)m_expect_intern(result, E_NUM).num;
}

b_list *
(nvim_buf_get_lines)(const unsigned bufnum, const int start, const int end)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d,d,d,d"), bufnum, start, end, 0);
        return m_expect_intern(result, E_STRLIST).ptr;
}


bstring *
(nvim_buf_get_name)(const int bufnum)
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
(nvim_command)(const bstring *cmd)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), cmd);
        const bool ret    = mpack_type(m_index(result, 2)) == MPACK_NIL;

        if (mpack_type(result->data.arr->items[2]) == MPACK_ARRAY) {
                bstring *errmsg = result->DAI[2]->DAI[1]->data.str;
                b_fwrite(stderr, errmsg, B("\n"));
        }
        mpack_destroy_object(result);

        return ret;
}

retval_t
(nvim_command_output)(const bstring *cmd, const mpack_expect_t expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), cmd);
        return m_expect_intern(result, expect);
}

retval_t
(nvim_call_function)(const bstring *function, const mpack_expect_t expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s,[]"), function);
        return m_expect_intern(result, expect);
}

retval_t
(nvim_call_function_args)(const bstring *function, const mpack_expect_t expect,
                          const bstring *fmt, ...)
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

retval_t
(nvim_eval)(const bstring *eval, const mpack_expect_t expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), eval);
        return m_expect_intern(result, expect);
}

/*--------------------------------------------------------------------------------------*/

retval_t
(nvim_get_var)(const bstring *varname, const mpack_expect_t expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), varname);
        return m_expect_intern(result, expect);
}

retval_t
(nvim_get_option)(const bstring *optname, const mpack_expect_t expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), optname);
        return m_expect_intern(result, expect);
}

retval_t
(nvim_buf_get_var)(const int bufnum, const bstring *varname, const mpack_expect_t expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d,s"), bufnum, varname);
        return m_expect_intern(result, expect);
}

retval_t
(nvim_buf_get_option)(const int bufnum, const bstring *optname, const mpack_expect_t expect)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d,s"), bufnum, optname);
        return m_expect_intern(result, expect);
}

unsigned
(nvim_buf_get_changedtick)(const int bufnum)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d"), bufnum);
        return (unsigned)m_expect_intern(result, E_NUM).num;
}

/*--------------------------------------------------------------------------------------*/

bool
nvim_set_var(const bstring *varname, const bstring *fmt, ...)
{
        va_list  ap;
        static const bstring fn  = BS_FROMARR(__func__);
        bstring       *tmp = b_sprintf("[s,!%s]", fmt);

        va_start(ap, fmt);
        mpack_obj *result = generic_call(true, &fn, tmp, varname, &ap);
        va_end(ap);

        b_destroy(tmp);
        return mpack_type(m_index(result, 2)) == MPACK_NIL;
}

/*--------------------------------------------------------------------------------------*/
/* Highlight related functions */

int
(nvim_buf_add_highlight)(const unsigned  bufnum,
                         const int       hl_id,
                         const bstring  *group,
                         const unsigned  line,
                         const unsigned  start,
                         const int       end)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("dd,s,ddd"), bufnum,
                                         hl_id, group, line, start, end);
        return (int)m_expect_intern(result, E_NUM).num;
}

void
(nvim_buf_clear_highlight)(const unsigned bufnum,
                           const int      hl_id,
                           const unsigned start,
                           const int      end)
{
        static const bstring fn = BS_FROMARR(__func__);
        /* mpack_obj *result =  */ generic_call(false, &fn, B("dd,dd"), bufnum, hl_id, start, end);
        /* PRINT_AND_DESTROY(result); */
}

mpack_dict_t *
(nvim_get_hl_by_name)(const bstring *name, const bool rgb)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("sB"), name, rgb);
        return m_expect_intern(result, E_MPACK_DICT).ptr;
}

mpack_dict_t *
(nvim_get_hl_by_id)(const int hlid, const bool rgb)
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
        PRINT_AND_DESTROY(result);
}

void
(nvim_subscribe)(const bstring *event)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("s"), event);
        PRINT_AND_DESTROY(result);
}

void
(nvim_buf_attach)(const int bufnum)
{
        static const bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(true, &fn, B("d,B,[]"), bufnum, false);
        PRINT_AND_DESTROY(result);
}

void
(nvim_set_client_info)(const bstring  *name,
                       const unsigned  major,
                       const unsigned  minor,
                       const bstring  *dev,
                       const bstring  *type,
                       const void     *methods,
                       const void     *attributes)
{
        static const bstring fn = BS_FROMARR(__func__);
        (void)generic_call(false, &fn, B("s; {s:d, s:d, s:s}; s; {}; {}"),
                           name,
                           B("major"), major,
                           B("minor"), minor,
                           B("dev"), dev,
                           type);

        /* mpack_obj *result = generic_call(true, &fn, B("s; {s:d, s:d, s:s}; s; {}; {}"), */
                                         /* name, B("major"), major, */
                                         /* B("minor"), minor, B("dev"), dev, type); */
        /* PRINT_AND_DESTROY(result); */
}

/*======================================================================================*/
/* The single most important api function gets its own section for no reason. */

void
(nvim_call_atomic)(const mpack_arg_array *calls)
{
#ifdef DEBUG
        static bool first = true;
        if (first) {
                char tmp[PATH_MAX];
                snprintf(tmp, PATH_MAX, "%s/.tag_highlight_log/atomic.log", HOME);
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

        const int  count = INC_COUNT();
        mpack_obj *pack  = mpack_encode_fmt(calls->qty, BS(fmt), MES_REQUEST,
                                            count, &fn, calls->args);
        b_destroy(fmt);
        special_call(false, &fn, pack, count);

        /* generic_call(false, &fn, B("d,B,[]"), bufnum, false); */

#if 0
        const int  count = INC_COUNT();
        mpack_obj *pack  = mpack_encode_fmt(calls->qty, BS(fmt), MES_REQUEST, count, &fn, calls->args);
        b_destroy(fmt);

        struct gencall *gc = malloc(sizeof *gc);
        *gc                = (struct gencall){pack, &fn, count};
        START_DETACHED_PTHREAD(generic_call_threaded, gc);
#endif

        /* (void)write_and_clean(pack, count, &fn, false, logfp); */
        /* mpack_obj *result = write_and_clean(pack, count, &fn, false, logfp); */
        /* mpack_print_object(logfp, result); */
        /* mpack_destroy_object(result); */
}
