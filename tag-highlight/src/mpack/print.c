#include "Common.h"

#include "mpack.h"

#define INDENT_LEN 2

struct printing_data {
        bstring *out;
        int      indent;
        int      recursion;
        bool     skip_indent;
} __attribute__((aligned(32)));

static void do_mpack_print_object(const mpack_obj *result, struct printing_data *data);
static void print_array (const mpack_obj *result, struct printing_data *data);
static void print_bool  (const mpack_obj *result, struct printing_data *data);
static void print_dict  (const mpack_obj *result, struct printing_data *data);
static void print_ext   (const mpack_obj *result, struct printing_data *data);
static void print_nil   (const mpack_obj *result, struct printing_data *data);
static void print_number(const mpack_obj *result, struct printing_data *data);
static void print_string(const mpack_obj *result, struct printing_data *data);

static pthread_mutex_t mpack_print_mutex = PTHREAD_MUTEX_INITIALIZER;

__attribute__((constructor))
static void init(void) 
{
        pthread_mutex_init(&mpack_print_mutex);
}

static __inline void
pindent(struct printing_data *data)
{
        if (data->skip_indent) {
                data->skip_indent = false;
                return;
        }
        if (data->indent <= 0)
                return;

        unsigned len = (data->indent * INDENT_LEN);

        b_alloc(data->out, data->out->slen + INDENT_LEN + len);
        for (unsigned i = 0; i < len; ++i)
                data->out->data[data->out->slen++] = ' ';
        data->out->data[data->out->slen] = '\0';
}


void
mpack_print_object(FILE *fp, const mpack_obj *result, bstring const *msg)
{
        if (!fp || !result || !(result->flags & MPACKFLG_ENCODE))
                return;
        int dummy;
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &dummy);
        pthread_mutex_lock(&mpack_print_mutex);

        struct printing_data data = {
                .out = b_create(16384U),
                .indent = 0,
                .recursion = 0,
                .skip_indent = false,
        };

        if (msg)
                b_fwrite(fp, B("\033[1;31mMSG:\t\033[0m"), msg, B("\n"));
        do_mpack_print_object(result, &data);
        b_fwrite(fp, data.out, B("\n"));
        fflush(fp);
#ifndef DOSISH
        fsync(fileno(fp));
#endif
        b_free(data.out);

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &dummy);
        pthread_mutex_unlock(&mpack_print_mutex);
}


static void
do_mpack_print_object(const mpack_obj *result, struct printing_data *data)
{

        /* b_sprintfa(data->out, "Type is 0x%x\n", mpack_type(result)); */

        ++data->recursion;
#if 0
        if (data->recursion++ > 0)
                b_append(data->out, B(", "));
#endif

        switch (mpack_type(result)) {
        case MPACK_ARRAY:    print_array (result, data);    break;
        case MPACK_BOOL:     print_bool  (result, data);    break;
        case MPACK_DICT:     print_dict  (result, data);    break;
        case MPACK_EXT:      print_ext   (result, data);    break;
        case MPACK_NIL:      print_nil   (result, data);    break;
        case MPACK_SIGNED:
        case MPACK_UNSIGNED: print_number(result, data);    break;
        case MPACK_STRING:   print_string(result, data); break;
        case MPACK_UNINITIALIZED:
        default:
                errx(1, "Got uninitialized item to print!");
        }

        if (--data->recursion == 0) {
                b_catchar(data->out, '\n');
        }
}


static void
print_array(const mpack_obj *result, struct printing_data *data)
{
#if 0
        pindent(data);
#endif

        if (!result->arr || result->arr->qty == 0) {
                 b_append(data->out, B("[]"));
        } else {
                if (data->recursion > 1 && data->recursion != 5) {
                        b_catchar(data->out, '\n');
                        pindent(data);
                }
                b_append(data->out, B("["));

                ++data->indent;
                for (unsigned i = 0; ; ++i) {
                        if (!result->arr->lst[i])
                                errx(1, "Null object??");

                        do_mpack_print_object(result->arr->lst[i], data);

                        if (i == (result->arr->qty - 1))
                                break;
                        b_append(data->out, B(", "));
                }
                --data->indent;

#if 0
                pindent(data);
#endif
                b_append(data->out, B("]"));
        }

}


static void
print_dict(const mpack_obj *result, struct printing_data *data)
{
#if 0
        pindent(data);
#endif

        if (!result->dict || result->dict->qty == 0) {
                b_append(data->out, B("{}"));
        } else {
                b_append(data->out, B("{"));
                ++data->indent;
                for (unsigned i = 0; ; ++i) {
#if 0
                        if (mpack_type(result->dict->lst[i]->key) == MPACK_STRING)
                                print_string(result->dict->lst[i]->key, 0);
                        else
#endif
                        do_mpack_print_object(result->dict->lst[i]->key, data);
                        /* data->out->data[--data->out->slen] = '\0'; */

#if 0
                        switch (mpack_type(result->dict->lst[i]->value)) {
                        case MPACK_ARRAY:
                        case MPACK_DICT:
                                b_append(data->out, B("  =>  (\n"));

                                ++data->indent;
                                do_mpack_print_object(result->dict->lst[i]->value);
                                --data->indent;

                                pindent(data);
                                b_append(data->out, B(")\n"));
                                break;
                        default:
#endif
                                b_append(data->out, B(" => "));
                                //data->skip_indent = true;
                                do_mpack_print_object(result->dict->lst[i]->value, data);

                                if (i == (result->dict->qty - 1))
                                        break;
                                b_append(data->out, B(", "));

#if 0
                                break;
                        }
#endif
                }
                --data->indent;
#if 0
                pindent(data);
#endif
                 b_append(data->out, B("}"));
        }

}


static void
print_string(const mpack_obj *result, struct printing_data *data)
{
#if 0
        if (ind)
                pindent(data);
#endif

        b_chomp(result->str);
        b_sprintfa(data->out, "\"%s\"", result->str);
}


static void
print_ext(const mpack_obj *result, struct printing_data *data)
{
#if 0
        pindent(data);
#endif
        b_formata(data->out, "(EXT: Type: %d -> Data: (%u, 0x%"PRIX64"))",
                  mpack_type(result), result->ext->type, result->ext->u64);
}


static void
print_nil(UNUSED const mpack_obj *result, struct printing_data *data)
{
#if 0
        pindent(data);
#endif
        b_append(data->out, B("NIL"));
}


static void
print_bool(const mpack_obj *result, struct printing_data *data)
{
#if 0
        pindent(data);
#endif
        if (result->boolean)
                b_append(data->out, B("TRUE"));
        else
                b_append(data->out, B("FALSE"));
}


static void
print_number(const mpack_obj *result, struct printing_data *data)
{
#if 0
        pindent(data);
#endif
        if (mpack_type(result) == MPACK_SIGNED)
                b_sprintfa(data->out, "%"PRId64"", result->num);
        else
                b_sprintfa(data->out, "%"PRIu64"", result->num);
}
