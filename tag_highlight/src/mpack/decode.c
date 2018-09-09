#include "util/util.h"
#include <stddef.h>
#include <sys/socket.h>

#include "intern.h"
#include "mpack.h"

extern int decode_log_raw;
extern FILE *decodelog;

static genlist *       mpack_mutex_list = NULL;
static pthread_mutex_t mpack_search_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef _MSC_VER
#  define restrict __restrict
#endif
#define INIT_MUTEXES (16)

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
static void free_mutexes(void);


#define IAT(NUM, AT) ((uint64_t)((NUM)[AT]))

#define decode_int16(NUM)                                \
        ((((NUM)[0]) << 010) | ((NUM)[1]))
#define decode_int32(NUM)                                \
        ((((NUM)[0]) << 030) | (((NUM)[1]) << 020) |     \
         (((NUM)[2]) << 010) | (((NUM)[3])))
#define decode_int64(NUM)                                \
        ((IAT(NUM, 0) << 070) | (IAT(NUM, 1) << 060) |   \
         (IAT(NUM, 2) << 050) | (IAT(NUM, 3) << 040) |   \
         (IAT(NUM, 4) << 030) | (IAT(NUM, 5) << 020) |   \
         (IAT(NUM, 6) << 010) | (IAT(NUM, 7)))

#define ERRMSG()                                                                         \
        errx(1, "Default (%d -> \"%s\") reached on line %d of file %s, in function %s.", \
             mask->type, mask->repr, __LINE__, __FILE__, FUNC_NAME)

struct mpack_mutex {
        int             fd;
        pthread_mutex_t mut;
};


/*============================================================================*/


mpack_obj *
decode_stream(int32_t fd)
{
        pthread_mutex_lock(&mpack_search_mutex);
        pthread_mutex_t *mut = NULL;
        if (!mpack_mutex_list || !mpack_mutex_list->lst) {
                mpack_mutex_list = genlist_create_alloc(INIT_MUTEXES);
                atexit(free_mutexes);
        }

        if (fd == 1)
                fd = 0;

        for (unsigned i = 0; i < mpack_mutex_list->qty; ++i) {
                struct mpack_mutex *cur = mpack_mutex_list->lst[i];
                if (cur->fd == fd) {
                        mut = &cur->mut;
                        break;
                }
        }
        if (!mut) {
                struct mpack_mutex *tmp = xmalloc(sizeof(struct mpack_mutex));
                tmp->fd  = fd;
                /* tmp->mut = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER; */
                pthread_mutex_init(&tmp->mut, NULL);
                genlist_append(mpack_mutex_list, tmp);
                mut = &tmp->mut;
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
                eprintf("For some incomprehensible reason the pack's type is %d.\n",
                        mpack_type(ret));
                abort();
        }
        pthread_mutex_unlock(mut);

        return ret;
}


mpack_obj *
decode_obj(bstring *buf)
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
#if 0
        mpack_obj *ret = do_decode(&obj_read, buf);

        if (!ret)
                errx(1, "Failed to decode stream.");
        if (mpack_type(ret) != MPACK_ARRAY) {
                eprintf("For some incomprehensible reason the pack's type is %d.\n",
                        mpack_type(ret));
                if (mpack_log) {
                        mpack_print_object(mpack_log, ret);
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
                default:               abort();
                case MES_REQUEST:      errx(1, "This will NEVER happen.");
                case MES_RESPONSE:     mpack_destroy(ret); return NULL;
                case MES_NOTIFICATION: handle_unexpected_notification(ret);
                }

                return decode_obj(buf, expected_type);
        }
#endif

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
        uint8_t    word[4] = {0, 0, 0, 0};


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
decode_string(const read_fn READ, void *src, const uint8_t byte, const struct mpack_masks *mask)
{
        mpack_obj *item    = xmalloc(sizeof *item);
        uint32_t   size    = 0;
        uint8_t    word[4] = {0, 0, 0, 0};

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
                value  = (int64_t)(byte ^ mask->val);
                value |= 0xFFFFFFFFFFFFFFE0llu;
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

        item->flags    = MPACK_NUM | MPACK_ENCODE;
        item->data.num = value;

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

        return item;
}


static mpack_obj *
decode_bool(const struct mpack_masks *mask)
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


static const struct mpack_masks *
id_pack_type(const uint8_t byte)
{
        const struct mpack_masks *mask = NULL;

        for (unsigned i = 0; i < m_masks_len; ++i) {
                const struct mpack_masks *m = &m_masks[i];

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
        do {
                const size_t n = read(fd, dest, (nbytes - nread));
                if (n > 0)
                        nread += n;
        } while (nread > nbytes);
#else
        const ssize_t n = recv(fd, dest, nbytes, MSG_WAITALL);
        assert((size_t)n == nbytes);
#endif

#ifdef MPACK_RAW
        UNUSED ssize_t m = write(decode_log_raw, dest, nbytes);
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


static void
free_mutexes(void)
{
        if (mpack_mutex_list)
                genlist_destroy(mpack_mutex_list);
}
