#include "tag_highlight.h"

#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "my_p99_common.h"
#include <signal.h>

#ifdef DOSISH
#  define SEPCHAR '\\'
#  define SEPSTR  "\\"
#  define FUTEX_INITIALIZER(VAL) (p99_futex)P99_FUTEX_INITIALIZER(VAL)
#else
#  define FUTEX_INITIALIZER(VAL) P99_FUTEX_INITIALIZER(VAL)
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
static void            init_filetype(int fd, struct filetype *ft);
static bool            check_norecurse_directories(const bstring *dir);
static bstring        *check_project_directories  (bstring *dir);
static void            bufdata_constructor(void) __attribute__((__constructor__));

/* #include "my_p99_common.h" */

/*======================================================================================*/

bool
(new_buffer)(const int fd, const int bufnum)
{
        bool ret = false;
        pthread_rwlock_wrlock(&buffers.lock);

        for (unsigned short i = 0; i < buffers.bad_bufs.qty; ++i)
                if (bufnum == buffers.bad_bufs.lst[i])
                        goto end;

        struct filetype *tmp = NULL;
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
                goto end;
        }
        for (unsigned i = 0; i < settings.ignored_ftypes->qty; ++i) {
                if (b_iseq(ft, settings.ignored_ftypes->lst[i])) {
                        buffers.bad_bufs.lst[buffers.bad_bufs.qty++] = bufnum;
                        goto end;
                }
        }

        b_destroy(ft);
        struct bufdata *bdata = get_bufdata(fd, bufnum, tmp);
        assert(bdata != NULL);
        if (bdata->ft->id != FT_NONE && !bdata->ft->initialized)
                init_filetype(fd, bdata->ft);

        {
                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
                pthread_mutex_init(&bdata->lock.total, &attr);
                pthread_mutex_init(&bdata->lock.ctick, &attr);
                pthread_mutex_init(&bdata->lock.update, &attr);
                bdata->lock.clang_count = FUTEX_INITIALIZER(0);
                /* pthread_mutex_init(&bdata->lock.clang, &attr); */
                /* bdata->lock.clang_flg = (atomic_flag)ATOMIC_FLAG_INIT; */
        }

        buffers.lst[buffers.mkr] = bdata;
        NEXT_MKR(buffers);
        ret = true;
end:
        pthread_rwlock_unlock(&buffers.lock);
        return ret;
}

struct bufdata *
get_bufdata(const int fd, const int bufnum, struct filetype *ft)
{
        struct bufdata *bdata = xcalloc(1, sizeof(struct bufdata));
        bdata->name.full      = nvim_buf_get_name(fd, bufnum);
        bdata->name.base      = b_basename(bdata->name.full);
        bdata->name.path      = b_dirname(bdata->name.full);
        bdata->lines          = ll_make_new();
        bdata->num            = (uint16_t)bufnum;
        bdata->ft             = ft;
        bdata->topdir         = init_topdir(fd, bdata); /* Topdir init must be the last step. */

        int64_t loc = b_strrchr(bdata->name.base, '.');
        if (loc > 0)
                for (unsigned i = loc, b = 0; i < bdata->name.base->slen && b < 8; ++i, ++b)
                        bdata->name.suffix[b] = bdata->name.base->data[i];
        
        atomic_store(&bdata->ctick, 0);
        atomic_store(&bdata->last_ctick, 0);

        return bdata;
}

void
destroy_bufdata(struct bufdata **bdata)
{
        extern void destroy_clangdata(struct bufdata *bdata);
        extern bool process_exiting;
        static pthread_mutex_t destruction_mutex = PTHREAD_MUTEX_INITIALIZER;

        if (!*bdata)
                return;
        pthread_mutex_lock(&destruction_mutex);

        if (!process_exiting) {
                log_prev_file((*bdata)->name.full);
                clear_highlight(*bdata);
        }
        const int index = find_buffer_ind((*bdata)->num);

        pthread_mutex_destroy(&(*bdata)->lock.total);
        pthread_mutex_destroy(&(*bdata)->lock.ctick);
        pthread_mutex_destroy(&(*bdata)->lock.update);

        b_destroy((*bdata)->name.full);
        b_destroy((*bdata)->name.base);
        b_destroy((*bdata)->name.path);
        ll_destroy((*bdata)->lines);

        if ((*bdata)->ft->is_c) {
                if ((*bdata)->clangdata)
                        destroy_clangdata(*bdata);
                if ((*bdata)->headers)
                        b_list_destroy((*bdata)->headers);
        } else {
                if ((*bdata)->calls)
                        _nvim_destroy_arg_array((*bdata)->calls);
        }

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

        pthread_rwlock_wrlock(&buffers.lock);
        buffers.lst[index] = NULL;
        pthread_rwlock_unlock(&buffers.lock);
        pthread_mutex_unlock(&destruction_mutex);
}

/*======================================================================================*/

struct bufdata *
find_buffer(const int bufnum)
{
        struct bufdata *ret = NULL;
        pthread_rwlock_rdlock(&buffers.lock);

        for (unsigned i = 0; i < buffers.mlen; ++i) {
                if (buffers.lst[i] && buffers.lst[i]->num == bufnum) {
                        ret = buffers.lst[i];
                        break;
                }
        }
        pthread_rwlock_unlock(&buffers.lock);
        return ret;
}

int
find_buffer_ind(const int bufnum)
{
        int ret = (-1);
        pthread_rwlock_rdlock(&buffers.lock);

        for (unsigned i = 0; i < buffers.mlen; ++i) {
                if (buffers.lst[i] && buffers.lst[i]->num == bufnum) {
                        ret = (int)i;
                        break;
                }
        }
        pthread_rwlock_unlock(&buffers.lock);
        return ret;
}

struct bufdata *
null_find_bufdata(int bufnum, struct bufdata *bdata)
{
        if (!bdata) {
                if (bufnum == (-1))
                        bufnum = nvim_get_current_buf();
                bdata = find_buffer(bufnum);
        }
        assert(bdata != NULL && !is_bad_buffer(bufnum));

        return bdata;
}

bool
is_bad_buffer(const int bufnum)
{
        bool ret = false;
        pthread_rwlock_rdlock(&buffers.lock);

        for (unsigned i = 0; i < buffers.bad_bufs.qty; ++i) {
                if (bufnum == buffers.bad_bufs.lst[i]) {
                        ret = true;
                        break;
                }
        }
        pthread_rwlock_unlock(&buffers.lock);
        return ret;
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

/**
 * This struct is primarily for filetypes that use ctags. It is convenient to run the
 * program once for a project tree and use the results for all files belonging to it, so
 * we run ctags on the `top_dir' of the tree. This routine populates that structure.
 */
static struct top_dir *
init_topdir(const int fd, struct bufdata *bdata)
{
        int      ret       = (-1);
        bstring *dir       = b_strcpy(bdata->name.path);
        dir                = check_project_directories(dir);
        const bool recurse = check_norecurse_directories(dir);
        const bool is_c    = bdata->ft->is_c;
        bstring   *base    = (!recurse || is_c) ? b_strcpy(bdata->name.full) : dir;

        assert(top_dirs != NULL && top_dirs->lst != NULL && base != NULL);
        ECHO("fname: %s, dir: %s, base: %s\n", bdata->name.full, dir, base);

        pthread_mutex_lock(&top_dirs->mut);
        for (unsigned i = 0; i < top_dirs->qty; ++i) {
                if (!top_dirs->lst[i] || !((struct top_dir *)top_dirs->lst[i])->pathname)
                        continue;

                if (b_iseq(((struct top_dir *)top_dirs->lst[i])->pathname, base)) {
                        ((struct top_dir *)top_dirs->lst[i])->refs++;
                        b_destroy(base);
                        echo("returning with topdir %s\n",
                             BS(((struct top_dir *)(top_dirs->lst[i]))->pathname));
                        pthread_mutex_unlock(&top_dirs->mut);
                        return top_dirs->lst[i];
                }
        }
        pthread_mutex_unlock(&top_dirs->mut);
        ECHO("Initializing new topdir \"%s\"\n", dir);

        struct top_dir *tdir = xcalloc(1, sizeof(struct top_dir));
        tdir->tmpfname = nvim_call_function(fd, B("tempname"), E_STRING).ptr;
        tdir->tmpfd    = safe_open(BS(tdir->tmpfname), O_CREAT|O_RDWR|O_BINARY, 0600);
        tdir->gzfile   = b_fromcstr_alloc(dir->mlen * 3, HOME);
        tdir->ftid     = bdata->ft->id;
        tdir->index    = top_dirs->qty;
        tdir->pathname = dir;
        tdir->recurse  = recurse;
        tdir->refs     = 1;

        tdir->pathname->flags |= BSTR_DATA_FREEABLE;
        b_catlit(tdir->gzfile, SEPSTR ".vim_tags" SEPSTR);
        if (tdir->tmpfd == (-1))
                errx(1, "Failed to open temporary file!");

        {       /* Set the vim 'tags' option. */
                char buf[8192];
                size_t n = snprintf(buf, 8192, "set tags+=%s", BS(tdir->tmpfname));
                nvim_command(fd, btp_fromblk(buf, n));
        }

        ECHO("Base is %s", base);

        /* Calling b_conchar lots of times is less efficient than just writing
         * to the string, but for an small operation like this it is better to
         * be safe than sorry. */
        for (unsigned i = 0; i < base->slen && i < tdir->gzfile->mlen; ++i) {
                if (base->data[i] == SEPCHAR || DOSCHECK(base->data[i])) {
                        b_conchar(tdir->gzfile, '_');
                        b_conchar(tdir->gzfile, '_');
                } else {
                        b_conchar(tdir->gzfile, base->data[i]);
                }
        }

        ECHO("got %s", tdir->gzfile);

        if (settings.comp_type == COMP_GZIP)
                ret = b_sprintfa(tdir->gzfile, ".%s.tags.gz", &bdata->ft->vim_name);
        else if (settings.comp_type == COMP_LZMA)
                ret = b_sprintfa(tdir->gzfile, ".%s.tags.xz", &bdata->ft->vim_name);
        else
                ret = b_sprintfa(tdir->gzfile, ".%s.tags", &bdata->ft->vim_name);

        assert(ret == BSTR_OK);
        genlist_append(top_dirs, tdir);
        if (!recurse || is_c)
                b_destroy(base);

        return tdir;
#undef DIRSTR
}

/**
 * For filetypes that use ctags to find highlight candidates we normally need to run the
 * program recursively on a directory. For some directories this is undesirable (eg.
 * $HOME, root, /usr/include, etc) because it will take a long time. Check the user
 * provided list of such directories and do not recurse ctags if we find a match.
 */
static bool
check_norecurse_directories(const bstring *const dir)
{
        for (unsigned i = 0; i < settings.norecurse_dirs->qty; ++i)
                if (b_iseq(dir, settings.norecurse_dirs->lst[i]))
                        return false;

        return true;
}

/** 
 * Check the file `~/.vim_tags/tag_highlight.txt' for any directories the
 * user has specified to be `project' directories. Anything under them in
 * the directory tree will use that directory as its base.
 */
static bstring *
check_project_directories(bstring *dir)
{
        FILE *fp = fopen(BS(settings.settings_file), "rb");
        if (!fp)
                return dir;

        b_list  *candidates = b_list_create();
        bstring *tmp;
        b_regularize_path(dir);

        while ((tmp = B_GETS(fp, '\n', false))) {
                if (strstr(BS(dir), BS(b_regularize_path(tmp))))
                        b_list_append(&candidates, tmp);
                else
                        b_destroy(tmp);
        }
        fclose(fp);
        if (candidates->qty == 0) {
                b_list_destroy(candidates);
                return dir;
        }

        unsigned x, i;
        for (i = x = 0; i < candidates->qty; ++i)
                if (candidates->lst[i]->slen > candidates->lst[x]->slen)
                        x = i;

        bstring *ret = candidates->lst[x];
        b_writeprotect(ret);
        b_list_destroy(candidates);
        b_writeallow(ret);
        b_destroy(dir);
        return ret;
}

/*======================================================================================*/

/**
 * Populate the non-static portions of the filetype structure, including a list of special
 * tags and groups of tags that should be ignored by this program when highlighting.
 */
static void
init_filetype(const int fd, struct filetype *ft)
{
        if (ft->initialized)
                return;
        static pthread_mutex_t ftdata_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&ftdata_mutex);

        ft->initialized = true;
        ECHO("Init filetype called for ft %s\n", &ft->vim_name);

        ft->order           = (nvim_get_var_fmt)(fd, E_STRING, PKG "%s#order", BTS(ft->vim_name)).ptr;
        mpack_array_t *tmp  = dict_get_key(settings.ignored_tags, E_MPACK_ARRAY, &ft->vim_name).ptr;

        if (tmp) {
                ft->ignored_tags = mpack_array_to_blist(tmp, false);
                B_LIST_SORT_FAST(ft->ignored_tags);
        } else {
                ft->ignored_tags = NULL;
        }

        mpack_dict_t *equiv = (nvim_get_var_fmt)(fd, E_MPACK_DICT, PKG "%s#equivalent",
                                                 BTS(ft->vim_name)).ptr;
        if (equiv) {
                ft->equiv = b_list_create_alloc(equiv->qty);

                for (unsigned i = 0; i < equiv->qty; ++i) {
                        bstring *toadd = equiv->entries[i]->key->data.str;
                        b_concat(toadd, equiv->entries[i]->value->data.str);
                        b_writeprotect(toadd);
                        mpack_destroy_object(equiv->entries[i]->key);
                        mpack_destroy_object(equiv->entries[i]->value);
                        xfree(equiv->entries[i]);
                        b_writeallow(toadd);
                        b_list_append(&ft->equiv, toadd);
                }

                xfree(equiv->entries);
                xfree(equiv);
        } else {
                ft->equiv = NULL;
        }

        pthread_mutex_unlock(&ftdata_mutex);
}

static void
bufdata_constructor(void)
{
        seen_files = b_list_create_alloc(32);
        top_dirs   = genlist_create_alloc(32);
}
