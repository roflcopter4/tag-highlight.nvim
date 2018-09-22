#include "util/util.h"

#include "mpack.h"

static void do_mpack_print_object(const mpack_obj *result);
static void print_array (const mpack_obj *result);
static void print_bool  (const mpack_obj *result);
static void print_dict  (const mpack_obj *result);
static void print_ext   (const mpack_obj *result);
static void print_nil   (const mpack_obj *result);
static void print_number(const mpack_obj *result);
static void print_string(const mpack_obj *result, bool ind);

#define DAI data.arr->items
#define DDE data.dict->entries

#define CAST_INT(OBJ)                                                   \
                ((OBJ)->type == M_INT_32)  ? (int32_t)(OBJ)->data.num : \
                (((OBJ)->type == M_INT_16) ? (int16_t)(OBJ)->data.num : \
                 (((OBJ)->type == M_INT_8) ? (int8_t)(OBJ)->data.num))

#define CAST_UINT(OBJ)                                                   \
                ((OBJ)->type == M_UINT_32)  ? (int32_t)(OBJ)->data.num : \
                (((OBJ)->type == M_UINT_16) ? (int16_t)(OBJ)->data.num : \
                 (((OBJ)->type == M_UINT_8) ? (int8_t)(OBJ)->data.num))

#define LOG(STRING) b_puts(print_log, B(STRING))

static pthread_mutex_t mpack_print_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *          print_log         = NULL;
static int             indent            = 0;
static int             recursion         = 0;


__attribute__((always_inline))
static inline void
pindent(void)
{
        if (indent <= 0)
                return;
        for (int i = 0; i < (indent * 4); ++i)
                fputc(' ', print_log);
}


void
mpack_print_object(FILE *fp, const mpack_obj *result)
{
#ifndef DEBUG
        return;
#endif
        //eprintf("Printing an object.....\n");
        /* assert(result != NULL);
        assert(fp != NULL); */
        if (!fp || !result)
                return;

        if (!(result->flags & MPACK_ENCODE))
                return;

        pthread_mutex_lock(&mpack_print_mutex);
        recursion = 0;
        print_log = fp;
        do_mpack_print_object(result);
        print_log = NULL;
        pthread_mutex_unlock(&mpack_print_mutex);
        //eprintf("DONE!\n");
}


static void
do_mpack_print_object(const mpack_obj *result)
{
        assert(print_log != NULL);
        if (ferror(print_log))
                abort();
        ++recursion;

        /* fprintf(print_log, "Type is 0x%x\n", mpack_type(result)); */

        switch (mpack_type(result)) {
        case MPACK_ARRAY:    print_array (result);    break;
        case MPACK_BOOL:     print_bool  (result);    break;
        case MPACK_DICT:     print_dict  (result);    break;
        case MPACK_EXT:      print_ext   (result);    break;
        case MPACK_NIL:      print_nil   (result);    break;
        case MPACK_SIGNED:
        case MPACK_UNSIGNED: print_number(result);    break;
        case MPACK_STRING:   print_string(result, 1); break;
        case MPACK_UNINITIALIZED:
        default:
                errx(1, "Got uninitialized item to print!");
        }

        if (--recursion == 0)
                putc('\n', print_log);
}

extern FILE *whyy;

static void
print_array(const mpack_obj *result)
{
        if (ferror(print_log))
                abort();
        pindent();

        if (!result->data.arr || result->data.arr->qty == 0) {
                b_fputs(print_log, B("[]\n"));
        } else {
                b_fputs(print_log, B("[\n"));

                ++indent;
                for (unsigned i = 0; i < result->data.arr->qty; ++i)
                        if (result->DAI[i])
                                do_mpack_print_object(result->DAI[i]);
                --indent;

                pindent();
                b_fputs(print_log, B("]\n"));
        }
}


static void
print_dict(const mpack_obj *result)
{
        if (ferror(print_log))
                abort();
        pindent();
        b_fputs(print_log, B("{\n"));
        ++indent;

        for (unsigned i = 0; i < result->data.dict->qty; ++i) {
                pindent();
                if (mpack_type(result->DDE[i]->key) == MPACK_STRING)
                        print_string(result->DDE[i]->key, 0);
                else
                        do_mpack_print_object(result->DDE[i]->key);

                fseek(print_log, -1, SEEK_CUR);
                const int tmp = indent;

                switch (mpack_type(result->DDE[i]->value)) {
                case MPACK_ARRAY:
                case MPACK_DICT:
                        b_fputs(print_log, B("  => (\n"));

                        ++indent;
                        do_mpack_print_object(result->DDE[i]->value);
                        --indent;

                        pindent();
                        b_fputs(print_log, B(")\n"));
                        break;
                default:
                        b_fputs(print_log, B("  =>  "));

                        indent = 0;
                        do_mpack_print_object(result->DDE[i]->value);
                        indent = tmp;
                        break;
                }

        }

        --indent;
        pindent();
        b_fputs(print_log, B("}\n"));
}


static void
print_string(const mpack_obj *result, const bool ind)
{
        if (ferror(print_log))
                abort();
        if (ind)
                pindent();

        b_chomp(result->data.str);
        fprintf(print_log, "\"%s\"\n", BS(result->data.str));
}


static void
print_ext(const mpack_obj *result)
{
        pindent();
        fprintf(print_log, "Type: %d -> Data: %d\n",
                mpack_type(result), result->data.ext->num);
}


static void
print_nil(UNUSED const mpack_obj *result)
{
        if (ferror(print_log))
                abort();
        pindent();

        b_fputs(print_log, B("NIL\n"));
}


static void
print_bool(const mpack_obj *result)
{
        pindent();
        if (result->data.boolean)
                b_fputs(print_log, B("true\n"));
        else
                b_fputs(print_log, B("false\n"));
}


static void
print_number(const mpack_obj *result)
{
        if (ferror(print_log))
                abort();
        pindent();

        if (mpack_type(result) == MPACK_SIGNED)
                fprintf(print_log, "%"PRId64"\n", result->data.num);
        else
                fprintf(print_log, "%"PRIu64"\n", result->data.num);
}
