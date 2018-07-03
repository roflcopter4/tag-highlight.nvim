#include "util.h"
#include <stddef.h>

#include "data.h"
#include "mpack.h"
#include "mpack_code.h"
extern FILE *decodelog;
extern int   sockfd;
extern pthread_mutex_t readlockstdin;
extern pthread_mutex_t readlocksocket;

static mpack_obj * do_decode_stream (int fd);
static mpack_obj * decode_array     (int fd, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_string    (int fd, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_dictionary(int fd, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_integer   (int fd, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_unsigned  (int fd, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_ext       (int fd, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_nil       (void);
static mpack_obj * decode_bool      (const struct mpack_masks *mask);
static const struct mpack_masks * id_pack_type(uint8_t byte);


#define decode_int16(WORD) \
        ((((WORD)[0]) << 8) | ((WORD)[1]) | (0xFFFF0000u))

#define decode_uint16(WORD) \
        ((((WORD)[0]) << 8) | ((WORD)[1]))

#define decode_int32(WORD) \
        ((((WORD)[0]) << 24) | (((WORD)[1]) << 16) | (((WORD)[2]) << 8) | ((WORD)[3]))

#define decode_uint32 decode_int32

#define ERRMSG()                                                                           \
        errx(1, "Default (%d -> \"%s\") reached on line %d of file %s, in function %s().", \
             mask->type, mask->repr, __LINE__, __FILE__, __func__)

#define LOG(...) fprintf(decodelog, __VA_ARGS__)


/* 
 * This is just a little wrapper to ensure that the requested number of bytes is
 * always read. read() often seens to return early for no apparent reason.
 */
__attribute__((always_inline))
static inline void
READ(int fildes, void *buf, size_t nbytes)
{
        size_t nread = 0;

        do {
                ssize_t n = read(fildes, buf, (nbytes - nread));
                if (n != (-1))
                        nread += n;
        } while (nread > nbytes);
}


mpack_obj *
decode_stream(int fd, const enum message_types expected_type)
{
        pthread_mutex_t *mut = NULL;

        if (fd == 1) {
                fd  = 0;
                mut = &readlockstdin;
        } else
                mut = &readlocksocket;

        pthread_mutex_lock(mut);

        mpack_obj *ret = do_decode_stream(fd);

        if (!ret)
                errx(1, "Failed to decode stream.");
        if (mpack_type(ret) != MPACK_ARRAY) {
                nvprintf("For some incomprehensible reason the pack's type is %d.\n",
                         mpack_type(ret));
                mpack_print_object(ret, mpack_log);
                fflush(mpack_log);
                abort();
        }

        pthread_mutex_unlock(mut);

        if (expected_type != MES_ANY &&
            expected_type != (unsigned)ret->data.arr->items[0]->data.num)
        {
                if (fd != 0)
                        nvprintf("Expected %d but got %ld\n", expected_type,
                                 ret->data.arr->items[0]->data.num);

                switch (ret->data.arr->items[0]->data.num) {
                case MES_REQUEST: 
                        errx(1, "This will NEVER happen.");
                case MES_RESPONSE:
                        mpack_destroy(ret);
                        return NULL;
                case MES_NOTIFICATION:
                        handle_unexpected_notification(ret);
                        break;
                default:
                        abort();
                }

                return decode_stream(fd, expected_type);
        }

        return ret;
}


static mpack_obj *
do_decode_stream(const int fd)
{
        uint8_t byte = 0;
        READ(fd, &byte, 1);
        const struct mpack_masks *mask = id_pack_type(byte);

        switch (mask->group) {
        case ARRAY:  return decode_array(fd, byte, mask);
        case MAP:    return decode_dictionary(fd, byte, mask);
        case STRING: return decode_string(fd, byte, mask);
        case INT:    return decode_integer(fd, byte, mask);
        case LINT:
        case UINT:   return decode_unsigned(fd, byte, mask);
        case EXT:    return decode_ext(fd, byte, mask);
        case NIL:    return decode_nil();
        case BOOL:   return decode_bool(mask);

        case BIN:    errx(2, "BIN not implemented.");
        default:     errx(1, "Default (%d) reached at %d in %s - %s",
                          mask->group, __LINE__, __FILE__, __func__);
        }
}


static mpack_obj *
decode_array(const int fd, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;
        uint8_t    word[4] = { 0, 0, 0, 0 };

        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_ARRAY_16:
                        READ(fd, word, 2);
                        size = decode_uint16(word);
                        break;

                case M_ARRAY_32:
                        READ(fd, word, 4);
                        size = decode_uint32(word);
                        break;

                default:
                        ERRMSG();
                }
        }

        item->flags           = MPACK_ARRAY;
        item->data.arr        = xmalloc(sizeof(struct mpack_array));
        item->data.arr->items = nmalloc(size, sizeof(mpack_obj *));
        item->data.arr->max   = size;
        item->data.arr->qty   = 0;

        LOG("It is an array! -> 0x%0X => size %u\n", byte, size);

        for (unsigned i = 0; i < item->data.arr->max; ++i) {
                mpack_obj *tmp = do_decode_stream(fd);
                item->data.arr->items[item->data.arr->qty++] = tmp;
        }

        return item;
}


static mpack_obj *
decode_dictionary(const int fd, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;
        uint8_t    word[4] = { 0, 0, 0, 0 };


        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_MAP_16:
                        READ(fd, word, 2);
                        size = decode_uint16(word);
                        break;

                case M_MAP_32:
                        READ(fd, word, 4);
                        size = decode_uint32(word);
                        break;

                default:
                        ERRMSG();
                }
        }

        item->flags              = MPACK_DICT;
        item->data.dict          = xmalloc(sizeof(struct mpack_dictionary));
        item->data.dict->entries = nmalloc(sizeof(struct dict_ent *), size);
        item->data.dict->qty     = item->data.dict->max = size;

        LOG("It is a dictionary! -> 0x%0X => size %u\n", byte, size);

        for (unsigned i = 0; i < item->data.arr->max; ++i) {
                item->data.dict->entries[i]        = xmalloc(sizeof(struct dict_ent));
                item->data.dict->entries[i]->key   = do_decode_stream(fd);
                item->data.dict->entries[i]->value = do_decode_stream(fd);
        }

        return item;
}


static mpack_obj *
decode_string(const int fd, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;
        uint8_t    word[4] = { 0, 0, 0, 0 };

        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_STR_8:
                        READ(fd, word, 1);
                        size = (uint32_t)word[0];
                        break;

                case M_STR_16:
                        READ(fd, word, 2);
                        size = decode_uint16(word);
                        break;

                case M_STR_32:
                        READ(fd, word, 4);
                        size = decode_uint32(word);
                        break;

                default:
                        ERRMSG();
                }
        }

        LOG("It is a string! -> 0x%0X : size: %u", byte, size);

        item->flags          = MPACK_STRING;
        item->data.str       = b_alloc_null(size + 1);
        item->data.str->slen = size;

        if (size > 0)
                READ(fd, item->data.str->data, size);

        item->data.str->data[size] = (uchar)'\0';

        b_fputs(decodelog, b_tmp(" : \""), item->data.str, b_tmp("\"\n"));

        return item;
}


static mpack_obj *
decode_integer(const int fd, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        int32_t    value   = 0;
        uint8_t    word[4] = { 0, 0, 0, 0 };


        if (mask->fixed) {
                value = (int32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_INT_8:
                        READ(fd, word, 1);
                        value  = (int32_t)word[0];
                        value |= 0xFFFFFF00u;
                        break;

                case M_INT_16:
                        READ(fd, word, 2);
                        value  = decode_int16(word);
                        value |= 0xFFFF0000u;
                        break;

                case M_INT_32:
                        READ(fd, word, 4);
                        value = decode_int32(word);
                        break;

                case M_INT_64:
                default:
                        ERRMSG();
                }
        }

        item->flags    = MPACK_NUM;
        item->data.num = (int64_t)value;

        LOG("It is a signed int! -> 0x%0X : value => %d\n", byte, value);

        return item;
}


static mpack_obj *
decode_unsigned(const int fd, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   value   = 0;
        uint8_t    word[4] = { 0, 0, 0, 0 };

        if (mask->fixed) {
                value = (uint32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_UINT_8:
                        READ(fd, word, 1);
                        value = (uint32_t)word[0];
                        break;

                case M_UINT_16:
                        READ(fd, word, 2);
                        value = (uint32_t)decode_uint16(word);
                        break;

                case M_UINT_32:
                        READ(fd, word, 4);
                        value = (uint32_t)decode_uint32(word);
                        break;

                case M_UINT_64:
                default:
                        ERRMSG();
                }
        }

        item->flags    = MPACK_NUM;
        item->data.num = (int64_t)value;

        LOG("It is an unsigned int! -> 0x%0X : value => %u\n", byte, value);

        return item;
}


static mpack_obj *
decode_ext(const int fd, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item  = xmalloc(sizeof *item);
        uint32_t   value = 0;
        int8_t     type;
        uint8_t    word[4];

        switch (mask->type) {
        case M_EXT_F1:
                READ(fd, &type, 1);
                READ(fd, word, 1);
                value = (uint32_t)word[0];
                break;
        case M_EXT_F2:
                READ(fd, &type, 1);
                READ(fd, word, 2);
                value = (uint32_t)decode_uint16(word);
                break;
        case M_EXT_F4:
                READ(fd, &type, 1);
                READ(fd, word, 4);
                value = (uint32_t)decode_uint32(word);
                break;

        default:
                ERRMSG();
        }

        item->flags          = MPACK_EXT;
        item->data.ext       = xmalloc(sizeof(struct mpack_ext));
        item->data.ext->type = type;
        item->data.ext->num  = value;

        LOG("It is an extention thingy! -> 0x%0X : data => %d, %d\n",
            byte, type, value);

        return item;
}


static mpack_obj *
decode_nil(void)
{
        mpack_obj *item = xmalloc(sizeof *item);
        item->flags     = MPACK_NIL;
        item->data.nil  = M_MASK_NIL;

        return item;
}


static mpack_obj *
decode_bool(const struct mpack_masks *mask)
{
        mpack_obj *item = xmalloc(sizeof *item);
        item->flags     = MPACK_BOOL;

        switch (mask->type) {
        case M_TRUE:  item->data.boolean = true;  break;
        case M_FALSE: item->data.boolean = false; break;
        default:      ERRMSG();
        }

        return item;
}


/*============================================================================*/


static const struct mpack_masks *
id_pack_type(const uint8_t byte)
{
        const struct mpack_masks *mask = NULL;

        for (unsigned i = 0; i < m_masks_len; ++i) {
                if (m_masks[i].fixed) {
                        if ((byte >> m_masks[i].shift) == (m_masks[i].val >> m_masks[i].shift)) {
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
                errx(1, "Failed to identify type for byte 0x%0X.", byte);
        LOG("Identified 0x%0X as a \"%s\"\n", byte, mask->repr);

        return mask;
}
