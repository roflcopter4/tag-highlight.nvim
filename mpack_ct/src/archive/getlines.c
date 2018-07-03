#include "archive_util.h"
#ifdef HAVE_CONFIG_H
#  include "util.h"
#else
#  include "../util.h"
#endif
#include <sys/stat.h>

#define safe_stat(PATH, ST)                                     \
     do {                                                       \
             if ((stat((PATH), (ST)) != 0))                     \
                     err(1, "Failed to stat file '%s", (PATH)); \
     } while (0)


static void ll_strsep      (b_list *tags, uint8_t *buf);
static void plain_getlines (b_list *tags, const bstring *filename);
static void gz_getlines    (b_list *tags, const bstring *filename);
#ifdef LZMA_SUPPORT
static void xz_getlines    (b_list *tags, const bstring *filename);
#endif

struct backups backup_pointers = { NULL, 0, 0 };

/* ========================================================================== */


int
getlines(b_list *tags, const bstring *comptype, const bstring *filename)
{
        warnx("Attempting to read tag file %s", BS(filename));

        if (b_iseq(comptype, b_tmp("none")))
                plain_getlines(tags, filename);
        else if (b_iseq(comptype, b_tmp("gzip")))
                gz_getlines(tags, filename);
#ifdef LZMA_SUPPORT
        else if (b_iseq(comptype, b_tmp("lzma")))
                xz_getlines(tags, filename);
#endif
        else {
                warnx("Unknown compression type %s!", BS(comptype));
                return 0;
        }
        return 1; /* 1 indicates success here... */
}


static void
ll_strsep(b_list *tags, uint8_t *buf)
{
        char *tok;
        /* Set this global pointer so the string can be free'd later... */
        add_backup(&backup_pointers, buf);

        while ((tok = strsep((char **)(&buf), "\n")) != NULL) {
                if (*tok == '\0')
                        continue;
                b_add_to_list(tags, b_refblk(tok, (char *)(buf) - tok - 1));
        }
}


#ifdef DEBUG
    static inline void
    report_size(struct archive_size *size)
    {
            __extension__ warnx("Using a buffer of size %'zu for output; filesize is %'zu\n",
                  size->uncompressed, size->archive);
    }
#else
#   define report_size(...)
#endif


/* ========================================================================== */
/* PLAIN */


static void
plain_getlines(b_list *tags, const bstring *filename)
{
        FILE *fp = safe_fopen(BS(filename), "rb");
        struct stat st;

        safe_stat(BS(filename), &st);
        uint8_t *buffer = xmalloc(st.st_size + 1LL);

        if (fread(buffer, 1, st.st_size, fp) != (size_t)st.st_size || ferror(fp))
                err(1, "Error reading file %s", BS(filename));

        buffer[st.st_size] = '\0';

        fclose(fp);
        ll_strsep(tags, buffer);
}


/* ========================================================================== */
/* GZIP */

#include <zlib.h>


static void
gz_getlines(b_list *tags, const bstring *filename)
{
        struct archive_size size;
        gzip_size(&size, BS(filename));
        report_size(&size);

        gzFile gfp = gzopen(BS(filename), "rb");
        if (!gfp)
                err(1, "Failed to open file");

        /* Magic macros to the rescue. */
        uint8_t *out_buf = xmalloc(size.uncompressed + 1);
        int64_t numread  = gzread(gfp, out_buf, size.uncompressed);

        assert (numread == 0 || numread == (int64_t)size.uncompressed);
        gzclose(gfp);

        /* Always remember to null terminate the thing. */
        out_buf[size.uncompressed] = '\0';
        ll_strsep(tags, out_buf);
}


/* ========================================================================== */
/* XZ */

#ifdef LZMA_SUPPORT
#   include <lzma.h>
/* extern const char * message_strm(lzma_ret); */


/* It would be nice if there were some magic macros to read an xz file too. */
static void
xz_getlines(b_list *tags, const bstring *filename)
{
        struct archive_size size;
        xz_size(&size, BS(filename));
        report_size(&size);

        uint8_t *in_buf  = xmalloc(size.archive + 1);
        uint8_t *out_buf = xmalloc(size.uncompressed + 1);

        /* Setup the stream and initialize the decoder */
        lzma_stream strm[] = {LZMA_STREAM_INIT};
        if ((lzma_auto_decoder(strm, UINT64_MAX, 0)) != LZMA_OK)
                errx(1, "Unhandled internal error.");

        lzma_ret ret = lzma_stream_decoder(strm, UINT64_MAX, LZMA_CONCATENATED);
        if (ret != LZMA_OK)
                errx(1, "%s\n", ret == LZMA_MEM_ERROR ?
                     strerror(ENOMEM) : "Internal error (bug)");

        /* avail_in is the number of bytes read from a file to the strm that
         * have not yet been decoded. avail_out is the number of bytes remaining
         * in the output buffer in which to place decoded bytes.*/
        strm->next_out     = out_buf;
        strm->next_in      = in_buf;
        strm->avail_out    = size.uncompressed;
        strm->avail_in     = 0;
        lzma_action action = LZMA_RUN;
        FILE *fp           = safe_fopen(BS(filename), "rb");

        /* We must read the size of the input buffer + 1 in order to
         * trigger an EOF condition.*/
        strm->avail_in = fread(in_buf, 1, size.archive + 1, fp);

        if (ferror(fp))
                err(1, "%s: Error reading input size", BS(filename));
        if (feof(fp))
                action = LZMA_FINISH;
        else
                errx(1, "Error reading file: buffer too small.");

        ret = lzma_code(strm, action);

        if (ret != LZMA_STREAM_END)
                warn("Unexpected error on line %d in file %s: %d => %s",
                     __LINE__, __FILE__, ret, lzma_message_strm(ret));

        out_buf[size.uncompressed] = '\0';
        fclose(fp);
        lzma_end(strm);
        free(in_buf);

        ll_strsep(tags, out_buf);
}
#endif
