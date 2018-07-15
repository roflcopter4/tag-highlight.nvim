#include "util.h"
#include <inttypes.h>

#include "mpack.h"

static void do_mpack_print_object(const mpack_obj *result);
static void print_array (const mpack_obj *result);
static void print_bool  (const mpack_obj *result);
static void print_dict  (const mpack_obj *result);
static void print_ext   (const mpack_obj *result);
static void print_nil   (const mpack_obj *result);
static void print_number(const mpack_obj *result);
static void print_string(const mpack_obj *result, bool ind);

#define CAST_INT(OBJ)                                                   \
                ((OBJ)->type == M_INT_32)  ? (int32_t)(OBJ)->data.num : \
                (((OBJ)->type == M_INT_16) ? (int16_t)(OBJ)->data.num : \
                 (((OBJ)->type == M_INT_8) ? (int8_t)(OBJ)->data.num))

#define CAST_UINT(OBJ)                                                   \
                ((OBJ)->type == M_UINT_32)  ? (int32_t)(OBJ)->data.num : \
                (((OBJ)->type == M_UINT_16) ? (int16_t)(OBJ)->data.num : \
                 (((OBJ)->type == M_UINT_8) ? (int8_t)(OBJ)->data.num))

#define LOG(STRING) b_puts(butt, B(STRING))

extern pthread_mutex_t printmutex;
static int             indent    = 0;
static int             recursion = 0;
static FILE *          butt      = NULL;


__attribute__((always_inline))
static inline void
pindent(void)
{
        if (indent <= 0)
                return;
        for (int i = 0; i < (indent * 4); ++i)
                fputc(' ', butt);
}


void
mpack_print_object(const mpack_obj *result, FILE *fp)
{
        assert(result != NULL);
        assert(fp != NULL);

        pthread_mutex_lock(&printmutex);
        recursion = 0;
        butt = fp;
        do_mpack_print_object(result);
        butt = NULL;
        pthread_mutex_unlock(&printmutex);
}


static void
do_mpack_print_object(const mpack_obj *result)
{
        assert(butt != NULL);
        if (ferror(butt))
                abort();
        ++recursion;

        switch (mpack_type(result)) {
        case MPACK_ARRAY:  print_array (result);    break;
        case MPACK_BOOL:   print_bool  (result);    break;
        case MPACK_DICT:   print_dict  (result);    break;
        case MPACK_EXT:    print_ext   (result);    break;
        case MPACK_NIL:    print_nil   (result);    break;
        case MPACK_NUM:    print_number(result);    break;
        case MPACK_STRING: print_string(result, 1); break;
        case MPACK_UNINITIALIZED:
        default:
                errx(1, "Got uninitialized item to print!");
        }

        if (--recursion == 0)
                fputc('\n', butt);
}

extern FILE *whyy;

static void
print_array(const mpack_obj *result)
{
        if (ferror(butt))
                abort();
        pindent();

        if (!result->data.arr || result->data.arr->qty == 0) {
                b_fputs(butt, B("[]\n"));
        } else {
                b_fputs(butt, B("[\n"));

                ++indent;
                for (unsigned i = 0; i < result->data.arr->qty; ++i)
                        if (result->data.arr->items[i])
                                do_mpack_print_object(result->data.arr->items[i]);
                --indent;

                pindent();
                b_fputs(butt, B("]\n"));
        }
}


static void
print_dict(const mpack_obj *result)
{
        if (ferror(butt))
                abort();
        pindent();
        b_fputs(butt, B("{\n"));
        ++indent;

        for (unsigned i = 0; i < result->data.dict->qty; ++i) {
                pindent();
                /* b_fputs(butt, B("KEY:  ")); */
                print_string(result->data.dict->entries[i]->key, 0);

                fseek(butt, -1, SEEK_CUR);
                int tmp = indent;

                switch (mpack_type(result->data.dict->entries[i]->value)) {
                case MPACK_ARRAY:
                case MPACK_DICT:
                        b_fputs(butt, B("  => (\n"));

                        ++indent;
                        do_mpack_print_object(result->data.dict->entries[i]->value);
                        --indent;

                        pindent();
                        b_fputs(butt, B(")\n"));
                        break;
                default:
                        b_fputs(butt, B("  =>  "));

                        indent = 0;
                        do_mpack_print_object(result->data.dict->entries[i]->value);
                        indent = tmp;
                        break;
                }

        }

        --indent;
        pindent();
        b_fputs(butt, B("}\n"));
}


static void
print_string(const mpack_obj *result, const bool ind)
{
        if (ferror(butt))
                abort();
        if (ind)
                pindent();

        fprintf(butt, "\"%s\"\n", BS(result->data.str));
}


static void
print_ext(const mpack_obj *result)
{
        pindent();
        fprintf(butt, "Type: %d -> Data: %d\n",
                mpack_type(result), result->data.ext->num);
}


static void
print_nil(UNUSED const mpack_obj *result)
{
        if (ferror(butt))
                abort();
        pindent();

        b_fputs(butt, B("NIL\n"));
}


static void
print_bool(const mpack_obj *result)
{
        pindent();
        if (result->data.boolean)
                b_fputs(butt, B("true\n"));
        else
                b_fputs(butt, B("false\n"));
}


static void
print_number(const mpack_obj *result)
{
        if (ferror(butt))
                abort();
        pindent();

        fprintf(butt, "%"PRId64"\n", result->data.num);
}
