#include "util.h"
#include <fcntl.h>

#include "data.h"
#include "mpack.h"

#ifdef _WIN32
#  define SEPCHAR '\\'
#  define SEPSTR  "\\"
#else
#  define SEPCHAR '/'
#  define SEPSTR  "/"
#endif

#define GZFILE_SUFFIX ".tags.xz"

extern pthread_mutex_t ftdata_mutex;

static void get_gzfile(struct bufdata *bdata);
static void init_filetype(int fd, struct ftdata_s *ft);


bool
new_buffer(const int fd, const int bufnum)
{
        for (unsigned short i = 0; i < buffers.bad_bufs.qty; ++i)
                if (bufnum == buffers.bad_bufs.lst[i])
                        return false;

        struct bufdata *bdata = get_bufdata(fd, bufnum);
        if (!bdata)
                abort();

        bstring *ft = nvim_buf_get_option(fd, bufnum, b_tmp("ft"),
                                          MPACK_STRING, NULL, 1);
        if (!ft)
                abort();

        for (unsigned i = 0; i < ftdata_len; ++i) {
                if (b_iseq(ft, &ftdata[i].vim_name)) {
                        bdata->ft = &ftdata[i];
                        break;
                }
        }

        b_free(ft);
        if (!bdata->ft) {
                nvprintf("Can't identify buffer %d, bailing!\n", bufnum);
                destroy_bufdata(&bdata);
                buffers.bad_bufs.lst[buffers.bad_bufs.qty++] = bufnum;
                return false;
        }

        if (bdata->ft->id != FT_NONE && !bdata->ft->initialized)
                init_filetype(fd, bdata->ft);

        buffers.lst[buffers.qty++] = bdata;

        return true;
}


struct bufdata *
find_buffer(const int bufnum)
{
        struct bufdata *ret = NULL;

        for (unsigned i = 0; i < buffers.qty; ++i) {
                if (buffers.lst[i] && buffers.lst[i]->num == bufnum) {
                        ret = buffers.lst[i];
                        break;
                }
        }

        return ret;
}


int
find_buffer_ind(const int bufnum)
{
        for (unsigned i = 0; i < buffers.qty; ++i)
                if (buffers.lst[i] && buffers.lst[i]->num == bufnum)
                        return (int)i;

        return (-1);
}


struct bufdata *
get_bufdata(const int fd, const int bufnum)
{
        struct bufdata *bdata = xmalloc(sizeof *bdata);
        bdata->num      = bufnum;
        bdata->filename = nvim_buf_get_name(fd, bufnum);
        bdata->tmpfname = nvim_call_function(fd, b_tmp("tempname"), MPACK_STRING, NULL, 1);
        bdata->tmpfd    = open(BS(bdata->tmpfname), O_CREAT|O_RDWR|O_TRUNC|O_DSYNC, 0600);
        bdata->ft       = NULL;
        bdata->lines    = ll_make_new();
        bdata->current  = NULL;
        bdata->ctick    = bdata->last_ctick = 0;
        

        get_gzfile(bdata);

        if (bdata->tmpfd == (-1))
                errx(1, "Failed to open temporary file!");

        return bdata;
}


void
destroy_bufdata(struct bufdata **bdata)
{
        if (!*bdata)
                return;
        close((*bdata)->tmpfd);
        unlink(BS((*bdata)->tmpfname));

        b_free((*bdata)->filename);
        b_free((*bdata)->gzfile);
        b_free((*bdata)->tmpfname);
        b_free((*bdata)->topdir);
        ll_destroy((*bdata)->lines);

        free(*bdata);
        *bdata = NULL;
}


static void
get_gzfile(struct bufdata *bdata)
{
        int64_t pos = b_strrchr(bdata->filename, '/');
        char   *cpy = b_bstr2cstr(bdata->filename, '?');
        if (pos < 0)
                abort();
        cpy[pos] = '\0';

        bstring *gzfile = b_fromcstr_alloc(bdata->filename->slen * 2,
                                           getenv("HOME"));
        b_catlit(gzfile, SEPSTR ".vim_tags" SEPSTR);

        for (unsigned i = 0; i < pos && i < bdata->filename->mlen; ++i) {
                if (bdata->filename->data[i] == SEPCHAR) {
                        gzfile->data[gzfile->slen++] = '_';
                        gzfile->data[gzfile->slen++] = '_';
                } else {
                        gzfile->data[gzfile->slen++] = cpy[i];
                }
        }

        b_catlit(gzfile, GZFILE_SUFFIX);
        bdata->gzfile = gzfile;
        bdata->topdir = b_fromcstr(cpy);

        free(cpy);
}


static void
init_filetype(int fd, struct ftdata_s *ft)
{
        if (ft->initialized)
                return;
        pthread_mutex_lock(&ftdata_mutex);

        ft->initialized   = true;
        ft->order         = nvim_get_var_fmt(fd, MPACK_STRING, NULL, 1,
                                             "tag_highlight#%s#order", BTS(ft->vim_name));
        ft->ignored_tags  = dict_get_key(settings.ignored_tags, 0, &ft->vim_name, 0);
        dictionary *equiv = nvim_get_var_fmt(fd, MPACK_DICT, NULL, 0,
                                             "tag_highlight#%s#equivalent", BTS(ft->vim_name));
        if (equiv)
                ft->equiv = b_list_create_alloc(equiv->qty);
        else
                ft->equiv = NULL;

        if (equiv) {
                for (unsigned i = 0; i < equiv->qty; ++i) {
                        bstring *toadd = equiv->entries[i]->key->data.str;
                        b_concat(toadd, equiv->entries[i]->value->data.str);
                        b_writeprotect(toadd);

                        mpack_destroy(equiv->entries[i]->key);
                        mpack_destroy(equiv->entries[i]->value);
                        free(equiv->entries[i]);

                        b_writeallow(toadd);
                        b_add_to_list(ft->equiv, toadd);
                }

                free(equiv->entries);
                free(equiv);
        }

        pthread_mutex_unlock(&ftdata_mutex);
}
