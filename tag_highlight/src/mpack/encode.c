#include "Common.h"

#include "intern.h"

#define encode_uint64(ARR, IT, VAL)                                     \
        do {                                                            \
                ((ARR)[(IT)++]) = (uint8_t)(((uint64_t)(VAL)) >> 070U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint64_t)(VAL)) >> 060U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint64_t)(VAL)) >> 050U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint64_t)(VAL)) >> 040U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint64_t)(VAL)) >> 030U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint64_t)(VAL)) >> 020U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint64_t)(VAL)) >> 010U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint64_t)(VAL)) & 0xFFU); \
        } while (0)

#define encode_uint32(ARR, IT, VAL)                                     \
        do {                                                            \
                ((ARR)[(IT)++]) = (uint8_t)(((uint32_t)(VAL)) >> 030U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint32_t)(VAL)) >> 020U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint32_t)(VAL)) >> 010U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint32_t)(VAL)) & 0xFFU); \
        } while (0)

#define encode_uint16(ARR, IT, VAL)                                     \
        do {                                                            \
                ((ARR)[(IT)++]) = (uint8_t)(((uint16_t)(VAL)) >> 010U); \
                ((ARR)[(IT)++]) = (uint8_t)(((uint16_t)(VAL)) & 0xFFU); \
        } while (0)

#define encode_int64(ARR, IT, VAL)                                    \
        do {                                                          \
                ((ARR)[(IT)++]) = (int8_t)(((uint64_t)(VAL)) >> 070); \
                ((ARR)[(IT)++]) = (int8_t)(((uint64_t)(VAL)) >> 060); \
                ((ARR)[(IT)++]) = (int8_t)(((uint64_t)(VAL)) >> 050); \
                ((ARR)[(IT)++]) = (int8_t)(((uint64_t)(VAL)) >> 040); \
                ((ARR)[(IT)++]) = (int8_t)(((uint64_t)(VAL)) >> 030); \
                ((ARR)[(IT)++]) = (int8_t)(((uint64_t)(VAL)) >> 020); \
                ((ARR)[(IT)++]) = (int8_t)(((uint64_t)(VAL)) >> 010); \
                ((ARR)[(IT)++]) = (int8_t)(((uint64_t)(VAL)) & 0xFF); \
        } while (0)

#define encode_int32(ARR, IT, VAL)                                    \
        do {                                                          \
                ((ARR)[(IT)++]) = (int8_t)(((uint32_t)(VAL)) >> 030); \
                ((ARR)[(IT)++]) = (int8_t)(((uint32_t)(VAL)) >> 020); \
                ((ARR)[(IT)++]) = (int8_t)(((uint32_t)(VAL)) >> 010); \
                ((ARR)[(IT)++]) = (int8_t)(((uint32_t)(VAL)) & 0xFF); \
        } while (0)

#define encode_int16(ARR, IT, VAL)                                    \
        do {                                                          \
                ((ARR)[(IT)++]) = (int8_t)(((uint16_t)(VAL)) >> 010); \
                ((ARR)[(IT)++]) = (int8_t)(((uint16_t)(VAL)) & 0xFF); \
        } while (0)

static void sanity_check(mpack_obj *root, mpack_obj **itemp, unsigned check, bool force);

void *_mpack_encode_talloc_ctx = NULL;
#define CTX _mpack_encode_talloc_ctx

/*============================================================================*/


/* God am I ever lazy */
#define D ((*root->packed)->data)
#define L ((*root->packed)->slen)
#define ITEM (*itemp)

mpack_obj *
mpack_make_new(UNUSED unsigned const len, bool const encode)
{
        mpack_obj *root = talloc_size(CTX, offsetof(mpack_obj, packed) + sizeof(bstring *));
        root->flags     = (uint8_t)(encode ? MPACKFLG_ENCODE : 0) |
                          (uint8_t)MPACKFLG_HAS_PACKED;
        *root->packed   = b_alloc_null(128);
        talloc_steal(root, *root->packed);

        return root;
}

void
mpack_encode_array(mpack_obj *    root,
                   mpack_obj **   itemp,
                   unsigned const len)
{
        sanity_check(root, itemp, 64, false);

        if (itemp && (root->flags & MPACKFLG_ENCODE)) {
                ITEM->arr      = talloc(ITEM, mpack_array);
                ITEM->arr->lst = talloc_array(ITEM->arr, mpack_obj *, len);
                ITEM->arr->qty = len;
                ITEM->arr->max = len;
                ITEM->flags   |= (uint8_t)MPACK_ARRAY;

                for (unsigned i = 0; i < len; ++i)
                        ITEM->arr->lst[i] = NULL;
        }

        if (len < 16U) {
                D[L++] = M_MASK_ARRAY_F | (uint8_t)len;
        } else if (len < UINT16_MAX) {
                D[L++] = M_MASK_ARRAY_16;
                encode_uint16(D, L, len);
        } else if (len < UINT32_MAX) {
                D[L++] = M_MASK_ARRAY_32;
                encode_uint32(D, L, len);
        } else {
                errx(1, "Array is too big!");
        }
}

void
mpack_encode_dictionary(mpack_obj *    root, 
                        mpack_obj **   itemp, 
                        unsigned const len)
{
        sanity_check(root, itemp, 64, false);

        if (itemp && (root->flags & MPACKFLG_ENCODE)) {
                ITEM->dict      = talloc(ITEM, mpack_dict);
                ITEM->dict->lst = talloc_array(ITEM->dict, mpack_dict_ent *, len);
                ITEM->dict->qty = len;
                ITEM->dict->max = len;
                ITEM->flags    |= (uint8_t)MPACK_DICT;

                for (unsigned i = 0; i < len; ++i) {
                        ITEM->dict->lst[i]        = talloc(ITEM->dict->lst, mpack_dict_ent);
                        ITEM->dict->lst[i]->key   = NULL;
                        ITEM->dict->lst[i]->value = NULL;
                }
        }

        if (len < 16U) {
                D[L++] = M_MASK_MAP_F | (uint8_t)len;
        } else if (len < UINT16_MAX) {
                D[L++] = M_MASK_MAP_16;
                encode_uint16(D, L, len);
        } else if (len < UINT32_MAX) {
                D[L++] = M_MASK_MAP_32;
                encode_uint32(D, L, len);
        } else {
                errx(1, "Dictionary is too big!");
        }
}

void
mpack_encode_integer(mpack_obj *   root,
                     mpack_obj **  itemp,
                     int64_t const value)
{
        sanity_check(root, itemp, 15, false);

        if (root->flags & MPACKFLG_ENCODE) {
                ITEM->num    = value;
                ITEM->flags |= (uint8_t)MPACK_SIGNED;
        }

        if (value >= 0) {
                if (value <= 127) {
                        D[L++] = (uint8_t)value;
                } else if(value < UINT8_MAX) {
                        D[L++] = M_MASK_UINT_8;
                        D[L++] = (uint8_t)value;
                } else if (value <= UINT16_MAX) {
                        D[L++] = M_MASK_UINT_16;
                        encode_uint16(D, L, value);
                } else if (value <= UINT32_MAX) {
                        D[L++] = M_MASK_UINT_32;
                        encode_uint32(D, L, value);
                } else if (value <= INT64_MAX) {
                        D[L++] = M_MASK_UINT_64;
                        encode_uint64(D, L, value);
                } else {
                        errx(1, "Value too high!");
                }
        } else {
                if (value >= (-32)) {
                        D[L++] = (int8_t)value;
                } else if (value > INT8_MIN) {
                        D[L++] = M_MASK_INT_8;
                        D[L++] = (int8_t)value;
                } else if (value >= INT16_MIN) {
                        D[L++] = M_MASK_INT_16;
                        encode_int16(D, L, value);
                } else if (value >= INT32_MIN) {
                        D[L++] = M_MASK_INT_32;
                        encode_int32(D, L, value);
                } else if (value >= INT64_MIN) {
                        D[L++] = M_MASK_INT_64;
                        encode_int64(D, L, value);
                } else {
                        errx(1, "Value too low!");
                }
        }
}

void
mpack_encode_unsigned(mpack_obj *    root,
                      mpack_obj **   itemp,
                      uint64_t const value)
{
        sanity_check(root, itemp, 15, false);

        if (root->flags & MPACKFLG_ENCODE) {
                ITEM->num    = value;
                ITEM->flags |= (uint8_t)MPACK_UNSIGNED;
        }

        if (value <= 127) {
                D[L++] = (uint8_t)value;
        } else if (value < UINT8_MAX) {
                D[L++] = M_MASK_UINT_8;
                D[L++] = (uint8_t)value;
        } else if (value <= UINT16_MAX) {
                D[L++] = M_MASK_UINT_16;
                encode_uint16(D, L, value);
        } else if (value <= UINT32_MAX) {
                D[L++] = M_MASK_UINT_32;
                encode_uint32(D, L, value);
        } else {
                D[L++] = M_MASK_UINT_64;
                encode_uint64(D, L, value);
        }
}

void
mpack_encode_string(mpack_obj *    root,
                    mpack_obj **   itemp,
                    bstring const *string)
{
        static const bstring nil_string = bt_init("");
        if (!string)
                string = &nil_string;
        sanity_check(root, itemp, string->slen + 5, false);

        if (root->flags & MPACKFLG_ENCODE) {
                ITEM->str    = b_strcpy(string);
                ITEM->flags |= (uint8_t)MPACK_STRING;
                talloc_steal(ITEM, ITEM->str);
        }

        if (string->slen < 32U) {
                D[L++] = M_MASK_STR_F | (uint8_t)string->slen;
        } else if (string->slen < UINT8_MAX) {
                D[L++] = M_MASK_STR_8;
                D[L++] = (uint8_t)string->slen;
        } else if (string->slen < UINT16_MAX) {
                D[L++] = M_MASK_STR_16;
                encode_uint16(D, L, string->slen);
        } else {
                D[L++] = M_MASK_STR_32;
                encode_uint32(D, L, string->slen);
        }

        b_concat(*root->packed, string);
}

void
mpack_encode_boolean(mpack_obj * root,
                     mpack_obj **itemp,
                     bool const  value)
{
        sanity_check(root, itemp, 2, false);

        if (root->flags & MPACKFLG_ENCODE) {
                ITEM->boolean = value;
                ITEM->flags  |= (uint8_t)MPACK_BOOL;
        }

        D[L++] = (value) ? M_MASK_TRUE : M_MASK_FALSE;
}

void
mpack_encode_nil(mpack_obj *root, mpack_obj **itemp)
{
        sanity_check(root, itemp, 2, false);

        if (root->flags & MPACKFLG_ENCODE) {
                ITEM->nil    = M_MASK_NIL;
                ITEM->flags |= (uint8_t)MPACK_NIL;
        }

        D[L++] = M_MASK_NIL;
}

/*============================================================================*/

static void
sanity_check(mpack_obj *    root,
             mpack_obj **   itemp,
             unsigned const check,
             bool const     force)
{
        if (itemp && !(*itemp) && (force || (root->flags & MPACKFLG_ENCODE))) {
                (*itemp)        = talloc(root, mpack_obj);
                (*itemp)->flags = 0;
        }

        if ((*root->packed)->slen > ((*root->packed)->mlen - check))
                b_alloc(*root->packed, (*root->packed)->mlen * 2);
}
