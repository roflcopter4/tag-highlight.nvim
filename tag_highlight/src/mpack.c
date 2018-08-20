#include "util.h"
#include <dirent.h>

#include "data.h"
#include "mpack.h"

static mpack_obj *generic_call(int *fd, const bstring *fn, const bstring *fmt, ...);
static void       write_and_clean(int fd, mpack_obj *pack, const bstring *func);
static void       collect_items (struct item_free_stack *tofree, mpack_obj *item);
static mpack_obj *find_key_value(mpack_dict_t *dict, const bstring *key);
static inline retval_t m_expect_intern(mpack_obj *root, mpack_expect_t type);

static unsigned        sok_count, io_count;
static pthread_mutex_t mpack_main_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef _MSC_VER
#  define restrict __restrict
#endif
#define ENCODE_FMT_ARRSIZE 8192
#define COUNT(FD_)         (((FD_) == 1) ? io_count : sok_count)
#define INC_COUNT(FD_)     (((FD_) == 1) ? io_count++ : sok_count++)
#define CHECK_DEF_FD(FD__) ((FD__) = (((FD__) == 0) ? DEFAULT_FD : (FD__)))
#define BS_FROMARR(ARRAY_) {(sizeof(ARRAY_) - 1), 0, (unsigned char *)(ARRAY_), 0}

/*======================================================================================*/


static mpack_obj *
generic_call(int *fd, const bstring *fn, const bstring *fmt, ...)
{
        pthread_mutex_lock(&mpack_main_mutex);
        CHECK_DEF_FD(*fd);
        mpack_obj *pack;

        if (fmt) {
                va_list        ap;
                const unsigned size = fmt->slen + 16u;
                char *         buf  = alloca(size);
                snprintf(buf, size, "d,d,s:[!%s]", BS(fmt));

                va_start(ap, fmt);
                pack = encode_fmt(0, buf, MES_REQUEST, INC_COUNT(*fd), fn, &ap);
                va_end(ap);
        } else {
                pack = encode_fmt(0, "d,d,s:[]", MES_REQUEST, INC_COUNT(*fd), fn);
        }

        write_and_clean(*fd, pack, fn);

        mpack_obj *result = decode_stream(*fd, MES_RESPONSE);
        mpack_print_object(mpack_log, result);
        pthread_mutex_unlock(&mpack_main_mutex);
        return result;
}


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

        mpack_obj *pack = encode_fmt(0, "d,d,s:[s]", MES_REQUEST, INC_COUNT(fd), func, mes);
        write_and_clean(fd, pack, func);
        mpack_obj *tmp  = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);

        mpack_print_object(mpack_log, tmp);
        mpack_destroy(tmp);
}

void
nvim_printf(const int fd, const char *const restrict fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *tmp = b_vformat(fmt, ap);
        va_end(ap);

        nvim_out_write(fd, tmp);
        b_destroy(tmp);
}

void
nvim_vprintf(const int fd, const char *const restrict fmt, va_list args)
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
        pthread_mutex_lock(&mpack_main_mutex);

        CHECK_DEF_FD(fd);
        mpack_obj *pack = encode_fmt(0, "d,d,s:[d,B,[]]", MES_REQUEST,
                                     INC_COUNT(fd), &fn, bufnum, false);
        write_and_clean(fd, pack, &fn);

        /* Don't wait for the response, it will be handled in the main thread. */
        pthread_mutex_unlock(&mpack_main_mutex);
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
        bstring       *buf = b_sprintf(B("[s,[!%s]]"), fmt);
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
nvim_buf_add_highlight(int fd, const unsigned bufnum, const int hl_id, const bstring *group,
                       const unsigned line, const unsigned start, const int end)
{
        static bstring fn = BS_FROMARR(__func__);
        mpack_obj *result = generic_call(&fd, &fn, B("dd,s,ddd"), bufnum,
                                         hl_id, group, line, start, end);
        return (int)m_expect_intern(result, E_NUM).num;
}

void
nvim_buf_clear_highlight(int fd, const unsigned bufnum, const int hl_id,
                         const unsigned start, const int end)
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
        bstring                 *fmt  = b_fromcstr_alloc(4096, "d,d,s:[:");

        if (calls->qty) {
                b_sprintfa(fmt, B("[ @[%n],"), calls->fmt[0]);

                for (unsigned i = 1; i < calls->qty; ++i)
                        b_sprintfa(fmt, B("[*%n],"), calls->fmt[i]);

                b_catlit(fmt, " ]:]");
        }

        pthread_mutex_lock(&mpack_main_mutex);
        mpack_obj *pack = encode_fmt(calls->qty, BS(fmt), MES_REQUEST,
                                     INC_COUNT(fd), &fn, args);
        write_and_clean(fd, pack, &fn);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        PRINT_AND_DESTROY(result);
        b_destroy(fmt);
}


/*======================================================================================*/


bstring *
get_notification(int fd)
{
        /* No mutex to lock here. */
        CHECK_DEF_FD(fd);
        mpack_obj *result = decode_stream(fd, MES_NOTIFICATION);
        bstring   *ret    = b_strcpy(result->DAI[1]->data.str);
        PRINT_AND_DESTROY(result);
        return ret;
}


/*======================================================================================*/


retval_t
m_expect(mpack_obj *obj, const mpack_expect_t type, bool destroy)
{
        retval_t ret;
        ret.ptr = NULL;
        int64_t       value;
        mpack_type_t  err_expect;

        if (!obj)
                return ret;
        if (mpack_log) {
                mpack_print_object(mpack_log, obj);
                fflush(mpack_log);
        }

        switch (type) {
        case E_MPACK_ARRAY:
                if (destroy)
                        mpack_spare_data(obj);
                ret.ptr = obj->data.arr;
                break;

        case E_MPACK_DICT:
                if (mpack_type(obj) != (err_expect = MPACK_DICT))
                        goto error;
                if (destroy)
                        mpack_spare_data(obj);
                ret.ptr = obj->data.dict;
                break;

        case E_MPACK_EXT:
                if (mpack_type(obj) != (err_expect = MPACK_EXT))
                        goto error;
                if (destroy)
                        mpack_spare_data(obj);
                ret.ptr = obj->data.ext;
                break;

        case E_MPACK_NIL:
                if (mpack_type(obj) != (err_expect = MPACK_NIL))
                        goto error;
                break;

        case E_BOOL:
                if (mpack_type(obj) != (err_expect = MPACK_BOOL)) {
                        if (mpack_type(obj) == MPACK_NUM)
                                value = obj->data.num;
                        else
                                goto error;
                } else {
                        value = obj->data.boolean;
                }
                ret.num = value;
                break;

        case E_NUM:
                if (mpack_type(obj) != (err_expect = MPACK_NUM)) {
                        if (mpack_type(obj) == MPACK_EXT)
                                value = obj->data.ext->num;
                        else
                                goto error;
                } else {
                        value = obj->data.num;
                }
                ret.num = value;
                break;

        case E_STRING:
                if (mpack_type(obj) != (err_expect = MPACK_STRING))
                        goto error;
                ret.ptr = obj->data.str;
                if (destroy)
                        b_writeprotect((bstring *)ret.ptr);
                break;

        case E_STRLIST:
                if (mpack_type(obj) != (err_expect = MPACK_ARRAY))
                        goto error;
                ret.ptr = mpack_array_to_blist(obj->data.arr, destroy);
                if (destroy) {
                        free(obj);
                        destroy = false;
                }
                break;

        case E_DICT2ARR:
                abort();
                        
        default:
                errx(1, "Invalid type given to %s()\n", __func__);
        }

        if (destroy) {
                mpack_destroy(obj);
                if (type == E_STRING)
                        b_writeallow((bstring *)ret.ptr);
        }

        return ret;

error:
        warnx("WARNING: Got mpack of type %s, expected type %s, possible error.",
              m_type_names[mpack_type(obj)], m_type_names[err_expect]);
        mpack_destroy(obj);
        ret.ptr = NULL;
        return ret;
}


static inline retval_t
m_expect_intern(mpack_obj *root, mpack_expect_t type)
{
        mpack_obj *errmsg = m_index(root, 2);
        mpack_obj *data   = m_index(root, 3);
        retval_t   ret    = { .ptr = NULL };

        if (mpack_type(errmsg) != MPACK_NIL) {
                bstring *err_str = m_expect(m_index(errmsg, 1), E_STRING, true).ptr;
                if (err_str) {
                        warnx("Neovim returned with an err_str: '%s'", BS(err_str));
                        b_destroy(err_str);
                        root->DAI[2] = NULL;
                }
        } else {
                ret = m_expect(data, type, true);
                root->DAI[3] = NULL;
        }

        mpack_destroy(root);
        return ret;
}


/*======================================================================================*/


void
mpack_destroy(mpack_obj *root)
{
        struct item_free_stack tofree = { nmalloc(sizeof(void *), 8192), 0, 8192 };
        collect_items(&tofree, root);

        for (int64_t i = (int64_t)(tofree.qty - 1); i >= 0; --i) {
                mpack_obj *cur = tofree.items[i];
                if (!cur)
                        continue;
                if (!(cur->flags & MPACK_SPARE_DATA)) {
                        switch (mpack_type(cur)) {
                        case MPACK_ARRAY:
                                if (cur->data.arr) {
                                        free(cur->DAI);
                                        free(cur->data.arr);
                                }
                                break;
                        case MPACK_DICT:
                                if (cur->data.dict) {
                                        unsigned j = 0;
                                        while (j < cur->data.dict->qty)
                                                free(cur->DDE[j++]);
                                        free(cur->DDE);
                                        free(cur->data.dict);
                                }
                                break;
                        case MPACK_STRING:
                                if (cur->data.str)
                                        b_destroy(cur->data.str);
                                break;
                        case MPACK_EXT:
                                if (cur->data.ext)
                                        free(cur->data.ext);
                                break;
                        default:
                                break;
                        }
                }

                if (cur->flags & MPACK_HAS_PACKED)
                        b_destroy(*cur->packed);

                if (!(cur->flags & MPACK_PHONY))
                        free(cur);
        }

        free(tofree.items);
}


static void
collect_items(struct item_free_stack *tofree, mpack_obj *item)
{
        if (!item)
                return;
        free_stack_push(tofree, item);
        if (item->flags & MPACK_SPARE_DATA)
                return;

        switch (mpack_type(item)) {
        case MPACK_ARRAY:
                if (!item->data.arr)
                        return;

                for (unsigned i = 0; i < item->data.arr->qty; ++i)
                        if (item->DAI[i])
                                collect_items(tofree, item->DAI[i]);
                break;
        case MPACK_DICT:
                if (!item->data.dict)
                        return;

                for (unsigned i = 0; i < item->data.dict->qty; ++i) {
                        if (item->DDE[i]->key)
                                free_stack_push(tofree, item->DDE[i]->key);
                        if (item->DDE[i]->value)
                                collect_items(tofree, item->DDE[i]->value);
                }
                break;
        case MPACK_UNINITIALIZED:
                errx(1, "Got uninitialized item to free!");
        default:
                break;
        }
}


void
destroy_call_array(struct atomic_call_array *calls)
{
        if (!calls)
                return;
        for (unsigned i = 0; i < calls->qty; ++i) {
                unsigned x = 0;
                for (const char *ptr = calls->fmt[i]; *ptr; ++ptr) {
                        switch (*ptr) {
                        case 'b': case 'B':
                        case 'd': case 'D':
                                ++x;
                                break;
                        case 's': case 'S':
                                b_destroy(calls->args[i][x++].str);
                                break;
                        }
                }
                free(calls->args[i]);
                free(calls->fmt[i]);
        }
        free(calls->args);
        free(calls->fmt);
        free(calls);
}


static void
write_and_clean(const int fd, mpack_obj *pack, const bstring *func)
{
#ifdef DEBUG
#  ifdef LOG_RAW_MPACK
        char tmp[512]; snprintf(tmp, 512, "%s/rawmpack.log", HOME);
        const int rawlog = safe_open(tmp, O_CREAT|O_APPEND|O_WRONLY|O_DSYNC|O_BINARY, 0644);
        b_write(rawlog, B("\n"), *pack->packed, B("\n"));
        close(rawlog);
#  endif
        if (func && mpack_log)
                fprintf(mpack_log, "=================================\n"
                        "Writing request no %d to fd %d: \"%s\"\n",
                        COUNT(fd) - 1, fd, BS(func));

        mpack_print_object(mpack_log, pack);
#endif
        b_write(fd, *pack->packed);
        mpack_destroy(pack);
}


/*======================================================================================*/
/* Type conversions */


b_list *
mpack_array_to_blist(mpack_array_t *array, const bool destroy)
{
        if (!array)
                return NULL;
        const unsigned size = array->qty;
        b_list        *ret  = b_list_create_alloc(size);

        if (destroy) {
                for (unsigned i = 0; i < size; ++i) {
                        b_writeprotect(array->items[i]->data.str);
                        b_list_append(&ret, array->items[i]->data.str);
                }

                destroy_mpack_array(array);
                b_list_writeallow(ret);
        } else {
                for (unsigned i = 0; i < size; ++i)
                        b_list_append(&ret, array->items[i]->data.str);
        }

        return ret;
}


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


retval_t
dict_get_key(mpack_dict_t *dict, const mpack_expect_t expect, const bstring *key)
{
        if (!dict || !key)
                abort();

        mpack_obj *tmp = find_key_value(dict, key);
        if (!tmp)
                return (retval_t){ .ptr = NULL };

        return m_expect(tmp, expect, false);
}


static mpack_obj *
find_key_value(mpack_dict_t *dict, const bstring *key)
{
        for (unsigned i = 0; i < dict->qty; ++i)
                if (b_iseq(dict->entries[i]->key->data.str, key))
                        return dict->entries[i]->value;

        return NULL;
}


/*======================================================================================*/

#define NEW_STACK(TYPE_, NAME_, SIZE_, NUM_)             \
        TYPE_    NAME_       = nmalloc((SIZE_), (NUM_)); \
        unsigned NAME_##_ctr = 0;

#define POP(STACK_) \
        (((STACK_##_ctr == 0) ? (abort(), 0) : 0), ((STACK_)[--STACK_##_ctr]))

#define PUSH(STACK_, VAL_) \
        ((STACK_)[STACK_##_ctr++] = (VAL_))

#define PEEK(STACK_) \
        (((STACK_##_ctr == 0) ? (abort(), 0) : 0), ((STACK_)[STACK_##_ctr - 1]))

#define RESET(STACK_) \
        ((STACK_##_ctr) = 0, (STACK_)[0] = 0)

#define STACK_CTR(STACK_) (STACK_##_ctr)

/*======================================================================================*/


enum encode_fmt_next_type { OWN_VALIST, OTHER_VALIST, ATOMIC_UNION };

#define NEXT(VAR_, TYPE_NAME_, MEMBER_)                                       \
        do {                                                                  \
                switch (next_type) {                                          \
                case OWN_VALIST:                                              \
                        (VAR_) = va_arg(args, TYPE_NAME_);                    \
                        break;                                                \
                case OTHER_VALIST:                                            \
                        assert(ref != NULL);                                  \
                        (VAR_) = va_arg(*ref, TYPE_NAME_);                    \
                        break;                                                \
                case ATOMIC_UNION:                                            \
                        assert(a_args);                                       \
                        assert(a_args[a_arg_ctr]);                            \
                        (VAR_) = ((a_args[a_arg_ctr][a_arg_subctr]).MEMBER_); \
                        ++a_arg_subctr;                                       \
                        break;                                                \
                default:                                                      \
                        abort();                                              \
                }                                                             \
        } while (0)

#define NEXT_NO_ATOMIC(VAR_, TYPE_NAME_)                    \
        do {                                                \
                switch (next_type) {                        \
                case OWN_VALIST:                            \
                        (VAR_) = va_arg(args, TYPE_NAME_);  \
                        break;                              \
                case OTHER_VALIST:                          \
                        assert(ref != NULL);                \
                        (VAR_) = va_arg(*ref, TYPE_NAME_);  \
                        break;                              \
                case ATOMIC_UNION:                          \
                default:                                    \
                        abort();                            \
                }                                           \
        } while (0)

#define INC_CTRS()                                     \
        do {                                           \
                ++len_ctr;                             \
                PUSH(obj_stack, *cur_obj);             \
                PUSH(len_stack, cur_ctr);              \
                cur_ctr  = &sub_ctrlist[subctr_ctr++]; \
                *cur_ctr = 0;                          \
        } while (0)

#define TWO_THIRDS(NUM_) ((1 * (NUM_)) / 3)


/* 
 * Relatively convenient way to encode an mpack object using a format string.
 * Some of the elements of the string are similar to printf format strings, but
 * otherwise it is entirely custom. All legal values either denote an argument
 * that must be supplied variadically to be encoded in the object, or else to an
 * ignored or special character. Values are not case sensitive.
 *
 * Standard values:
 *     d: Integer value (int)
 *     l: Long integer values (int64_t)
 *     b: Boolean value (int)
 *     s: String (bstring *) - must be a bstring, not a standard C string
 *     c: Char Array (const char *) - NUL terminated C string
 *     n: Nil - no argument expected
 *
 * Any values enclosed in '[' and ']' are placed in a sub-array.
 * Any values enclosed in '{' and '}' are placed in a dictionary, which must
 * have an even number of elements.
 *
 * Three additional special values are recognized:
 *     !: Denotes a pointer to an initialized va_list, from which all arguments
 *        thereafter will be taken, until either another '!' or '@' is encountered.
 *     @: Denotes an argument of type "union atomic_call_args **", that is to
 *        say an array of arrays of union atomic_call_args objects. When '@'
 *        is encountered, this double pointer is taken from the current argument
 *        source and used thereafter as such. The first sub array is used until
 *        a '*' is encountered in the format string, at which point the next
 *        array is used, and so on.
 *     *: Increments the current sub array of the atomic_call_args ** object.
 *
 * All of the following characters are ignored in the format string, and may be
 * used to make it clearer or more visually pleasing: ':'  ';'  ','  ' '
 *
 * All errors are fatal.
 */

mpack_obj *
encode_fmt(const unsigned size_hint, const char *const restrict fmt, ...)
{
        assert(fmt != NULL && *fmt != '\0');
        union atomic_call_args    **a_args    = NULL;
        enum encode_fmt_next_type   next_type = OWN_VALIST;
        const unsigned              arr_size  = (size_hint)
                                                ? ENCODE_FMT_ARRSIZE + (size_hint * 6)
                                                : ENCODE_FMT_ARRSIZE;
        va_list      args;
        int          ch;
        unsigned    *sub_lengths = nmalloc(sizeof(unsigned), arr_size);
        unsigned    *cur_len     = &sub_lengths[0];
        unsigned     len_ctr     = 1;
        const char  *ptr         = fmt;
        va_list     *ref         = NULL;
        *cur_len                 = 0;

        NEW_STACK(unsigned **, len_stack, sizeof(unsigned *), TWO_THIRDS(arr_size));
        va_start(args, fmt);

        /* Go through the format string once to get the number of arguments and
         * in particular the number and size of any arrays. */
        while ((ch = *ptr++)) {
                assert(len_ctr < arr_size);

                switch (ch) {
                /* Legal values. Increment size and continue. */
                case 'b': case 'B': case 'l': case 'L':
                case 'd': case 'D': case 's': case 'S':
                case 'n': case 'N': case 'c': case 'C':
                        ++*cur_len;
                        break;

                /* New array. Increment current array size, push it onto the
                 * stack, and initialize the next counter. */
                case '[': case '{':
                        ++*cur_len;
                        PUSH(len_stack, cur_len);
                        cur_len = &sub_lengths[len_ctr++];
                        *cur_len = 0;
                        break;

                /* End of array. Pop the previous counter off the stack and
                 * continue on adding any further elements to it. */
                case ']': case '}':
                        cur_len = POP(len_stack);
                        break;

                /* Legal values that do not increment the current size. */
                case ':': case '.': case ' ': case ',':
                case '!': case '@': case '*':
                        break;

                default:
                        errx(1, "Illegal character \"%c\" found in format.", ch);
                }
        }

        if (STACK_CTR(len_stack) != 0)
                errx(1, "Invalid encode format string: undetermined array/dictionary.");
        mpack_obj *pack = NULL;
        if (sub_lengths[0] == 0)
                goto cleanup;

#ifdef DEBUG
        pack = mpack_make_new(sub_lengths[0], true);
#else
        pack = mpack_make_new(sub_lengths[0], false);
#endif

        unsigned   *sub_ctrlist  = nmalloc(sizeof(unsigned), arr_size);
        unsigned   *cur_ctr      = sub_ctrlist;
        mpack_obj **cur_obj      = NULL;
        unsigned    subctr_ctr   = 1;
        unsigned    a_arg_ctr    = 0;
        unsigned    a_arg_subctr = 0;
        len_ctr                  = 1;
        ptr                      = fmt;
        cur_obj                  = &pack->DAI[0];
        *cur_ctr                 = 1;

        RESET(len_stack);
        NEW_STACK(mpack_obj **, obj_stack, sizeof(mpack_obj *), TWO_THIRDS(arr_size));
        NEW_STACK(unsigned *, dict_stack, sizeof(unsigned), TWO_THIRDS(arr_size));
        PUSH(len_stack, cur_ctr);
        PUSH(obj_stack, pack);
        PUSH(dict_stack, 0);

        /* This loop is where all of the actual interpretation and encoding
         * happens. A few stacks are used when recursively encoding arrays and
         * dictionaries to store the state of the enclosing object(s). */
        while ((ch = *ptr++)) {
                switch (ch) {
                case 'b': case 'B': {
                        bool arg = 0;
                        NEXT(arg, int, boolean);
                        mpack_encode_boolean(pack, cur_obj, arg);
                        break;
                }
                case 'd': case 'D': {
                        int arg = 0;
                        NEXT(arg, int, num);
                        mpack_encode_integer(pack, cur_obj, arg);
                        break;
                }
                case 'l': case 'L': {
                        int64_t arg = 0;
                        NEXT(arg, int64_t, num);
                        mpack_encode_integer(pack, cur_obj, arg);
                        break;
                }
                case 's': case 'S': {
                        bstring *arg = NULL;
                        NEXT(arg, bstring *, str);
                        mpack_encode_string(pack, cur_obj, arg);
                        break;
                }
                case 'c': case 'C': {
                        const char *arg;
                        NEXT(arg, char *, c_str);
                        bstring tmp = bt_fromcstr(arg);
                        mpack_encode_string(pack, cur_obj, &tmp);
                        break;
                }
                case 'n': case 'N':
                        mpack_encode_nil(pack, cur_obj);
                        break;

                case '[':
                        mpack_encode_array(pack, cur_obj, sub_lengths[len_ctr]);
                        PUSH(dict_stack, 0);
                        PUSH(obj_stack, *cur_obj);
                        PUSH(len_stack, cur_ctr);

                        ++len_ctr;
                        cur_ctr  = &sub_ctrlist[subctr_ctr++];
                        *cur_ctr = 0;
                        break;

                case '{':
                        assert((sub_lengths[len_ctr] & 1) == 0);
                        mpack_encode_dictionary(pack, cur_obj, (sub_lengths[len_ctr] / 2));
                        PUSH(dict_stack, 1);
                        PUSH(obj_stack, *cur_obj);
                        PUSH(len_stack, cur_ctr);

                        ++len_ctr;
                        cur_ctr  = &sub_ctrlist[subctr_ctr++];
                        *cur_ctr = 0;
                        break;

                case ']':
                case '}':
                        (void)POP(dict_stack);
                        (void)POP(obj_stack);
                        cur_ctr = POP(len_stack);
                        break;

                case '!':
                        NEXT_NO_ATOMIC(ref, va_list *);
                        next_type = OTHER_VALIST;
                        continue;

                case '@':
                        NEXT_NO_ATOMIC(a_args, union atomic_call_args **);
                        a_arg_ctr    = 0;
                        a_arg_subctr = 0;
                        assert(a_args[a_arg_ctr]);
                        next_type = ATOMIC_UNION;
                        continue;

                case '*':
                        assert(next_type == ATOMIC_UNION);
                        ++a_arg_ctr;
                        a_arg_subctr = 0;
                        continue;

                case ':': case '.': case ' ': case ',':
                        continue;

                default:
                        abort();
                }

                if (PEEK(dict_stack)) {
                        if (PEEK(obj_stack)->data.dict->max > (*cur_ctr / 2)) {
                                if ((*cur_ctr & 1) == 0)
                                        cur_obj = &PEEK(obj_stack)->DDE[*cur_ctr / 2]->key;
                                else
                                        cur_obj = &PEEK(obj_stack)->DDE[*cur_ctr / 2]->value;
                        }
                } else {
                        if (PEEK(obj_stack)->data.arr->max > *cur_ctr)
                                cur_obj = &PEEK(obj_stack)->DAI[*cur_ctr];
                }

                ++*cur_ctr;
        }

        free(dict_stack);
        free(obj_stack);
        free(sub_ctrlist);
cleanup:
        free(sub_lengths);
        free(len_stack);
        va_end(args);
        return pack;
}
