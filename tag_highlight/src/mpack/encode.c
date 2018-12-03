#include "Common.h"

#include "intern.h"

#define DAI data.arr->items
#define DDE data.dict->entries

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

#if 0
union int_size_cheat { uint64_t u; int64_t i; };

#define _encode_size(SIZE, ARR, IT, VAL)                      \
        do {                                                  \
                uint##SIZE##_t conv = htobe##SIZE(VAL);       \
                memcpy((ARR), &conv, sizeof(uint##SIZE##_t)); \
                (IT) += sizeof(uint##SIZE##_t);               \
        } while (0)

#define encode_uint16(ARR, IT, VAL) _encode_size(16, ARR, IT, VAL)
#define encode_uint32(ARR, IT, VAL) _encode_size(32, ARR, IT, VAL)
#define encode_uint64(ARR, IT, VAL) _encode_size(64, ARR, IT, VAL)

#define _encode_signed(SIZE, ARR, IT, VAL)                    \
        do {                                                  \
                union int_size_cheat m_cheat_ = {.i = (VAL)}; \
                _encode_size(SIZE, ARR, IT, m_cheat_.u);      \
        } while (0)

#define encode_int16(ARR, IT, VAL) _encode_signed(16, ARR, IT, VAL)
#define encode_int32(ARR, IT, VAL) _encode_signed(32, ARR, IT, VAL)
#define encode_int64(ARR, IT, VAL) _encode_signed(64, ARR, IT, VAL)
#endif

#if 0
#define encode_uint16(ARR, IT, VAL)                     \
        do {                                            \
                uint16_t conv = htobe16(VAL);           \
                memcpy((ARR), &conv, sizeof(uint16_t)); \
                (IT) += sizeof(uint16_t);               \
        } while (0)

#define encode_uint32(ARR, IT, VAL)                     \
        do {                                            \
                uint32_t conv = htobe32(VAL);           \
                memcpy((ARR), &conv, sizeof(uint32_t)); \
                (IT) += sizeof(uint32_t);               \
        } while (0)

#define encode_uint64(ARR, IT, VAL)                     \
        do {                                            \
                uint64_t conv = htobe64(VAL);           \
                memcpy((ARR), &conv, sizeof(uint64_t)); \
                (IT) += sizeof(uint64_t);               \
        } while (0)
#endif


static void sanity_check(mpack_obj *root, mpack_obj **item, unsigned check, bool force);

/* God am I ever lazy */
#define D ((*root->packed)->data)
#define L ((*root->packed)->slen)

mpack_obj *
mpack_make_new(UNUSED const unsigned len, const bool encode)
{
        mpack_obj *root = xmalloc((offsetof(mpack_obj, packed) +
                                   sizeof(bstring *)));
        root->flags     = (uint8_t)(encode ? MPACK_ENCODE : 0) |
                          (uint8_t)MPACK_HAS_PACKED;
        *root->packed   = b_alloc_null(128);

        return root;
}

void
mpack_encode_array(mpack_obj       *root,
                   mpack_obj      **item,
                   const unsigned   len)
{
        sanity_check(root, item, 64, false);

        if (item && (root->flags & MPACK_ENCODE)) {
                (*item)->data.arr        = xmalloc(sizeof(mpack_array_t));
                (*item)->data.arr->items = nmalloc(len, sizeof(mpack_obj *));
                (*item)->data.arr->qty   = len;
                (*item)->data.arr->max   = len;
                (*item)->flags          |= (uint8_t)MPACK_ARRAY;

                for (unsigned i = 0; i < len; ++i)
                        (*item)->DAI[i] = NULL;
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
mpack_encode_dictionary(mpack_obj       *root,
                        mpack_obj      **item,
                        const unsigned   len)
{
        sanity_check(root, item, 64, false);

        if (item && (root->flags & MPACK_ENCODE)) {
                (*item)->data.dict          = xmalloc(sizeof(mpack_dict_t));
                (*item)->data.dict->entries = nmalloc(len, sizeof(struct dict_ent *));
                (*item)->data.dict->qty     = len;
                (*item)->data.dict->max     = len;
                (*item)->flags             |= (uint8_t)MPACK_DICT;

                for (unsigned i = 0; i < len; ++i) {
                        (*item)->DDE[i]        = xmalloc(sizeof(struct dict_ent));
                        (*item)->DDE[i]->key   = NULL;
                        (*item)->DDE[i]->value = NULL;
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
mpack_encode_integer(mpack_obj      *root,
                     mpack_obj     **item,
                     const int64_t   value)
{
        sanity_check(root, item, 15, false);

        if (root->flags & MPACK_ENCODE) {
                (*item)->data.num = value;
                (*item)->flags   |= (uint8_t)MPACK_SIGNED;
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
                        errx(1, "Value too big!");
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
                        errx(1, "Value too small!");
                }
        }
}

void
mpack_encode_unsigned(mpack_obj      *root,
                      mpack_obj     **item,
                      const uint64_t  value)
{
        sanity_check(root, item, 15, false);

        if (root->flags & MPACK_ENCODE) {
                (*item)->data.num = value;
                (*item)->flags    |= (uint8_t)MPACK_UNSIGNED;
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
mpack_encode_string(mpack_obj      *root,
                    mpack_obj     **item,
                    const bstring  *string)
{
        if (!string)
                string = B("");
        sanity_check(root, item, string->slen + 5, false);

        if (root->flags & MPACK_ENCODE) {
                (*item)->data.str = b_strcpy(string);
                (*item)->flags   |= (uint8_t)MPACK_STRING;
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
mpack_encode_boolean(mpack_obj  *root,
                     mpack_obj **item,
                     const bool  value)
{
        sanity_check(root, item, 2, false);

        if (root->flags & MPACK_ENCODE) {
                (*item)->data.boolean = value;
                (*item)->flags       |= (uint8_t)MPACK_BOOL;
        }

        D[L++] = (value) ? M_MASK_TRUE : M_MASK_FALSE;
}

void
mpack_encode_nil(mpack_obj *root, mpack_obj **item)
{
        sanity_check(root, item, 2, false);

        if (root->flags & MPACK_ENCODE) {
                (*item)->data.nil = M_MASK_NIL;
                (*item)->flags   |= (uint8_t)MPACK_NIL;
        }

        D[L++] = M_MASK_NIL;
}

/*============================================================================*/

static void
sanity_check(mpack_obj       *root,
             mpack_obj      **item,
             const unsigned   check,
             const bool       force)
{
        if (item && !(*item) && (force || (root->flags & MPACK_ENCODE))) {
                (*item)        = xmalloc(sizeof(mpack_obj));
                (*item)->flags = 0;
        }

        if ((*root->packed)->slen > ((*root->packed)->mlen - check))
                b_alloc(*root->packed, (*root->packed)->mlen * 2);
}
