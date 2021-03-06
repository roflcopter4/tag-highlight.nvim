#include "util/archive.h"

#ifdef HAVE_TOPCONFIG_H
#  include "Common.h"
#else
#  include "util.h"
#endif
#include <sys/stat.h>

#ifndef HAVE_FSYNC
#  define fsync(FILDES)
#endif

#define SAFE_STAT(PATH, ST)                                     \
     do {                                                       \
             if ((stat((PATH), (ST)) != 0))                     \
                     err(1, "Failed to stat file '%s", (PATH)); \
     } while (0)


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
        DIE_UNLESS((fwrite(buf, 1, (size_t)st.st_size, wfp) == (size_t)st.st_size));
        fclose(wfp);
        talloc_free(buf);
}


#include <zlib.h>

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
#include <lzma.h>

#if 0
void
lazy_write_lzma(struct top_dir *topdir)
{
        fsync(topdir->tmpfd);
        unlink(BS(topdir->gzfile));
        bstring *cmd = b_format("7z a '%s' -mmt='%d' '%s'", BS(topdir->gzfile), find_num_cpus(), BS(topdir->tmpfname));
        b_fwrite(stderr, cmd, B("\n"));
        assert(system(BS(cmd)) == 0);
        b_destroy(cmd);
}
#endif

void
write_lzma(struct top_dir *topdir)
{
        /* struct stat st; */
        /* assert(fstat(topdir->tmpfd, &st) == 0); */

        /* uint8_t *in_buf = malloc(st.st_size); */

        /* warnx("trying to read file"); */

        /* lseek(topdir->tmpfd, 0, SEEK_SET); */

        /* const int flgs = fcntl(topdir->tmpfd, F_GETFL); */
        /* fcntl(topdir->tmpfd, F_SETFL, flgs | O_NONBLOCK); */
        /* assert(read(topdir->tmpfd, in_buf, st.st_size) == st.st_size); */
        /* fcntl(topdir->tmpfd, F_SETFL, flgs); */

        /* while ((nread += read(topdir->tmpfd, in_buf, st.st_size)) < st.st_size)
                echo("read %zd so far\n", nread); */

#if 0
        FILE *fp1 = fopen(BS(topdir->tmpfname), "rb");
        /* assert(fread(in_buf, 1, st.st_size, fp1) == (size_t)st.st_size); */
        /* assert(feof(fp1)); */
        bstring *asswipe = B_READ(fp1);
        fclose(fp1);
#endif
#if 0
        lzma_mt mt_opts = { 
                .flags      = 0,
                .threads    = find_num_cpus(),
                .block_size = 0,
                .timeout    = 200/*ms*/,
                .preset     = 6,
                /* .preset     = LZMA_PRESET_EXTREME, */
                .filters    = NULL,
                .check      = LZMA_CHECK_CRC64,
                /* 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL */
        };
#endif

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

        /* uint8_t *in_buf = asswipe->data; */
        /* uint64_t    memuse = lzma_stream_encoder_mt_memusage(&mt_opts); */

        lzma_stream strm = LZMA_STREAM_INIT;
        /* lzma_ret    ret  = lzma_stream_encoder_mt(&strm, &mt_opts); */
        /* lzma_ret    ret  = lzma_easy_encoder(&strm, settings.comp_level, LZMA_CHECK_CRC64); */
        lzma_ret    ret  = lzma_easy_encoder(&strm, 1, LZMA_CHECK_CRC64);
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

#if 0
        struct write_wrapper_data *data = malloc(sizeof *data);
        *data = (struct write_wrapper_data){ topdir, out_buf, (size_t)st.st_size };
        /* pthread_t tid;
        pthread_create(&tid, NULL, &write_wrapper, data); */
        write_wrapper(data);
#endif

        /* int fd = open(BS(topdir->gzfile), O_CREAT|O_TRUNC|O_WRONLY, 0644);
        assert(fd != (-1));
        assert(write(fd, out_buf, size) == (ssize_t)size);
        close(fd); */

        ALWAYS_ASSERT(strstr(BS(topdir->gzfile), ".tags.xz"));
        FILE *fp = safe_fopen(BS(topdir->gzfile), "wb");
        ALWAYS_ASSERT(fwrite(out_buf, 1, strm.total_out, fp) == strm.total_out);
        fclose(fp);

        lzma_end(&strm);
        talloc_free(in_buf);
        talloc_free(out_buf);
        /* b_free(asswipe); */
}
#endif
