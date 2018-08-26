#include "util/util.h"

#include "api.h"
#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include <signal.h>

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
#  define DOSCHECK(CH_) \
        ((CH_) == ':' || (CH_) == '/')
#else
#  define DOSCHECK(CH_) (false)
#endif


extern b_list         *seen_files;

static void            log_prev_file(const bstring *filename);
static struct top_dir *init_topdir(int fd, struct bufdata *bdata);
static void            init_filetype(int fd, struct ftdata_s *ft);
static bool            check_norecurse_directories(const bstring *dir);
static bstring        *check_project_directories  (bstring *dir);


bool
new_buffer(const int fd, const int bufnum)
{
        if (!seen_files)
                seen_files = b_list_create_alloc(32);
        if (!top_dirs)
                top_dirs = genlist_create_alloc(32);
        for (unsigned short i = 0; i < buffers.bad_bufs.qty; ++i)
                if (bufnum == buffers.bad_bufs.lst[i])
                        return false;

        struct ftdata_s *tmp = NULL;
        bstring         *ft  = nvim_buf_get_option(fd, bufnum, B("ft"), E_STRING).ptr;
        assert(ft != NULL);

        for (unsigned i = 0; i < ftdata_len; ++i) {
                if (b_iseq(ft, &ftdata[i].vim_name)) {
                        tmp = &ftdata[i];
                        break;
                }
        }

        if (!tmp) {
                ECHO("Can't identify buffer %d, (ft '%s') bailing!\n", bufnum, ft);
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
        struct bufdata *bdata = get_bufdata(fd, bufnum, tmp);
        assert(bdata != NULL);

        if (bdata->ft->id != FT_NONE && !bdata->ft->initialized)
                init_filetype(fd, bdata->ft);

        buffers.lst[buffers.mkr] = bdata;
        NEXT_MKR(buffers);

        return true;
}

struct bufdata *
get_bufdata(const int fd, const int bufnum, struct ftdata_s *ft)
{
        struct bufdata *bdata = xmalloc(sizeof *bdata);
        bdata->calls       = NULL;
        bdata->ctick       = bdata->last_ctick = 0;
        bdata->filename    = nvim_buf_get_name(fd, bufnum);
        bdata->ft          = ft;
        bdata->lastref     = NULL;
        bdata->lines       = ll_make_new();
        bdata->num         = bufnum;
        bdata->thread_pid  = (-1);
        bdata->initialized = false; /* Initialized is true only after recieving the buffer contents. */
        bdata->topdir = init_topdir(fd, bdata); /* Topdir init must be the last step. */

        return bdata;
}


void
destroy_bufdata(struct bufdata **bdata)
{
        extern bool process_exiting;
        if (!*bdata)
                return;
        if (!process_exiting)
                log_prev_file((*bdata)->filename);
        const int index = find_buffer_ind((*bdata)->num);

        b_destroy((*bdata)->filename);
        ll_destroy((*bdata)->lines);
        destroy_call_array((*bdata)->calls);

        if (--((*bdata)->topdir->refs) == 0) {
                struct top_dir *topdir = (*bdata)->topdir;


                close(topdir->tmpfd);
                unlink(BS(topdir->tmpfname));

                b_destroy(topdir->gzfile);
                b_destroy(topdir->pathname);
                b_destroy(topdir->tmpfname);
                b_list_destroy(topdir->tags);

                for (unsigned i = 0; i < top_dirs->qty; ++i)
                        if (top_dirs->lst[i] == topdir)
                                genlist_remove_index(top_dirs, i);
        }

        xfree(*bdata);
        *bdata = NULL;
        buffers.lst[index] = NULL;
}


/*======================================================================================*/


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

static void
log_prev_file(const bstring *filename)
{
        if (have_seen_file(filename))
                return;
        b_list_append(&seen_files, b_strcpy(filename));
}

bool
have_seen_file(const bstring *filename)
{
        unsigned i;
        B_LIST_FOREACH(seen_files, file, i)
                if (b_iseq(file, filename))
                        return true;
        return false;
}


/*======================================================================================*/


static struct top_dir *
init_topdir(const int fd, struct bufdata *bdata)
{
        /* Emulate dirname() */
        int           ret  = (-1);
        const int64_t pos  = b_strrchr(bdata->filename, SEPCHAR);
        bstring      *dir  = b_strcpy(bdata->filename);

        if (pos < 0)
                abort();

        dir->data[pos]     = '\0';
        dir->slen          = pos;
        dir                = check_project_directories(dir);
        const bool recurse = check_norecurse_directories(dir);
        const bool is_c    = bdata->ft->id == FT_C || bdata->ft->id == FT_CPP;
        bstring   *base    = (!recurse || is_c) ? b_strcpy(bdata->filename) : dir;

        for (unsigned i = 0; i < top_dirs->qty; ++i) {
                echo("Comparing '%s' to '%s'",
                     BS(((struct top_dir *)top_dirs->lst[i])->pathname), BS(base));

                if (b_iseq(((struct top_dir *)top_dirs->lst[i])->pathname, base)) {
                        ((struct top_dir *)top_dirs->lst[i])->refs++;
                        b_destroy(base);
                        return top_dirs->lst[i];
                }
        }

        ECHO("Initializing new topdir \"%s\"\n", dir);

        struct top_dir *tmp = xmalloc(sizeof(struct top_dir));
        tmp->tmpfname = nvim_call_function(fd, B("tempname"), E_STRING).ptr;
        tmp->tmpfd    = safe_open(BS(tmp->tmpfname), O_CREAT|O_RDWR|O_BINARY, 0600);
        tmp->gzfile   = b_fromcstr_alloc(dir->mlen * 3, HOME);
        tmp->ftid     = bdata->ft->id;
        tmp->index    = top_dirs->qty;
        tmp->is_c     = is_c;
        tmp->pathname = dir;
        tmp->recurse  = recurse;
        tmp->refs     = 1;
        tmp->tags     = NULL;

        tmp->pathname->flags |= BSTR_DATA_FREEABLE;
        b_catlit(tmp->gzfile, SEPSTR ".vim_tags" SEPSTR);

        if (tmp->tmpfd == (-1))
                errx(1, "Failed to open temporary file!");

        {       /* Set the vim 'tags' option. */
                char buf[8192];
                size_t n = snprintf(buf, 8192, "set tags+=%s", BS(tmp->tmpfname));
                nvim_command(fd, btp_fromblk(buf, n));
        }

        for (unsigned i = 0; i < base->slen && i < tmp->gzfile->mlen; ++i) {
                if (base->data[i] == SEPCHAR || DOSCHECK(base->data[i])) {
                        tmp->gzfile->data[tmp->gzfile->slen++] = '_';
                        tmp->gzfile->data[tmp->gzfile->slen++] = '_';
                } else
                        tmp->gzfile->data[tmp->gzfile->slen++] = base->data[i];
        }

        if (settings.comp_type == COMP_GZIP)
                ret = B_sprintfa(tmp->gzfile, ".%s.tags.gz", &bdata->ft->vim_name);
        else if (settings.comp_type == COMP_LZMA)
                ret = B_sprintfa(tmp->gzfile, ".%s.tags.xz", &bdata->ft->vim_name);
        else
                ret = B_sprintfa(tmp->gzfile, ".%s.tags", &bdata->ft->vim_name);

        assert(ret == BSTR_OK);

        genlist_append(top_dirs, tmp);
        if (!recurse || is_c)
                b_free(base);

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
        FILE *fp = fopen(BS(settings.settings_file), "rb");
        if (!fp)
                return dir;

        b_list  *candidates = b_list_create();
        bstring *tmp;

        while ((tmp = B_GETS(fp, '\n', false))) {
                if (strstr(BS(dir), BS(tmp)))
                        b_list_append(&candidates, tmp);
                else
                        b_destroy(tmp);
        }

        fclose(fp);
        if (candidates->qty == 0) {
                b_list_destroy(candidates);
                return dir;
        }

        unsigned i = 0, x = 0;
        for (; i < candidates->qty; ++i)
                if (candidates->lst[i]->slen > candidates->lst[x]->slen)
                        x = i;

        bstring *ret = candidates->lst[x];
        b_writeprotect(ret);
        b_list_destroy(candidates);
        b_writeallow(ret);

        b_free(dir);
        return ret;
}


/*======================================================================================*/


static void
init_filetype(const int fd, struct ftdata_s *ft)
{
        static pthread_mutex_t ftdata_mutex = PTHREAD_MUTEX_INITIALIZER;
        if (ft->initialized)
                return;
        pthread_mutex_lock(&ftdata_mutex);
        ft->initialized = true;

        ECHO("Init filetype called for ft %s\n", &ft->vim_name);

        ft->order          = nvim_get_var_fmt(fd, E_STRING, "tag_highlight#%s#order",
                                              BTS(ft->vim_name)).ptr;
        mpack_array_t *tmp = dict_get_key(settings.ignored_tags,
                                          E_MPACK_ARRAY, &ft->vim_name).ptr;

        ft->ignored_tags    = (tmp) ? mpack_array_to_blist(tmp, false) : NULL;
        mpack_dict_t *equiv = nvim_get_var_fmt(fd, E_MPACK_DICT,
                                               "tag_highlight#%s#equivalent",
                                               BTS(ft->vim_name)).ptr;

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
                        b_list_append(&ft->equiv, toadd);
                }

                xfree(equiv->entries);
                xfree(equiv);
        } else {
                ECHO("No equiv...");
                ft->equiv = NULL;
        }

        pthread_mutex_unlock(&ftdata_mutex);
}
