#include "Common.h"
#include <stddef.h>

#include "intern.h"
#include "mpack.h"

typedef void (*read_fn)(void *restrict src, void *restrict dest, size_t nbytes);

static mpack_obj *do_decode(read_fn READ, void *src);
static mpack_obj *decode_array(read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj *decode_string(read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj *decode_dictionary(read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj *decode_integer(read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj *decode_unsigned(read_fn READ, void *src, uint8_t ch, mpack_mask const *mask);
static mpack_obj *decode_ext(read_fn READ, void *src, mpack_mask const *mask);
static mpack_obj *decode_nil(void);
static mpack_obj *decode_bool(mpack_mask const *mask);

static void stream_read(void *restrict src, void *restrict dest, size_t nbytes);
static void obj_read(void *restrict src, void *restrict dest, size_t nbytes);

static mpack_mask const *id_pack_type(uint8_t ch) __attribute__((pure));

extern FILE *mpack_raw_read;

/*----------------------------------------------------------------------------*/

#if __has_builtin(__builtin_expect)
#  define likely(x)   __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#else
#  define likely(x)   (x)
#  define unlikely(x) (x)
#endif

#undef talloc
#define talloc(ctx, type) \
      (type *)talloc_named_const(ctx, sizeof(type), __location__ " - " #type)
#undef talloc_array
#define talloc_array(ctx, type, count) \
      (type *)_talloc_array(ctx, sizeof(type), count, __location__ " - " #type)

#define ERRMSG()                                                                       \
      errx(1, "Default (%d -> \"%s\") reached on line %d of file %s, in function %s.", \
           mask->type, mask->repr, __LINE__, __FILE__, FUNC_NAME)

#define CTX mpack_decode_talloc_ctx_

#define decode_int16(NUM) MY_BSWAP_16(NUM)
#define decode_int32(NUM) MY_BSWAP_32(NUM)
#define decode_int64(NUM) MY_BSWAP_64(NUM)
#define NUM_MUTEXES (128)

P99_DECLARE_STRUCT(mpack_mutex);
struct mpack_mutex {
      intptr_t        fd;
      bool            init;
      pthread_mutex_t mut;
} __attribute__((aligned(64)));

void *mpack_decode_talloc_ctx_ = NULL;

static pthread_mutex_t mpack_search_mutex = PTHREAD_MUTEX_INITIALIZER;
static mpack_mutex     mpack_mutex_list[NUM_MUTEXES];

/*============================================================================*/

mpack_obj *
mpack_decode_stream(intptr_t fd)
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
            if (i >= NUM_MUTEXES)
                  errx(1, "Too many open file descriptors.");
            mpack_mutex_list[i].init = true;
            mpack_mutex_list[i].fd   = fd;
            mut                      = &mpack_mutex_list[i].mut;
            pthread_mutex_init(mut, NULL);
      }

      pthread_mutex_lock(mut);
      pthread_mutex_unlock(&mpack_search_mutex);

      mpack_obj *ret = do_decode(&stream_read, &fd);
      if (!ret)
            errx(1, "Failed to decode stream.");

      if (unlikely(mpack_type(ret) != MPACK_ARRAY)) {
            if (mpack_log) {
                  mpack_print_object(mpack_log, ret, B("\033[1;31mERROR: MESSAGE INVALID\033[0m"));
                  fflush(mpack_log);
            }
            errx(1, "For some incomprehensible reason the pack's type is %d.\n",
                 mpack_type(ret));
      }
#if defined DEBUG && defined DEBUG_LOGS
      mpack_print_object(mpack_log, ret, B("\033[1;32mDECODED MESSAGE\033[0m"));
#endif
      pthread_mutex_unlock(mut);

      return ret;
}


mpack_obj *
mpack_decode_obj(bstring *buf)
{
      mpack_obj *ret = do_decode(&obj_read, buf);
      if (!ret)
            errx(1, "Failed to decode stream.");

      if (unlikely(mpack_type(ret) != MPACK_ARRAY)) {
            if (mpack_log) {
                  mpack_print_object(mpack_log, ret, B("\033[1;31mERROR: MESSAGE INVALID\033[0m"));
                  fflush(mpack_log);
            }
            errx(1, "For some incomprehensible reason the pack's type is %d.\n",
                 mpack_type(ret));
      }
#if defined DEBUG && defined DEBUG_LOGS
      mpack_print_object(mpack_log, ret, B("\033[1;32mDECODED MESSAGE\033[0m"));
#endif
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
      case G_NIL:    return decode_nil();
      case G_BOOL:   return decode_bool(mask);
      case G_ARRAY:  return decode_array(READ, src, ch, mask);
      case G_MAP:    return decode_dictionary(READ, src, ch, mask);
      case G_STRING: return decode_string(READ, src, ch, mask);
      case G_EXT:    return decode_ext(READ, src, mask);
      case G_NLINT:
      case G_INT:    return decode_integer(READ, src, ch, mask);
      case G_PLINT:
      case G_UINT:   return decode_unsigned(READ, src, ch, mask);

      case G_BIN:
            errx(2, "ERROR: Bin format is not implemented.");
      default:
            errx(3, "Default (%d) reached at %d in %s - %s", mask->group, __LINE__,
                 __FILE__, FUNC_NAME);
      }
}


static mpack_obj *
decode_array(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
      mpack_obj *item = talloc(CTX, mpack_obj);
      uint32_t   size = 0;

      if (mask->fixed) {
            size = (uint32_t)(ch ^ mask->val);
      } else {
            switch (mask->type) {
            case M_ARRAY_16: {
                  uint16_t tmp;
                  READ(src, &tmp, 2);
                  size = decode_int16(tmp);
                  break;
            }
            case M_ARRAY_32: {
                  uint32_t tmp;
                  READ(src, &tmp, 4);
                  size = decode_int32(tmp);
                  break;
            }
            default:
                  ERRMSG();
            }
      }

      item->flags    = MPACK_ARRAY | MPACKFLG_ENCODE;
      item->arr      = talloc(item, mpack_array);
      item->arr->lst = talloc_array(item->arr, mpack_obj *, size);
      item->arr->max = size;
      item->arr->qty = 0;

      for (unsigned i = 0; i < item->arr->max; ++i) {
            mpack_obj *tmp                   = do_decode(READ, src);
            item->arr->lst[item->arr->qty++] = talloc_move(item->arr->lst, &tmp);
      }

      return item;
}


static mpack_obj *
decode_dictionary(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
      mpack_obj *item = talloc(CTX, mpack_obj);
      uint32_t   size = 0;


      if (mask->fixed) {
            size = (uint32_t)(ch ^ mask->val);
      } else {
            switch (mask->type) {
            case M_MAP_16: {
                  uint16_t tmp;
                  READ(src, &tmp, 2);
                  size = decode_int16(tmp);
                  break;
            }
            case M_MAP_32: {
                  uint32_t tmp;
                  READ(src, &tmp, 4);
                  size = decode_int32(tmp);
                  break;
            }
            default:
                  ERRMSG();
            }
      }

      item->flags     = MPACK_DICT | MPACKFLG_ENCODE;
      item->dict      = talloc(item, mpack_dict);
      item->dict->lst = talloc_array(item->dict, mpack_dict_ent *, size);
      item->dict->qty = item->dict->max = size;

#define ENTRY (item->dict->lst[i])
      for (uint32_t i = 0; i < item->arr->max; ++i) {
            ENTRY        = talloc(item->dict->lst, mpack_dict_ent);
            ENTRY->key   = do_decode(READ, src);
            ENTRY->value = do_decode(READ, src);
            talloc_steal(ENTRY, ENTRY->key);
            talloc_steal(ENTRY, ENTRY->value);
      }
#undef ENTRY

      return item;
}


static mpack_obj *
decode_string(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
      mpack_obj *item = talloc(CTX, mpack_obj);
      uint32_t   size = 0;

      if (mask->fixed) {
            size = (uint32_t)(ch ^ mask->val);
      } else {
            switch (mask->type) {
            case M_STR_8: {
                  uint8_t tmp;
                  READ(src, &tmp, 1);
                  size = (uint32_t)tmp;
                  break;
            }
            case M_STR_16: {
                  uint16_t tmp;
                  READ(src, &tmp, 2);
                  size = decode_int16(tmp);
                  break;
            }
            case M_STR_32: {
                  uint32_t tmp;
                  READ(src, &tmp, 4);
                  size = decode_int32(tmp);
                  break;
            }
            default:
                  ERRMSG();
            }
      }

      item->flags     = MPACK_STRING | MPACKFLG_ENCODE;
      item->str       = b_alloc_null(size + 1);
      item->str->slen = size;
      talloc_steal(item, item->str);

      if (size > 0)
            READ(src, item->str->data, size);

      item->str->data[size] = (uchar)'\0';

      return item;
}


static mpack_obj *
decode_integer(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
      mpack_obj *item  = talloc(CTX, mpack_obj);
      int64_t    value = 0;

      if (mask->fixed) {
            value = (int64_t)(ch ^ mask->val);
            value = (int64_t)((uint64_t)value | 0xFFFFFFFFFFFFFFE0LLU);
      } else {
            switch (mask->type) {
            case M_INT_8: {
                  uint8_t tmp;
                  READ(src, &tmp, 1);
                  value = (int64_t)((uint64_t)value | 0xFFFFFFFFFFFFFF00LLU);
                  break;
            }
            case M_INT_16: {
                  uint16_t tmp;
                  READ(src, &tmp, 2);
                  value = (int64_t)decode_int16(tmp);
                  value = (int64_t)((uint64_t)value | 0xFFFFFFFFFFFF0000LLU);
                  break;
            }
            case M_INT_32: {
                  uint32_t tmp;
                  READ(src, &tmp, 4);
                  value = (int64_t)decode_int32(tmp);
                  value = (int64_t)((uint64_t)value | 0xFFFFFFFF00000000LLU);
                  break;
            }
            case M_INT_64: {
                  uint64_t tmp;
                  READ(src, &tmp, 8);
                  value = (int64_t)decode_int64(tmp);
                  break;
            }
            default:
                  ERRMSG();
            }
      }

      item->flags = MPACK_SIGNED | MPACKFLG_ENCODE;
      item->num   = value;

      return item;
}


static mpack_obj *
decode_unsigned(read_fn const READ, void *src, uint8_t const ch, mpack_mask const *mask)
{
      mpack_obj *item  = talloc(CTX, mpack_obj);
      uint64_t   value = 0;

      if (mask->fixed) {
            value = (uint64_t)(ch ^ mask->val);
      } else {
            switch (mask->type) {
            case M_UINT_8: {
                  uint8_t tmp;
                  READ(src, &tmp, 1);
                  value = (uint64_t)tmp;
                  break;
            }
            case M_UINT_16: {
                  uint16_t tmp;
                  READ(src, &tmp, 2);
                  value = (uint64_t)decode_int16(tmp);
                  break;
            }
            case M_UINT_32: {
                  uint32_t tmp;
                  READ(src, &tmp, 4);
                  value = (uint64_t)decode_int32(tmp);
                  break;
            }
            case M_UINT_64: {
                  uint64_t tmp;
                  READ(src, &tmp, 8);
                  value = (uint64_t)decode_int64(tmp);
                  break;
            }
            default:
                  ERRMSG();
            }
      }

      item->flags = MPACK_UNSIGNED | MPACKFLG_ENCODE;
      item->num   = value;

      return item;
}

static mpack_obj *
decode_ext(read_fn const READ, void *src, mpack_mask const *mask)
{
      mpack_obj *item = talloc(CTX, mpack_obj);
      item->ext       = talloc_zero(item, mpack_ext);
      item->flags     = MPACK_EXT | MPACKFLG_ENCODE;


      switch (mask->type) {
      case M_FIXEXT_1: {
            uint8_t tmp;
            READ(src, &item->ext->type, 1);
            READ(src, &tmp, 1);
            item->ext->u8  = tmp;
            item->ext->len = 1;
            break;
      }
      case M_FIXEXT_2: {
            uint16_t tmp;
            READ(src, &item->ext->type, 1);
            READ(src, &tmp, 2);
            item->ext->u16 = tmp;
            item->ext->len = 2;
            break;
      }
      case M_FIXEXT_4: {
            uint32_t tmp;
            READ(src, &item->ext->type, 1);
            READ(src, &tmp, 4);
            item->ext->u32 = tmp;
            item->ext->len = 4;
            break;
      }
      case M_FIXEXT_8: {
            uint64_t tmp;
            READ(src, &item->ext->type, 1);
            READ(src, &tmp, 8);
            item->ext->u64 = tmp;
            item->ext->len = 8;
            break;
      }
      case M_EXT_8: {
            uint8_t  len;
            uint64_t tmp = UINT64_C(0);
            READ(src, &len, 1);
            READ(src, &item->ext->type, 1);
            if (P99_UNLIKELY(len > 8))
                  err(1, "Got an msgpack ext object with a specified length greater than 8 bytes (%u). Type: (%u).",
                      len, item->ext->type);
            READ(src, &tmp, len);
            item->ext->len = len;

            memcpy(item->ext->u8x8, &tmp, len);

#if 0
            switch (len) {
            case 1:  item->ext->u64 = tmp;                         break;
            case 2:  item->ext->u64 = (uint64_t)decode_int16(tmp); break;
            case 4:  item->ext->u64 = (uint64_t)decode_int32(tmp); break;
            case 8:  item->ext->u64 = (uint64_t)decode_int64(tmp); break;
            default: memcpy(item->ext->u8x8, &tmp, len);           break;
            }
            value = (uint64_t)((
                len == 1
                    ? tmp
                    : (len == 2
                           ? decode_int16(tmp)
                           : (len == 4
                                  ? decode_int32(tmp)
                                  : (len == 8 ? decode_int64(tmp)
                                              : (handle_invalid_ext_length(tmp, len, type),
                                                 0))))));
#endif
            break;
      }
      default:
            ERRMSG();
      }

      return item;
}


static mpack_obj *
decode_bool(mpack_mask const *mask)
{
      mpack_obj *item = talloc(CTX, mpack_obj);
      item->flags     = MPACK_BOOL | MPACKFLG_ENCODE;

      switch (mask->type) {
      case M_TRUE:
            item->boolean = true;
            break;
      case M_FALSE:
            item->boolean = false;
            break;
      default:
            ERRMSG();
      }

      return item;
}


static mpack_obj *
decode_nil(void)
{
      mpack_obj *item = talloc(CTX, mpack_obj);
      item->flags     = MPACK_NIL | MPACKFLG_ENCODE;
      item->nil       = M_MASK_NIL;

      return item;
}


/*============================================================================*/

#include "highlight.h"

static const mpack_mask *
id_pack_type(uint8_t const ch)
{
      mpack_mask const *mask = CTX;

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

      if (!mask) {
            char fname[PATH_MAX + 1];
            char tmp[256];
            struct tm tm_buf, *tm_ret;
            time_t now = time(NULL);
#if defined HAVE_LOCALTIME_S
            localtime_s((tm_ret = &tm_buf), &now);
#elif defined HAVE_LOCALTIME_R
            tm_ret = localtime_r(&now, &tm_buf);
#else
#  pragma message "WARNING: Defaulting to unsafe \"localtime()\""
            tm_ret = localtime(&now);
#endif
            strftime(tmp, 256, "%F %I:%M:%S %p", tm_ret);
            snprintf(fname, PATH_MAX, "%*s/decode_error.log", BSC(settings.cache_dir));

            FILE *fp = safe_fopen(fname, "ab");
            fprintf(fp, "[%s]:\t Failed to identify byte 0x%0X\n", tmp, ch);
            fclose(fp);

            errx(1, "Failed to identify type for byte 0x%0X.", ch);
      }

      return mask;
}

#if defined _WIN32 && USE_EVENT_LIB != EVENT_LIB_LIBEVENT

static void
stream_read(void *restrict src, void *restrict dest, size_t const nbytes)
{
      int const fd    = (int)*((intptr_t *)src);
      size_t    nread = 0;

      while (nread < nbytes) {
            int n = (int)read(fd, dest + nread, nbytes - nread);
            if (unlikely(n < 0))
                  err(1, "read() error");
            nread += (size_t)n;
      }

#if defined DEBUG && defined DEBUG_LOGS
      if (P99_LIKELY(mpack_raw_read))
            fwrite(dest, 1, nbytes, mpack_raw_read);
#endif
}

#else /* !defined _WIN32 */

static void
stream_read(void *restrict src, void *restrict dest, size_t const nbytes)
{
      socket_t const sock = *((socket_t *)src);
      //eprintf("Reading %zu bytes from socket %llu\n", nbytes, (long long unsigned)sock);
      //fflush(stderr);

#if 0
      ssize_t const nread = recv(sock, dest, nbytes, MSG_WAITALL);
      if (unlikely(nread < 0))
            err(1, "recv() error");
      if (unlikely((size_t)nread != nbytes))
            err(1, "recv() returned too few bytes (%zd != %zu)!", nread, nbytes);
#endif

      size_t nread = 0;

      while (nread < nbytes) {
            ssize_t n = recv(sock, (char *)dest + nread, nbytes - nread, 0);
            if (unlikely(n < 0))
                  err(1, "read() error");
            nread += (size_t)n;
      }

#if defined DEBUG && defined DEBUG_LOGS
      if (P99_LIKELY(mpack_raw_read))
            fwrite(dest, 1, nbytes, mpack_raw_read);
#endif
}

#endif /* defined _WIN32 */


static void
obj_read(void *restrict src, void *restrict dest, size_t const nbytes)
{
      bstring *buf = src;
      if (buf->slen < nbytes)
            errx(1,
                 "Buffer does not contain a complete msgpack object. Since this code "
                 "is written terribly, this is fatal. (available: %u, need %zu)",
                 buf->slen, nbytes);
      memcpy(dest, buf->data, nbytes);
      buf->data += nbytes;
      buf->slen -= nbytes;
}

/*----------------------------------------------------------------------------*/

