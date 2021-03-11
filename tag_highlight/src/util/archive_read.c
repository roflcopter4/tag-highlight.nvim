#include "archive.h"
#ifdef HAVE_CONFIG_H
#  include "Common.h"
#else
#  include "util.h"
#endif
#include <sys/stat.h>

#define SAFE_STAT(PATH, ST)                                        \
        do {                                                       \
                if ((stat((PATH), (ST)) != 0))                     \
                        err(1, "Failed to stat file '%s", (PATH)); \
        } while (0)


static void ll_strsep(b_list *tags, uint8_t *buf);
static int  plain_getlines(b_list *tags, const bstring *filename);
static int  gz_getlines(b_list *tags, const bstring *filename);
#ifdef LZMA_SUPPORT
static int xz_getlines(b_list *tags, const bstring *filename);
#endif

/* ========================================================================== */


int
getlines(b_list *tags, const comp_type_t comptype, const bstring *filename)
{
        int ret;
        ECHO("Attempting to read tag file %s", filename);

        if (comptype == COMP_NONE)
                ret = plain_getlines(tags, filename);
        else if (comptype == COMP_GZIP)
                ret = gz_getlines(tags, filename);
#ifdef LZMA_SUPPORT
        else if (comptype == COMP_LZMA)
                ret = xz_getlines(tags, filename);
#endif
        else {
                warnx("Unknown compression type!");
                ret = 0;
        }
        ECHO("Done");
        return ret; /* 1 indicates success here... */
}


static void
ll_strsep(b_list *tags, uint8_t *buf)
{
        char    *tok;
        uint8_t *bak = buf;

        while ((tok = strsep((char **)(&buf), "\n")) != NULL) {
                if (*tok == '\0')
                        continue;
                b_list_append(tags, b_fromblk(tok, (char *)(buf)-tok - 1));
        }

        talloc_free(bak);
}


#if defined(DEBUG) && defined(POINTLESS_DEBUG)
static inline void
report_size(struct archive_size *size)
{
        __extension__ warnx(
            "Using a buffer of size %'zu for output; filesize is %'zu\n",
            size->uncompressed, size->archive);
}
#else
#define report_size(...)
#endif


/* ========================================================================== */
/* PLAIN */


static int
plain_getlines(b_list *tags, const bstring *filename)
{
        FILE *      fp = safe_fopen(BS(filename), "rb");
        struct stat st;

        SAFE_STAT(BS(filename), &st);
        uint8_t *buffer = talloc_size(NULL, st.st_size + 1LLU);

        if (fread(buffer, 1, st.st_size, fp) != (size_t)st.st_size || ferror(fp))
                err(1, "Error reading file %s", BS(filename));

        buffer[st.st_size] = '\0';

        fclose(fp);
        ll_strsep(tags, buffer);

        return 1;
}


/* ========================================================================== */
/* GZIP */

#include <zlib.h>


static int
gz_getlines(b_list *tags, const bstring *filename)
{
        struct archive_size size;
        gzip_size(&size, BS(filename));
        report_size(&size);

        gzFile gfp = gzopen(BS(filename), "rb");
        if (!gfp) {
                warn("Failed to open file '%s'", BS(filename));
                return 0;
        }

        /* Magic macros to the rescue. */
        uint8_t *     out_buf = talloc_size(NULL, size.uncompressed + 1LLU);
        int64_t const numread = gzread(gfp, out_buf, size.uncompressed);

        ALWAYS_ASSERT(numread == 0 || numread == (int64_t)size.uncompressed);
        gzclose(gfp);

        out_buf[size.uncompressed] = '\0';
        ll_strsep(tags, out_buf);
        return 1;
}


/* ========================================================================== */
/* XZ */

#ifdef LZMA_SUPPORT
#include <lzma.h>
/* extern const char * message_strm(lzma_ret); */


/* It would be nice if there were some magic macros to read an xz file too. */
static int
xz_getlines(b_list *tags, const bstring *filename)
{
        struct archive_size size = {0, 0};
        if (!xz_size(&size, BS(filename)))
                return 0;
        report_size(&size);

        uint8_t *in_buf  = talloc_size(NULL, size.archive + 1LLU);
        uint8_t *out_buf = talloc_size(NULL, size.uncompressed + 1LLU);

        /* Setup the stream and initialize the decoder */
        lzma_stream *strm = (lzma_stream[]){LZMA_STREAM_INIT};
        if ((lzma_auto_decoder(strm, UINT64_MAX, 0)) != LZMA_OK)
                errx(1, "Unhandled internal error.");

        lzma_ret ret = lzma_stream_decoder(strm, UINT64_MAX, 0);
        if (ret != LZMA_OK)
                errx(1, "%s\n",
                     ret == LZMA_MEM_ERROR ? strerror(ENOMEM)
                                           : "Internal error (bug)");

        /* avail_in is the number of bytes read from a file to the strm that
         * have not yet been decoded. avail_out is the number of bytes remaining
         * in the output buffer in which to place decoded bytes.*/
        strm->next_out  = out_buf;
        strm->next_in   = in_buf;
        strm->avail_out = size.uncompressed;
        strm->avail_in  = 0;
        FILE *fp        = safe_fopen(BS(filename), "rb");

        /* We must read the size of the input buffer + 1 in order to
         * trigger an EOF condition.*/
        strm->avail_in = fread(in_buf, 1, size.archive + 1, fp);

        if (ferror(fp))
                err(1, "%s: Error reading input size", BS(filename));
        if (!feof(fp))
                errx(1, "Error reading file: buffer too small.");

        ret = lzma_code(strm, LZMA_RUN);

        if (ret != LZMA_STREAM_END)
                ret = lzma_code(strm, LZMA_FINISH);
        if (ret != LZMA_STREAM_END)
                warn("Unexpected error on line %d in file %s: %d => %s",
                     __LINE__, __FILE__, ret, lzma_message_strm(ret));

        out_buf[size.uncompressed] = '\0';
        fclose(fp);
        lzma_end(strm);
        talloc_free(in_buf);

        ll_strsep(tags, out_buf);
        return 1;
}
#endif
