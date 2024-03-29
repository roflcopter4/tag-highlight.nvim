#include "Common.h"
#include "util/archive.h"
#include <sys/stat.h>

#include <lzma.h>


#ifdef _MSC_VER
typedef int64_t ssize_t;
#endif

#if BUFSIZ <= 1024
#define IO_BUFFER_SIZE 8192
#else
#define IO_BUFFER_SIZE (BUFSIZ & ~7U)
#endif

#ifdef DOSISH
#include <windows.h>
#else
#include <unistd.h>
#endif

#define safe_close(FILE_DESC)       \
      do                            \
            if ((FILE_DESC) > 2)    \
                  close(FILE_DESC); \
      while (0)
#define percentage(IA, IB) (((double)(IA) / (double)(IB)) * 100)


//=============================================================================

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#define ARRAY_SIZE(_ARRAY_) (sizeof(_ARRAY_) / sizeof(*(_ARRAY_)))

char *prog_name;

typedef enum {
      IO_WAIT_MORE,    // Reading or writing is possible.
      IO_WAIT_ERROR,   // Error or user_abort
      IO_WAIT_TIMEOUT, // poll() timed out
} io_wait_ret;


/// Information about a .xz file
typedef struct xz_file_info_s {
      lzma_index *idx;
      uint64_t    stream_padding;
      uint64_t    memusage_max;
      bool        all_have_sizes;
      uint32_t    min_version;
} xz_file_info;


typedef union {
      uint8_t  u8[IO_BUFFER_SIZE];
      uint32_t u32[IO_BUFFER_SIZE / sizeof(uint32_t)];
      uint64_t u64[IO_BUFFER_SIZE / sizeof(uint64_t)];
} io_buf;


struct xz_file {
      const char *name;
      int         fd;
      bool        eof;
      struct stat st;
};


//=============================================================================

#if LZMA_VERSION > 50030000

static bool   parse_indexes(struct xz_file *file, xz_file_info *xfi);
static size_t io_read(struct xz_file *file, io_buf *buf_union, size_t size);

//=============================================================================

/// Opens the source file. Returns false on success, true on error.
static bool
io_open_src_real(struct xz_file *file)
{
      const int flags = O_RDONLY | O_BINARY | O_NOCTTY;

      file->fd = open(file->name, flags);

      if (file->fd == -1) {
            ALWAYS_ASSERT(errno != EINTR);
            warn("%s:", file->name);
            return true;
      }

      if (fstat(file->fd, &file->st))
            goto error_msg;

      if (!S_ISREG(file->st.st_mode)) {
            warnx("%s: Not a regular file, skipping\n", file->name);
            goto error;
      }

      return false;

error_msg:
      warn("%s", file->name);
error:
      (void)close(file->fd);
      return true;
}


static struct xz_file *
io_open_src(const char *name)
{
      if (*name == '\0')
            return NULL;
      static struct xz_file file;

      file = (struct xz_file){
          .name = name,
          .fd   = -1,
          .eof  = false,
      };

      const bool error = io_open_src_real(&file);

      return error ? NULL : &file;
}


//=============================================================================

static bool
io_seek_src(struct xz_file *file, off_t pos)
{
      ALWAYS_ASSERT(pos >= 0);

      if (lseek(file->fd, pos, SEEK_SET) != pos) {
            warn("%s: Error seeking the file", file->name);
            return true;
      }

      file->eof = false;

      return false;
}


#define MEM_LIMIT UINT64_MAX

static bool
parse_indexes(struct xz_file *file, xz_file_info *xfi)
{
      if (file->st.st_size <= 0) {
            warnx("%s: File is empty\n", file->name);
            return false;
      }

      if (file->st.st_size < (SIZE_C(2) * LZMA_STREAM_HEADER_SIZE)) {
            warnx("%s: Too small to be a valid .xz file\n", file->name);
            return false;
      }

      io_buf      buf;
      lzma_stream strm[] = {LZMA_STREAM_INIT};
      lzma_index *idx    = NULL;
      lzma_ret    ret =
          lzma_file_info_decoder(strm, &idx, MEM_LIMIT, (uint64_t)(file->st.st_size));
      if (ret != LZMA_OK)
            return false;

      for (bool done = false; !done;) {
            if (strm->avail_in == 0) {
                  strm->next_in  = buf.u8;
                  strm->avail_in = io_read(file, &buf, IO_BUFFER_SIZE);
                  if (strm->avail_in == SIZE_MAX)
                        warn("Unkown IO error");
            }

            ret = lzma_code(strm, LZMA_RUN);

            switch (ret) {
            case LZMA_OK:
                  break;

            case LZMA_SEEK_NEEDED:
                  // The cast is safe because liblzma won't ask us to seek past
                  // the known size of the input file which did fit into off_t.
                  ALWAYS_ASSERT(strm->seek_pos <= (uint64_t)(file->st.st_size));
                  if (io_seek_src(file, (off_t)(strm->seek_pos))) {
                        warnx("%d, %s: %s\n", ret, file->name, lzma_message_strm(ret));
                        return false;
                  }

                  strm->avail_in = 0;
                  break;

            case LZMA_STREAM_END:
                  xfi->idx = idx;

                  // Calculate xfi->stream_padding.
                  lzma_index_iter iter;
                  lzma_index_iter_init(&iter, xfi->idx);
                  while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM))
                        xfi->stream_padding += iter.stream.padding;

                  done = true;
                  break;

            default:
                  warnx("%d, %s: %s\n", ret, file->name, lzma_message_strm(ret));
                  return false;
            }
      }

      lzma_end(strm);
      return true;
}


static size_t
io_read(struct xz_file *file, io_buf *buf_union, size_t size)
{
      uint8_t *buf  = buf_union->u8;
      size_t   left = size;

      while (left > 0) {
            const ssize_t amount = read(file->fd, buf, left);

            if (amount == 0) {
                  file->eof = true;
                  break;
            }

            if (amount == -1) {
                  warn("%s: Read error", file->name);
                  return SIZE_MAX;
            }

            buf += (size_t)(amount);
            left -= (size_t)(amount);
      }

      return size - left;
}


char *
lzma_message_strm(unsigned code)
{
      switch (code) {
      case LZMA_OK:                return "Everything is ok";
      case LZMA_STREAM_END:        return "Unexpected end of stream";
      case LZMA_NO_CHECK:          return "No integrity check; not verifying file integrity";
      case LZMA_UNSUPPORTED_CHECK: return "Unsupported type of integrity check; not verifying file integrity";
      case LZMA_GET_CHECK:         return STRINGIFY(LZMA_GET_CHECK);
      case LZMA_MEM_ERROR:         return strerror(ENOMEM);
      case LZMA_MEMLIMIT_ERROR:    return "Memory usage limit reached";
      case LZMA_FORMAT_ERROR:      return "File format not recognized";
      case LZMA_OPTIONS_ERROR:     return "Unsupported options";
      case LZMA_DATA_ERROR:        return "Compressed data is corrupt";
      case LZMA_BUF_ERROR:         return "Unexpected end of input";
      case LZMA_PROG_ERROR:        return STRINGIFY(LZMA_PROG_ERROR);
      case LZMA_SEEK_NEEDED:       return STRINGIFY(LZMA_SEEK_NEEDED);
      default:                     return "Internal error (bug)";
      }
}


//=============================================================================


#define XZ_FILE_INFO_INIT {NULL, 0, 0, true, 50000002}

int
xz_get_uncompressed_size(struct archive_size *size, const char *filename)
{
      int             ret;
      struct xz_file *file = io_open_src(filename);
      if (file == NULL)
            err(1, "Failed to open file %s", filename);

      xz_file_info xfi = XZ_FILE_INFO_INIT;

      if (parse_indexes(file, &xfi)) {
            size->archive      = lzma_index_file_size(xfi.idx);
            size->uncompressed = lzma_index_uncompressed_size(xfi.idx);
            lzma_index_end(xfi.idx, NULL);
            safe_close(file->fd);
            ret = 1;
      } else {
            safe_close(file->fd);
            warnx("Error: cannot read file.\n");
            ret = 0;
      }

      return ret;
}


/***************************************************************************************/
/***************************************************************************************/
/***************************************************************************************/
#else
/***************************************************************************************/
/***************************************************************************************/
/***************************************************************************************/


static bool   parse_indexes(struct xz_file *file, xz_file_info *xfi);
static size_t io_read(struct xz_file *file, io_buf *buf_union, size_t size);


//=============================================================================


/// Opens the source file. Returns false on success, true on error.
static bool
io_open_src_real(struct xz_file *file)
{
      const int flags = O_RDONLY | O_BINARY | O_NOCTTY;

      file->fd = open(file->name, flags);

      if (file->fd == -1) {
            ALWAYS_ASSERT(errno != EINTR);
            warn("%s:", file->name);
            return true;
      }

      if (fstat(file->fd, &file->st))
            goto error_msg;

      if (!S_ISREG(file->st.st_mode)) {
            warnx("%s: Not a regular file, skipping\n", file->name);
            goto error;
      }

      return false;

error_msg:
      warn("%s", file->name);
error:
      (void)close(file->fd);
      return true;
}


static struct xz_file *
io_open_src(const char *name)
{
      if (*name == '\0')
            return NULL;
      static struct xz_file file;

      file = (struct xz_file){
          .name = name,
          .fd   = -1,
          .eof  = false,
      };

      const bool error = io_open_src_real(&file);

      return error ? NULL : &file;
}


//=============================================================================

#define MEM_LIMIT UINT64_MAX

static bool
parse_indexes(struct xz_file *file, xz_file_info *xfi)
{
      if (file->st.st_size <= 0) {
            warnx("%s: File is empty\n", file->name);
            return false;
      }

      if (file->st.st_size < (SIZE_C(2) * LZMA_STREAM_HEADER_SIZE)) {
            warnx("%s: Too small to be a valid .xz file\n", file->name);
            return false;
      }

      io_buf      buf;
      lzma_stream strm[] = {LZMA_STREAM_INIT};
      lzma_index *idx    = NULL;
      lzma_ret    ret    = lzma_index_decoder(strm, &idx, MEM_LIMIT);

      if (ret != LZMA_OK)
            return false;

      for (bool done = false; !done;) {
            if (strm->avail_in == 0) {
                  strm->next_in  = buf.u8;
                  strm->avail_in = io_read(file, &buf, IO_BUFFER_SIZE);
                  if (strm->avail_in == SIZE_MAX)
                        warn("Unkown IO error");
            }

            ret = lzma_code(strm, LZMA_RUN);

            switch (ret) {
            case LZMA_OK:
                  break;
            case LZMA_STREAM_END:
                  xfi->idx = idx;

                  // Calculate xfi->stream_padding.
                  lzma_index_iter iter;
                  lzma_index_iter_init(&iter, xfi->idx);
                  while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM))
                        xfi->stream_padding += iter.stream.padding;

                  done = true;
                  break;

            default:
                  warnx("%d, %s: %s\n", ret, file->name, lzma_message_strm(ret));
                  return false;
            }
      }

      lzma_end(strm);
      return true;
}


static size_t
io_read(struct xz_file *file, io_buf *buf_union, size_t size)
{
      uint8_t *buf  = buf_union->u8;
      size_t   left = size;

      while (left > 0) {
            const ssize_t amount = read(file->fd, buf, left);

            if (amount == 0) {
                  file->eof = true;
                  break;
            }

            if (amount == -1) {
                  warn("%s: Read error", file->name);
                  return SIZE_MAX;
            }

            buf += (size_t)(amount);
            left -= (size_t)(amount);
      }

      return size - left;
}


char *
lzma_message_strm(unsigned code)
{
      switch (code) {
      case LZMA_OK:                return "Everything is ok";
      case LZMA_STREAM_END:        return "Unexpected end of stream";
      case LZMA_NO_CHECK:          return "No integrity check; not verifying file integrity";
      case LZMA_UNSUPPORTED_CHECK: return "Unsupported type of integrity check; not verifying file integrity";
      case LZMA_GET_CHECK:         return STRINGIFY(LZMA_GET_CHECK);
      case LZMA_MEM_ERROR:         return strerror(ENOMEM);
      case LZMA_MEMLIMIT_ERROR:    return "Memory usage limit reached";
      case LZMA_FORMAT_ERROR:      return "File format not recognized";
      case LZMA_OPTIONS_ERROR:     return "Unsupported options";
      case LZMA_DATA_ERROR:        return "Compressed data is corrupt";
      case LZMA_BUF_ERROR:         return "Unexpected end of input";
      case LZMA_PROG_ERROR:        return STRINGIFY(LZMA_PROG_ERROR);
      default:                     return "Internal error (bug)";
      }
}


//=============================================================================


#define XZ_FILE_INFO_INIT {NULL, 0, 0, true, LZMA_VERSION}

int
xz_size(struct archive_size *size, const char *filename)
{
      int             ret;
      struct xz_file *file = io_open_src(filename);
      if (file == NULL)
            err(1, "Failed to open file %s", filename);

      xz_file_info xfi = XZ_FILE_INFO_INIT;

      if (parse_indexes(file, &xfi)) {
            size->archive      = lzma_index_file_size(xfi.idx);
            size->uncompressed = lzma_index_uncompressed_size(xfi.idx);
            lzma_index_end(xfi.idx, NULL);
            safe_close(file->fd);
            ret = 1;
      } else {
            safe_close(file->fd);
            warnx("Error: cannot read file.\n");
            ret = 0;
      }

      return ret;
}

static uint8_t *
read_raw_archive(bstring const *filename, struct archive_size *size)
{
      {
            struct stat st;
            if (stat(BS(filename), &st) == (-1))
                  err(1, "stat");
            size->archive = st.st_size;
      }

      /* We must read the size of the input buffer + 1 in order to
       * trigger an EOF condition.*/
      uint8_t *in_buf = malloc(size->archive + 1LLU);
      FILE    *fp     = safe_fopen(BS(filename), "rb");
      size_t   nread  = fread(in_buf, 1, size->archive + 1, fp);

      ALWAYS_ASSERT(nread == size->archive);
      if (ferror(fp))
            err(1, "%s: Error reading input size", BS(filename));
      if (!feof(fp))
            errx(1, "Error reading file: buffer too small.");
      fclose(fp);

      return in_buf;
}

uint8_t *
badly_attempt_to_decode_lzma_buffer(bstring const *fname)
{
      struct archive_size size = {0};
      uint8_t *inbuf = read_raw_archive(fname, &size);

      lzma_index *idx = lzma_index_init(NULL);
      size_t      inpos    = 0;
      uint64_t    memlimit = UINT64_MAX;
      lzma_ret    ret      = lzma_index_buffer_decode(&idx, &memlimit, NULL, inbuf, &inpos, size.archive);

      switch (ret) {
      case LZMA_OK:
      case LZMA_STREAM_END:
            size.uncompressed = lzma_index_uncompressed_size(idx);
            lzma_index_end(idx, NULL);
            break;
      default:
            warnx("%d, %*s: %s\n", ret, BSC(fname), lzma_message_strm(ret));
            lzma_index_end(idx, NULL);
            free(inbuf);
            return NULL;
      }

      warnd("Have raw size %zu, uncompressed size %zu for %*s", size.archive, size.uncompressed, BSC(fname));

      uint8_t *outbuf = talloc_size(NULL, size.uncompressed + SIZE_C(1));
      size_t   outpos = 0;

      ret = lzma_stream_buffer_decode(&memlimit, 0, NULL, inbuf, &inpos, size.archive, outbuf, &outpos, size.uncompressed);
      free(inbuf);

      switch (ret) {
      case LZMA_OK:
      case LZMA_STREAM_END:
            outbuf[size.uncompressed] = '\0';
            break;
      default:
            talloc_free(outbuf);
            warnx("%d, %*s: %s\n", ret, BSC(fname), lzma_message_strm(ret));
            return NULL;
      }

      return outbuf;
}


#endif
