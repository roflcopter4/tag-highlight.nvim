#include "mytags.h"
#include <stddef.h>

#include "data.h"
#include "mpack_code.h"

static mpack_obj * decode_array     (bstring *buf, const struct mpack_masks *mask, bool skip_3);
static mpack_obj * decode_string    (bstring *buf, const struct mpack_masks *mask);
static mpack_obj * decode_dictionary(bstring *buf, const struct mpack_masks *mask);
static mpack_obj * decode_integer   (bstring *buf, const struct mpack_masks *mask);
static mpack_obj * decode_unsigned  (bstring *buf, const struct mpack_masks *mask);
static mpack_obj * decode_ext       (bstring *buf, const struct mpack_masks *mask);
static const struct mpack_masks * id_pack_type(bstring *buf);


#define decode_int16(BYTE) \
        (((*(BYTE) << 8u) | (*((BYTE) + 1))) | 0xFFFF0000u)
#define decode_uint16(BYTE) \
        ((*(BYTE) << 8u) | (*((BYTE) + 1)))
#define decode_int32(BYTE) \
        ((*(BYTE) << 24u) | (*((BYTE) + 1) << 16u) | (*((BYTE) + 2) << 8u) | *((BYTE) + 3))

#define decode_uint32 decode_int32


mpack_obj *
decode_pack(bstring *buf, bool skip_3)
{
        const struct mpack_masks *mask = id_pack_type(buf);

        switch (mask->group) {
        case ARRAY:  return decode_array(buf, mask, skip_3);
        case MAP:    return decode_dictionary(buf, mask);
        case STRING: return decode_string(buf, mask);
        case LINT:
        case INT:    return decode_integer(buf, mask);
        case UINT:   return decode_unsigned(buf, mask);
        case EXT:    return decode_ext(buf, mask);

        case BIN:    errx(2, "BIN not implemented.");
        case NIL:    ++buf->data; return NULL;

        default:
                errx(1, "Default (%d) reached at %d in %s - %s",
                     mask->group, __LINE__, __FILE__, __func__);
        }
}


static mpack_obj *
decode_array(bstring *buf, const struct mpack_masks *mask, const bool skip_3)
{
        mpack_obj *item;
        uint32_t   size = 0;
        if (skip_3) {
                item = xmalloc((offsetof(mpack_obj, packed) + sizeof(bstring *)));
                item->flags = MPACK_HAS_PACKED;
        } else {
                item = xmalloc(sizeof *item);
                item->flags = 0;
        }

        if (mask->fixed) {
                size = (uint32_t)(*buf->data++ ^ mask->val);
                --buf->slen;
        } else {
                uint8_t *byte = ++buf->data;

                switch (mask->type) {
                case M_ARRAY_16:
                        size = decode_uint16(byte);
                        buf->data += 2;
                        buf->slen -= 2;
                        break;

                case M_ARRAY_32:
                        size = decode_uint32(byte);
                        buf->data += 4;
                        buf->slen -= 4;
                        break;

                default:
                        errx(1, "Default (0x%0X) reached at %d in %s - %s",
                             mask->val, __LINE__, __FILE__, __func__);
                }
        }

        fprintf(stderr, "Array is of size %u!\n", size);

        item->flags          |= MPACK_ARRAY;
        item->data.arr        = xmalloc(sizeof(struct mpack_array));
        item->data.arr->items = nmalloc(size, sizeof(mpack_obj *));
        item->data.arr->max   = size;
        item->data.arr->qty   = 0;

        extern FILE *mpack_log;

        for (unsigned i = 0; i < item->data.arr->max; ++i) {
                mpack_obj *tmp = decode_pack(buf, false);
                if (tmp) {
                        if (skip_3 && i < 3)
                                mpack_destroy(tmp);
                        else
                                item->data.arr->items[item->data.arr->qty++] = tmp;
                } else
                        b_fputs(mpack_log, b_tmp("Got a null object\n"));
        }

        return item;
}


static mpack_obj *
decode_dictionary(bstring *buf, const struct mpack_masks *mask)
{
        mpack_obj *item = xmalloc(sizeof *item);
        uint32_t   size;

        if (mask->fixed) {
                size = (unsigned)(*buf->data++ ^ mask->val);
                --buf->slen;
        } else {
                uint8_t *byte = ++buf->data;

                switch (mask->type) {
                case M_MAP_16:
                        size = decode_int16(byte);
                        buf->data += 2;
                        buf->slen -= 2;
                        break;

                case M_MAP_32:
                        size = decode_int32(byte);
                        buf->data += 4;
                        buf->slen -= 4;
                        break;

                default:
                        errx(1, "Default reached on line %d of file %s, in function %s().",
                             __LINE__, __FILE__, __func__);
                }
        }

        item->flags              = MPACK_DICT;
        item->data.dict          = xmalloc(sizeof(struct mpack_dictionary));
        item->data.dict->entries = nmalloc(sizeof(struct dict_ent *), size);
        item->data.dict->qty     = item->data.dict->max = size;

        for (unsigned i = 0; i < item->data.arr->max; ++i) {
                item->data.dict->entries[i]        = xmalloc(sizeof(struct dict_ent));
                item->data.dict->entries[i]->key   = decode_pack(buf, false);
                item->data.dict->entries[i]->value = decode_pack(buf, false);
        }

        return item;
}


static mpack_obj *
decode_string(bstring *buf, const struct mpack_masks *mask)
{
        mpack_obj *item = xmalloc(sizeof *item);
        uint32_t   size;

        if (mask->fixed) {
                size = *buf->data++ ^ mask->val;
                --buf->slen;
        } else {
                uint8_t *byte = ++buf->data;

                switch (mask->type) {
                case M_STR_8:
                        size = (*byte);
                        buf->data += 1;
                        buf->slen -= 1;
                        break;

                case M_STR_16:
                        size = decode_uint16(byte);
                        buf->data += 2;
                        buf->slen -= 2;
                        break;

                case M_STR_32:
                        size = decode_uint32(byte);
                        buf->data += 4;
                        buf->slen -= 4;
                        break;

                default:
                        errx(1, "Default reached on line %d of file %s, in function %s().",
                             __LINE__, __FILE__, __func__);
                }
        }

        item->flags    = MPACK_STRING;
        item->data.str = b_blk2bstr(buf->data, (int)size);
        buf->data     += size;
        buf->slen     -= size;

        return item;
}


static mpack_obj *
decode_integer(bstring *buf, const struct mpack_masks *mask)
{
        mpack_obj *item  = xmalloc(sizeof *item);
        int32_t    value = 0;

        if (mask->fixed) {
                value = *buf->data++ ^ mask->val;
                --buf->slen;
        } else {
                uint8_t *byte = ++buf->data;

                switch (mask->type) {
                case M_INT_8:
                        value = (*byte);
                        value |= 0xFFFFFF00u;
                        buf->data += 1;
                        buf->slen -= 1;
                        break;

                case M_INT_16:
                        value = decode_int16(byte);
                        value |= 0xFFFF0000u;
                        buf->data += 2;
                        buf->slen -= 2;
                        break;

                case M_INT_32:
                        value = decode_int32(byte);
                        buf->data += 4;
                        buf->slen -= 4;
                        break;

                case M_INT_64:
                default:
                        errx(1, "Default reached on line %d of file %s, in function %s().",
                             __LINE__, __FILE__, __func__);
                }
        }

        item->flags    = MPACK_NUM;
        item->data.num = (int64_t)value;

        return item;
}


static mpack_obj *
decode_unsigned(bstring *buf, const struct mpack_masks *mask)
{
        mpack_obj *item  = xmalloc(sizeof *item);
        uint32_t   value = 0;

        if (mask->fixed) {
                value = *buf->data++ ^ mask->val;
                --buf->slen;
        } else {
                uint8_t *byte = ++buf->data;

                switch (mask->type) {
                case M_UINT_8:
                        value = (uint32_t)(*byte);
                        buf->data += 1;
                        buf->slen -= 1;
                        break;

                case M_UINT_16:
                        value = (uint32_t)decode_uint16(byte);
                        buf->data += 2;
                        buf->slen -= 2;
                        break;

                case M_UINT_32:
                        value = (uint32_t)decode_uint32(byte);
                        buf->data += 4;
                        buf->slen -= 4;
                        break;

                case M_UINT_64:
                default:
                        errx(1, "Default (%d -> \"%s\") reached on line %d of file %s, in function %s().",
                             mask->type, mask->repr, __LINE__, __FILE__, __func__);
                }
        }

        item->flags    = MPACK_NUM;
        item->data.num = (int64_t)value;

        return item;
}


static mpack_obj *
decode_ext(bstring *buf, const struct mpack_masks *mask)
{
        mpack_obj *item  = xmalloc(sizeof *item);
        uint8_t   *byte  = ++buf->data;
        uint32_t   value = 0;
        int8_t     type, shift;

        switch (mask->type) {
        case M_EXT_F1:
                type  = (int8_t)(*byte++);
                value = (uint32_t)(*byte);
                shift = 1;
                break;
        case M_EXT_F2:
                type  = (int8_t)(*byte++);
                value = (uint32_t)decode_uint16(byte);
                shift = 2;
                break;
        case M_EXT_F4:
                type  = (int8_t)(*byte++);
                value = (uint32_t)decode_uint32(byte);
                shift = 4;
                break;

        default:
                errx(1, "Default reached on line %d of file %s, in function %s().",
                     __LINE__, __FILE__, __func__);
        }

        buf->data += shift;
        buf->slen -= shift;

        item->flags          = MPACK_EXT;
        item->data.ext       = xmalloc(sizeof(struct mpack_ext));
        item->data.ext->type = type;
        item->data.ext->num  = value;

        return item;
}


static const struct mpack_masks *
id_pack_type(bstring *buf)
{
        const struct mpack_masks *mask = NULL;
        if (!buf || !buf->data)
                errx(1, "Error, uninitialized data.");
        const uint8_t byte = *buf->data;

        for (unsigned i = 0; i < m_masks_len; ++i) {
                if (m_masks[i].fixed) {
                        if ((byte & m_masks[i].val) == m_masks[i].val) {
                                mask = (m_masks + i);
                                break;
                        }
                } else {
                        if (byte == m_masks[i].val) {
                                mask = (m_masks + i);
                                break;
                        }
                }
        }

        if (!mask)
                err(1, "Failed to identify type.");

        return mask;
}
