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
enum mpack_expect_types {
        E_MPACK_EXT,
        E_MPACK_ARRAY,
        E_MPACK_DICT,
        E_MPACK_NIL,
        E_BOOL,
        E_NUM,
        E_STRING,
        E_STRLIST,
        E_DICT2ARR
};
enum message_types { MES_REQUEST, MES_RESPONSE, MES_NOTIFICATION, MES_ANY };
enum nvim_write_type { NW_STANDARD, NW_ERROR, NW_ERROR_LN };


#define MPACK_HAS_PACKED	0x80	/* 10000000 */ 
#define MPACK_ENCODE		0x40	/* 01000000 */ 
#define MPACK_PHONY		0x20	/* 00100000 */ 
#define MPACK_SPARE_DATA	0x10	/* 00010000 */ 

#define mpack_type(MPACK_)         ((MPACK_)->flags & 0x0F)
#define mpack_spare_data(MPACK_)   ((MPACK_)->flags |=  (MPACK_SPARE_DATA))
#define mpack_unspare_data(MPACK_) ((MPACK_)->flags &= ~(MPACK_SPARE_DATA))

typedef enum   mpack_types        mpack_type_t;
typedef enum   mpack_expect_types mpack_expect_t;
typedef struct mpack_item         mpack_obj;
typedef struct mpack_ext          mpack_ext_t;
typedef struct mpack_array        mpack_array_t;
typedef struct mpack_dictionary   mpack_dict_t;

#pragma pack(push, 1)

struct mpack_item {
        union mpack_item_data {
                bool           boolean;
                int16_t        nil;
                int64_t        num;
                /* char          *c_str; */
                bstring       *str;
                mpack_array_t *arr;
                mpack_dict_t  *dict;
                mpack_ext_t   *ext;
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
        union atomic_call_args {
                bool     boolean;
                int64_t  num;
                bstring *str;
                char    *c_str;
        } **args;

        uint32_t qty;
        uint32_t mlen;
};

union universal_retval {
        void    *ptr;
        int64_t  num;
};
typedef union universal_retval retval_t;


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
extern void     * nvim_buf_get_option     (int fd, int bufnum, const bstring *optname, mpack_expect_t expect);
extern void     * nvim_buf_get_var        (int fd, int bufnum, const bstring *varname, mpack_expect_t expect);
extern unsigned   nvim_buf_line_count     (int fd, int bufnum);
extern void       nvim_call_atomic        (int fd, const struct atomic_call_array *calls);
extern void     * nvim_call_function      (int fd, const bstring *function, mpack_expect_t expect);
extern void     * nvim_call_function_args (int fd, const bstring *function, mpack_expect_t expect, const bstring *fmt, ...);
extern bool       nvim_command            (int fd, const bstring *cmd);
extern void     * nvim_command_output     (int fd, const bstring *cmd, mpack_expect_t expect);
extern void       nvim_get_api_info       (int fd);
extern int        nvim_get_current_buf    (int fd);
extern bstring  * nvim_get_current_line   (int fd);
extern void     * nvim_get_option         (int fd, const bstring *optname, mpack_expect_t expect);
extern void     * nvim_get_var            (int fd, const bstring *varname, mpack_expect_t expect);
extern void     * nvim_list_bufs          (int fd);
extern void       nvim_subscribe          (int fd, const bstring *event);
extern bool       nvim_set_var            (int fd, const bstring *varname, const bstring *fmt, ...);

extern bstring * get_notification(int fd);


/* Decode and Destroy */
extern void        mpack_print_object (FILE *fp, const mpack_obj *result);
extern void        mpack_destroy      (mpack_obj *root);
extern void        free_stack_push    (struct item_free_stack *list, void *item);
extern mpack_obj * decode_stream      (int fd, enum message_types expected_type);
extern mpack_obj * decode_obj         (bstring *buf, enum message_types expected_type);


/* Encode */
extern mpack_obj * mpack_make_new         (unsigned len, bool encode);
extern void        mpack_encode_array     (mpack_obj *root, mpack_obj **item, unsigned len);
extern void        mpack_encode_integer   (mpack_obj *root, mpack_obj **item, int64_t value);
extern void        mpack_encode_string    (mpack_obj *root, mpack_obj **item, const bstring *string);
extern void        mpack_encode_boolean   (mpack_obj *root, mpack_obj **item, bool value);
extern void        mpack_encode_dictionary(mpack_obj *root, mpack_obj **item, unsigned len);
extern void        mpack_encode_nil       (mpack_obj *root, mpack_obj **item);
extern mpack_obj * encode_fmt             (unsigned size_hint, const char *fmt, ...);

/* Convenience Macros */
#define nvim_out_write(FD, MES) __nvim_write((FD), NW_STANDARD, (MES))
#define nvim_err_write(FD, MES) __nvim_write((FD), NW_ERROR, (MES))


/*============================================================================*/
/* Type conversions and Misc */

extern b_list * mpack_array_to_blist(mpack_array_t *array, bool destroy);
/* extern b_list * blist_from_var_fmt  (int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3))); */
extern void   * nvim_get_var_fmt    (int fd, mpack_expect_t expect, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
extern void   * dict_get_key        (mpack_dict_t *dict, mpack_expect_t expect, const bstring *key);
extern void     destroy_call_array  (struct atomic_call_array *calls);

/* extern void   * get_expect          (mpack_obj *result, mpack_type_t expect, bool destroy, bool is_retval); */

void      * m_expect    (mpack_obj *obj, mpack_expect_t type, bool destroy);
retval_t m_expect_2  (mpack_obj *obj, mpack_expect_t type, bool destroy);

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
P2I(int64_t *ptr)
{
        int64_t tmp = *ptr;
        free(ptr);
        return tmp;
}

static inline mpack_obj *
m_index(mpack_obj *obj, const unsigned index)
{
        if (mpack_type(obj) != MPACK_ARRAY)
                return NULL;
        if (obj->data.arr->qty < index)
                return NULL;

        return obj->data.arr->items[index];
}


#define blist_from_var(FDES, VARNAME, FATAL_) \
        mpack_array_to_blist(                        \
            nvim_get_var((FDES), B(VARNAME), MPACK_ARRAY, (FATAL_)), true)

#define nvim_get_var_num(FDES, VARNAME, FATAL_) \
        P2I(nvim_get_var((FDES), B(VARNAME), MPACK_NUM, (FATAL_)))


#define PKG "tag_highlight"

#define blist_from_var_pkg(FDES, VARNAME, FATAL_) \
        mpack_array_to_blist(                            \
            nvim_get_var((FDES), B(PKG "#" VARNAME), MPACK_ARRAY, (FATAL_)), true)

#define nvim_get_var_num_pkg(FDES, VARNAME, FATAL_) \
        P2I(nvim_get_var((FDES), B(PKG "#" VARNAME), MPACK_NUM, (FATAL_)))

/* I am very lazy. */
#define DAI data.arr->items
#define DDE data.dict->entries

extern FILE *mpack_log;
#ifdef DEBUG
#  define PRINT_AND_DESTROY(RESULT_)                      \
        do {                                              \
                mpack_print_object(mpack_log, (RESULT_)); \
                mpack_destroy(RESULT_);                   \
        } while (0)
#else
#  define PRINT_AND_DESTROY(RESULT_) mpack_destroy(RESULT_)
#endif

/*============================================================================*/

/* #define mpack_expect(OBJ_, MTYPE_, CAST_, DESTROY_)                                                 \ */
#define mpack_expect(OBJ_, MTYPE_, DESTROY_)                                                 \
        ({                                                                                   \
                mpack_obj *    mobj_ = (OBJ_);                                               \
                mpack_expect_t etyp_ = (MTYPE_);                                             \
                bool           dest_ = (bool)(DESTROY_);                                     \
                union {                                                                      \
                        int64_t   num;                                                       \
                        void *    ptr;                                                       \
                        uintptr_t HAX;                                                       \
                } hack_;                                                                     \
                                                                                             \
                switch (etyp_) {                                                             \
                case E_MPACK_EXT:                                                            \
                        hack_.ptr = get_expect(mobj_, MPACK_EXT, dest_, 0);                  \
                        break;                                                               \
                case E_MPACK_ARRAY:                                                          \
                        hack_.ptr = get_expect(mobj_, MPACK_ARRAY, dest_, 0);                \
                        break;                                                               \
                case E_MPACK_DICT:                                                           \
                        hack_.ptr = get_expect(mobj_, MPACK_DICT, dest_, 0);                 \
                        break;                                                               \
                case E_MPACK_NIL:                                                            \
                        hack_.ptr = get_expect(mobj_, MPACK_NIL, dest_, 0);                  \
                        break;                                                               \
                case E_NUM: {                                                                \
                        int64_t *tmp_ = get_expect(mobj_, MPACK_NUM, dest_, 0);              \
                        hack_.num     = P2I(tmp_);                                 \
                        break;                                                               \
                }                                                                            \
                case E_STRING:                                                               \
                        hack_.ptr = get_expect(mobj_, MPACK_STRING, dest_, 0);               \
                        break;                                                               \
                case E_STRLIST:                                                              \
                        hack_.ptr = get_expect(mobj_, MPACK_ARRAY, dest_, 0);                \
                        hack_.ptr = mpack_array_to_blist((mpack_array_t *)hack_.ptr, dest_); \
                        break;                                                               \
                case E_DICT2ARR:                                                             \
                default: abort();                                                            \
                }                                                                            \
                                                                                             \
                /* (CAST_)hack_.HAX;\ */                                                     \
                hack_.HAX;                                                                   \
        })


/*============================================================================*/
#endif /* mpack.h */
