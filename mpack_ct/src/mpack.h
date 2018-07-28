#ifndef SRC_MPACK_H
#define SRC_MPACK_H

#ifdef _WIN32
#  define __attribute__(...)
#endif

#include "bstring/bstrlib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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

enum message_types { MES_ANY = 0, MES_REQUEST, MES_RESPONSE, MES_NOTIFICATION };
enum nvim_write_type { NW_STANDARD, NW_ERROR, NW_ERROR_LN };


#define MPACK_HAS_PACKED	0x80	/* 10000000 */ 
#define MPACK_ENCODE		0x40	/* 01000000 */ 
#define MPACK_PHONY		0x20	/* 00100000 */ 
#define MPACK_SPARE_DATA	0x10	/* 00010000 */ 

#define mpack_type(MPACK_)         ((MPACK_)->flags & 0x0F)
#define mpack_spare_data(MPACK_)   ((MPACK_)->flags |=  (MPACK_SPARE_DATA))
#define mpack_unspare_data(MPACK_) ((MPACK_)->flags &= ~(MPACK_SPARE_DATA))

typedef enum   mpack_types      mpack_type_t;
typedef struct mpack_item       mpack_obj;
typedef struct mpack_ext        mpack_ext_t;
typedef struct mpack_array      mpack_array_t;
typedef struct mpack_dictionary mpack_dict_t;

#pragma pack(push, 1)

struct mpack_item {
        union mpack_item_data {
                bool        boolean;
                int16_t     nil;
                int64_t     num;
                bstring     *str;
                mpack_ext_t   *ext;
                mpack_array_t *arr;
                mpack_dict_t  *dict;
        } data;
        uint8_t flags;
        bstring *packed[];
};

struct mpack_dictionary {
        struct dict_ent {
                mpack_obj *key;
                mpack_obj *value;
        } **entries;
        uint16_t qty;
        uint16_t max;
};

struct mpack_array {
        mpack_obj **items;
        uint16_t    qty;
        uint16_t    max;
};

struct mpack_ext {
        int8_t   type;
        uint32_t num;
};

#pragma pack(pop)


struct item_free_stack {
        mpack_obj **items;
        unsigned    qty;
        unsigned    max;
};

struct atomic_call_array {
        char    **fmt;
        /* uint8_t  *free_type; */
        union atomic_call_args {
                bool     boolean;
                int64_t  num;
                bstring *str;
        } **args;

        uint32_t qty;
        uint32_t mlen;
};


/*============================================================================*/


/* API Wrappers */
extern void       __nvim_write (int fd, enum nvim_write_type type, const bstring *mes);
extern void       nvim_printf  (int fd, const char *__restrict fmt, ...) __attribute__((format(printf, 2, 3)));
extern void       nvim_vprintf (int fd, const char *__restrict fmt, va_list args);

extern int        nvim_buf_add_highlight  (int fd, unsigned bufnum, int hl_id, const bstring *group, unsigned line, unsigned start, int end);
extern b_list   * nvim_buf_attach         (int fd, int bufnum);
extern void       nvim_buf_clear_highlight(int fd, unsigned bufnum, int hl_id, unsigned start, int end);
extern unsigned   nvim_buf_get_changedtick(int fd, int bufnum);
extern b_list   * nvim_buf_get_lines      (int fd, unsigned bufnum, int start, int end);
extern bstring  * nvim_buf_get_name       (int fd, int bufnum);
extern void     * nvim_buf_get_option     (int fd, int bufnum, const bstring *optname, mpack_type_t expect, const bstring *key, bool fatal);
extern void     * nvim_buf_get_var        (int fd, int bufnum, const bstring *varname, mpack_type_t expect, const bstring *key, bool fatal);
extern unsigned   nvim_buf_line_count     (int fd, int bufnum);
extern void       nvim_call_atomic        (int fd, const struct atomic_call_array *calls);
extern void     * nvim_call_function      (int fd, const bstring *function, mpack_type_t expect, const bstring *key, bool fatal);
extern void     * nvim_call_function_args (int fd, const bstring *function, mpack_type_t expect, const bstring *key, bool fatal, const char *fmt, ...);
extern bool       nvim_command            (int fd, const bstring *cmd, bool fatal);
extern void     * nvim_command_output     (int fd, const bstring *cmd, mpack_type_t expect, const bstring *key, bool fatal);
extern void       nvim_get_api_info       (int fd);
extern int        nvim_get_current_buf    (int fd);
extern bstring  * nvim_get_current_line   (int fd);
extern void     * nvim_get_option         (int fd, const bstring *optname, mpack_type_t expect, const bstring *key, bool fatal);
extern void     * nvim_get_var            (int fd, const bstring *varname, mpack_type_t expect, const bstring *key, bool fatal);
extern void     * nvim_list_bufs          (int fd);
extern void       nvim_subscribe          (int fd, const bstring *event);

extern bstring * get_notification(int fd);


/* Decode and Destroy */
//extern mpack_obj * decode_pack        (bstring *buf, bool skip_3);
extern mpack_obj * decode_stream      (int fd, enum message_types expected_type);
extern void        mpack_print_object (const mpack_obj *result, FILE *fp);
extern void        mpack_destroy      (mpack_obj *root);
extern void        free_stack_push    (struct item_free_stack *list, void *item);

//mpack_obj * decode_stream(int fd, enum message_types expected_type);
//mpack_obj * decode_obj(bstring *buf, enum message_types expected_type);


/* Encode */
extern mpack_obj * mpack_make_new       (unsigned len, bool encode);
extern void        mpack_encode_array   (mpack_obj *root, mpack_array_t *parent, mpack_obj **item, unsigned len);
extern void        mpack_encode_integer (mpack_obj *root, mpack_array_t *parent, mpack_obj **item, int64_t value);
extern void        mpack_encode_string  (mpack_obj *root, mpack_array_t *parent, mpack_obj **item, const bstring *string);
extern void        mpack_encode_boolean (mpack_obj *root, mpack_array_t *parent, mpack_obj **item, bool value);
extern mpack_obj * encode_fmt           (unsigned size_hint, const char *fmt, ...);

/* Convenience Macros */
#define nvim_out_write(FD, MES) __nvim_write((FD), NW_STANDARD, (MES))
#define nvim_err_write(FD, MES) __nvim_write((FD), NW_ERROR, (MES))


/*============================================================================*/
/* Type conversions and Misc */

extern b_list * mpack_array_to_blist(mpack_array_t *array, bool destroy);
extern b_list * blist_from_var_fmt  (int fd, const bstring *key, bool fatal, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
extern void   * nvim_get_var_fmt    (int fd, mpack_type_t expect, const bstring *key, bool fatal, const char *fmt, ...) __attribute__((format(printf, 5, 6)));
extern void   * dict_get_key        (mpack_dict_t *dict, mpack_type_t expect, const bstring *key, bool fatal);
extern void     destroy_call_array  (struct atomic_call_array *calls);


static inline void
destroy_mpack_dict(mpack_dict_t *dict)
{
        mpack_obj tmp;
        tmp.flags     = MPACK_DICT | MPACK_PHONY;
        tmp.data.dict = dict;
        mpack_destroy(&tmp);
}

static inline void
destroy_mpack_array(mpack_array_t *array)
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


#define blist_from_var(FD__, VARNAME_, KEY_, FATAL_) \
        mpack_array_to_blist(                        \
            nvim_get_var((FD__), B(VARNAME_), MPACK_ARRAY, (KEY_), (FATAL_)), true)

#define nvim_get_var_num(FD__, VARNAME_, FATAL_) \
        numptr_to_num(nvim_get_var((FD__), B(VARNAME_), MPACK_NUM, NULL, (FATAL_)))


#define PKG "tag_highlight"

#define blist_from_var_pkg(FD__, VARNAME_, KEY_, FATAL_) \
        mpack_array_to_blist(                            \
            nvim_get_var((FD__), B(PKG "#" VARNAME_), MPACK_ARRAY, (KEY_), (FATAL_)), true)

#define nvim_get_var_num_pkg(FD__, VARNAME_, FATAL_) \
        numptr_to_num(nvim_get_var((FD__), B(PKG "#" VARNAME_), MPACK_NUM, NULL, (FATAL_)))


extern FILE *mpack_log;
#ifdef DEBUG
#  define print_and_destroy(RESULT_)                    \
        do {                                            \
                mpack_print_object(RESULT_, mpack_log); \
                mpack_destroy(RESULT_);                 \
        } while (0)
#else
#  define print_and_destroy(RESULT_) mpack_destroy(RESULT_)
#endif


/*============================================================================*/
#endif /* mpack.h */
