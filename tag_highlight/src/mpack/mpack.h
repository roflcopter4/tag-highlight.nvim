#ifndef SRC_MPACK_H
#define SRC_MPACK_H

#ifdef _WIN32
#  define __attribute__(...)
#endif

#include "bstring/bstring.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Structures, Etc */

enum mpack_types {
        MPACK_UNINITIALIZED,
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

typedef   enum mpack_types        mpack_type_t;
typedef   enum mpack_expect_types mpack_expect_t;
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

typedef union {
        void    *ptr;
        int64_t  num;
} retval_t;

extern const char *const m_message_type_repr[4];

/*============================================================================*/
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

/*============================================================================*/
/* Type conversions and Misc */

extern b_list * mpack_array_to_blist(mpack_array_t *array, bool destroy);
extern retval_t nvim_get_var_fmt    (int fd, mpack_expect_t expect, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
extern retval_t dict_get_key        (mpack_dict_t *dict, mpack_expect_t expect, const bstring *key);
extern void     destroy_call_array  (struct atomic_call_array *calls);
extern retval_t m_expect            (mpack_obj *obj, mpack_expect_t type, bool destroy);

/* extern b_list * blist_from_var_fmt  (int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3))); */
/* extern void   * get_expect          (mpack_obj *result, mpack_type_t expect, bool destroy, bool is_retval); */
/* void      * m_expect    (mpack_obj *obj, mpack_expect_t type, bool destroy); */

static inline void
destroy_mpack_dict(mpack_dict_t *dict)
{
        mpack_obj tmp;
        tmp.flags     = MPACK_DICT | MPACK_ENCODE | MPACK_PHONY;
        tmp.data.dict = dict;
        mpack_destroy(&tmp);
}

static inline void
destroy_mpack_array(mpack_array_t *array)
{
        mpack_obj tmp;
        tmp.flags    = MPACK_ARRAY | MPACK_ENCODE | MPACK_PHONY;
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


#ifdef __cplusplus
}
#endif
/*============================================================================*/
#endif /* mpack.h */
