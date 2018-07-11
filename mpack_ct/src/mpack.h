#ifndef SRC_MPACK_H
#define SRC_MPACK_H

#include "bstring/bstrlib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* #include "macros.h" */

/*============================================================================*/
/* Structures, Etc */

enum mpack_types {
        MPACK_UNINITIALIZED = 0,
        MPACK_BOOL,
        MPACK_NIL,
        MPACK_NUM,
        MPACK_EXT,
        MPACK_STRING,
        MPACK_ARRAY,
        MPACK_DICT,
};
typedef enum mpack_types mpack_type_t;

enum message_types { MES_ANY = 0, MES_REQUEST, MES_RESPONSE, MES_NOTIFICATION };
enum nvim_write_type { STANDARD, ERROR, ERROR_LN };


#define MPACK_HAS_PACKED	0x80	/* 10000000 */ 
#define MPACK_ENCODE		0x40	/* 01000000 */ 
#define MPACK_PHONY		0x20	/* 00100000 */ 
#define MPACK_SPARE_DATA	0x10	/* 00010000 */ 

#define mpack_type(MPACK_)         ((MPACK_)->flags & 0x0F)
#define mpack_spare_data(MPACK_)   ((MPACK_)->flags |=  (MPACK_SPARE_DATA))
#define mpack_unspare_data(MPACK_) ((MPACK_)->flags &= ~(MPACK_SPARE_DATA))


#pragma pack(push, 1)

struct mpack_item {
        union mpack_item_data {
                bool                boolean;
                int16_t             nil;
                int64_t             num;
                bstring            *str;
                struct mpack_ext   *ext;
                struct mpack_array *arr;
                struct mpack_dictionary *dict;
        } data;
        uint8_t flags;
        bstring *packed[];
};

struct mpack_dictionary {
        struct dict_ent {
                struct mpack_item *key;
                struct mpack_item *value;
        } **entries;
        uint16_t qty;
        uint16_t max;
};

struct mpack_array {
        struct mpack_item **items;
        uint16_t    qty;
        uint16_t    max;
};

struct mpack_ext {
        int8_t   type;
        uint32_t num;
};

#pragma pack(pop)


typedef struct mpack_item       mpack_obj;
typedef struct mpack_dictionary dictionary;


struct item_free_stack {
        mpack_obj **items;
        unsigned    qty;
        unsigned    max;
};

struct request_stack {
        int lst[1024];
        int qty;
        int count;
};


/*============================================================================*/


/* API Wrappers */
extern void       __nvim_write (int fd, enum nvim_write_type type, const bstring *mes) __attribute__((__noreturn__));
extern void       nvim_printf  (int fd, const char *const restrict fmt, ...) __attribute__((__noreturn__, format(printf, 2, 3)));

extern b_list   * nvim_buf_attach         (int fd, int bufnum);
extern b_list   * nvim_buf_get_lines      (int fd, unsigned buf, int start, int end);
extern bstring  * nvim_buf_get_name       (int fd, int bufnum);
extern void     * nvim_buf_get_option     (int fd, int bufnum, const bstring *optname, mpack_type_t expect, const bstring *key, bool fatal);
extern unsigned   nvim_buf_line_count     (int fd, int bufnum);
extern void     * nvim_call_function      (int fd, const bstring *function, mpack_type_t expect, const bstring *key, bool fatal);
extern void     * nvim_call_function_args (int fd, const bstring *function, mpack_type_t expect, const bstring *key, bool fatal, const char *fmt, ...);
extern bool       nvim_command            (int fd, const bstring *cmd, bool fatal);
extern void     * nvim_command_output     (int fd, const bstring *cmd, mpack_type_t expect, const bstring *key, bool fatal);
extern void       nvim_get_api_info       (int fd);
extern int        nvim_get_current_buf    (int fd);
extern bstring  * nvim_get_current_line   (int fd);
extern void     * nvim_get_option         (int fd, const bstring *optname, mpack_type_t expect, const bstring *key, bool fatal);
extern void     * nvim_get_var            (int fd, const bstring *varname, mpack_type_t expect, const bstring *key, bool fatal);
extern int        nvim_buf_add_highlight  (int fd, unsigned bufnum, int hl_id, const bstring *group, unsigned line, unsigned start, int end);
extern void       nvim_buf_clear_highlight(int fd, unsigned bufnum, int hl_id, unsigned start, int end);
extern void     * nvim_list_bufs          (int fd);
extern void       nvim_subscribe          (int fd, const bstring *event);


/* Decode and Destroy */
extern mpack_obj * decode_pack        (bstring *buf, bool skip_3);
extern mpack_obj * decode_stream      (int fd, enum message_types expected_type);
extern void        mpack_print_object (const mpack_obj *result, FILE *fp);
extern void        mpack_destroy      (mpack_obj *root);
extern void        free_stack_push    (struct item_free_stack *list, void *item);

mpack_obj * decode_stream(int fd, const enum message_types expected_type);
mpack_obj * decode_obj(bstring *buf, const enum message_types expected_type);


/* Encode */
extern mpack_obj * mpack_make_new       (unsigned len, bool encode);
extern void        mpack_encode_array   (mpack_obj *root, struct mpack_array *parent, mpack_obj **item, unsigned len);
extern void        mpack_encode_integer (mpack_obj *root, struct mpack_array *parent, mpack_obj **item, int64_t value);
extern void        mpack_encode_string  (mpack_obj *root, struct mpack_array *parent, mpack_obj **item, const bstring *string);
extern void        mpack_encode_boolean (mpack_obj *root, struct mpack_array *parent, mpack_obj **item, bool value);
extern mpack_obj * encode_fmt           (const char *fmt, ...);

/* extern mpack_obj * v_encode_fmt(mpack_obj *root, const char *fmt, va_list va); */

/* extern mpack_obj *v_encode_fmt(mpack_obj *root, const char *fmt, va_list va); */

/* typedef union vararg {
        const int64_t d;
        const bstring *s;
        const bool b;
} var_t; */

/* #define VD(VAR__) ((var_t[]){{.d = (VAR__)}})
#define VS(VAR__) ((var_t[]){{.s = (VAR__)}})
#define VB(VAR__) ((var_t[]){{.b = (VAR__)}}) */

/* extern mpack_obj * P99_FSYMB(encode_fmt)(va_list va, const char *fmt, size_t number, var_t **arr);
#define encode_fmt(ROOT_, FMT_, ...) P99_FSYMB(encode_fmt)((ROOT_), (FMT_), P99_LENGTH_ARR_ARG(var_t *, __VA_ARGS__)) */

/* P99_PROTOTYPE(mpack_obj *, P99_FSYMB(encode_fmt), va_list, const char *const restrict, size_t, var_t **); */
/* #define encode_fmt(...) P99_CALL_DEFARG(P99_FSYMB(encode_fmt), 4, P99_LENGTH_ARR_ARG(__VA_ARGS__)) */
/* #define encode_fmt_defarg_1() NULL */
/* #define eee(...) encode_fmt() */


/* Convenience Macros */
#define nvim_out_write(FD, MES) __nvim_write((FD), STANDARD, (MES))
#define nvim_err_write(FD, MES) __nvim_write((FD), ERROR, (MES))
/* #define nvprintf(...) fprintf(stderr, __VA_ARGS__) */
/* #define nvprintf warnx */
/* #define echo(STRING_) (b_fputs(stderr, b_tmp(STRING_ "\n"))) */


/*============================================================================*/
/* Type conversions and Misc */

extern b_list * mpack_array_to_blist(struct mpack_array *array, bool destroy);
extern b_list * blist_from_var_fmt  (int fd, const bstring *key, bool fatal, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
extern void   * nvim_get_var_fmt    (int fd, mpack_type_t expect, const bstring *key, bool fatal, const char *fmt, ...) __attribute__((format(printf, 5, 6)));
extern void   * dict_get_key        (dictionary *dict, const mpack_type_t expect, const bstring *key, const bool fatal);


static inline void
destroy_dictionary(dictionary *dict)
{
        mpack_obj tmp;
        tmp.flags     = MPACK_DICT | MPACK_PHONY;
        tmp.data.dict = dict;
        mpack_destroy(&tmp);
}

static inline void
destroy_mpack_array(struct mpack_array *array)
{
        mpack_obj tmp;
        tmp.flags    = MPACK_ARRAY | MPACK_PHONY;
        tmp.data.arr = array;
        mpack_destroy(&tmp);
}

static inline int64_t
numptr_to_num(int64_t *ptr)
{
        int64_t tmp = *ptr;
        free(ptr);
        return tmp;
}


#define PKG "tag_highlight"

#define nvim_get_var_l(FD__, VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var((FD__), b_tmp(VARNAME_), (EXPECT_), (KEY_), (FATAL_))

#define blist_from_var(FD__, VARNAME_, KEY_, FATAL_)                   \
        mpack_array_to_blist(nvim_get_var((FD__), B(PKG "#" VARNAME_), \
                                          MPACK_ARRAY, (KEY_), (FATAL_)), true)

extern FILE *mpack_log;
#define print_and_destroy(RESULT_)                \
        do {                                      \
                mpack_print_object(RESULT_, mpack_log); \
                mpack_destroy(RESULT_);           \
        } while (0)


/*============================================================================*/
#endif /* mpack.h */
