#include "Common.h"

/* #include "highlight.h" */
#include "mpack.h"

P99_DEFINE_ENUM(mpack_expect_t);
static mpack_obj *find_key_value (mpack_dict *dict, bstring const *key) __attribute__((pure));

FILE  *mpack_log;

#if defined __GNUC__ && !defined __clang__
#  define PRAGMA_NO_NONHEAP()           \
        _Pragma("GCC diagnostic push"); \
        _Pragma("GCC diagnostic ignored \"-Wfree-nonheap-object\"")
#  define PRAGMA_NO_NONHEAP_POP() \
        _Pragma("GCC diagnostic pop")
#else
#  define PRAGMA_NO_NONHEAP()
#  define PRAGMA_NO_NONHEAP_POP()
#endif


#define SHUTUPGCC __attribute__((__unused__)) ssize_t n =
#include <execinfo.h>
#define SHOW_STACKTRACE()                                    \
      __extension__({                                        \
            void  *arr[128];                                 \
            size_t num = backtrace(arr, 128);                \
            fflush(stderr);                                  \
            fsync(2);                                        \
            SHUTUPGCC write(2, SLS("<<< STACKTRACE >>>\n")); \
            backtrace_symbols_fd(arr, num, 2);               \
            fsync(2);                                        \
      })

/*======================================================================================*/

mpack_retval
(mpack_expect)(mpack_obj *const obj, mpack_expect_t const type, bool destroy)
{
        mpack_retval ret = {.ptr = NULL};
        uint64_t     value;
        mpack_type_t err_expect;

        if (!obj)
                return ret;
#if 0
        if (mpack_log) {
                mpack_print_object(mpack_log, obj);
                fflush(mpack_log);
        }
#endif

        mpack_type_t const obj_type = mpack_type(obj);

        switch (type) {
        case E_MPACK_ARRAY:
                if (destroy)
                        talloc_steal(NULL, obj->arr);
                ret.ptr = obj->arr;
                break;

        case E_MPACK_DICT:
                if (obj_type != (err_expect = MPACK_DICT))
                        goto error;
                if (destroy)
                        talloc_steal(NULL, obj->dict);
                ret.ptr = obj->dict;
                break;

        case E_MPACK_EXT:
                if (obj_type != (err_expect = MPACK_EXT))
                        goto error;
                if (destroy)
                        talloc_steal(NULL, obj->ext);
                ret.ptr = obj->ext;
                break;

        case E_MPACK_NIL:
                if (obj_type != (err_expect = MPACK_NIL))
                        goto error;
                break;

        case E_BOOL:
                if (obj_type != (err_expect = MPACK_BOOL)) {
                        if (obj_type == MPACK_SIGNED || obj_type == MPACK_UNSIGNED)
                                value = obj->num;
                        else
                                goto error;
                } else {
                        value = obj->boolean;
                }
                ret.num = value;
                break;

        case E_NUM:
                if (obj_type != (err_expect = MPACK_SIGNED) &&
                    obj_type != MPACK_UNSIGNED)
                {
                        if (obj_type == MPACK_EXT)
                                ret.num = obj->ext->num;
                        else
                                goto error;
                } else {
                        ret.num = obj->num;
                }
                break;

        case E_STRING:
                if (obj_type != (err_expect = MPACK_STRING))
                        goto error;
                ret.ptr = obj->str;
                if (destroy)
                        talloc_steal(NULL, ret.ptr);
                break;

        case E_WSTRING:
                if (obj_type != (err_expect = MPACK_STRING))
                        goto error;
                ret.ptr = (wchar_t *)obj->str->data;
                if (destroy)
                        talloc_steal(NULL, ret.ptr);
                break;

        case E_STRLIST:
                if (obj_type != (err_expect = MPACK_ARRAY))
                        goto error;
                ret.ptr = mpack_array_to_blist(obj->arr, destroy);
                break;

        case E_DICT2ARR:
                abort();

        default:
                errx(1, "Invalid type given to %s()\n", FUNC_NAME);
        }

        if (destroy)
                talloc_free(obj);

        return ret;

error:
        warnx("WARNING: Got mpack of type %s, expected type %s, possible error.",
              m_type_names[mpack_type(obj)], m_type_names[err_expect]);
        SHOW_STACKTRACE();
        talloc_free(obj);
        ret.ptr = NULL;
        return ret;
        //errx(1, "Exiting");
}

/*======================================================================================*/
/* Type conversions */

b_list *
mpack_array_to_blist(mpack_array *array, bool const destroy)
{
        if (!array)
                return NULL;
        unsigned const size = array->qty;
        b_list        *ret  = b_list_create_alloc(size);

        if (destroy) {
                for (unsigned i = 0; i < size; ++i) {
                        b_list_append(ret, array->lst[i]->str);
                        array->lst[i]->str = NULL;
                }
                talloc_free(array);
        } else {
                for (unsigned i = 0; i < size; ++i)
                        b_list_append(ret, array->lst[i]->str);
        }

        return ret;
}

mpack_retval
mpack_dict_get_key(mpack_dict *dict, mpack_expect_t const expect, bstring const *key)
{
        assert(dict && key);

        mpack_obj *tmp = find_key_value(dict, key);
        if (!tmp)
                return (mpack_retval){ .ptr = NULL };

        return mpack_expect(tmp, expect, false);
}

static mpack_obj *
find_key_value(mpack_dict *dict, bstring const *key)
{
        for (unsigned i = 0; i < dict->qty; ++i)
                if (b_iseq(dict->lst[i]->key->str, key))
                        return dict->lst[i]->value;

        return NULL;
}

/*======================================================================================*/

void
mpack_destroy_arg_array(mpack_arg_array *calls)
{
        if (!calls)
                return;
        talloc_free(calls);
}

/*======================================================================================*/

enum encode_fmt_next_type { OWN_VALIST, OTHER_VALIST, ARG_ARRAY };

#define NEW_STACK(TYPE, NAME)             \
      struct {                            \
            unsigned ctr;                 \
            TYPE     arr[128];            \
      } NAME = {.ctr = 0}

#define POP(STACK) \
        ((((STACK).ctr == 0) ? abort() : (void)0), ((STACK).arr[--(STACK).ctr]))

#define PUSH(STACK, VAL) \
        ((STACK).arr[(STACK).ctr++] = (VAL))

#define PEEK(STACK) \
        ((((STACK).ctr == 0) ? abort() : (void)0), ((STACK).arr[(STACK).ctr - 1U]))

#define RESET(STACK) \
        ((STACK).ctr = 0, (STACK).arr[0] = 0)

#define STACK_CTR(STACK) ((STACK).ctr)

#ifdef DEBUG
#  define POP_DEBUG  POP
#  define PUSH_DEBUG PUSH
#else
#  define POP_DEBUG(STACK)       ((void)0)
#  define PUSH_DEBUG(STACK, VAL) ((void)0)
#endif

/*
 * Ugly macros to simplify the code below.
 */
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
                case ARG_ARRAY:                                               \
                        assert(a_args);                                       \
                        assert(a_args[a_arg_ctr]);                            \
                        (VAR_) = ((a_args[a_arg_ctr][a_arg_subctr]).MEMBER_); \
                        ++a_arg_subctr;                                       \
                        break;                                                \
                default:                                                      \
                        abort();                                              \
                }                                                             \
        } while (0)

#define NEXT_VALIST_ONLY(VAR_, TYPE_NAME_)                 \
        do {                                               \
                switch (next_type) {                       \
                case OWN_VALIST:                           \
                        (VAR_) = va_arg(args, TYPE_NAME_); \
                        break;                             \
                case OTHER_VALIST:                         \
                        assert(ref != NULL);               \
                        (VAR_) = va_arg(*ref, TYPE_NAME_); \
                        break;                             \
                case ARG_ARRAY:                            \
                default:                                   \
                        abort();                           \
                }                                          \
        } while (0)

#define ENCODE_FMT_ARRSIZE 128

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
 *     !: Denotes that the next argument is a pointer to an initialized va_list,
 *        from which all arguments thereafter will be taken, until either another
 *        '!' or '@' is encountered.
 *     @: Denotes an argument of type "mpack_argument **", that is to say an
 *        array of arrays of mpack_argument objects. When '@' is encountered, this
 *        double pointer is taken from the current argument source and used
 *        thereafter as such. The first sub array is used until a '*' is encountered
 *        in the format string, at which point the next array is used, and so on.
 *     *: Increments the current sub array of the mpack_argument ** object.
 *
 * All of the following characters are ignored in the format string, and may be
 * used to make it clearer or more visually pleasing: ':'  ';'  ','  '.', ' '
 *
 * All errors are fatal.
 */
mpack_obj *
mpack_encode_fmt(unsigned const size_hint, char const *const restrict fmt, ...)
{
        assert(fmt != NULL && *fmt != '\0');
        unsigned const arr_size = (size_hint)
                                    ? ENCODE_FMT_ARRSIZE + (size_hint * 6U)
                                    : ENCODE_FMT_ARRSIZE;

        mpack_argument **a_args;
        unsigned    sub_lengths[arr_size];
        int         ch;
        va_list     args;
        va_list    *ref       = NULL;
        char const *ptr       = fmt;
        unsigned   *cur_len   = &sub_lengths[0];
        unsigned    len_ctr   = 1;
        int         next_type = OWN_VALIST;
        *cur_len = 0;
        a_args   = NULL;

        NEW_STACK(unsigned *, len_stack);
        va_start(args, fmt);

        /* Go through the format string once to get the number of arguments and
         * in particular the number and size of any arrays. */
        while ((ch = (uchar)*ptr++)) {
                switch (ch) {
                /* Legal values. Increment size and continue. */
                case 'b': case 'B': case 'l': case 'L':
                case 'd': case 'D': case 's': case 'S':
                case 'n': case 'N': case 'c': case 'C':
                case 'u': case 'U':
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
                case ';': case ':': case '.': case ' ': case ',':
                case '!': case '@': case '*':
                        break;

                default:
                        errx(1, "Illegal character \"%c\" found in format.", ch);
                }
        }

        if (STACK_CTR(len_stack) != 0)
                errx(1, "Invalid encode format string: undetermined array/dictionary.\n\"%s\"", fmt);
        if (sub_lengths[0] > 1)
                errx(1, "Invalid encode format string: Cannot encode multiple items "
                        "at the top level. Put them in an array.\n\"%s\"", fmt);
        if (sub_lengths[0] == 0) {
                va_end(args);
                return NULL;
        }

#ifdef DEBUG
        mpack_obj  *pack    = mpack_make_new(sub_lengths[0], true);
        mpack_obj **cur_obj = &pack;
#else
        mpack_obj  *pack    = mpack_make_new(sub_lengths[0], false);
        mpack_obj **cur_obj = NULL;
#endif
        unsigned  sub_ctrlist[len_ctr + 1];
        unsigned *cur_ctr      = sub_ctrlist;
        unsigned  subctr_ctr   = 1;
        unsigned  a_arg_ctr    = 0;
        unsigned  a_arg_subctr = 0;
        len_ctr                = 1;
        ptr                    = fmt;
        *cur_ctr               = 1;

        RESET(len_stack);
        NEW_STACK(unsigned char, dict_stack);
        PUSH(len_stack, cur_ctr);
        PUSH(dict_stack, 0);
#ifdef DEBUG
        NEW_STACK(mpack_obj *, obj_stack);
        PUSH(obj_stack, pack);
#endif

        /* This loop is where all of the actual interpretation and encoding
         * happens. A few stacks are used when recursively encoding arrays and
         * dictionaries to store the state of the enclosing object(s). */
        while ((ch = (uchar)*ptr++)) {
                switch (ch) {
                case 'b': case 'B': {
                        bool arg = 0;
                        NEXT(arg, int, boolean);
                        mpack_encode_boolean(pack, cur_obj, arg);
                }       break;

                case 'd': case 'D': {
                        int arg = 0;
                        NEXT(arg, int, num);
                        mpack_encode_integer(pack, cur_obj, arg);
                }       break;

                case 'l': case 'L': {
                        int64_t arg = 0;
                        NEXT(arg, int64_t, num);
                        mpack_encode_integer(pack, cur_obj, arg);
                }       break;

                case 'u': case 'U': {
                        uint64_t arg = 0;
                        NEXT(arg, uint64_t, uint);
                        mpack_encode_unsigned(pack, cur_obj, arg);
                }       break;

                case 's': case 'S': {
                        bstring *arg = NULL;
                        NEXT(arg, bstring *, str);
                        mpack_encode_string(pack, cur_obj, arg);
                }       break;

                case 'c': case 'C': {
                        char const *arg;
                        NEXT(arg, char *, c_str);
                        bstring tmp = bt_fromcstr(arg);
                        mpack_encode_string(pack, cur_obj, &tmp);
                }       break;

                case 'n': case 'N':
                        mpack_encode_nil(pack, cur_obj);
                        break;

                case '[':
                        mpack_encode_array(pack, cur_obj, sub_lengths[len_ctr]);
                        PUSH(dict_stack, 0);
                        PUSH(len_stack, cur_ctr);
                        PUSH_DEBUG(obj_stack, *cur_obj);
                        ++len_ctr;
                        cur_ctr  = &sub_ctrlist[subctr_ctr++];
                        *cur_ctr = 0;
                        break;

                case '{':
                        assert((sub_lengths[len_ctr] & 1) == 0);
                        mpack_encode_dictionary(pack, cur_obj, (sub_lengths[len_ctr] / 2));
                        PUSH(dict_stack, 1);
                        PUSH(len_stack, cur_ctr);
                        PUSH_DEBUG(obj_stack, *cur_obj);
                        ++len_ctr;
                        cur_ctr  = &sub_ctrlist[subctr_ctr++];
                        *cur_ctr = 0;
                        break;

                case ']':
                case '}':
                        (void)POP(dict_stack);
                        cur_ctr = POP(len_stack);
                        (void)POP_DEBUG(obj_stack);
                        break;

                /* The following use `continue' to skip incrementing the counter */
                case '!':
                        NEXT_VALIST_ONLY(ref, va_list *);
                        next_type = OTHER_VALIST;
                        continue;

                case '@':
                        NEXT_VALIST_ONLY(a_args, mpack_argument **);
                        a_arg_ctr    = 0;
                        a_arg_subctr = 0;
                        assert(a_args[a_arg_ctr]);
                        next_type = ARG_ARRAY;
                        continue;

                case '*':
                        assert(next_type == ARG_ARRAY);
                        ++a_arg_ctr;
                        a_arg_subctr = 0;
                        continue;

                case ';': case ':': case '.': case ' ': case ',':
                        continue;

                default:
                        errx(1, "Somehow (?!) found an invalid character in an mpack format string.");
                }

#ifdef DEBUG
                if (PEEK(dict_stack)) {
                        if (PEEK(obj_stack)->dict->max > (*cur_ctr / 2))
                                cur_obj = (*cur_ctr & 1) == 0
                                        ? &PEEK(obj_stack)->dict->lst[*cur_ctr / 2]->key
                                        : &PEEK(obj_stack)->dict->lst[*cur_ctr / 2]->value;
                } else if (PEEK(obj_stack)->arr->max > *cur_ctr) {
                        cur_obj = &PEEK(obj_stack)->arr->lst[*cur_ctr];
                }
#endif

                ++*cur_ctr;
        }

        va_end(args);
        return pack;
}
