#include "util.h"
#include <stddef.h>

#include "data.h"
#include "mpack.h"
#include "mpack_code.h"

extern int decode_log_raw;
extern FILE *decodelog;
static pthread_mutex_t mpack_stdin_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mpack_socket_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef _MSC_VER
#  define restrict __restrict
#endif

typedef void (*read_fn)(void *restrict src, uint8_t *restrict dest, size_t nbytes);

static mpack_obj * do_decode        (read_fn READ, void *src);
static mpack_obj * decode_array     (read_fn READ, void *src, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_string    (read_fn READ, void *src, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_dictionary(read_fn READ, void *src, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_integer   (read_fn READ, void *src, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_unsigned  (read_fn READ, void *src, uint8_t byte, const struct mpack_masks *mask);
static mpack_obj * decode_ext       (read_fn READ, void *src, const struct mpack_masks *mask);
static mpack_obj * decode_nil       (void);
static mpack_obj * decode_bool      (const struct mpack_masks *mask);
static const struct mpack_masks * id_pack_type(uint8_t byte);
static void stream_read(void *restrict src, uint8_t *restrict dest, size_t nbytes);
static void obj_read   (void *restrict src, uint8_t *restrict dest, size_t nbytes);


#define IAT(NUM_, AT__) ((uint64_t)((NUM_)[AT__]))

#define decode_int16(NUM_)                                     \
        ((((NUM_)[0]) << 010) | ((NUM_)[1]))
#define decode_int32(NUM_)                                     \
        ((((NUM_)[0]) << 030) | (((NUM_)[1]) << 020) |         \
         (((NUM_)[2]) << 010) | (((NUM_)[3])))
#define decode_int64(NUM_)                                     \
        ((IAT(NUM_, 0) << 070) | (IAT(NUM_, 1) << 060) |       \
         (IAT(NUM_, 2) << 050) | (IAT(NUM_, 3) << 040) |       \
         (IAT(NUM_, 4) << 030) | (IAT(NUM_, 5) << 020) |       \
         (IAT(NUM_, 6) << 010) | (IAT(NUM_, 7)))

#define ERRMSG()                                                                         \
        errx(1, "Default (%d -> \"%s\") reached on line %d of file %s, in function %s.", \
             mask->type, mask->repr, __LINE__, __FILE__, FUNC_NAME)

//#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define LOG(...)


/*============================================================================*/


mpack_obj *
decode_stream(int32_t fd, const enum message_types expected_type)
{
        pthread_mutex_t *mut = NULL;

        if (fd == 1) {
                fd  = 0;
                mut = &mpack_stdin_mutex;
        } else
                mut = &mpack_socket_mutex;

        pthread_mutex_lock(mut);

        mpack_obj *ret = do_decode(&stream_read, &fd);

        if (!ret)
                errx(1, "Failed to decode stream.");
        if (mpack_type(ret) != MPACK_ARRAY) {
                if (mpack_log) {
                        mpack_print_object(ret, mpack_log);
                        fflush(mpack_log);
                }
                eprintf("For some incomprehensible reason the pack's type is %d.\n",
                        mpack_type(ret));
                abort();
        }

        pthread_mutex_unlock(mut);

        if (expected_type != MES_ANY &&
            expected_type != (ret->DAI[0]->data.num + 1))
        {
                /* eprintf("Expected %d but got %"PRId64"\n",
                        expected_type, ret->DAI[0]->data.num); */

                switch (ret->DAI[0]->data.num + 1) {
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

        //UNUSED ssize_t n = write(decode_log_raw, "\n\n", 2);
        return ret;
}


mpack_obj *
decode_obj(bstring *buf, const enum message_types expected_type)
{
        mpack_obj *ret = do_decode(&obj_read, buf);

        if (!ret)
                errx(1, "Failed to decode stream.");
        if (mpack_type(ret) != MPACK_ARRAY) {
                eprintf("For some incomprehensible reason the pack's type is %d.\n",
                        mpack_type(ret));
                if (mpack_log) {
                        mpack_print_object(ret, mpack_log);
                        fflush(mpack_log);
                }
                abort();
        }

        if (expected_type != MES_ANY &&
            expected_type != ((uint32_t)ret->DAI[0]->data.num + 1u))
        {
                if (buf != 0) {
                        eprintf("Expected %d but got %"PRId64"\n", expected_type,
                                ret->DAI[0]->data.num);
                }

                switch (ret->DAI[0]->data.num + 1) {
                case MES_REQUEST:      errx(1, "This will NEVER happen.");
                case MES_RESPONSE:     mpack_destroy(ret); return NULL;
                case MES_NOTIFICATION: handle_unexpected_notification(ret); break;
                default:               abort();
                }

                return decode_obj(buf, expected_type);
        }

        return ret;
}


/*============================================================================*/


static mpack_obj *
do_decode(const read_fn READ, void *src)
{
        uint8_t byte = 0;
        READ(src, &byte, 1);
        const struct mpack_masks *mask = id_pack_type(byte);

        switch (mask->group) {
        case G_ARRAY:  return decode_array(READ, src, byte, mask);
        case G_MAP:    return decode_dictionary(READ, src, byte, mask);
        case G_STRING: return decode_string(READ, src, byte, mask);
        case G_INT:    return decode_integer(READ, src, byte, mask);
        case G_LINT:
        case G_UINT:   return decode_unsigned(READ, src, byte, mask);
        case G_EXT:    return decode_ext(READ, src, mask);
        case G_BOOL:   return decode_bool(mask);
        case G_NIL:    return decode_nil();

        case G_BIN:    errx(2, "ERROR: Bin format is not implemented.");
        default:       errx(3, "Default (%d) reached at %d in %s - %s",
                            mask->group, __LINE__, __FILE__, FUNC_NAME);
        }
}


static mpack_obj *
decode_array(const read_fn READ, void *src, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;
        uint8_t    word[4] = {0, 0, 0, 0};

        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_ARRAY_16:
                        READ(src, word, 2);
                        size = decode_int16(word);
                        break;
                case M_ARRAY_32:
                        READ(src, word, 4);
                        size = decode_int32(word);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags           = MPACK_ARRAY | MPACK_ENCODE;
        item->data.arr        = xmalloc(sizeof(mpack_array_t));
        item->data.arr->items = nmalloc(size, sizeof(mpack_obj *));
        item->data.arr->max   = size;
        item->data.arr->qty   = 0;

        LOG("It is an array! -> 0x%0X => size %u\n", byte, size);

        for (unsigned i = 0; i < item->data.arr->max; ++i) {
                mpack_obj *tmp = do_decode(READ, src);
                item->DAI[item->data.arr->qty++] = tmp;
        }

        return item;
}


static mpack_obj *
decode_dictionary(const read_fn READ, void *src, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;
        uint8_t    word[4] = { 0, 0, 0, 0 };


        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_MAP_16:
                        READ(src, word, 2);
                        size = decode_int16(word);
                        break;
                case M_MAP_32:
                        READ(src, word, 4);
                        size = decode_int32(word);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags              = MPACK_DICT | MPACK_ENCODE;
        item->data.dict          = xmalloc(sizeof(struct mpack_dictionary));
        item->data.dict->entries = nmalloc(sizeof(struct dict_ent *), size);
        item->data.dict->qty     = item->data.dict->max = size;

        LOG("It is a mpack_dict_t! -> 0x%0X => size %u\n", byte, size);

        for (uint32_t i = 0; i < item->data.arr->max; ++i) {
                item->DDE[i]        = xmalloc(sizeof(struct dict_ent));
                item->DDE[i]->key   = do_decode(READ, src);
                item->DDE[i]->value = do_decode(READ, src);
        }

        return item;
}


static mpack_obj *
decode_string(const read_fn READ, void *src, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;
        uint8_t    word[4] = { 0, 0, 0, 0 };

        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_STR_8:
                        READ(src, word, 1);
                        size = (uint32_t)word[0];
                        break;
                case M_STR_16:
                        READ(src, word, 2);
                        size = decode_int16(word);
                        break;
                case M_STR_32:
                        READ(src, word, 4);
                        size = decode_int32(word);
                        break;
                default:
                        ERRMSG();
                }
        }

        LOG("It is a string! -> 0x%0X : size: %u", byte, size);

        item->flags          = MPACK_STRING | MPACK_ENCODE;
        item->data.str       = b_alloc_null(size + 1);
        item->data.str->slen = size;

        if (size > 0)
                READ(src, item->data.str->data, size);

        item->data.str->data[size] = (uchar)'\0';

        return item;
}


static mpack_obj *
decode_integer(const read_fn READ, void *src, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        int64_t    value   = 0;
        uint8_t    word[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        if (mask->fixed) {
                value = (int32_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_INT_8:
                        READ(src, word, 1);
                        value  = (int64_t)word[0];
                        value |= 0xFFFFFFFFFFFFFF00llu;
                        break;
                case M_INT_16:
                        READ(src, word, 2);
                        value  = (int64_t)decode_int16(word);
                        value |= 0xFFFFFFFFFFFF0000llu;
                        break;
                case M_INT_32:
                        READ(src, word, 4);
                        value  = (int64_t)decode_int32(word);
                        value |= 0xFFFFFFFF00000000llu;
                        break;
                case M_INT_64:
                        READ(src, word, 8);
                        value  = (int64_t)decode_int64(word);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags    = MPACK_NUM;
        item->data.num = value;

        LOG("It is a signed int32_t! -> 0x%0X : value => %d\n", byte, value);

        return item;
}


static mpack_obj *
decode_unsigned(const read_fn READ, void *src, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint64_t   value   = 0;
        uint8_t    word[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        if (mask->fixed) {
                value = (uint64_t)(byte ^ mask->val);
        } else {
                switch (mask->type) {
                case M_UINT_8:
                        READ(src, word, 1);
                        value  = (uint64_t)word[0];
                        break;
                case M_UINT_16:
                        READ(src, word, 2);
                        value  = (uint64_t)decode_int16(word);
                        break;
                case M_UINT_32:
                        READ(src, word, 4);
                        value  = (uint64_t)decode_int32(word);
                        break;
                case M_UINT_64:
                        READ(src, word, 8);
                        value  = (uint64_t)decode_int64(word);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags    = MPACK_NUM | MPACK_ENCODE;
        item->data.num = (int64_t)value;

        LOG("It is an uint32_t int32_t! -> 0x%0X : value => %u\n", byte, value);

        return item;
}


static mpack_obj *
decode_ext(const read_fn READ, void *src, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   value   = 0;
        uint8_t    word[4] = {0, 0, 0, 0};
        uint8_t    type;

        switch (mask->type) {
        case M_EXT_F1:
                READ(src, &type, 1);
                READ(src, word, 1);
                value = (uint32_t)word[0];
                break;
        case M_EXT_F2:
                READ(src, &type, 1);
                READ(src, word, 2);
                value = (uint32_t)decode_int16(word);
                break;
        case M_EXT_F4:
                READ(src, &type, 1);
                READ(src, word, 4);
                value = (uint32_t)decode_int32(word);
                break;
        default:
                ERRMSG();
        }

        item->flags          = MPACK_EXT | MPACK_ENCODE;
        item->data.ext       = xmalloc(sizeof(mpack_ext_t));
        item->data.ext->type = type;
        item->data.ext->num  = value;

        LOG("It is an extention thingy! -> : data => %d, %d\n",
            type, value);

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


static mpack_obj *
decode_nil(void)
{
        mpack_obj *item = xmalloc(sizeof *item);
        item->flags     = MPACK_NIL | MPACK_ENCODE;
        item->data.nil  = M_MASK_NIL;

        return item;
}


/*============================================================================*/


static const struct mpack_masks *
id_pack_type(const uint8_t byte)
{
        const struct mpack_masks *mask = NULL;

        for (unsigned i = 0; i < m_masks_len; ++i) {
                if (m_masks[i].fixed) {
                        if ((byte >> m_masks[i].shift) ==
                            (m_masks[i].val >> m_masks[i].shift))
                        {
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


static void
stream_read(void *restrict src, uint8_t *restrict dest, const size_t nbytes)
{
        const int32_t fd    = *((int32_t *)src);
        size_t        nread = 0;

        do {
                const ssize_t n = read(fd, dest, (nbytes - nread));
                if (n != (-1ll))
                        nread += (size_t)n;
        } while (nread > nbytes);

#ifdef DEBUG
        UNUSED ssize_t n = write(decode_log_raw, dest, nbytes);
#endif
}


static void
obj_read(void *restrict src, uint8_t *restrict dest, const size_t nbytes)
{
        bstring *buf = src;
        for (unsigned i = 0; i < nbytes; ++i)
                dest[i] = *buf->data++;
        buf->slen -= nbytes;
}
