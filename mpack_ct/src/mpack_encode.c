#include "util.h"
#include <limits.h>
#include <stddef.h>

#include "data.h"
#include "mpack_code.h"


#define encode_uint32(ARR, IT, VAL)                                            \
        do {                                                                   \
                ((ARR)[(IT)++]) = (uint8_t)(((uint32_t)(VAL)) >> 24u);         \
                ((ARR)[(IT)++]) = (uint8_t)(((uint32_t)(VAL)) >> 16u);         \
                ((ARR)[(IT)++]) = (uint8_t)(((uint32_t)(VAL)) >> 8u);          \
                ((ARR)[(IT)++]) = (uint8_t)(((uint32_t)(VAL)) & 0xFFu);        \
        } while (0)

#define encode_uint16(ARR, IT, VAL)                                            \
        do {                                                                   \
                ((ARR)[(IT)++]) = (uint8_t)(((uint16_t)(VAL)) >> 8u);          \
                ((ARR)[(IT)++]) = (uint8_t)(((uint16_t)(VAL)) & 0xFFu);        \
        } while (0)

#define encode_int32(ARR, IT, VAL)                                             \
        do {                                                                   \
                ((ARR)[(IT)++]) = (int8_t)(((int32_t)(VAL)) >> 24);            \
                ((ARR)[(IT)++]) = (int8_t)(((int32_t)(VAL)) >> 16);            \
                ((ARR)[(IT)++]) = (int8_t)(((int32_t)(VAL)) >> 8);             \
                ((ARR)[(IT)++]) = (int8_t)(((int32_t)(VAL)) & 0xFF);           \
        } while (0)

#define encode_int16(ARR, IT, VAL)                                             \
        do {                                                                   \
                ((ARR)[(IT)++]) = (int8_t)(((int16_t)(VAL)) >> 8);             \
                ((ARR)[(IT)++]) = (int8_t)(((int16_t)(VAL)) & 0xFF);           \
        } while (0)


static void sanity_check(mpack_obj *root, struct mpack_array *parent,
                         mpack_obj **item, unsigned check, bool force);


/* God am I ever lazy */
#define D ((*root->packed)->data)
#define L ((*root->packed)->slen)


mpack_obj *
mpack_make_new(const unsigned len, const bool encode)
{
        mpack_obj *root = xmalloc((offsetof(mpack_obj, packed) + sizeof(bstring *)));
        root->flags     = (encode ? MPACK_ENCODE : 0) | MPACK_HAS_PACKED;
        *root->packed   = b_alloc_null(128);

        mpack_encode_array(root, NULL, &root, len);
        return root;
}


void
mpack_encode_array(mpack_obj           *root,
                   struct mpack_array  *parent,
                   mpack_obj          **item,
                   const unsigned       len)
{
        if (!root)
                errx(1, "Root is null! Shut up clang!");

        sanity_check(root, parent, item, 64, true);

        (*item)->data.arr        = xmalloc(sizeof(struct mpack_array));
        (*item)->data.arr->items = nmalloc(sizeof(mpack_obj *), len);
        (*item)->data.arr->qty   = 0;
        (*item)->data.arr->max   = len;
        (*item)->flags          |= MPACK_ARRAY;

        for (unsigned i = 0; i < len; ++i)
                (*item)->data.arr->items[i] = NULL;

        if (len <= M_ARRAY_F_MAX) {
                D[L++] = M_MASK_ARRAY_F | (uint8_t)len;
        } else if (len < UINT16_MAX) {
                D[L++] = M_MASK_ARRAY_16;
                encode_uint16(D, L, len);
        } else if (len < UINT32_MAX) {
                D[L++] = M_MASK_ARRAY_32;
                encode_uint32(D, L, len);
        } else
                errx(1, "Array is too big!");
}


void
mpack_encode_integer(mpack_obj           *root,
                     struct mpack_array  *parent,
                     mpack_obj          **item,
                     const int64_t        value)
{
        sanity_check(root, parent, item, 15, false);

        if (root->flags & MPACK_ENCODE) {
                (*item)->data.num = value;
                (*item)->flags   |= MPACK_NUM;
        }

        if (value >= 0) {
                if (value < UINT8_MAX) {
                        D[L++] = M_MASK_UINT_8;
                        D[L++] = (uint8_t)value;
                } else if (value < UINT16_MAX) {
                        D[L++] = M_MASK_UINT_16;
                        encode_uint16(D, L, value);
                } else if (value < UINT32_MAX) {
                        D[L++] = M_MASK_UINT_32;
                        encode_uint32(D, L, value);
                } else {
                        errx(1, "Value too big!");
                }
        } else {
                if (value > INT8_MIN) {
                        D[L++] = M_MASK_INT_8;
                        D[L++] = (int8_t)value;
                } else if (value > INT16_MIN) {
                        D[L++] = M_MASK_INT_16;
                        encode_int16(D, L, value);
                } else if (value > INT32_MIN) {
                        D[L++] = M_MASK_INT_32;
                        encode_int32(D, L, value);
                } else {
                        errx(1, "Value too small!");
                }
        }
}


void
mpack_encode_string(mpack_obj           *root,
                    struct mpack_array  *parent,
                    mpack_obj          **item,
                    const bstring       *string)
{
        sanity_check(root, parent, item, string->slen + 5, false);

        if (root->flags & MPACK_ENCODE) {
                (*item)->data.str = b_strcpy(string);
                (*item)->flags   |= MPACK_STRING;
        }

        if (string->slen <= 31) {
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

        /* memcpy((D + L), string->data, string->slen); */
        /* (*root->packed)->slen += string->slen; */
        b_concat(*root->packed, string);
}


void
mpack_encode_boolean(mpack_obj           *root,
                     struct mpack_array  *parent,
                     mpack_obj          **item,
                     const bool           value)
{
        sanity_check(root, parent, item, 2, false);

        if (root->flags & MPACK_ENCODE) {
                (*item)->data.boolean = value;
                (*item)->flags       |= MPACK_BOOL;
        }

        D[L++] = (value) ? M_MASK_TRUE : M_MASK_FALSE;
        /* b_conchar(D, (value) ? M_MASK_TRUE : M_MASK_FALSE); */
}


/*============================================================================*/


static void
sanity_check(mpack_obj           *root,
             struct mpack_array  *parent,
             mpack_obj          **item,
             const unsigned       check,
             const bool           force)
{
        if (parent) {
                if (!(*item) && ((root->flags & MPACK_ENCODE) || force)) {
                        (*item)        = xmalloc(sizeof(mpack_obj));
                        (*item)->flags = 0;
                }
                ++parent->qty;
        }

        if ((*root->packed)->slen > ((*root->packed)->mlen - check))
                b_alloc(*root->packed, (*root->packed)->mlen * 2);
}
