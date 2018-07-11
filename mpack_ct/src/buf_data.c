#include "util.h"
#include <fcntl.h>

#include "data.h"
#include "highlight.h"
#include "mpack.h"

#ifdef _WIN32
#  define SEPCHAR '\\'
#  define SEPSTR  "\\"
#else
#  define SEPCHAR '/'
#  define SEPSTR  "/"
#endif

#define NEXT_MKR(STRUCT_)                                                              \
        do {                                                                           \
                const int init_ = (STRUCT_).mkr++;                                     \
                                                                                       \
                while ((STRUCT_).lst[(STRUCT_).mkr] != NULL && (STRUCT_).mkr != init_) \
                        (STRUCT_).mkr = ((STRUCT_).mkr + 1) % (STRUCT_).mlen;          \
                                                                                       \
                assert((STRUCT_).lst[(STRUCT_).mkr] == NULL);                          \
        } while (0)

#ifdef DOSISH
#  define DOSCHECK() \
        (cpy[i] == ':' || cpy[i] == '/')
#else
#  define DOSCHECK() (false)
#endif


extern pthread_mutex_t ftdata_mutex;

static struct top_dir *init_topdir(int fd, struct bufdata *bdata);
static void            init_filetype(int fd, struct ftdata_s *ft);

static bool     check_norecurse_directories(const bstring *dir);
static bstring *check_project_directories  (bstring *dir);


bool
new_buffer(const int fd, const int bufnum)
{
        for (unsigned short i = 0; i < buffers.bad_bufs.qty; ++i)
                if (bufnum == buffers.bad_bufs.lst[i])
                        return false;

        struct ftdata_s *tmp = NULL;
        bstring         *ft  = nvim_buf_get_option(fd, bufnum, b_tmp("ft"),
                                                   MPACK_STRING, NULL, 1);
        assert(ft != NULL);

        for (unsigned i = 0; i < ftdata_len; ++i) {
                if (b_iseq(ft, &ftdata[i].vim_name)) {
                        tmp = &ftdata[i];
                        break;
                }
        }

        if (!tmp) {
                echo("Can't identify buffer %d, bailing!\n", bufnum);
                buffers.bad_bufs.lst[buffers.bad_bufs.qty++] = bufnum;
                b_destroy(ft);
                return false;
        }
        for (unsigned i = 0; i < settings.ignored_ftypes->qty; ++i) {
                if (b_iseq(ft, settings.ignored_ftypes->lst[i])) {
                        buffers.bad_bufs.lst[buffers.bad_bufs.qty++] = bufnum;
                        return false;
                }
        }

        b_destroy(ft);
        struct bufdata *bdata = get_bufdata(fd, bufnum);
        assert(bdata != NULL);
        bdata->ft = tmp;

        if (bdata->ft->id != FT_NONE && !bdata->ft->initialized)
                init_filetype(fd, bdata->ft);

        buffers.lst[buffers.mkr] = bdata;
        NEXT_MKR(buffers);

        return true;
}

struct bufdata *
get_bufdata(const int fd, const int bufnum)
{
        struct bufdata *bdata = xmalloc(sizeof *bdata);
        bdata->num       = bufnum;
        bdata->filename  = nvim_buf_get_name(fd, bufnum);
        bdata->ft        = NULL;
        bdata->current   = NULL;
        bdata->cmd_cache = NULL;
        bdata->ctick     = bdata->last_ctick = 0;
        bdata->lines     = ll_make_new();
        bdata->topdir    = init_topdir(fd, bdata);

        return bdata;
}


void
destroy_bufdata(struct bufdata **bdata)
{
        if (!*bdata)
                return;

        b_destroy((*bdata)->filename);
        ll_destroy((*bdata)->lines);
        b_list_destroy((*bdata)->cmd_cache);

        if (--((*bdata)->topdir->refs) == 0) {
                struct top_dir *topdir = (*bdata)->topdir;

                close(topdir->tmpfd);
                unlink(BS(topdir->tmpfname));

                b_destroy(topdir->gzfile);
                b_destroy(topdir->pathname);
                b_destroy(topdir->tmpfname);
                b_list_destroy(topdir->tags);

                const int index = topdir->index;
                xfree(topdir);
                top_dirs.lst[index] = NULL;
        }

        xfree(*bdata);
        *bdata = NULL;
}


/*============================================================================*/


struct bufdata *
find_buffer(const int bufnum)
{
        for (unsigned i = 0; i < buffers.mlen; ++i)
                if (buffers.lst[i] && buffers.lst[i]->num == bufnum)
                        return buffers.lst[i];

        return NULL;
}


int
find_buffer_ind(const int bufnum)
{
        for (unsigned i = 0; i < buffers.mlen; ++i)
                if (buffers.lst[i] && buffers.lst[i]->num == bufnum)
                        return (int)i;

        return (-1);
}


struct bufdata *
null_find_bufdata(const int bufnum, struct bufdata *bdata)
{
        if (!bdata) {
                assert(bufnum > 0);
                bdata = find_buffer(bufnum);
        }
        assert(bdata != NULL || is_bad_buffer(bufnum));

        return bdata;
}


bool
is_bad_buffer(const int bufnum)
{
        for (unsigned i = 0; i < buffers.bad_bufs.qty; ++i)
                if (bufnum == buffers.bad_bufs.lst[i])
                        return true;
        return false;
}


/*============================================================================*/


static struct top_dir *
init_topdir(const int fd, struct bufdata *bdata)
{
        /* Emulate dirname() */
        int64_t pos  = b_strrchr(bdata->filename, SEPCHAR);
        bstring *dir = b_strcpy(bdata->filename);

        if (pos < 0)
                abort();

        dir->data[pos] = '\0';
        dir->slen      = pos;
        dir            = check_project_directories(dir);

        /* echo("Using top dir \"%s\"\n", BS(dir)); */

        /* If this buffer shares a directory with another previously opened
         * buffer then there's no need to re-read the tags file.
         */
        for (unsigned i = 0; i < top_dirs.mlen; ++i) {
                if (top_dirs.lst[i] && b_iseq(top_dirs.lst[i]->pathname, dir)) {
                        ++top_dirs.lst[i]->refs;
                        b_destroy(dir);
                        return top_dirs.lst[i];
                }
        }

        echo("Initializing new topdir \"%s\"\n", BS(dir));

        struct top_dir *tmp = xmalloc(sizeof(struct top_dir));
        tmp->tmpfname = nvim_call_function(fd, B("tempname"), MPACK_STRING, NULL, 1);
        tmp->tmpfd    = open(BS(tmp->tmpfname), O_CREAT|O_RDWR|O_BINARY, 0600);
        tmp->refs     = 1;
        tmp->tags     = NULL;
        tmp->index    = top_dirs.mkr;
        tmp->pathname = dir;
        tmp->gzfile   = b_fromcstr_alloc(dir->mlen * 3, HOME);
        tmp->recurse  = check_norecurse_directories(dir);

        tmp->pathname->flags |= BSTR_DATA_FREEABLE;
        b_catlit(tmp->gzfile, SEPSTR ".vim_tags" SEPSTR);

        if (tmp->tmpfd == (-1))
                errx(1, "Failed to open temporary file!");

        /* Set the vim 'tags' option. */
        {
                char buf[8192];
                size_t n = snprintf(buf, 8192, "set tags+=%s", BS(tmp->tmpfname));
                nvim_command(0, bt_fromblk(buf, n), 0);
        }

        /* In UNIX-y systems, swap '/' with '__', and do the same with '\\' and
         * ':' in Windows (because drive letters). Actually, we have to check
         * for all three in Windows, since it still generally tolerates paths
         * with forward slashes. */
        echo("slen -> %u, mlen-> %u\n", dir->slen, tmp->gzfile->mlen);
        for (unsigned i = 0; i < dir->slen && i < tmp->gzfile->mlen; ++i) {
                if (dir->data[i] == SEPCHAR || DOSCHECK()) {
                        tmp->gzfile->data[tmp->gzfile->slen++] = '_';
                        tmp->gzfile->data[tmp->gzfile->slen++] = '_';
                } else
                        tmp->gzfile->data[tmp->gzfile->slen++] = dir->data[i];
        }
        int ret;
        if (settings.comp_type == COMP_GZIP)
                ret = b_concat(tmp->gzfile, B(".tags.gz"));
        else if (settings.comp_type == COMP_LZMA)
                ret = b_concat(tmp->gzfile, B(".tags.xz"));
        else
                ret = b_concat(tmp->gzfile, B(".tags"));

        warnx("dir: %s\ngzfile: %s\nslen: %u, strlen: %zu", BS(dir),
              BS(tmp->gzfile), tmp->gzfile->slen, strlen(BS(tmp->gzfile)));
        assert(ret == BSTR_OK);

        get_initial_taglist(bdata, tmp);
        top_dirs.lst[top_dirs.mkr] = tmp;
        NEXT_MKR(top_dirs);

        return tmp;
}


static bool
check_norecurse_directories(const bstring *const dir)
{
        for (unsigned i = 0; i < settings.norecurse_dirs->qty; ++i)
                if (b_iseq(dir, settings.norecurse_dirs->lst[i]))
                        return false;

        return true;
}


static bstring *
check_project_directories(bstring *dir)
{
        char buf[PATH_MAX];
        b_list *candidates = b_list_create();

        snprintf(buf, PATH_MAX, "%s/.vim_tags/mytags.txt", HOME);

        FILE *fp = fopen(buf, "rb");
        if (!fp)
                return dir;
                /* return b_refblk(dir, len); */

        bstring *tmp;
        while ((tmp = B_GETS(fp, '\n'))) {
                tmp->data[tmp->slen -= 3] = '\0';

                /* warnx("Comparing \"%s\" to \"%s\"", dir, BS(tmp)); */

                if (strstr(BS(dir), BS(tmp)))
                        b_add_to_list(candidates, tmp);
                else
                        b_destroy(tmp);
        }

        fclose(fp);
        if (candidates->qty == 0) {
                b_list_destroy(candidates);
                return dir;
        }
                /* return b_refblk(dir, len); */

        unsigned x = 0;

        for (unsigned i = 0; i < candidates->qty; ++i)
                if (candidates->lst[i]->slen > candidates->lst[x]->slen)
                        x = i;

        bstring *ret = candidates->lst[x];
        bstring *test = b_strcpy(ret);
        b_writeprotect(test);
        b_free(test);
        b_writeallow(test);
        b_free(test);

        b_writeprotect(ret);
        b_list_destroy(candidates);
        b_writeallow(ret);

        b_free(dir);
        return ret;
}


/*============================================================================*/


static void
init_filetype(int fd, struct ftdata_s *ft)
{
        if (ft->initialized)
                return;
        pthread_mutex_lock(&ftdata_mutex);

        echo("Init filetype called for ft %s\n", BTS(ft->vim_name));

        ft->initialized = true;
        ft->order = nvim_get_var_fmt(fd, MPACK_STRING, NULL, true,
                                     "tag_highlight#%s#order", BTS(ft->vim_name));

        mpack_array_t *tmp = dict_get_key(settings.ignored_tags,
                                          MPACK_ARRAY, &ft->vim_name, false);
        if (tmp)
                ft->ignored_tags = mpack_array_to_blist(tmp, false);

        mpack_dict_t *equiv = nvim_get_var_fmt(
            fd, MPACK_DICT, NULL, false, "tag_highlight#%s#equivalent", BTS(ft->vim_name));

        if (equiv) {
                ft->equiv = b_list_create_alloc(equiv->qty);

                for (unsigned i = 0; i < equiv->qty; ++i) {
                        bstring *toadd = equiv->entries[i]->key->data.str;
                        b_concat(toadd, equiv->entries[i]->value->data.str);
                        b_writeprotect(toadd);

                        mpack_destroy(equiv->entries[i]->key);
                        mpack_destroy(equiv->entries[i]->value);
                        xfree(equiv->entries[i]);

                        b_writeallow(toadd);
                        b_add_to_list(ft->equiv, toadd);
                }

                xfree(equiv->entries);
                xfree(equiv);
        } else
                ft->equiv = NULL;

        /* ft->restore_cmds             = NULL; */
        /* ft->restore_cmds_initialized = false; */

        pthread_mutex_unlock(&ftdata_mutex);
}
