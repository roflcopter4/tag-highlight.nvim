#include "util/archive.h"

#ifdef HAVE_TOPCONFIG_H
#  include "Common.h"
#else
#  include "util.h"
#endif

#include <zlib.h>
#ifdef LZMA_SUPPORT
#  include <lzma.h>
#endif

#ifndef HAVE_FSYNC
#  define fsync(FILDES)
#endif

#define SAFE_STAT(PATH, ST)                                     \
     do {                                                       \
             if ((stat((PATH), (ST)) != 0))                     \
                     err(1, "Failed to stat file '%s", (PATH)); \
     } while (0)

/*****************************************************************************/

void
write_plain_from_buffer(struct top_dir const *topdir, bstring const *buf)
{
        FILE *wfp = safe_fopen(BS(topdir->gzfile), "wb");
        ALWAYS_ASSERT(b_fwrite(wfp, buf) == buf->slen);
        fclose(wfp);
}

void
write_gzip_from_buffer(struct top_dir const *topdir, bstring const *buf)
{
        gzFile gfp = gzopen(BS(topdir->gzfile), "wb");
        ALWAYS_ASSERT((uint32_t)gzwrite(gfp, buf->data, buf->slen) == buf->slen);
        gzclose(gfp);
}

void
write_lzma_from_buffer(struct top_dir const *topdir, bstring const *buf)
{
        /* lzma_ret    ret  = lzma_easy_encoder(&strm, settings.comp_level, LZMA_CHECK_CRC64); */
        lzma_stream strm = LZMA_STREAM_INIT;
        lzma_ret    ret  = lzma_easy_encoder(&strm, 6, LZMA_CHECK_CRC64);
        if (ret != LZMA_OK)
                errx(1, "LZMA error: %s", lzma_message_strm(ret));

        uint8_t *out_buf = talloc_size(NULL, buf->slen);
        strm.next_out    = out_buf;
        strm.next_in     = buf->data;
        strm.avail_out   = buf->slen;
        strm.avail_in    = buf->slen;

        do {
                ret = lzma_code(&strm, LZMA_FINISH);
        } while (ret == LZMA_OK);

        if (ret != LZMA_STREAM_END)
                warnx("Unexpected error on line %d in file %s: %d => %s",
                      __LINE__, __FILE__, ret, lzma_message_strm(ret));

        assert(strstr(BS(topdir->gzfile), ".tags.xz"));
        FILE *fp = safe_fopen(BS(topdir->gzfile), "wb");
        ALWAYS_ASSERT(fwrite(out_buf, 1, strm.total_out, fp) == strm.total_out);
        fclose(fp);

        lzma_end(&strm);
        talloc_free(out_buf);
}

/*****************************************************************************/

void
write_plain(struct top_dir *topdir)
{
        struct stat st;
        fsync(topdir->tmpfd);
        if (fstat(topdir->tmpfd, &st) != 0)
                err(1, "stat failed");

        uint8_t      *buf    = talloc_zero_size(NULL, st.st_size + 1);
        FILE         *readfp = safe_fopen(BS(topdir->tmpfname), "rb");
        const size_t nread   = fread(buf, 1, st.st_size + 1, readfp);
        fclose(readfp);

        if (nread == (size_t)st.st_size)
                err(1, "fread(): %zu != %zu", nread, (size_t)st.st_size);

#if 0
        ftruncate(topdir->tmpfd, 0);
        lseek(topdir->tmpfd, 0, SEEK_SET);
        assert(write(topdir->tmpfd, buf, (size_t)(st.st_size + 1)) == (ssize_t)(st.st_size + 1));

        free(buf);
        fsync(topdir->tmpfd);
#endif

        FILE *wfp = safe_fopen(BS(topdir->gzfile), "wb");
        ALWAYS_ASSERT(fwrite(buf, 1, (size_t)st.st_size, wfp) == (size_t)st.st_size);
        fclose(wfp);
        talloc_free(buf);
}

void
write_gzip(struct top_dir *topdir)
{
        struct stat st;
        if (fstat(topdir->tmpfd, &st) != 0)
                err(1, "stat failed");

        uint8_t *buf = talloc_size(NULL, st.st_size);
        /* assert(read(topdir->tmpfd, buf, st.st_size) == st.st_size); */

        FILE  *readfp = safe_fopen(BS(topdir->tmpfname), "rb");
        errno = 0;
        const size_t nread   = fread(buf, 1, st.st_size + 1, readfp);

        if (nread != (size_t)st.st_size && !feof(readfp))
                err(1, "fread(): %zu != %zu", nread, (size_t)st.st_size);

        fclose(readfp);

        gzFile gfp = gzopen(BS(topdir->gzfile), "wb");
        gzwrite(gfp, buf, st.st_size);
        gzclose(gfp);

        talloc_free(buf);
}

#ifdef LZMA_SUPPORT

void
write_lzma(struct top_dir *topdir)
{
        struct stat st = { .st_size = 0LL };
        fsync(topdir->tmpfd);
        if (fstat(topdir->tmpfd, &st) != 0)
                err(1, "stat failed");

        const size_t   size   = (size_t)st.st_size;
        uint8_t       *in_buf = talloc_zero_size(NULL, size + 1);
        FILE          *readfp = safe_fopen(BS(topdir->tmpfname), "rb");
        const size_t   nread  = fread(in_buf, 1, size + 1, readfp);
        fclose(readfp);
        if (nread != size)
                errx(1, "Read %zd, expected %zu", nread, size + 1);

        lzma_stream strm = LZMA_STREAM_INIT;
        lzma_ret    ret  = lzma_easy_encoder(&strm, 6, LZMA_CHECK_CRC64);
        if (ret != LZMA_OK)
                errx(1, "LZMA error: %s", lzma_message_strm(ret));

        /* uint8_t *out_buf = calloc(st.st_size, 1); */
        uint8_t *out_buf = talloc_zero_size(NULL, size);

        strm.next_out  = out_buf;
        strm.next_in   = in_buf;
        strm.avail_out = size;
        strm.avail_in  = size;

        do {
                ret = lzma_code(&strm, LZMA_FINISH);
        } while (ret == LZMA_OK);


        if (ret != LZMA_STREAM_END)
                warnx("Unexpected error on line %d in file %s: %d => %s",
                      __LINE__, __FILE__, ret, lzma_message_strm(ret));

        ALWAYS_ASSERT(strstr(BS(topdir->gzfile), ".tags.xz"));
        FILE *fp = safe_fopen(BS(topdir->gzfile), "wb");
        ALWAYS_ASSERT(fwrite(out_buf, 1, strm.total_out, fp) == strm.total_out);
        fclose(fp);

        lzma_end(&strm);
        talloc_free(in_buf);
        talloc_free(out_buf);
}
#endif
