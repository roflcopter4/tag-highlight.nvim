#include "Common.h"

/* #include "highlight.h" */
#include "mpack.h"

struct item_free_stack {
        mpack_obj **items;
        uint32_t    qty;
        uint32_t    max;
};

static void       collect_items  (struct item_free_stack *tofree, mpack_obj *item);
static mpack_obj *find_key_value (mpack_dict *dict, bstring const *key) __attribute__((pure));
static void       free_stack_push(struct item_free_stack *list, void *item);

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

pthread_mutex_t mpack_rw_lock = PTHREAD_MUTEX_INITIALIZER;
__attribute__((__constructor__))
static void _mpack_mutex_constructor(void)
{
        pthread_mutexattr_t attr;
        memset(&attr, 0, sizeof(attr));
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mpack_rw_lock, &attr);
}

/*======================================================================================*/

mpack_retval
(mpack_expect)(mpack_obj *const obj, mpack_expect_t const type, bool destroy)
{
        mpack_retval ret = {.ptr = NULL};
        uint64_t     value;
        mpack_type_t err_expect;
        pthread_mutex_lock(&mpack_rw_lock);

        if (!obj)
                return ret;
        if (mpack_log) {
                mpack_print_object(mpack_log, obj);
                fflush(mpack_log);
        }

        mpack_type_t const obj_type = mpack_type(obj);

        switch (type) {
        case E_MPACK_ARRAY:
                if (destroy)
                        mpack_spare_data(obj);
                ret.ptr = obj->data.arr;
                break;

        case E_MPACK_DICT:
                if (obj_type != (err_expect = MPACK_DICT))
                        goto error;
                if (destroy)
                        mpack_spare_data(obj);
                ret.ptr = obj->data.dict;
                break;

        case E_MPACK_EXT:
                if (obj_type != (err_expect = MPACK_EXT))
                        goto error;
                if (destroy)
                        mpack_spare_data(obj);
                ret.ptr = obj->data.ext;
                break;

        case E_MPACK_NIL:
                if (obj_type != (err_expect = MPACK_NIL))
                        goto error;
                break;

        case E_BOOL:
                if (obj_type != (err_expect = MPACK_BOOL)) {
                        if (obj_type == MPACK_SIGNED || obj_type == MPACK_UNSIGNED)
                                value = obj->data.num;
                        else
                                goto error;
                } else {
                        value = obj->data.boolean;
                }
                ret.num = value;
                break;

        case E_NUM:
                if (obj_type != (err_expect = MPACK_SIGNED) &&
                    obj_type != MPACK_UNSIGNED)
                {
                        if (obj_type == MPACK_EXT)
                                ret.num = obj->data.ext->num;
                        else
                                goto error;
                } else {
                        ret.num = obj->data.num;
                }
                break;

        case E_STRING:
                if (obj_type != (err_expect = MPACK_STRING))
                        goto error;
                ret.ptr = obj->data.str;
                if (destroy)
                        b_writeprotect((bstring *)ret.ptr);
                break;

        case E_STRLIST:
                if (obj_type != (err_expect = MPACK_ARRAY))
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
                errx(1, "Invalid type given to %s()\n", FUNC_NAME);
        }

        if (destroy) {
                mpack_destroy_object(obj);
                if (type == E_STRING)
                        b_writeallow((bstring *)ret.ptr);
        }

        pthread_mutex_unlock(&mpack_rw_lock);
        return ret;

error:
        warnx("WARNING: Got mpack of type %s, expected type %s, possible error.",
              m_type_names[mpack_type(obj)], m_type_names[err_expect]);
        mpack_destroy_object(obj);
        ret.ptr = NULL;
        abort();
}

/*======================================================================================*/

void
mpack_destroy_object(mpack_obj *root)
{
        pthread_mutex_lock(&mpack_rw_lock);

        if (!(root->flags & MPACKFLG_ENCODE)) {
                if (root->flags & MPACKFLG_HAS_PACKED)
                        b_destroy(*root->packed);

                /* This gcc specific warning is a false positive. Non-heap
                 * objects all have the `phony' flag and won't be free'd. */
                PRAGMA_NO_NONHEAP();
                if (!(root->flags & MPACKFLG_PHONY))
                        free(root);
                PRAGMA_NO_NONHEAP_POP();
                pthread_mutex_unlock(&mpack_rw_lock);
                return;
        }

        struct item_free_stack tofree = {nmalloc(1024, sizeof(void *)), 0, 1024};
        collect_items(&tofree, root);

        for (int64_t i = (int64_t)(tofree.qty - 1); i >= 0; --i) {
                mpack_obj *cur = tofree.items[i];
                if (!cur)
                        continue;
                if (!(cur->flags & MPACKFLG_SPARE_DATA)) {
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

                if (cur->flags & MPACKFLG_HAS_PACKED)
                        b_destroy(*cur->packed);
                if (!(cur->flags & MPACKFLG_PHONY))
                        free(cur);
        }

        free(tofree.items);
        pthread_mutex_unlock(&mpack_rw_lock);
}

static void
collect_items(struct item_free_stack *tofree, mpack_obj *item)
{
        if (!item)
                return;
        free_stack_push(tofree, item);
        if (item->flags & MPACKFLG_SPARE_DATA)
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
                                collect_items(tofree, item->DDE[i]->key);
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

static void
free_stack_push(struct item_free_stack *list, void *item)
{
        if (list->qty == (list->max - 1))
                list->items = nrealloc(list->items, (list->max *= 2),
                                       sizeof(*list->items));
        list->items[list->qty++] = item;
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
                        b_writeprotect(array->items[i]->data.str);
                        b_list_append(ret, array->items[i]->data.str);
                }

                mpack_array_destroy(array);
                b_list_writeallow(ret);
        } else {
                for (unsigned i = 0; i < size; ++i)
                        b_list_append(ret, array->items[i]->data.str);
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
                if (b_iseq(dict->entries[i]->key->data.str, key))
                        return dict->entries[i]->value;

        return NULL;
}

/*======================================================================================*/

void
mpack_destroy_arg_array(mpack_arg_array *calls)
{
        if (!calls)
                return;
        for (unsigned i = 0; i < calls->qty; ++i) {
                unsigned x = 0;
                for (char const *ptr = calls->fmt[i]; *ptr; ++ptr) {
                        switch (*ptr) {
                        case 'b': case 'B':
                        case 'd': case 'D':
                                ++x;
                                break;
                        case 's': case 'S':
                                b_destroy(calls->args[i][x].str); ++x;
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

enum encode_fmt_next_type { OWN_VALIST, OTHER_VALIST, ARG_ARRAY };

/*
 * I'm using two variables, stack and counter, rather than some struct simply to
 * get around any type checking problems. This way allows the stack to be
 * composed of the actual type it holds (rather than a void pointer) and also
 * allows the elements to be integers without requiring some hack.
 */
#define NEW_STACK(TYPE, NAME) \
        TYPE     NAME[128];    \
        unsigned NAME##_ctr = 0

#define POP(STACK) \
        (((STACK##_ctr == 0) ? abort() : (void)0), ((STACK)[--STACK##_ctr]))

#define PUSH(STACK, VAL) \
        ((STACK)[STACK##_ctr++] = (VAL))

#define PEEK(STACK) \
        (((STACK##_ctr == 0) ? abort() : (void)0), ((STACK)[STACK##_ctr - 1]))

#define RESET(STACK) \
        ((STACK##_ctr) = 0, (STACK)[0] = 0)

#define STACK_CTR(STACK) (STACK##_ctr)

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
 *     !: Denotes a pointer to an initialized va_list, from which all arguments
 *        thereafter will be taken, until either another '!' or '@' is encountered.
 *     @: Denotes an argument of type "mpack_argument **", that is to
 *        say an array of arrays of mpack_argument objects. When '@'
 *        is encountered, this double pointer is taken from the current argument
 *        source and used thereafter as such. The first sub array is used until
 *        a '*' is encountered in the format string, at which point the next
 *        array is used, and so on.
 *     *: Increments the current sub array of the mpack_argument ** object.
 *
 * All of the following characters are ignored in the format string, and may be
 * used to make it clearer or more visually pleasing: ':'  ';'  ','  ' '
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

        unsigned         sub_lengths[arr_size];
        int              ch;
        va_list          args;
        va_list *        ref       = NULL;
        mpack_argument **a_args    = NULL;
        char const *     ptr       = fmt;
        unsigned *       cur_len   = &sub_lengths[0];
        unsigned         len_ctr   = 1;
        int              next_type = OWN_VALIST;
        *cur_len                   = 0;

#ifdef DEBUG
        memset(sub_lengths, 0, (size_t)arr_size * sizeof(*sub_lengths));
#endif
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
                case 'u':
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

                case 'u': {
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
                        abort();
                }

#ifdef DEBUG
                if (PEEK(dict_stack)) {
                        if (PEEK(obj_stack)->data.dict->max > (*cur_ctr / 2))
                                cur_obj = (*cur_ctr & 1) == 0
                                        ? &PEEK(obj_stack)->DDE[*cur_ctr / 2]->key
                                        : &PEEK(obj_stack)->DDE[*cur_ctr / 2]->value;
                } else if (PEEK(obj_stack)->data.arr->max > *cur_ctr) {
                        cur_obj = &PEEK(obj_stack)->DAI[*cur_ctr];
                }
#endif

                ++*cur_ctr;
        }

        va_end(args);
        return pack;
}
