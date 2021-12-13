#ifndef MPACK_MPACK_H_
#define MPACK_MPACK_H_
#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*/
/* Structures, Etc */

enum mpack_types {
      MPACK_UNINITIALIZED = 0,
      MPACK_BOOL          = 1,
      MPACK_NIL           = 2,
      MPACK_SIGNED        = 3,
      MPACK_UNSIGNED      = 4,
      MPACK_EXT           = 5,
      MPACK_STRING        = 6,
      MPACK_ARRAY         = 7,
      MPACK_DICT          = 8,
};

enum mpack_expect_t {
      E_MPACK_NOTHING = 0,
      E_MPACK_EXT     = 0x0001,
      E_MPACK_ARRAY   = 0x0002,
      E_MPACK_DICT    = 0x0004,
      E_MPACK_NIL     = 0x0008,
      E_BOOL          = 0x0010,
      E_NUM           = 0x0020,
      E_STRING        = 0x0040,
      E_STRLIST       = 0x0080,
      E_DICT2ARR      = 0x0100,
      E_WSTRING       = 0x0200,
};
typedef enum mpack_expect_t mpack_expect_t;

#if 0
P99_DECLARE_ENUM(mpack_expect_t, unsigned,
      E_MPACK_EXT,
      E_MPACK_ARRAY,
      E_MPACK_DICT,
      E_MPACK_NIL,
      E_BOOL,
      E_NUM,
      E_STRING,
      E_STRLIST,
      E_DICT2ARR,
      E_WSTRING
);
#endif

enum mpack_flag_values {
      MPACKFLG_SPARE_DATA = 0x10U,
      MPACKFLG_PHONY      = 0x20U,
      MPACKFLG_ENCODE     = 0x40U,
      MPACKFLG_HAS_PACKED = 0x80U,
};

//enum mpack_ext_tag {
//      MPACK_EXT_TAG_F1,
//      MPACK_EXT_TAG_F2,
//      MPACK_EXT_TAG_F4,
//      MPACK_EXT_TAG_F8,
//      MPACK_EXT_TAG_F16,
//      MPACK_EXT_TAG_VAR,
//};

#define mpack_type(MPACK_) ((MPACK_)->flags & 0x0FU)

#ifndef __cplusplus
typedef struct mpack_object       mpack_obj;
typedef struct mpack_arg_array    mpack_arg_array;
typedef struct mpack_array        mpack_array;
typedef struct mpack_dict_ent     mpack_dict_ent;
typedef struct mpack_dictionary   mpack_dict;
typedef struct mpack_ext          mpack_ext;
typedef enum   mpack_types        mpack_type_t;
typedef union  mpack_argument     mpack_argument;
typedef union  mpack_retval       mpack_retval;
#endif

/*----------------------------------------------------------------------------*/

struct mpack_object {
        uint8_t  flags;
        union {
                bool         boolean;
                int16_t      nil;
                uint64_t     num; /* An unsigned var can hold a signed value */
                bstring     *str;
                mpack_array *arr;
                mpack_dict  *dict;
                mpack_ext   *ext;
        };
        bstring *packed[];
};

struct mpack_dictionary {
        struct mpack_dict_ent {
                mpack_obj *key;
                mpack_obj *value;
        } **lst;
        uint32_t qty;
        uint32_t max;
};

struct mpack_array {
        mpack_obj **lst;
        uint32_t    qty;
        uint32_t    max;
};

struct mpack_ext {
        //uint32_t len;
        //uint8_t  tag;
        int8_t   type;
        uint8_t  len;
        union {
              uint8_t  u8x8[8];
              uint16_t u16x4[4];
              uint32_t u32x2[2];
              uint8_t  u8;
              uint16_t u16;
              uint32_t u32;
              uint64_t u64;
        };
};

struct mpack_arg_array {
        char **fmt;
        union mpack_argument {
                bool     boolean;
                int64_t  num;
                uint64_t uint;
                bstring *str;
                char    *c_str;
        } **args;

        uint32_t qty;
        uint32_t mlen;
};

union mpack_retval {
        void    *ptr;
        uint64_t num;
};

extern const char *const m_message_type_repr[4];
extern const char *const m_type_names[];

/*============================================================================*/
/* Decode and Destroy */
extern void        mpack_print_object     (FILE *fp, const mpack_obj *result, bstring const *msg);
extern int         mpack_destroy_object   (mpack_obj *root);
extern void        mpack_destroy_arg_array(mpack_arg_array *calls);
extern mpack_obj * mpack_decode_stream    (int fd);
extern mpack_obj * mpack_decode_obj       (bstring *buf);

/* Encode */
extern mpack_obj * mpack_make_new         (unsigned len, bool encode);
extern void        mpack_encode_array     (mpack_obj *root, mpack_obj **item, unsigned len);
extern void        mpack_encode_integer   (mpack_obj *root, mpack_obj **item, int64_t value);
extern void        mpack_encode_unsigned  (mpack_obj *root, mpack_obj **item, uint64_t value);
extern void        mpack_encode_string    (mpack_obj *root, mpack_obj **item, const bstring *string);
extern void        mpack_encode_boolean   (mpack_obj *root, mpack_obj **item, bool value);
extern void        mpack_encode_dictionary(mpack_obj *root, mpack_obj **item, unsigned len);
extern void        mpack_encode_nil       (mpack_obj *root, mpack_obj **item);
extern mpack_obj * mpack_encode_fmt       (unsigned size_hint, const char *fmt, ...);

/*============================================================================*/
/* Type conversions and Misc */

extern b_list *     mpack_array_to_blist(mpack_array *array, bool destroy);
extern mpack_retval mpack_dict_get_key  (mpack_dict *dict, mpack_expect_t expect, const bstring *key);
extern mpack_retval mpack_expect        (mpack_obj *obj, mpack_expect_t type, bool destroy, mpack_expect_t alternatives);

extern pthread_mutex_t mpack_rw_lock;

ALWAYS_INLINE mpack_obj *
mpack_index(mpack_obj *obj, const unsigned index)
{
        mpack_obj *ret = NULL;
        if (mpack_type(obj) == MPACK_ARRAY && obj->arr->qty >= index)
                ret = obj->arr->lst[index];
        return ret;
}

/* I am very lazy. */
#define DAI data.arr->items
#define DDE data.dict->entries

extern FILE *mpack_log;
extern FILE *mpack_raw_write;
#ifdef DEBUG
#  define PRINT_AND_DESTROY(RESULT_)                      \
        do {                                              \
                mpack_print_object(mpack_log, (RESULT_)); \
                mpack_destroy_object(RESULT_);            \
        } while (0)
#else
#  define PRINT_AND_DESTROY(RESULT_) mpack_destroy_object(RESULT_)
#endif

#ifdef MPACK_USE_P99
#  include "contrib/p99/p99_defarg.h"
#  include "contrib/p99/p99_map.h"
#  define mpack_expect(...) P99_CALL_DEFARG(mpack_expect, 4, __VA_ARGS__)
#  define mpack_expect_defarg_2() false
#  define mpack_expect_defarg_3() 0
#endif


#ifdef __cplusplus
}
#endif
/*============================================================================*/
#endif /* mpack.h */
