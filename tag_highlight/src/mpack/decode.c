#include "Common.h"
#include <stddef.h>

#include "intern.h"
#include "mpack.h"

typedef void (*read_fn)(void *restrict src, uint8_t *restrict dest, size_t nbytes);

static mpack_obj * do_decode        (read_fn READ, void *src);
static mpack_obj * decode_array     (read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj * decode_string    (read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj * decode_dictionary(read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj * decode_integer   (read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj * decode_unsigned  (read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj * decode_ext       (read_fn READ, void *src, mpack_mask const *mask);
static mpack_obj * decode_nil       (void);
static mpack_obj * decode_bool      (mpack_mask const *mask);
static void        stream_read      (void *restrict src, uint8_t *restrict dest, size_t nbytes);
static void        obj_read         (void *restrict src, uint8_t *restrict dest, size_t nbytes);

static mpack_mask const *id_pack_type(uint8_t ch) __attribute__((pure));

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

extern FILE *mpack_raw;
FILE *mpack_raw;
static pthread_mutex_t mpack_search_mutex = PTHREAD_MUTEX_INITIALIZER;

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

        pthread_mutex_lock(mut);
        pthread_mutex_unlock(&mpack_search_mutex);

        mpack_obj *ret = do_decode(&stream_read, &fd);

        if (!ret)
                errx(1, "Failed to decode stream.");
        if (mpack_type(ret) != MPACK_ARRAY) {
                if (mpack_log) {
                        mpack_print_object(mpack_log, ret);
                        fflush(mpack_log);
                }
                errx(1, "For some incomprehensible reason the pack's type is %d.\n",
                     mpack_type(ret));
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
                errx(1, "For some incomprehensible reason the pack's type is %d.\n",
                     mpack_type(ret));
        }
        return ret;
}


/*============================================================================*/


static mpack_obj *
do_decode(read_fn const READ, void *src)
{
        uint8_t ch;
        READ(src, &ch, 1);
        mpack_mask const *mask = id_pack_type(ch);

        switch (mask->group) {
        case G_ARRAY:  return decode_array(READ, src, ch, mask);
        case G_MAP:    return decode_dictionary(READ, src, ch, mask);
        case G_STRING: return decode_string(READ, src, ch, mask);
        case G_NLINT:
        case G_INT:    return decode_integer(READ, src, ch, mask);
        case G_PLINT:
        case G_UINT:   return decode_unsigned(READ, src, ch, mask);
        case G_EXT:    return decode_ext(READ, src, mask);
        case G_BOOL:   return decode_bool(mask);
        case G_NIL:    return decode_nil();

        case G_BIN:    errx(2, "ERROR: Bin format is not implemented.");
        default:       errx(3, "Default (%d) reached at %d in %s - %s",
                            mask->group, __LINE__, __FILE__, FUNC_NAME);
        }
}


static mpack_obj *
decode_array(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
        mpack_obj *item    = malloc(sizeof *item);
        uint32_t   size    = 0;

        if (mask->fixed) {
                size = (uint32_t)(ch ^ mask->val);
        } else {
                uint8_t word32[4] = {0, 0, 0, 0};

                switch (mask->type) {
                case M_ARRAY_16:
                        READ(src, word32, 2);
                        size = decode_int16(word32);
                        break;
                case M_ARRAY_32:
                        READ(src, word32, 4);
                        size = decode_int32(word32);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags           = MPACK_ARRAY | MPACKFLG_ENCODE;
        item->data.arr        = malloc(sizeof(mpack_array));
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
decode_dictionary(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
        mpack_obj *item = malloc(sizeof *item);
        uint32_t   size = 0;


        if (mask->fixed) {
                size = (uint32_t)(ch ^ mask->val);
        } else {
                uint8_t word32[4] = {0, 0, 0, 0};

                switch (mask->type) {
                case M_MAP_16:
                        READ(src, word32, 2);
                        size = decode_int16(word32);
                        break;
                case M_MAP_32:
                        READ(src, word32, 4);
                        size = decode_int32(word32);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags              = MPACK_DICT | MPACKFLG_ENCODE;
        item->data.dict          = malloc(sizeof(mpack_dict));
        item->data.dict->entries = nmalloc(size, sizeof(mpack_dict_ent *));
        item->data.dict->qty     = item->data.dict->max = size;

        for (uint32_t i = 0; i < item->data.arr->max; ++i) {
                item->DDE[i]        = malloc(sizeof(mpack_dict_ent));
                item->DDE[i]->key   = do_decode(READ, src);
                item->DDE[i]->value = do_decode(READ, src);
        }

        return item;
}


static mpack_obj *
decode_string(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
        mpack_obj *item    = malloc(sizeof *item);
        uint32_t   size    = 0;

        if (mask->fixed) {
                size = (uint32_t)(ch ^ mask->val);
        } else {
                uint8_t word32[4] = {0, 0, 0, 0};

                switch (mask->type) {
                case M_STR_8:
                        READ(src, word32, 1);
                        size = (uint32_t)word32[0];
                        break;
                case M_STR_16:
                        READ(src, word32, 2);
                        size = decode_int16(word32);
                        break;
                case M_STR_32:
                        READ(src, word32, 4);
                        size = decode_int32(word32);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags          = MPACK_STRING | MPACKFLG_ENCODE;
        item->data.str       = b_alloc_null(size + 1);
        item->data.str->slen = size;

        if (size > 0)
                READ(src, item->data.str->data, size);

        item->data.str->data[size] = (uchar)'\0';

        return item;
}


static mpack_obj *
decode_integer(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
        mpack_obj *item    = malloc(sizeof *item);
        int64_t    value   = 0;

        if (mask->fixed) {
                value  = (int64_t)(ch ^ mask->val);
                value |= 0xFFFFFFFFFFFFFFE0LLU;
        } else {
                uint8_t word64[8] = {0, 0, 0, 0, 0, 0, 0, 0};

                switch (mask->type) {
                case M_INT_8:
                        READ(src, word64, 1);
                        value  = (int64_t)word64[0];
                        value |= 0xFFFFFFFFFFFFFF00LLU;
                        break;
                case M_INT_16:
                        READ(src, word64, 2);
                        value  = (int64_t)decode_int16(word64);
                        value |= 0xFFFFFFFFFFFF0000LLU;
                        break;
                case M_INT_32:
                        READ(src, word64, 4);
                        value  = (int64_t)decode_int32(word64);
                        value |= 0xFFFFFFFF00000000LLU;
                        break;
                case M_INT_64:
                        READ(src, word64, 8);
                        value  = (int64_t)decode_int64(word64);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags     = MPACK_SIGNED | MPACKFLG_ENCODE;
        item->data.num = value;

        return item;
}


static mpack_obj *
decode_unsigned(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
        mpack_obj *item    = malloc(sizeof *item);
        uint64_t   value   = 0;

        if (mask->fixed) {
                value = (uint64_t)(ch ^ mask->val);
        } else {
                uint8_t word64[8] = {0, 0, 0, 0, 0, 0, 0, 0};

                switch (mask->type) {
                case M_UINT_8:
                        READ(src, word64, 1);
                        value  = (uint64_t)word64[0];
                        break;
                case M_UINT_16:
                        READ(src, word64, 2);
                        value  = (uint64_t)decode_int16(word64);
                        break;
                case M_UINT_32:
                        READ(src, word64, 4);
                        value  = (uint64_t)decode_int32(word64);
                        break;
                case M_UINT_64:
                        READ(src, word64, 8);
                        value  = (uint64_t)decode_int64(word64);
                        break;
                default:
                        ERRMSG();
                }
        }

        item->flags    = MPACK_UNSIGNED | MPACKFLG_ENCODE;
        item->data.num = value;

        return item;
}


static mpack_obj *
decode_ext(read_fn const READ, void *src, mpack_mask const *mask)
{
        mpack_obj *item    = malloc(sizeof *item);
        uint32_t   value   = 0;
        uint8_t    word32[4] = {0, 0, 0, 0};
        uint8_t    type;

        switch (mask->type) {
        case M_EXT_F1:
                READ(src, &type, 1);
                READ(src, word32, 1);
                value = (uint32_t)word32[0];
                break;
        case M_EXT_F2:
                READ(src, &type, 1);
                READ(src, word32, 2);
                value = (uint32_t)decode_int16(word32);
                break;
        case M_EXT_F4:
                READ(src, &type, 1);
                READ(src, word32, 4);
                value = (uint32_t)decode_int32(word32);
                break;
        default:
                ERRMSG();
        }

        item->flags          = MPACK_EXT | MPACKFLG_ENCODE;
        item->data.ext       = malloc(sizeof(mpack_ext));
        item->data.ext->type = type;
        item->data.ext->num  = value;

        return item;
}


static mpack_obj *
decode_bool(mpack_mask const *mask)
{
        mpack_obj *item = malloc(sizeof *item);
        item->flags     = MPACK_BOOL | MPACKFLG_ENCODE;

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
        mpack_obj *item = malloc(sizeof *item);
        item->flags     = MPACK_NIL | MPACKFLG_ENCODE;
        item->data.nil  = M_MASK_NIL;

        return item;
}


/*============================================================================*/


static const mpack_mask *
id_pack_type(uint8_t const ch)
{
        mpack_mask const *mask = NULL;

        for (unsigned i = 0; i < m_masks_len; ++i) {
                mpack_mask const *m = &m_masks[i];

                if (m->fixed) {
                        if ((ch >> m->shift) == (m->val >> m->shift)) {
                                mask = m;
                                break;
                        }
                } else if (ch == m->val) {
                        mask = m;
                        break;
                }
        }

        if (!mask)
                errx(1, "Failed to identify type for byte 0x%0X.", ch);

        return mask;
}


static void
stream_read(void *restrict src, uint8_t *restrict dest, size_t const nbytes)
{
        int const fd = *((int *)src);

#ifdef DOSISH
        size_t nread = 0;
        while (nread < nbytes) {
                int n = read(fd, dest, nbytes - nread);
                if (n < 0)
                        err(1, "read() error");
                nread += (size_t)n;
        }
#else
        int const n = recv(fd, dest, nbytes, MSG_WAITALL);
        if (n < 0)
                err(1, "recv() error");
        if ((size_t)n != nbytes)
                err(1, "recv() returned too few bytes (%d != %zu)!", n, nbytes);
#endif

#if defined DEBUG && defined DEBUG_LOGS
        fwrite(dest, 1, nbytes, mpack_raw);
#endif
}


static void
obj_read(void *restrict src, uint8_t *restrict dest, size_t const nbytes)
{
        bstring *buf = src;
        for (unsigned i = 0; i < nbytes; ++i)
                dest[i] = *buf->data++;
        buf->slen -= nbytes;
}
