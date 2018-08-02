#include "util.h"
#include <dirent.h>

#include "data.h"
#include "mpack.h"

static void       write_and_clean(int fd, mpack_obj *pack, const bstring *func);
static void       collect_items (struct item_free_stack *tofree, mpack_obj *item);
static mpack_obj *find_key_value(mpack_dict_t *dict, const bstring *key);
static void      *get_expect    (mpack_obj *result, const mpack_type_t expect,
                                 const bstring *key, bool destroy, bool is_retval);

static unsigned        sok_count, io_count;
static pthread_mutex_t mpack_main_mutex;

#ifdef _MSC_VER
#  define restrict __restrict
#endif
#define ENCODE_FMT_ARRSIZE 8192
#define STD_API_FMT        "d,d,s,"
#define COUNT(FD_)         (((FD_) == 1) ? io_count : sok_count)
#define INC_COUNT(FD_)     (((FD_) == 1) ? io_count++ : sok_count++)
#define CHECK_DEF_FD(FD__) ((FD__) = ((FD__) == 0) ? DEFAULT_FD : (FD__))

#define encode_fmt_api(FD__, FMT_, ...) \
        encode_fmt(0, STD_API_FMT "[" FMT_ "]", 0, INC_COUNT(FD__), __VA_ARGS__)

#define INIT_STATIC_FUNC()                        \
        static bstring func[] = {{.data = NULL}}; \
        if (!func[0].data)                        \
                func[0] = bt_fromarray(__func__);

#define WRITE_API__(FMT_, ...)                                           \
        do {                                                             \
                INIT_STATIC_FUNC()                                       \
                CHECK_DEF_FD(fd);                                        \
                mpack_obj *pack = encode_fmt_api(fd, FMT_, __VA_ARGS__); \
                write_and_clean(fd, pack, func);                         \
        } while (0)

#define WRITE_API(FMT_, ...) WRITE_API__(FMT_, func, __VA_ARGS__)
#define WRITE_API_NIL()      WRITE_API__("", func)

#define FATAL(...)                                      \
        do {                                            \
                if (!ret && fatal)                      \
                        errx(1, "ERROR: " __VA_ARGS__); \
        } while (0)

#define VALIDATE_EXPECT(EXPECT, ...)                               \
        do {                                                       \
                const mpack_type_t lst[] = {__VA_ARGS__};          \
                bool found = false;                                \
                for (unsigned i_ = 0; i_ < ARRSIZ(lst); ++i_) {    \
                        if ((EXPECT) == lst[i_]) {                 \
                                found = true;                      \
                                break;                             \
                        }                                          \
                }                                                  \
                if (!found)                                        \
                        errx(1, "Invalid argument \"%s\" in %s()," \
                                " line %d of file %s",             \
                             (expect <= 7 ? m_type_names[EXPECT]   \
                                          : "TOO LARGE"),          \
                             FUNC_NAME, __LINE__, __FILE__);       \
        } while (0)


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

        mpack_obj *pack = encode_fmt_api(fd, "s", func, mes);
        write_and_clean(fd, pack, func);
        mpack_obj *tmp  = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);

        mpack_print_object(tmp, mpack_log);
        mpack_destroy(tmp);
}

void
nvim_printf(const int fd, const char *const restrict fmt, ...)
{
        va_list va;
        va_start(va, fmt);
        bstring *tmp = b_vformat(fmt, va);
        va_end(va);

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


/*--------------------------------------------------------------------------------------*/

#define RETVAL (result->DAI[3])


b_list *
nvim_buf_attach(int fd, const int bufnum)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("d,B,[]", bufnum, true);
        /* Don't wait for the response, it will be handled in the main thread. */
        pthread_mutex_unlock(&mpack_main_mutex);
        return NULL;
}

/*--------------------------------------------------------------------------------------*/

void *
nvim_list_bufs(int fd)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API_NIL();

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = get_expect(result, MPACK_ARRAY, NULL, true, true);

        pthread_mutex_unlock(&mpack_main_mutex);
        return ret;
}

int
nvim_get_current_buf(int fd)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API_NIL();

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);

        const int ret = RETVAL->data.ext->num;
        print_and_destroy(result);
        return ret;
}

bstring *
nvim_get_current_line(int fd)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API_NIL();

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        void *ret = get_expect(result, MPACK_STRING, NULL, true, true);
        
        return ret;
}

unsigned
nvim_buf_line_count(int fd, const int bufnum)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("d", bufnum);

        mpack_obj *result    = decode_stream(fd, MES_RESPONSE);
        unsigned   linecount = (unsigned)(RETVAL->data.num);
        print_and_destroy(result);
        
        pthread_mutex_unlock(&mpack_main_mutex);
        return linecount;
}

b_list *
nvim_buf_get_lines(int            fd,
                   const unsigned bufnum,
                   const int      start,
                   const int      end)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("d,d,d,d", bufnum, start, end, 0);

        b_list *ret = mpack_array_to_blist(get_expect(decode_stream(fd, MES_RESPONSE),
                                                      MPACK_ARRAY, NULL, true, true),
                                           true);
        pthread_mutex_unlock(&mpack_main_mutex);
        return ret;
}

bstring *
nvim_buf_get_name(int fd, const int bufnum)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("d", bufnum);

        char       fullname[PATH_MAX];
        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        bstring   *ret    = get_expect(result, MPACK_STRING, NULL, true, true);
        char      *tmp    = realpath(BS(ret), fullname);
        b_assign_cstr(ret, tmp);

        pthread_mutex_unlock(&mpack_main_mutex);
        return ret;
}

/*--------------------------------------------------------------------------------------*/

bool
nvim_command(int fd, const bstring *cmd, const bool fatal)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("s", cmd);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        const bool ret    = mpack_type(result->DAI[2]) == MPACK_NIL;

        if (mpack_type(result->data.arr->items[2]) == MPACK_ARRAY) {
                bstring *errmsg = result->DAI[2]->DAI[1]->data.str;
                b_fputs(stderr, errmsg, B("\n"));
        }

        print_and_destroy(result);
        assert(!(fatal && !ret));
        pthread_mutex_unlock(&mpack_main_mutex);
        return ret;
}

void *
nvim_command_output(int                fd,
                    const bstring *    cmd,
                    const mpack_type_t expect,
                    const bstring *    key,
                    const bool         fatal)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("s", cmd);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = NULL;

        if (mpack_type(result->data.arr->items[2]) == MPACK_ARRAY) {
                bstring *errmsg = result->DAI[2]->DAI[1]->data.str;
                b_fputs(stderr, errmsg, B("\n"));
        } else
                ret = get_expect(result, expect, key, true, true);

        assert(!(fatal && !ret));
        pthread_mutex_unlock(&mpack_main_mutex);

        return ret;
}

void *
nvim_call_function(int                fd,
                   const bstring *    function,
                   const mpack_type_t expect,
                   const bstring *    key,
                   const bool         fatal)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("s,[]", function);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = get_expect(result, expect, key, true, true);

        FATAL("Failed to analyze out put of function \"%s\".", BS(function));

        pthread_mutex_unlock(&mpack_main_mutex);
        return ret;
}

void *
nvim_call_function_args(int                  fd,
                        const bstring       *function,
                        const mpack_type_t   expect,
                        const bstring       *key,
                        const bool           fatal,
                        const char *restrict fmt,
                        ...)
{
        pthread_mutex_lock(&mpack_main_mutex);
        CHECK_DEF_FD(fd);
        static bstring func[] = {bt_init("nvim_call_function_args")};
        char buf[2048];
        snprintf(buf, 2048, STD_API_FMT "[s,[!%s]]", fmt);

        va_list ap;
        va_start(ap, fmt);
        mpack_obj *pack = encode_fmt(0, buf, 0, INC_COUNT(fd), &func, function, &ap);
        va_end(ap);

        mpack_print_object(pack, mpack_log); fflush(mpack_log);
        write_and_clean(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = get_expect(result, expect, key, true, true);

        FATAL("Failed to analyze output of function \"%s\".", BS(function));

        pthread_mutex_unlock(&mpack_main_mutex);
        return ret;
}

/*--------------------------------------------------------------------------------------*/

void *
nvim_get_var(int                fd,
             const bstring *    varname,
             const mpack_type_t expect,
             const bstring *    key,
             const bool         fatal)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("s", varname);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        void *ret = get_expect(result, expect, key, true, true);

        FATAL("Failed to retrieve variable \"%s\".", BS(varname));
        return ret;
}

void *
nvim_get_option(int                fd,
                const bstring *    optname,
                const mpack_type_t expect,
                const bstring *    key,
                const bool         fatal)
{
        pthread_mutex_lock(&mpack_main_mutex);
        VALIDATE_EXPECT(expect, MPACK_STRING, MPACK_NUM, MPACK_BOOL);
        WRITE_API("s", optname);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = get_expect(result, expect, key, true, true);

        FATAL("Failed to retrieve option \"g:%s\".", BS(optname));

        pthread_mutex_unlock(&mpack_main_mutex);
        return ret;
}

void *
nvim_buf_get_var(int                fd,
                 const int          bufnum,
                 const bstring *    varname,
                 const mpack_type_t expect,
                 const bstring *    key,
                 const bool         fatal)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("d,s", bufnum, varname);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        void *ret = get_expect(result, expect, key, true, true);

        FATAL("Failed to retrieve variable \"b:%s\".", BS(varname));
        return ret;
}

void *
nvim_buf_get_option(int                fd,
                    const int          bufnum,
                    const bstring *    optname,
                    const mpack_type_t expect,
                    const bstring *    key,
                    const bool         fatal)
{
        pthread_mutex_lock(&mpack_main_mutex);
        VALIDATE_EXPECT(expect, MPACK_STRING, MPACK_NUM, MPACK_BOOL);
        WRITE_API("d,s", bufnum, optname);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        void *ret = get_expect(result, expect, key, true, true);

        FATAL("Failed to retrieve option \"%s\".", BS(optname));

        return ret;
}

unsigned
nvim_buf_get_changedtick(int fd, const int bufnum)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("d", bufnum);

        mpack_obj      *result = decode_stream(fd, MES_RESPONSE);
        const unsigned  ret    = RETVAL->data.num;

        mpack_destroy(result);
        pthread_mutex_unlock(&mpack_main_mutex);
        return ret;
}

/*--------------------------------------------------------------------------------------*/

bool
nvim_set_var(int fd, const bstring *varname, const char *const restrict fmt, ...)
{
        CHECK_DEF_FD(fd);
        INIT_STATIC_FUNC();
        char       buf[4096];
        mpack_obj *pack = NULL;
        va_list    ap;
        va_start(ap, fmt);
        snprintf(buf, 4096, STD_API_FMT "[s,!%s]", fmt);

        pack = encode_fmt(0, buf, 0, INC_COUNT(fd), func, varname, &ap);
        pthread_mutex_lock(&mpack_main_mutex);
        write_and_clean(fd, pack, func);
        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);

        va_end(ap);
        const bool ret = mpack_type(result->DAI[2]) == MPACK_NIL;
        print_and_destroy(result);
        return ret;
}

/*--------------------------------------------------------------------------------------*/

int
nvim_buf_add_highlight(int             fd,
                       const unsigned  bufnum,
                       const int       hl_id,
                       const bstring  *group,
                       const unsigned  line,
                       const unsigned  start,
                       const int       end)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("dd,s,ddd", bufnum, hl_id, group, line, start, end);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        const int ret = RETVAL->data.num;
        print_and_destroy(result);

        return ret;
}

void
nvim_buf_clear_highlight(int            fd,
                         const unsigned bufnum,
                         const int      hl_id,
                         const unsigned start,
                         const int      end)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("dd,dd", bufnum, hl_id, start, end);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        mpack_destroy(result);
}

/*--------------------------------------------------------------------------------------*/

void
nvim_get_api_info(int fd)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API_NIL();

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        print_and_destroy(result);
        pthread_mutex_unlock(&mpack_main_mutex);
}

void
nvim_subscribe(int fd, const bstring *event)
{
        pthread_mutex_lock(&mpack_main_mutex);
        WRITE_API("s", event);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        print_and_destroy(result);
}

void
nvim_call_atomic(int fd, const struct atomic_call_array *calls)
{
        CHECK_DEF_FD(fd);
        INIT_STATIC_FUNC();
        pthread_mutex_lock(&mpack_main_mutex);
        union atomic_call_args **args = calls->args;

        bstring *fmt = b_fromcstr_alloc(4096, STD_API_FMT "[");

        if (calls->qty) {
                b_catlit(fmt, "[@[");
                b_catcstr(fmt, calls->fmt[0]);
                b_conchar(fmt, ']');

                for (unsigned i = 1; i < calls->qty; ++i) {
                        b_catlit(fmt, "[*");
                        b_catcstr(fmt, calls->fmt[i]);
                        b_conchar(fmt, ']');
                }

                b_catlit(fmt, "]]");
        }

        mpack_obj *pack = encode_fmt(calls->qty, BS(fmt), 0, INC_COUNT(fd), &func, args);

        write_and_clean(fd, pack, func);
        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main_mutex);
        print_and_destroy(result);

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
        print_and_destroy(result);
        return ret;
}


/*======================================================================================*/


static void *
get_expect(mpack_obj *        result,
           const mpack_type_t expect,
           const bstring *    key,
           const bool         destroy,
           const bool         is_retval)
{
        mpack_obj *cur;
        assert(result != NULL);

        if (is_retval)
                cur = RETVAL;
        else
                cur = result;

        void      *ret = NULL;
        int64_t    value;
        mpack_print_object(result, mpack_log);
        fflush(mpack_log);

        if (mpack_type(cur) == MPACK_NIL) {
                eprintf("Neovim returned nil!\n");
                if (destroy)
                        mpack_destroy(result);
                return NULL;
        }
        if (mpack_type(cur) != expect) {
                if (mpack_type(cur) == MPACK_DICT && key) {
                        cur = find_key_value(cur->data.dict, key);
                } else if (mpack_type(cur) == MPACK_EXT && expect == MPACK_NUM) {
                        value = cur->data.ext->num;
                        goto num_from_ext;
                }
        }
        if (!cur || mpack_type(cur) != expect) {
                if (destroy)
                        mpack_destroy(result);
                return NULL;
        }

        switch (expect) {
        case MPACK_ARRAY:
                if (destroy)
                        mpack_spare_data(cur);
                ret = cur->data.arr;
                break;
        case MPACK_DICT:
                if (destroy)
                        mpack_spare_data(cur);
                ret = cur->data.dict;
                break;
        case MPACK_STRING:
                ret = cur->data.str;
                if (destroy)
                        b_writeprotect((bstring *)ret);
                break;
        case MPACK_EXT:
                if (destroy)
                        mpack_spare_data(cur);
                ret = cur->data.ext;
                break;
        case MPACK_NUM:
                value = RETVAL->data.num;
        num_from_ext:
                ret = xmalloc(sizeof(int64_t));
                *((int64_t *)ret) = value;
                break;

        default: /* UNREACHABLE */ abort();
        }

        if (destroy) {
                mpack_destroy(result);

                if (expect == MPACK_STRING)
                        b_writeallow((bstring *)ret);
        }

        return ret;
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
        if (func)
                fprintf(mpack_log, "=================================\n"
                        "Writing request no %d to fd %d: \"%s\"\n",
                        COUNT(fd) - 1, fd, BS(func));

        mpack_print_object(pack, mpack_log);
#endif
        b_write(fd, *pack->packed);
        mpack_destroy(pack);
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


/*======================================================================================*/
/* Type conversions */


b_list *
mpack_array_to_blist(mpack_array_t *array, const bool destroy)
{
        if (!array)
                return NULL;
        unsigned size = array->qty;
        b_list * ret  = b_list_create_alloc(size);

        if (destroy) {
                for (unsigned i = 0; i < size; ++i) {
                        b_writeprotect(array->items[i]->data.str);
                        b_list_append(&ret, array->items[i]->data.str);
                }

                destroy_mpack_array(array);

                for (unsigned i = 0; i < size; ++i)
                        b_writeallow(ret->lst[i]);
        } else {
                for (unsigned i = 0; i < size; ++i)
                        b_list_append(&ret, array->items[i]->data.str);
        }

        return ret;
}


b_list *
blist_from_var_fmt(
    const int fd, const bstring *key, const bool fatal, const char *fmt, ...)
{
        va_list va;
        va_start(va, fmt);
        bstring *varname = b_vformat(fmt, va);
        va_end(va);

        void *ret = nvim_get_var(fd, varname, MPACK_ARRAY, key, fatal);
        b_destroy(varname);
        return mpack_array_to_blist(ret, true);
}


void *
nvim_get_var_fmt(const int          fd,
                 const mpack_type_t expect,
                 const bstring *    key,
                 const bool         fatal,
                 const char *       fmt,
                 ...)
{
        va_list va;
        va_start(va, fmt);
        bstring *varname = b_vformat(fmt, va);
        va_end(va);

        void *ret = nvim_get_var(fd, varname, expect, key, fatal);
        b_destroy(varname);
        return ret;
}


void *
dict_get_key(mpack_dict_t *     dict,
             const mpack_type_t expect,
             const bstring *    key,
             const bool         fatal)
{
        if (!dict || !key)
                abort();

        mpack_obj tmp;
        tmp.flags     = MPACK_DICT | MPACK_PHONY;
        tmp.data.dict = dict;

        void *ret = get_expect(&tmp, expect, key, false, false);

        FATAL("Key \"%s\" not found or data is invalid.", BS(key));
        return ret;
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

#define INC_CTRS()                                         \
        do {                                               \
                ++len_ctr;                                 \
                *(++obj_stackp) = *cur_obj;                \
                *len_stackp++   = cur_ctr;                 \
                cur_ctr         = &sub_ctrs[subctr_ctr++]; \
                *cur_ctr        = 0;                       \
        } while (0)


#define TWO_THIRDS(NUM_) ((1 * (NUM_)) / 3)

enum encode_fmt_next_type { OWN_VALIST, OTHER_VALIST, ATOMIC_UNION };


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
 *     s: String (bstring *) - must be a bstring, not a standard c string
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
        /* eprintf("Fmt is \"%s\"\n", fmt); */
        union atomic_call_args    **a_args    = NULL;
        enum encode_fmt_next_type   next_type = OWN_VALIST;
        const unsigned              arr_size  = (size_hint)
                                                ? ENCODE_FMT_ARRSIZE + (size_hint * 6)
                                                : ENCODE_FMT_ARRSIZE;
        va_list      args;
        int          ch;
        int *        sub_lengths = nmalloc(sizeof(int), arr_size);
        int **       len_stack   = nmalloc(sizeof(int *), TWO_THIRDS(arr_size));
        va_list *    ref         = NULL;
        const char * ptr         = fmt;
        unsigned     len_ctr     = 0;
        int **       len_stackp  = len_stack;
        int *        cur_len     = &sub_lengths[len_ctr++];
        *cur_len                 = 0;

        fprintf(mpack_log, "%s\n", fmt);

        va_start(args, fmt);

        /* Go through the format string once to get the number of arguments and
         * in particular the number and size of any arrays. */
        while ((ch = *ptr++)) {
                assert(len_ctr < arr_size);

                switch (ch) {
                /* Legal values. Increment size and continue. */
                case 'b': case 'B': case 'l': case 'L':
                case 'd': case 'D': case 's': case 'S':
                case 'n': case 'N':
                        ++(*cur_len);
                        break;

                /* New array. Increment current array size, push it onto the
                 * stack, and initialize the next counter. */
                case '[': case '{':
                        ++(*cur_len);
                        *len_stackp++ = cur_len;
                        cur_len = &sub_lengths[len_ctr++];
                        *cur_len = 0;
                        break;

                /* End of array. Pop the previous counter off the stack and
                 * continue on adding any further elements to it. */
                case ']': case '}':
                        assert(len_stackp != len_stack);
                        cur_len = *(--len_stackp);
                        break;

                /* Legal values that do not increment the current size. */
                case ':': case '.': case ' ': case ',':
                case '!': case '@': case '*':
                        break;

                default:
                        errx(1, "Illegal character \"%c\" found in format.", ch);
                }
        }

        mpack_obj *pack = NULL;
        if (sub_lengths[0] == 0)
                goto cleanup;

#ifdef DEBUG
        pack = mpack_make_new(sub_lengths[0], true);
#else
        pack = mpack_make_new(sub_lengths[0], false);
#endif

        int        *sub_ctrs     = nmalloc(sizeof(int), arr_size);
        int        *dict_stack   = nmalloc(sizeof(int), TWO_THIRDS(arr_size));
        mpack_obj **obj_stack    = nmalloc(sizeof(mpack_obj *), TWO_THIRDS(arr_size));
        int        *cur_ctr      = sub_ctrs;
        int        *dict_stackp  = dict_stack;
        mpack_obj **obj_stackp   = obj_stack;
        mpack_obj **cur_obj      = NULL;
        unsigned    subctr_ctr   = 1;
        unsigned    a_arg_ctr    = 0;
        unsigned    a_arg_subctr = 0;
        *dict_stackp             = 0;
        len_ctr                  = 1;
        len_stackp               = len_stack;
        *obj_stackp              = pack;
        *(len_stackp++)          = cur_ctr;
        ptr                      = fmt;
        cur_obj                  = &pack->DAI[0];
        *cur_ctr                 = 1;

        while ((ch = *ptr++)) {
                switch (ch) {
                case 'b': case 'B': {
                        bool arg;
                        NEXT(arg, int, boolean);
                        mpack_encode_boolean(pack, cur_obj, arg);
                        break;
                }
                case 'd': case 'D': {
                        int arg;
                        NEXT(arg, int, num);
                        mpack_encode_integer(pack, cur_obj, arg);
                        break;
                }
                case 'l': case 'L': {
                        int64_t arg;
                        NEXT(arg, int64_t, num);
                        mpack_encode_integer(pack, cur_obj, arg);
                        break;
                }
                case 's': case 'S': {
                        bstring *arg;
                        NEXT(arg, bstring *, str);
                        mpack_encode_string(pack, cur_obj, arg);
                        break;
                }
                case 'n': case 'N':
                        mpack_encode_nil(pack, cur_obj);
                        break;

                case '[':
                        *(++dict_stackp) = 0;
                        mpack_encode_array(pack, cur_obj, sub_lengths[len_ctr]);
                        INC_CTRS();
                        break;

                case '{':
                        assert((sub_lengths[len_ctr] & 1) == 0);
                        *(++dict_stackp) = 1;
                        mpack_encode_dictionary(pack, cur_obj, (sub_lengths[len_ctr] / 2));
                        INC_CTRS();
                        break;

                case ']':
                case '}':
                        assert(obj_stackp != obj_stack);
                        assert(len_stackp != len_stack);
                        assert(dict_stackp != dict_stack);
                        --obj_stackp;
                        cur_ctr = *(--len_stackp);
                        --dict_stackp;
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

                if (*dict_stackp) {
                        if ((*obj_stackp)->data.dict->max > (*cur_ctr / 2)) {
                                if ((*cur_ctr & 1) == 0)
                                        cur_obj = &(*obj_stackp)->DDE[*cur_ctr / 2]->key;
                                else
                                        cur_obj = &(*obj_stackp)->DDE[*cur_ctr / 2]->value;
                        }
                } else {
                        if ((*obj_stackp)->data.arr->max > *cur_ctr)
                                cur_obj = &(*obj_stackp)->DAI[*cur_ctr];
                }

                ++(*cur_ctr);
        }

        free(dict_stack);
        free(obj_stack);
        free(sub_ctrs);
cleanup:
        free(sub_lengths);
        free(len_stack);
        va_end(args);
        return pack;
}
