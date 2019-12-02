#include "Common.h"
#include "highlight.h"
#include <signal.h>
#include <sys/stat.h>

#ifdef DOSISH
#  define SEPCHAR '\\'
#  define SEPSTR "\\"
#  define FUTEX_INITIALIZER(VAL) (p99_futex) P99_FUTEX_INITIALIZER(VAL)
#else
#  define FUTEX_INITIALIZER(VAL) P99_FUTEX_INITIALIZER(VAL)
#  define SEPCHAR '/'
#  define SEPSTR "/"
#endif

#define NEXT_MKR(STRUCT_)                                                              \
        do {                                                                           \
                int const init_ = (STRUCT_).mkr++;                                     \
                                                                                       \
                while ((STRUCT_).lst[(STRUCT_).mkr] != NULL && (STRUCT_).mkr != init_) \
                        (STRUCT_).mkr = ((STRUCT_).mkr + 1) % (STRUCT_).mlen;          \
                                                                                       \
                assert((STRUCT_).lst[(STRUCT_).mkr] == NULL);                          \
        } while (0)

#ifdef DOSISH
#  define DOSCHECK(CH_) ((CH_) == ':' || (CH_) == '/')
#else
#  define DOSCHECK(CH_) (false)
#endif

typedef struct filetype Filetype;
typedef struct top_dir  Top_Dir;

extern b_list *seen_files;

static void     log_prev_file(bstring const *filename);
static Top_Dir *init_topdir(Buffer *bdata);
static void     init_filetype(Filetype *ft);
static bool     check_norecurse_directories(bstring const *dir) __attribute__((pure));
static bstring *check_project_directories(bstring *dir, Filetype const *ft);
static void     bufdata_constructor(void) __attribute__((constructor));

static pthread_mutex_t     ftdata_mutex;
static p99_futex  volatile destruction_futex[DATA_ARRSIZE];

static struct seen_bufnum_stack {
        int data[DATA_ARRSIZE];
        int i;
} seen_bufnum_stack;

/* 
 * Actually initializing these things seems mandatory on Windows.
 */
__attribute__((constructor)) static void
mutex_constructor(void)
{
        pthread_mutex_init(&ftdata_mutex);
        for (int i = 0; i < DATA_ARRSIZE; ++i)
                p99_futex_init((p99_futex *)&destruction_futex[i], 0);
}

/*======================================================================================*/

Buffer *
new_buffer(int const bufnum)
{
        pthread_mutex_lock(&buffers.lock);
        if (buffers.lst[bufnum]) {
                pthread_mutex_unlock(&buffers.lock);
                p99_futex_wait(&destruction_futex[bufnum]);
                pthread_mutex_lock(&buffers.lock);
        }

        Buffer   *ret = NULL;
        Filetype *tmp = NULL;
        bstring  *ft  = nvim_buf_get_option(bufnum, B("ft"), E_STRING).ptr;

        for (unsigned i = 0; i < ftdata_len; ++i) {
                if (b_iseq(ft, &ftdata[i].vim_name)) {
                        tmp = &ftdata[i];
                        break;
                }
        }
        if (!tmp) {
                ECHO("Can't identify buffer %d, (ft '%s') bailing!", bufnum, ft);
                goto skip_buffer;
        }
        if (bufnum >= buffers.mlen) {
                SHOUT("Too many open buffers.");
                goto skip_buffer;
        }
        for (unsigned i = 0; i < settings.ignored_ftypes->qty; ++i)
                if (b_iseq(ft, settings.ignored_ftypes->lst[i]))
                        goto skip_buffer;

        Buffer *bdata = get_bufdata(bufnum, tmp);
        if (bdata->ft->id != FT_NONE && !bdata->ft->initialized)
                init_filetype(bdata->ft);

        {
                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
                pthread_mutex_init(&bdata->lock.total, &attr);
                pthread_mutex_init(&bdata->lock.ctick, &attr);
        }

        p99_count_init((p99_count *)&bdata->lock.num_workers, 0);
        buffers.lst[bufnum] = bdata;
        ret = bdata;

        if (!have_seen_bufnum(bufnum))
                seen_bufnum_stack.data[seen_bufnum_stack.i++] = bufnum;

skip_buffer:
        pthread_mutex_unlock(&buffers.lock);
        b_destroy(ft);
        return ret;
}

void
get_initial_lines(Buffer *bdata)
{
        pthread_mutex_lock(&bdata->lock.total);
        b_list *tmp = nvim_buf_get_lines(bdata->num);
        if (bdata->lines->qty == 1)
                ll_delete_node(bdata->lines, bdata->lines->head);
        ll_insert_blist_after(bdata->lines, bdata->lines->head, tmp, 0, (-1));

        free(tmp->lst);
        free(tmp);
        bdata->initialized = true;
        pthread_mutex_unlock(&bdata->lock.total);
}

Buffer *
get_bufdata(int const bufnum, Filetype *ft)
{
        Buffer *bdata    = calloc(1, sizeof(Buffer));
        bdata->name.full = nvim_buf_get_name(bufnum);
        bdata->name.base = b_basename(bdata->name.full);
        bdata->name.path = b_dirname(bdata->name.full);
        bdata->lines     = ll_make_new();
        bdata->num       = (uint16_t)bufnum;
        bdata->ft        = ft;
        bdata->topdir    = init_topdir(bdata); // Topdir init must be the last step.

        int64_t loc = b_strrchr(bdata->name.base, '.');
        if (loc > 0)
                for (unsigned i = loc, b = 0; i < bdata->name.base->slen && b < 8; ++i, ++b)
                        bdata->name.suffix[b] = bdata->name.base->data[i];

        atomic_store_explicit(&bdata->ctick, 0, memory_order_relaxed);
        atomic_store_explicit(&bdata->last_ctick, 0, memory_order_relaxed);
        atomic_store_explicit(&bdata->is_normal_mode, 0, memory_order_relaxed);

        return bdata;
}

void
destroy_bufdata(Buffer **bdata_p)
{
        extern void destroy_clangdata(Buffer * bdata);
        extern bool process_exiting;
        Buffer *bdata = *bdata_p;
        if (!bdata)
                return;
        pthread_mutex_lock(&buffers.lock);
        pthread_mutex_lock(&bdata->lock.total);
        pthread_mutex_lock(&bdata->lock.ctick);

        if (!process_exiting)
                log_prev_file(bdata->name.full);
        /* int const index = find_buffer_ind(bdata->num); */
        int const index  = bdata->num;
        int const bufnum = bdata->num;

        b_destroy(bdata->name.full);
        b_destroy(bdata->name.base);
        b_destroy(bdata->name.path);
        ll_destroy(bdata->lines);

        if (--bdata->topdir->refs == 0) {
                Top_Dir *topdir = bdata->topdir;
                close(topdir->tmpfd);
                unlink(BS(topdir->tmpfname));

                b_destroy(topdir->gzfile);
                b_destroy(topdir->pathname);
                b_destroy(topdir->tmpfname);
                b_list_destroy(topdir->tags);

                for (unsigned i = 0; i < top_dirs->qty; ++i) {
                        if (top_dirs->lst[i] == topdir) {
                                genlist_remove_index(top_dirs, i);
                                top_dirs->lst[i] = NULL;
                        }
                }
        }

        if (bdata->ft->is_c) {
                if (bdata->clangdata)
                        destroy_clangdata(bdata);
                if (bdata->headers)
                        b_list_destroy(bdata->headers);
        } else {
                if (bdata->calls)
                        mpack_destroy_arg_array(bdata->calls);
        }

        pthread_mutex_unlock(&bdata->lock.ctick);
        pthread_mutex_destroy(&bdata->lock.ctick);
        pthread_mutex_unlock(&bdata->lock.total);
        pthread_mutex_destroy(&bdata->lock.total);

        free(bdata);
        bdata              = NULL;
        buffers.lst[index] = NULL;
        pthread_mutex_unlock(&buffers.lock);
        p99_futex_wakeup(&destruction_futex[bufnum]);
}

/*======================================================================================*/

Buffer *
find_buffer(int const bufnum)
{
        Buffer *ret = NULL;
        pthread_mutex_lock(&buffers.lock);

        if (bufnum < buffers.mlen && buffers.lst[bufnum])
                ret = buffers.lst[bufnum];

        pthread_mutex_unlock(&buffers.lock);
        return ret;
}

static void
log_prev_file(bstring const *filename)
{
        if (have_seen_file(filename))
                return;
        b_list_append(&seen_files, b_strcpy(filename));
}

bool
have_seen_file(bstring const *filename)
{
        unsigned i;
        B_LIST_FOREACH (seen_files, file, i)
        if (b_iseq(file, filename))
                return true;
        return false;
}

bool
have_seen_bufnum(int const bufnum)
{
        for (int n = 0; n < seen_bufnum_stack.i; ++n)
                if (bufnum == seen_bufnum_stack.data[n])
                        return true;
        return false;
}

/*======================================================================================*/

/**
 * This struct is primarily for filetypes that use ctags. It is convenient to run the
 * program once for a project tree and use the results for all files belonging to it, so
 * we run ctags on the `top_dir' of the tree. This routine populates that structure.
 */
static Top_Dir *
init_topdir(Buffer *bdata)
{
        bstring    *dir     = check_project_directories(b_strcpy(bdata->name.path), bdata->ft);
        bool const  recurse = check_norecurse_directories(dir);
        bstring    *base    = (!recurse) ? b_strcpy(bdata->name.full) : dir;

        assert(top_dirs != NULL && top_dirs->lst != NULL && base != NULL);
        ECHO("fname: %s, dir: %s, base: %s", bdata->name.full, dir, base);

        /* Search to see if this topdir is already open. */
        pthread_mutex_lock(&top_dirs->mut);
        for (unsigned i = 0; i < top_dirs->qty; ++i) {
                Top_Dir *cur = top_dirs->lst[i];

                if (!cur || !cur->pathname)
                        continue;
                if (cur->ftid != bdata->ft->id) {
                        echo("cur->ftid (%d - %s) does not equal the current ft (%d - %s), skipping",
                             cur->ftid, BTS(ftdata[cur->ftid].vim_name),
                             bdata->ft->id, BTS(bdata->ft->vim_name));
                        continue;
                }

                if (b_iseq(cur->pathname, base)) {
                        cur->refs++;
                        b_destroy(base);
                        SHOUT("Using already initialized project directory \"%s\"", BS(cur->pathname));
                        pthread_mutex_unlock(&top_dirs->mut);
                        return cur;
                }
        }
        pthread_mutex_unlock(&top_dirs->mut);

        SHOUT("Initializing project directory \"%s\"", BS(dir));

        Top_Dir *tdir   = calloc(1, sizeof(Top_Dir));
        tdir->tmpfname  = nvim_call_function(B("tempname"), E_STRING).ptr;
        tdir->gzfile    = b_strcpy(settings.cache_dir);
        tdir->tmpfd     = safe_open(BS(tdir->tmpfname), O_CREAT|O_RDWR|O_TRUNC|O_BINARY, 0600);
        tdir->ftid      = bdata->ft->id;
        tdir->index     = top_dirs->qty;
        tdir->pathname  = dir;
        tdir->recurse   = recurse;
        tdir->refs      = 1;
        tdir->timestamp = (time_t)0;

        if (tdir->tmpfd == (-1))
                errx(1, "Failed to open temporary file!");

        tdir->pathname->flags |= BSTR_DATA_FREEABLE;
        b_catlit(tdir->gzfile, SEPSTR "tags" SEPSTR);

        { /* Make sure the ctags cache directory exists. */
                struct stat st;
                if (stat(BS(tdir->gzfile), &st) != 0)
                        if (mkdir(BS(tdir->gzfile), 0755) != 0)
                                err(1, "Failed to create cache directory");

        }
        { /* Set the vim 'tags' option. */
                char buf[8192];
                size_t n = snprintf(buf, 8192, "set tags+=%s", BS(tdir->tmpfname));
                nvim_command(btp_fromblk(buf, n));
        }

        ECHO("Base is %s", base);

        /* Calling b_conchar lots of times is less efficient than just writing
         * to the string, but for an small operation like this it is better to
         * be safe than sorry. */
        for (unsigned i = 0; i < base->slen && i < tdir->gzfile->mlen; ++i) {
                if (base->data[i] == SEPCHAR || DOSCHECK(base->data[i]))
                        b_catlit(tdir->gzfile, "__");
                else
                        b_conchar(tdir->gzfile, base->data[i]);
        }

        ECHO("got %s", tdir->gzfile);

        if (settings.comp_type == COMP_GZIP)
                b_sprintfa(tdir->gzfile, ".%s.tags.gz", &bdata->ft->vim_name);
#ifdef LZMA_SUPPORT
        else if (settings.comp_type == COMP_LZMA)
                b_sprintfa(tdir->gzfile, ".%s.tags.xz", &bdata->ft->vim_name);
#endif
        else
                b_sprintfa(tdir->gzfile, ".%s.tags", &bdata->ft->vim_name);

        genlist_append(top_dirs, tdir);
        if (!recurse)
                b_destroy(base);

        return tdir;

#undef DIRSTR
}

/**
 * For filetypes that use ctags to find highlight candidates we normally need to
 * run the program recursively on a directory. For some directories this is
 * undesirable (eg. $HOME, /, /usr/include, or something like C:\, etc) because
 * it will take a long time. Check the user provided list of such directories
 * and do not recurse ctags if we find a match.
 */
static bool
check_norecurse_directories(bstring const *const dir)
{
        for (unsigned i = 0; i < settings.norecurse_dirs->qty; ++i)
                if (b_iseq(dir, settings.norecurse_dirs->lst[i]))
                        return false;

        return true;
}

/**
 * Check the file `tag_highlight.txt' for any directories the user has specified
 * to be `project' directories. Anything under them in the directory tree will
 * use that directory as its base.
 */

static bstring *
check_project_directories(bstring *dir, Filetype const *ft)
{
        FILE *fp = fopen(BS(settings.settings_file), "rb");
        if (!fp)
                return dir;

        b_list  *candidates = b_list_create();
        bstring *tmp;
        b_regularize_path(dir);

        for (tmp = NULL; ( tmp = B_GETS(fp, '\n', false) ); b_destroy(tmp)) {
                ECHO("Looking at \"%s\"", tmp);
                int64_t n = b_strchr(tmp, '\t');
                if (n < 0) {
                        echo("Got %"PRId64" from strchr, skipping \"%s\"", n, BS(tmp));
                        continue;
                }
                if (n > UINT_MAX)
                        errx(1, "Index %"PRId64" is too large.", n);

                tmp->data[n]      = '\0';
                tmp->slen         = (unsigned)n;
                char const *tmpft = (char *)(tmp->data + n + 1);

                if (ft->is_c) {
                        if (!STREQ(tmpft, "c") && !STREQ(tmpft, "cpp"))
                                continue;
                } else {
                        if (!STREQ(tmpft, BTS(ft->vim_name)))
                                continue;
                }

                b_regularize_path(tmp);

                if (strstr(BS(dir), BS(tmp))) {
                        ECHO("Success! Matched \"%s\" and its ft \"%n\"", tmp, tmpft);
                        b_writeprotect(tmp);
                        b_list_append(&candidates, tmp);
                        continue;
                }

                ECHO("Failed to match \"%s\" with ft \"%s\" at all", tmp, &ft->vim_name);
        }

        fclose(fp);
        if (candidates->qty == 0) {
                b_list_destroy(candidates);
                return dir;
        }


        /* Find the longest match */
        bstring *longest = candidates->lst[0];
        for (unsigned i = 0; i < candidates->qty; ++i)
                if (candidates->lst[i]->slen > longest->slen)
                        longest = candidates->lst[i];

        b_list_writeallow(candidates);
        b_writeprotect(longest);
        b_list_destroy(candidates);
        b_writeallow(longest);
        b_destroy(dir);

        return longest;
}

static void
bufdata_constructor(void)
{
        seen_files = b_list_create_alloc(32);
        top_dirs   = genlist_create_alloc(32);
}


/*======================================================================================*/

static void     get_ignored_tags(Filetype *ft);
static void     get_tags_from_restored_groups(Filetype *ft, b_list *restored_groups);
static bstring *get_restore_cmds(b_list *restored_groups);

/**
 * Populate the non-static portions of the filetype structure, including a list of special
 * tags and groups of tags that should be ignored by this program when highlighting.
 */
static void
init_filetype(Filetype *ft)
{
        if (ft->initialized)
                return;
        pthread_mutex_lock(&ftdata_mutex);

        ft->initialized  = true;
        ft->order        = nvim_get_var_fmt(E_STRING, PKG "%s#order", BTS(ft->vim_name)).ptr;
        mpack_array *tmp = mpack_dict_get_key(settings.ignored_tags, E_MPACK_ARRAY,
                                              &ft->vim_name).ptr;

        ECHO("Init filetype called for ft %s", &ft->vim_name);

        if (tmp) {
                ft->ignored_tags = mpack_array_to_blist(tmp, false);
                B_LIST_SORT_FAST(ft->ignored_tags);
        } else
                ft->ignored_tags = NULL;

        ft->restore_cmds = NULL;
        get_ignored_tags(ft);

        mpack_dict *equiv = nvim_get_var_fmt(E_MPACK_DICT, PKG "%s#equivalent",
                                             BTS(ft->vim_name)).ptr;
        if (equiv) {
                ft->equiv = b_list_create_alloc(equiv->qty);

                for (unsigned i = 0; i < equiv->qty; ++i) {
                        mpack_dict_ent *ent   = equiv->entries[i];
                        bstring        *toadd = ent->key->data.str;
                        b_concat(toadd, ent->value->data.str);

                        b_writeprotect(toadd);
                        mpack_destroy_object(ent->key);
                        mpack_destroy_object(ent->value);
                        free(ent);
                        b_writeallow(toadd);

                        b_list_append(&ft->equiv, toadd);
                }

                free(equiv->entries);
                free(equiv);
        } else
                ft->equiv = NULL;

        pthread_mutex_unlock(&ftdata_mutex);
}

static void
get_ignored_tags(Filetype *ft)
{
        mpack_dict *tmp             = nvim_get_var(B(PKG "restored_groups"), E_MPACK_DICT).ptr;
        b_list     *restored_groups = mpack_dict_get_key(tmp, E_STRLIST, &ft->vim_name).ptr;

        if (restored_groups) {
                b_list_writeprotect(restored_groups);
                mpack_dict_destroy(tmp);
                b_list_writeallow(restored_groups);

                if (ft->has_parser) {
                        get_tags_from_restored_groups(ft, restored_groups);
                        B_LIST_SORT_FAST(ft->ignored_tags);
                } else
                        ft->restore_cmds = get_restore_cmds(restored_groups);

                b_list_destroy(restored_groups);
        } else
                mpack_dict_destroy(tmp);

        ft->restore_cmds_initialized = true;
}

static void
get_tags_from_restored_groups(Filetype *ft, b_list *restored_groups)
{
        if (!ft->ignored_tags)
                ft->ignored_tags = b_list_create();

        ECHO("Getting ignored tags for ft %d", ft->id);

        for (unsigned i = 0; i < restored_groups->qty; ++i) {
                char         cmd[4096], *ptr;
                size_t const len = snprintf(cmd, 4096, "syntax list %s",
                                            BS(restored_groups->lst[i]));
                bstring *output = nvim_command_output(btp_fromblk(cmd, len), E_STRING).ptr;

                if (!output)
                        continue;
                if (!(ptr = strstr(BS(output), "xxx"))) {
                        b_destroy(output);
                        continue;
                }

                uchar *bak    = output->data;
                output->data  = (uchar *)(ptr += 4); /* Add 4 to skip the "xxx " */
                output->slen -= (unsigned)PSUB(ptr, bak);

                if (!b_starts_with(output, B("match /"))) {
                        bstring line = BSTR_WRITEABLE_STATIC_INIT;

                        while (b_memsep(&line, output, '\n')) {
                                while (isblank(*line.data)) {
                                        ++line.data;
                                        --line.slen;
                                }
                                if (b_starts_with(&line, B("links to ")))
                                        break;

                                bstring tok = BSTR_STATIC_INIT;

                                while (b_memsep(&tok, &line, ' ')) {
                                        bstring *toadd = b_fromblk(tok.data, tok.slen);
                                        toadd->flags  |= BSTR_MASK_USR1;
                                        b_list_append(&ft->ignored_tags, toadd);
                                }
                        }
                }

                free(bak);
                free(output);
        }
}

static bstring *
get_restore_cmds(b_list *restored_groups)
{
        assert(restored_groups);
        b_list *allcmds = b_list_create_alloc(restored_groups->qty);

        for (unsigned i = 0; i < restored_groups->qty; ++i) {
                bstring *cmd    = b_sprintf("syntax list %s", restored_groups->lst[i]);
                bstring *output = nvim_command_output(cmd, E_STRING).ptr;
                b_destroy(cmd);
                if (!output)
                        continue;

                char *ptr = strstr(BS(output), "xxx");
                if (!ptr) {
                        b_destroy(output);
                        continue;
                }

                cmd  = b_alloc_null(64u + output->slen);
                ptr += 4;
                ALWAYS_ASSERT(!isblank(*ptr));
                b_sprintfa(cmd, "syntax clear %s | ", restored_groups->lst[i]);

                b_list *toks = b_list_create();

                /* Only syntax keywords can replace previously supplied items,
                 * so just ignore any match groups. */
                if (strncmp(ptr, SLS("match /")) != 0) {
                        char *tmp;
                        char link_name[1024];
                        b_sprintfa(cmd, "syntax keyword %s ", restored_groups->lst[i]);

                        while ((tmp = strchr(ptr, '\n'))) {
                                b_list_append(&toks, b_fromblk(ptr, PSUB(tmp, ptr)));
                                while (isblank(*++tmp))
                                        ;
                                if (strncmp((ptr = tmp), "links to ", 9) == 0
                                            && !((tmp = strchr(ptr, '\n')) + 1))
                                        break;
                        }

                        b_list_remove_dups(&toks);
                        for (unsigned x = 0; x < toks->qty; ++x)
                                b_append_all(cmd, toks->lst[x], B(" "));

                        b_list_destroy(toks);

                        size_t const n = my_strlcpy(link_name, (ptr += 9), sizeof(link_name));
                        ALWAYS_ASSERT(n > 0);
                        b_sprintfa(cmd, " | hi! link %s %n",
                                   restored_groups->lst[i], link_name);

                        b_list_append(&allcmds, cmd);
                }

                b_destroy(output);
        }

        bstring *ret = b_join(allcmds, B(" | "));
        b_list_destroy(allcmds);
        return ret;
}
