#include "Common.h"
#include <stddef.h>

#include "intern.h"
#include "mpack.h"

extern FILE *mpack_raw;
FILE *mpack_raw;
static pthread_mutex_t mpack_search_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef _MSC_VER
#  define restrict __restrict
#endif
#define INIT_MUTEXES (16)

typedef void (*read_fn)(void *restrict src, uint8_t *restrict dest, size_t nbytes);

static mpack_obj * do_decode        (read_fn READ, void *src);
static mpack_obj * decode_array     (read_fn READ, void *src, uint8_t byte, const mpack_mask *mask);
static mpack_obj * decode_string    (read_fn READ, void *src, uint8_t byte, const mpack_mask *mask);
static mpack_obj * decode_dictionary(read_fn READ, void *src, uint8_t byte, const mpack_mask *mask);
static mpack_obj * decode_integer   (read_fn READ, void *src, uint8_t byte, const mpack_mask *mask);
static mpack_obj * decode_unsigned  (read_fn READ, void *src, uint8_t byte, const mpack_mask *mask);
static mpack_obj * decode_ext       (read_fn READ, void *src, const mpack_mask *mask);
static mpack_obj * decode_nil       (void);
static mpack_obj * decode_bool      (const mpack_mask *mask);
static void        stream_read      (void *restrict src, uint8_t *restrict dest, size_t nbytes);
static void        obj_read         (void *restrict src, uint8_t *restrict dest, size_t nbytes);
static const mpack_mask *id_pack_type(uint8_t byte);


#define IAT(NUM, AT) ((uint64_t)((NUM)[AT]))

#define decode_int16(NUM)                                  \
        ((((NUM)[0]) << 010U) | ((NUM)[1]))
#define decode_int32(NUM)                                  \
        ((((NUM)[0]) << 030U) | (((NUM)[1]) << 020U) |     \
         (((NUM)[2]) << 010U) | (((NUM)[3])))
#define decode_int64(NUM)                                  \
        ((IAT(NUM, 0) << 070U) | (IAT(NUM, 1) << 060U) |   \
         (IAT(NUM, 2) << 050U) | (IAT(NUM, 3) << 040U) |   \
         (IAT(NUM, 4) << 030U) | (IAT(NUM, 5) << 020U) |   \
         (IAT(NUM, 6) << 010U) | (IAT(NUM, 7)))

#define ERRMSG()                                                                         \
        errx(1, "Default (%d -> \"%s\") reached on line %d of file %s, in function %s.", \
             mask->type, mask->repr, __LINE__, __FILE__, FUNC_NAME)

#define NUM_MUTEXES (128)
P99_DECLARE_STRUCT(mpack_mutex);
struct mpack_mutex {
        int             fd;
        bool            init;
        pthread_mutex_t mut;
};
static mpack_mutex mpack_mutex_list[NUM_MUTEXES];


/*============================================================================*/


mpack_obj *
mpack_decode_stream(int32_t fd)
{
        pthread_mutex_lock(&mpack_search_mutex);
        pthread_mutex_t *mut = NULL;
        if (fd == 1)
                fd = 0;

        for (unsigned i = 0; i < NUM_MUTEXES; ++i) {
                mpack_mutex *cur = &mpack_mutex_list[i];
                if (cur->init && cur->fd == fd) {
                        mut = &cur->mut;
                        break;
                }
        }
        if (!mut) {
                unsigned i = 0;
                while (i < NUM_MUTEXES && mpack_mutex_list[i].init)
                        ++i;
                if (i == NUM_MUTEXES)
                        errx(1, "Too many open file descriptors.");
                mpack_mutex_list[i].init = true;
                mpack_mutex_list[i].fd   = fd;
                mut = &mpack_mutex_list[i].mut;
                pthread_mutex_init(mut, NULL);
        }
        pthread_mutex_unlock(&mpack_search_mutex);

        pthread_mutex_lock(mut);
        mpack_obj *ret = do_decode(&stream_read, &fd);

        if (!ret)
                errx(1, "Failed to decode stream.");
        if (mpack_type(ret) != MPACK_ARRAY) {
                if (mpack_log) {
                        mpack_print_object(mpack_log, ret);
                        fflush(mpack_log);
                }
                eprintf("For some incomprehensible reason the pack's type is \"%s\" (%d).\n",
                        m_type_names[mpack_type(ret)], mpack_type(ret));
                abort();
        }
        pthread_mutex_unlock(mut);

        return ret;
}


mpack_obj *
mpack_decode_obj(bstring *buf)
{
        mpack_obj *ret = do_decode(&obj_read, buf);

        if (!ret)
                errx(1, "Failed to decode stream.");
        if (mpack_type(ret) != MPACK_ARRAY) {
                if (mpack_log) {
                        mpack_print_object(mpack_log, ret);
                        fflush(mpack_log);
                }
                eprintf("For some incomprehensible reason the pack's type is %d.\n",
                        mpack_type(ret));
                abort();
        }
        return ret;
}


/*============================================================================*/


static mpack_obj *
do_decode(const read_fn READ, void *src)
{
        uint8_t byte;
        READ(src, &byte, 1);
        const mpack_mask *mask = id_pack_type(byte);

        switch (mask->group) {
        case G_ARRAY:  return decode_array(READ, src, byte, mask);
        case G_MAP:    return decode_dictionary(READ, src, byte, mask);
        case G_STRING: return decode_string(READ, src, byte, mask);
        case G_NLINT:
        case G_INT:    return decode_integer(READ, src, byte, mask);
        case G_PLINT:
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
decode_array(const read_fn READ, void *src, const uint8_t byte, const mpack_mask *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;

        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                uint8_t word[4] = {0, 0, 0, 0};

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

        for (unsigned i = 0; i < item->data.arr->max; ++i) {
                mpack_obj *tmp = do_decode(READ, src);
                item->DAI[item->data.arr->qty++] = tmp;
        }

        return item;
}


static mpack_obj *
decode_dictionary(const read_fn READ, void *src, const uint8_t byte, const mpack_mask *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;


        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                uint8_t word[4] = {0, 0, 0, 0};

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
        item->data.dict          = xmalloc(sizeof(mpack_dict_t));
        item->data.dict->entries = nmalloc(size, sizeof(struct dict_ent *));
        item->data.dict->qty     = item->data.dict->max = size;

        for (uint32_t i = 0; i < item->data.arr->max; ++i) {
                item->DDE[i]        = xmalloc(sizeof(struct dict_ent));
                item->DDE[i]->key   = do_decode(READ, src);
                item->DDE[i]->value = do_decode(READ, src);
        }

        return item;
}


static mpack_obj *
decode_string(const read_fn READ, void *src, const uint8_t byte, const mpack_mask *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;

        if (mask->fixed) {
                size = (uint32_t)(byte ^ mask->val);
        } else {
                uint8_t word[4] = {0, 0, 0, 0};

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

        item->flags          = MPACK_STRING | MPACK_ENCODE;
        item->data.str       = b_alloc_null(size + 1);
        item->data.str->slen = size;

        if (size > 0)
                READ(src, item->data.str->data, size);

        item->data.str->data[size] = (uchar)'\0';

        return item;
}


static mpack_obj *
decode_integer(const read_fn READ, void *src, const uint8_t byte, const mpack_mask *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        int64_t    value   = 0;

        if (mask->fixed) {
                value  = (int64_t)(byte ^ mask->val);
                value |= 0xFFFFFFFFFFFFFFE0LLU;
        } else {
                uint8_t word[8] = {0, 0, 0, 0, 0, 0, 0, 0};

                switch (mask->type) {
                case M_INT_8:
                        READ(src, word, 1);
                        value  = (int64_t)word[0];
                        value |= 0xFFFFFFFFFFFFFF00LLU;
                        break;
                case M_INT_16:
                        READ(src, word, 2);
                        value  = (int64_t)decode_int16(word);
                        value |= 0xFFFFFFFFFFFF0000LLU;
                        break;
                case M_INT_32:
                        READ(src, word, 4);
                        value  = (int64_t)decode_int32(word);
                        value |= 0xFFFFFFFF00000000LLU;
                        break;
                case M_INT_64:
                        READ(src, word, 8);
                        value  = (int64_t)decode_int64(word);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags     = MPACK_SIGNED | MPACK_ENCODE;
        item->data.num = value;

        return item;
}


static mpack_obj *
decode_unsigned(const read_fn READ, void *src, const uint8_t byte, const mpack_mask *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint64_t   value   = 0;

        if (mask->fixed) {
                value = (uint64_t)(byte ^ mask->val);
        } else {
                uint8_t word[8] = {0, 0, 0, 0, 0, 0, 0, 0};

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

        item->flags    = MPACK_UNSIGNED | MPACK_ENCODE;
        item->data.num = value;

        return item;
}


static mpack_obj *
decode_ext(const read_fn READ, void *src, const mpack_mask *mask)
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

        return item;
}


static mpack_obj *
decode_bool(const mpack_mask *mask)
{
        mpack_obj *item = xmalloc(sizeof *item);
        item->flags     = MPACK_BOOL | MPACK_ENCODE;

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


static const mpack_mask *
id_pack_type(const uint8_t byte)
{
        const mpack_mask *mask = NULL;

        for (unsigned i = 0; i < m_masks_len; ++i) {
                const mpack_mask *m = &m_masks[i];

                if (m->fixed) {
                        if ((byte >> m->shift) == (m->val >> m->shift)) {
                                mask = m;
                                break;
                        }
                } else if (byte == m->val) {
                        mask = m;
                        break;
                }
        }

        if (!mask)
                errx(1, "Failed to identify type for byte 0x%0X.", byte);

        return mask;
}


static void
stream_read(void *restrict src, uint8_t *restrict dest, const size_t nbytes)
{
        const int fd = *((int *)src);

#ifdef DOSISH
        size_t nread = 0;
        while (nread < nbytes) {
                int n = read(fd, dest, nbytes - nread);
                nread += (n>0)?n:0;
        }
#else
        errno = 0;
        const int n = recv(fd, dest, nbytes, MSG_WAITALL);
        if ((size_t)n != nbytes)
                err(1, "%d != %zu!", n, nbytes);
        /* assert((size_t)n == nbytes); */
#endif

#ifdef DEBUG
        if (mpack_raw)
                fwrite(dest, 1, nbytes, mpack_raw);
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
