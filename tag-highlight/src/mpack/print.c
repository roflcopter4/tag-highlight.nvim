#include "Common.h"

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
                ((OBJ)->type == M_INT_32)  ? (int32_t)(OBJ)->num : \
                (((OBJ)->type == M_INT_16) ? (int16_t)(OBJ)->num : \
                 (((OBJ)->type == M_INT_8) ? (int8_t)(OBJ)->num))

#define CAST_UINT(OBJ)                                                   \
                ((OBJ)->type == M_UINT_32)  ? (int32_t)(OBJ)->num : \
                (((OBJ)->type == M_UINT_16) ? (int16_t)(OBJ)->num : \
                 (((OBJ)->type == M_UINT_8) ? (int8_t)(OBJ)->num))

#define LOG(STRING) b_puts(print_log, B(STRING))

static pthread_mutex_t mpack_print_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *          print_log         = NULL;
static int             indent            = 0;
static int             recursion         = 0;
static bool            skip_indent       = false;

__attribute__((always_inline))
static inline void
pindent(void)
{
        if (skip_indent) {
                skip_indent = false;
                return;
        }
        if (indent <= 0)
                return;
        for (int i = 0; i < (indent * 4); ++i)
                fputc(' ', print_log);
}


void
mpack_print_object(FILE *fp, const mpack_obj *result)
{
        if (!fp || !result)
                return;
        if (!(result->flags & MPACKFLG_ENCODE))
                return;
        pthread_mutex_lock(&mpack_print_mutex);

#ifdef HAVE_OPEN_MEMSTREAM
        char   *buf;
        size_t  stream_size;
        int     is_stdstream;

        if (fp == stdout) 
                is_stdstream = 1;
        else if (fp == stderr)
                is_stdstream = 2;
        else
                is_stdstream = 0;

        if (is_stdstream)
                fp = open_memstream(&buf, &stream_size);
#endif

        skip_indent = false;
        recursion   = 0;
        print_log   = fp;
        do_mpack_print_object(result);
        print_log   = NULL;

#ifdef HAVE_OPEN_MEMSTREAM
        if (is_stdstream) {
                fclose(fp);
                if (is_stdstream == 1)
                        fwrite(buf, 1, stream_size, stdout);
                else if (is_stdstream == 2)
                        fwrite(buf, 1, stream_size, stderr);
                else
                        errx(1, "Invalid stream somehow specified");
                free(buf);
        }
#endif
        pthread_mutex_unlock(&mpack_print_mutex);
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

        if (!result->arr || result->arr->qty == 0) {
                b_fwrite(print_log, B("[]\n"));
        } else {
                b_fwrite(print_log, B("[\n"));

                ++indent;
                for (unsigned i = 0; i < result->arr->qty; ++i)
                        if (result->arr->lst[i])
                                do_mpack_print_object(result->arr->lst[i]);
                --indent;

                pindent();
                b_fwrite(print_log, B("]\n"));
        }
}


static void
print_dict(const mpack_obj *result)
{
        if (ferror(print_log))
                abort();
        pindent();

        if (!result->dict || result->dict->qty == 0) {
                b_fwrite(print_log, B("{}\n"));
        } else {
                b_fwrite(print_log, B("{\n"));
                ++indent;
                for (unsigned i = 0; i < result->dict->qty; ++i) {
#if 0
                        if (mpack_type(result->dict->lst[i]->key) == MPACK_STRING)
                                print_string(result->dict->lst[i]->key, 0);
                        else
#endif
                        do_mpack_print_object(result->dict->lst[i]->key);
                        fseek(print_log, -1, SEEK_CUR);

#if 0
                        switch (mpack_type(result->dict->lst[i]->value)) {
                        case MPACK_ARRAY:
                        case MPACK_DICT:
                                b_fwrite(print_log, B("  =>  (\n"));

                                ++indent;
                                do_mpack_print_object(result->dict->lst[i]->value);
                                --indent;

                                pindent();
                                b_fwrite(print_log, B(")\n"));
                                break;
                        default:
#endif
                                b_fwrite(print_log, B("  =>  "));
                                skip_indent = true;
                                do_mpack_print_object(result->dict->lst[i]->value);
#if 0
                                break;
                        }
#endif
                }
                --indent;
                pindent();
                b_fwrite(print_log, B("}\n"));
        }

}


static void
print_string(const mpack_obj *result, const bool ind)
{
        if (ferror(print_log))
                abort();
        if (ind)
                pindent();

        b_chomp(result->str);
        fprintf(print_log, "\"%s\"\n", BS(result->str));
}


static void
print_ext(const mpack_obj *result)
{
        pindent();
        fprintf(print_log, "Type: %d -> Data: %d\n",
                mpack_type(result), result->ext->num);
}


static void
print_nil(UNUSED const mpack_obj *result)
{
        if (ferror(print_log))
                abort();
        pindent();

        b_fwrite(print_log, B("NIL\n"));
}


static void
print_bool(const mpack_obj *result)
{
        pindent();
        if (result->boolean)
                b_fwrite(print_log, B("true\n"));
        else
                b_fwrite(print_log, B("false\n"));
}


static void
print_number(const mpack_obj *result)
{
        if (ferror(print_log))
                abort();
        pindent();

        if (mpack_type(result) == MPACK_SIGNED)
                fprintf(print_log, "%"PRId64"\n", result->num);
        else
                fprintf(print_log, "%"PRIu64"\n", result->num);
}
