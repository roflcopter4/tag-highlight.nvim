#include "util.h"

#include "data.h"
/* #include "macros.h" */
#include "mpack.h"

static void       collect_items (struct item_free_stack *tofree, mpack_obj *item);
static mpack_obj *find_key_value(dictionary *dict, const bstring *key);
static void      *get_expect    (mpack_obj *result, const enum mpack_types expect,
                                 const bstring *key, bool destroy, bool is_retval);

static unsigned        sok_count, io_count;
extern pthread_mutex_t mpack_main;

#define ENCODE_FMT_ARRSIZE 512
#define DAI data.arr->items
#define DDE data.dict->entries

#define COUNT(FD_) (((FD_) == 1) ? io_count++ : sok_count++)


#define WRITE_AND_CLEAN(FD__, MPACK, func)                                     \
        do {                                                                   \
                fprintf(mpack_log, "Writing request no %d to fd %d: \"%s\"\n", \
                        COUNT(fd) - 1, (FD__), BS(func));                      \
                b_write((FD__), *(MPACK)->packed);                             \
                mpack_destroy(MPACK);                                          \
        } while (0)


#define validate_expect(EXPECT, ...)                                                   \
        do {                                                                           \
                const enum mpack_types lst[] = {__VA_ARGS__};                          \
                bool found = false;                                                    \
                for (unsigned i_ = 0; i_ < ARRSIZ(lst); ++i_) {                        \
                        if ((EXPECT) == lst[i_]) {                                     \
                                found = true;                                          \
                                break;                                                 \
                        }                                                              \
                }                                                                      \
                if (!found)                                                            \
                        errx(1, "Invalid argument \"%s\" in %s(), line %d of file %s", \
                             (expect <= 7 ? m_type_names[EXPECT] : "TOO LARGE"),       \
                             __func__, __LINE__, __FILE__);                            \
        } while (0)


#define FATAL(...)                                      \
        do {                                            \
                if (!ret && fatal)                      \
                        errx(1, "ERROR: " __VA_ARGS__); \
        } while (0)


#define BT_FUNCNAME()                                                         \
        static bstring func[1] = {{.data = NULL}};                            \
        if (!func[0].data)                                                    \
                func[0] = (bstring){ .slen = sizeof(__func__) - 1, .mlen = 0, \
                                     .data = (uchar *)__func__, .flags = 0 }


#define encode_fmt_api(FD__, FMT_, ...) \
        encode_fmt(("d:d:s:[" FMT_ "]"), 0, COUNT(FD__), __VA_ARGS__)


/*============================================================================*/


void
__nvim_write(const int fd, const enum nvim_write_type type, const bstring *mes)
{
        /* pthread_mutex_lock(&mpack_main); */
        abort();
        bstring *func;
        switch (type) {
        case STANDARD: func = B("nvim_out_write");   break;
        case ERROR:    func = B("nvim_err_write");   break;
        case ERROR_LN: func = B("nvim_err_writeln"); break;
        default:       errx(1, "Should be unreachable!");
        }

        mpack_obj *pack = encode_fmt_api(fd, "s", func, mes);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *tmp = decode_stream(fd, MES_RESPONSE);
        /* pthread_mutex_unlock(&mpack_main); */

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


/*----------------------------------------------------------------------------*/

#define RETVAL (result->DAI[3])


b_list *
nvim_buf_attach(const int fd, const int bufnum)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "d,B,[]", func, bufnum, true);
        WRITE_AND_CLEAN(fd, pack, func);
        
        pthread_mutex_unlock(&mpack_main);
        return NULL;
}

/*----------------------------------------------------------------------------*/

void *
nvim_list_bufs(const int fd)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "", func);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = get_expect(result, MPACK_ARRAY, NULL, true, true);

        pthread_mutex_unlock(&mpack_main);
        return ret;
}

int
nvim_get_current_buf(const int fd)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "", func);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main);

        const int ret = RETVAL->data.ext->num;
        print_and_destroy(result);
        return ret;
}

bstring *
nvim_get_current_line(const int fd)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "", func);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main);
        void *ret = get_expect(result, MPACK_STRING, NULL, true, true);
        
        return ret;
}

unsigned
nvim_buf_line_count(const int fd, const int bufnum)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "d", func, bufnum);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result    = decode_stream(fd, MES_RESPONSE);
        unsigned   linecount = (unsigned)(RETVAL->data.num);
        print_and_destroy(result);
        
        pthread_mutex_unlock(&mpack_main);
        return linecount;
}

b_list *
nvim_buf_get_lines(const int      fd,
                   const unsigned bufnum,
                   const int      start,
                   const int      end)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "d,d,d,d", func, bufnum, start, end, 0);
        WRITE_AND_CLEAN(fd, pack, func);

        b_list *ret = mpack_array_to_blist(get_expect(decode_stream(fd, MES_RESPONSE),
                                           MPACK_ARRAY, NULL, true, true), true);
        pthread_mutex_unlock(&mpack_main);
        return ret;
}

bstring *
nvim_buf_get_name(const int fd, const int bufnum)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "d", func, bufnum);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        bstring   *ret    = get_expect(result, MPACK_STRING, NULL, true, true);

        char fullname[PATH_MAX];
        char *tmp = realpath(BS(ret), fullname);
        b_assign_cstr(ret, tmp);

        pthread_mutex_unlock(&mpack_main);
        return ret;
}

/*----------------------------------------------------------------------------*/

bool
nvim_command(const int fd, const bstring *cmd, const bool fatal)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "s", func, cmd);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        bool       ret    = mpack_type(result->DAI[2]) == MPACK_NIL;

        if (mpack_type(result->data.arr->items[2]) == MPACK_ARRAY) {
                bstring *errmsg = result->DAI[2]->DAI[1]->data.str;
                b_fputs(stderr, errmsg, B("\n"));
        }

        print_and_destroy(result);
        assert(!(fatal && !ret));
        pthread_mutex_unlock(&mpack_main);
        return ret;
}

void *
nvim_command_output(const int              fd,
                    const bstring *        cmd,
                    const enum mpack_types expect,
                    const bstring *        key,
                    const bool             fatal)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "s", func, cmd);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = NULL;

        if (mpack_type(result->data.arr->items[2]) == MPACK_ARRAY) {
                bstring *errmsg = result->DAI[2]->DAI[1]->data.str;
                b_fputs(stderr, errmsg, B("\n"));
        } else
                ret = get_expect(result, expect, key, true, true);

        assert(!(fatal && !ret));
        pthread_mutex_unlock(&mpack_main);

        return ret;
}

void *
nvim_call_function(const int              fd,
                   const bstring *        function,
                   const enum mpack_types expect,
                   const bstring *        key,
                   const bool             fatal)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "s,[]", func, function);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = get_expect(result, expect, key, true, true);

        FATAL("Failed to analyze out put of function \"%s\".", BS(function));

        pthread_mutex_unlock(&mpack_main);
        return ret;
}

void *
nvim_call_function_args(const int              fd,
                        const bstring *        function,
                        const enum mpack_types expect,
                        const bstring *        key,
                        const bool             fatal,
                        const char *fmt,
                        ...)
{
        static const bstring func = bt_init("nvim_call_function");
        va_list va;
        char buf[2048];
        snprintf(buf, 2048, "d:d:s:[s,[!%s]]", fmt);
        pthread_mutex_lock(&mpack_main);

        va_start(va, fmt);
        mpack_obj *pack = encode_fmt(buf, 0, COUNT(fd), &func, function, &va);
        mpack_print_object(pack, mpack_log); fflush(mpack_log);
        WRITE_AND_CLEAN(fd, pack, &func);
        va_end(va);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void      *ret    = get_expect(result, expect, key, true, true);

        FATAL("Failed to analyze output of function \"%s\".", BS(function));

        pthread_mutex_unlock(&mpack_main);
        return ret;
}

/*----------------------------------------------------------------------------*/

void *
nvim_get_var(const int              fd,
             const bstring *        varname,
             const enum mpack_types expect,
             const bstring *        key,
             const bool             fatal)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "s", func, varname);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main);
        void *ret = get_expect(result, expect, key, true, true);

        FATAL("Failed to retrieve variable \"%s\".", BS(varname));

        return ret;
}

void *
nvim_get_option(const int              fd,
                const bstring *        optname,
                const enum mpack_types expect,
                const bstring *        key,
                const bool             fatal)
{
        pthread_mutex_lock(&mpack_main);
        validate_expect(expect, MPACK_STRING, MPACK_NUM, MPACK_BOOL);
        BT_FUNCNAME();

        mpack_obj *pack = encode_fmt_api(fd, "s", func, optname);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        void *     ret    = get_expect(result, expect, key, true, true);

        FATAL("Failed to retrieve option \"%s\".", BS(optname));

        pthread_mutex_unlock(&mpack_main);
        return ret;
}

void *
nvim_buf_get_option(const int              fd,
                    const int              bufnum,
                    const bstring *        optname,
                    const enum mpack_types expect,
                    const bstring *        key,
                    const bool             fatal)
{
        pthread_mutex_lock(&mpack_main);
        validate_expect(expect, MPACK_STRING, MPACK_NUM, MPACK_BOOL);
        BT_FUNCNAME();

        mpack_obj *pack = encode_fmt_api(fd, "d:s", func, bufnum, optname);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main);
        void *ret = get_expect(result, expect, key, true, true);

        FATAL("Failed to retrieve option \"%s\".", BS(optname));

        return ret;
}

/*----------------------------------------------------------------------------*/

void
nvim_get_api_info(const int fd)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "", func);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        print_and_destroy(result);
        pthread_mutex_unlock(&mpack_main);
}

void
nvim_subscribe(const int fd, const bstring *event)
{
        pthread_mutex_lock(&mpack_main);
        BT_FUNCNAME();
        mpack_obj *pack = encode_fmt_api(fd, "s", func, event);
        WRITE_AND_CLEAN(fd, pack, func);

        mpack_obj *result = decode_stream(fd, MES_RESPONSE);
        pthread_mutex_unlock(&mpack_main);
        print_and_destroy(result);
}


/*============================================================================*/


static void *
get_expect(mpack_obj *            result,
           const enum mpack_types expect,
           const bstring *        key,
           const bool             destroy,
           const bool             is_retval)
{
        mpack_obj *cur;

        if (is_retval)
                cur = RETVAL;
        else
                cur = result;

        void      *ret = NULL;
        int64_t    value;
        assert(mpack_log != NULL);
        mpack_print_object(result, mpack_log);

        if (mpack_type(cur) == MPACK_NIL) {
                echo("Neovim returned nil!");
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


/*============================================================================*/


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
                                        for (uint j = 0; j < cur->data.dict->qty; ++j)
                                                free(cur->DDE[j]);
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


/*============================================================================*/
/* Type conversions */


b_list *
mpack_array_to_blist(struct mpack_array *array, const bool destroy)
{
        unsigned size = array->qty;
        b_list * ret  = b_list_create_alloc(size);

        if (destroy) {
                for (unsigned i = 0; i < size; ++i) {
                        b_writeprotect(array->items[i]->data.str);
                        b_add_to_list(ret, array->items[i]->data.str);
                }

                destroy_mpack_array(array);

                for (unsigned i = 0; i < size; ++i)
                        b_writeallow(ret->lst[i]);
        } else {
                for (unsigned i = 0; i < size; ++i)
                        b_add_to_list(ret, array->items[i]->data.str);
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
        b_free(varname);
        return mpack_array_to_blist(ret, true);
}


void *
nvim_get_var_fmt(const int              fd,
                 const enum mpack_types expect,
                 const bstring *        key,
                 const bool             fatal,
                 const char *           fmt,
                 ...)
{
        va_list va;
        va_start(va, fmt);
        bstring *varname = b_vformat(fmt, va);
        va_end(va);

        void *ret = nvim_get_var(fd, varname, expect, key, fatal);
        b_free(varname);
        return ret;
}


void *
dict_get_key(dictionary *           dict,
             const enum mpack_types expect,
             const bstring *        key,
             const bool             fatal)
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
find_key_value(dictionary *dict, const bstring *key)
{
        mpack_obj *ret = NULL;

        for (unsigned i = 0; i < dict->qty; ++i) {
                if (b_iseq(dict->entries[i]->key->data.str, key)) {
                        /* nvprintf("Found at pos %u key %s", i,
                                 BS(dict->entries[i]->key->data.str)); */
                        ret = dict->entries[i]->value;
                        break;
                }
        }

        return ret;
}


/*============================================================================*/


#define NEXT(TYPE_NAME_)                          \
        ((ref == NULL) ? va_arg(args, TYPE_NAME_) \
                       : va_arg(*ref, TYPE_NAME_))

#define ENCODE(TYPE, VALUE) \
        mpack_encode_##TYPE(pack, cur->data.arr, &(cur->DAI[sub_ctrs[ctr]++]), (VALUE))

mpack_obj *
encode_fmt(const char *const restrict fmt, ...)
{
        int         sub_lengths[ENCODE_FMT_ARRSIZE] = ZERO_512;
        int         sub_ctrs[ENCODE_FMT_ARRSIZE]    = ZERO_512;
        int         ch, sub = 0, ctr = 0;
        const char *ptr = fmt;
        va_list     args, *ref = NULL;

        va_start(args, fmt);

        while ((ch = *ptr++)) {
                assert(ctr >=0 && ctr < ENCODE_FMT_ARRSIZE);

                switch (ch) {
                case 'b': case 'B':
                case 'm': case 'M':
                case 'd': case 'D':
                case 's': case 'S':
                        ++sub_lengths[ctr];
                        break;
                case '[':
                        ++sub_lengths[ctr++];
                        ++sub;
                        break;
                case ']':
                        --ctr;
                        break;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                        sub_lengths[ctr] += xatoi(ptr - 1);
                        ptr = strchrnul(ptr, ']');
                        break;

                case ':': case '.': case ' ': case ',': case '!':
                        continue;

                default:
                        errx(1, "Illegal character \"%c\" found in format.", ch);
                }
        }

        mpack_obj *pack = mpack_make_new(sub_lengths[0], true);

        if (sub_lengths[0] == 0) {
                va_end(args);
                return pack;
        }

        ctr = 1;
        ptr = fmt;
        mpack_obj *stack[ENCODE_FMT_ARRSIZE];
        mpack_obj **stackp = stack;
        mpack_obj *cur     = pack;

        while ((ch = *ptr++)) {
                switch (ch) {
                case 'b': case 'B': {
                        bool arg = NEXT(int);
                        ENCODE(boolean, arg);
                        break;
                }
                case 'd': case 'D': {
                        int arg = NEXT(int);
                        ENCODE(integer, arg);
                        break;
                }
                case 's': case 'S': {
                        bstring *arg = NEXT(bstring *);
                        ENCODE(string, arg);
                        break;
                }
                case '[':
                        ENCODE(array, sub_lengths[ctr]);
                        *(stackp++) = cur;
                        cur = cur->DAI[sub_ctrs[ctr++] - 1];
                        break;

                case ']':
                        assert(ctr > 1);
                        cur = *(--stackp);
                        --ctr;
                        break;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                        for (; sub_ctrs[ctr] < sub_lengths[ctr]; ++sub_ctrs[ctr])
                                cur->DAI[sub_ctrs[ctr]] = NULL;

                        ptr = strchrnul(ptr, ']');
                        break;

                case '!':
                        ref = NEXT(va_list *);
                        break;

                case ':': case '.': case ' ': case ',':
                        continue;

                case 'm': case 'M':
                default:
                        abort();
                }

        }

        va_end(args);
        return pack;
}
