#include "Common.h"
#include "highlight.h"
#include "lang/golang/golang.h"

/* #include "buffers.h" */
#include <signal.h>
#include <sys/stat.h>

#ifdef _WIN32
#define SEPCHAR                '\\'
#define SEPSTR                 "\\"
#define FUTEX_INITIALIZER(VAL) (p99_futex) P99_FUTEX_INITIALIZER(VAL)
#else
#define FUTEX_INITIALIZER(VAL) P99_FUTEX_INITIALIZER(VAL)
#define SEPCHAR                '/'
#define SEPSTR                 "/"
#endif

#ifdef _WIN32
#define DOSCHECK(CH_) ((CH_) == ':' || (CH_) == '/')
#else
#define DOSCHECK(CH_) (false)
#endif

#define CTX buffer_talloc_ctx_
void        *buffer_talloc_ctx_ = NULL;
linked_list *buffer_list;

P99_DECLARE_STRUCT(buffer_node);
struct buffer_node {
      unsigned          num;
      bool              isopen;
      pthread_rwlock_t *lock;
      Buffer           *bdata;
};

typedef struct top_dir Top_Dir;

//static p99_futex volatile destruction_futex[DATA_ARRSIZE];
static pthread_mutex_t ftdata_mutex;
static pthread_mutex_t wtf_mutex;

static int      destroy_buffer_node(buffer_node *bnode);
static int      destroy_topdir(Top_Dir *topdir);
static void     init_filetype(Filetype *ft);
static Top_Dir *init_topdir(Buffer *bdata);
static int      destroy_buffer_wrapper(Buffer *bdata);


/* Actually initializing these things seems to be mandatory on Windows. */
INITIALIZER_HACK_N(buffer_c_constructor, 500)
{
      pthread_mutexattr_t attr;
      pthread_mutexattr_init(&attr);
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      pthread_mutex_init(&ftdata_mutex, &attr);
      pthread_mutex_init(&wtf_mutex, &attr);

      //for (int i = 0; i < DATA_ARRSIZE; ++i)
      //      p99_futex_init((p99_futex *)&destruction_futex[i], 0);

      extern void talloc_emergency_library_init(void);
      talloc_emergency_library_init();
      buffer_list = ll_make_new(CTX);
      top_dirs    = ll_make_new(CTX);
}

/*======================================================================================
 * /------------------------------------\
 * |Create new, or find existing, buffer|
 * \------------------------------------/
 *=====================================================================================*/

static inline bool         should_skip_buffer(bstring const *ft) __attribute__((__pure__));
static inline buffer_node *new_buffer_node(unsigned bufnum);
static buffer_node        *find_buffer_node(unsigned bufnum);
static Buffer             *make_new_buffer(buffer_node *bnode);

Buffer *
new_buffer(unsigned const bufnum)
{
      buffer_node *bnode = find_buffer_node(bufnum);
      if (!bnode) {
            bnode = new_buffer_node(bufnum);
            if (!bnode)
                  return NULL;
      }
      Buffer *ret = make_new_buffer(bnode);
      if (ret)
            ll_append(buffer_list, bnode);
      else
            talloc_free(bnode);
      return ret;
}

static Buffer *
make_new_buffer(buffer_node *bnode)
{
      assert(bnode != NULL);
      pthread_rwlock_wrlock(bnode->lock);

      if (bnode->isopen) {
            nvim_err_write(B("Can't open an open buffer!"));
            pthread_rwlock_unlock(bnode->lock);
            return NULL;
      }

      bnode->isopen    = true;
      Buffer   *ret    = NULL;
      Filetype *ft     = NULL;
      bstring  *ftname = nvim_buf_get_option(bnode->num, B("ft"), E_STRING).ptr;
      if (!ftname || ftname->slen == 0)
            goto error;

      for (unsigned i = 0; i < ftdata_len; ++i) {
            if (b_iseq(ftname, &ftdata[i]->vim_name)) {
                  ft = ftdata[i];
                  break;
            }
      }

      if (ft && !should_skip_buffer(ftname)) {
            bnode->bdata = ret = get_bufdata(bnode->num, ft);
            if (!ret)
                  goto error;
            talloc_steal(bnode, ret);
            talloc_set_destructor(ret, destroy_buffer_wrapper);
            unsigned tmp = nvim_buf_get_changedtick(ret->num);
            p99_futex_init(&ret->ctick, tmp);
      }

error:
      talloc_free(ftname);
      pthread_rwlock_unlock(bnode->lock);
      return ret;
}

static buffer_node *
find_buffer_node(unsigned const bufnum)
{
      buffer_node *bnode = NULL;
      pthread_mutex_lock(&buffer_list->lock);

      LL_FOREACH_F (buffer_list, node) {
            buffer_node *bnode_tmp = node->data;

            pthread_rwlock_rdlock(bnode_tmp->lock);
            if (bnode_tmp->num == bufnum)
                  bnode = bnode_tmp;
            pthread_rwlock_unlock(bnode_tmp->lock);

            if (bnode)
                  break;
      }

      pthread_mutex_unlock(&buffer_list->lock);
      return bnode;
}

static inline buffer_node *
new_buffer_node(unsigned const bufnum)
{
      buffer_node *bnode = talloc(CTX, buffer_node);
      bnode->lock        = talloc(bnode, pthread_rwlock_t);
      bnode->bdata       = NULL;
      bnode->isopen      = false;
      bnode->num         = bufnum;
      pthread_rwlock_init(bnode->lock, NULL);
      talloc_set_destructor(bnode, destroy_buffer_node);
      return bnode;
}

static inline bool
should_skip_buffer(bstring const *ft)
{
      for (unsigned i = 0; i < settings.ignored_ftypes->qty; ++i)
            if (b_iseq(ft, settings.ignored_ftypes->lst[i]))
                  return true;
      return false;
}

/*--------------------------------------------------------------------------------------*/

static inline void init_buffer_mutexes(Buffer *bdata);

Buffer *
get_bufdata(unsigned const bufnum, Filetype *ft)
{
      Buffer *bdata    = talloc_zero(CTX, Buffer);
      bdata->name.full = nvim_buf_get_name(bufnum);
      bdata->name.base = b_basename(bdata->name.full);
      bdata->name.path = b_dirname(bdata->name.full);
      bdata->lines     = ll_make_new(bdata);
      bdata->num       = bufnum;
      bdata->ft        = ft;
      bdata->topdir    = init_topdir(bdata); // Topdir init must be the last step.

      talloc_steal(bdata, bdata->name.full);
      talloc_steal(bdata, bdata->name.base);
      talloc_steal(bdata, bdata->name.path);
      b_regularize_path_sep(bdata->name.full, '\\');
      b_regularize_path_sep(bdata->name.base, '\\');
      b_regularize_path_sep(bdata->name.path, '\\');

      int64_t loc = b_strrchr(bdata->name.base, '.');
      if (loc > 0)
            for (unsigned i = loc, b = 0; i < bdata->name.base->slen && b < 8; ++i, ++b)
                  bdata->name.suffix[b] = (char)bdata->name.base->data[i];

      p99_futex_exchange(&bdata->ctick, 0);
      atomic_store_explicit(&bdata->last_ctick, 0, memory_order_relaxed);
      init_buffer_mutexes(bdata);

      if (bdata->ft->id != FT_NONE && !bdata->ft->initialized)
            init_filetype(bdata->ft);
      if (bdata->ft->id == FT_GO) {
            golang_buffer_init(bdata);
      }

      return bdata;
}

static inline void
init_buffer_mutexes(Buffer *bdata)
{
      {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&bdata->lock.total, &attr);
            pthread_mutex_init(&bdata->lock.lang_mtx, &attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
#ifndef _WIN32
            pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
#endif
      }

      p99_count_init((p99_count *)&bdata->lock.hl_waiters, 0);
      p99_count_init((p99_count *)&bdata->lock.num_workers, 0);
      p99_futex_init(&bdata->ctick, 0);
      p99_futex_init(&bdata->ctick, 0);
}

/*--------------------------------------------------------------------------------------*/

Buffer *
find_buffer(unsigned const bufnum)
{
      Buffer      *ret   = NULL;
      buffer_node *bnode = find_buffer_node(bufnum);
      if (bnode) {
            pthread_rwlock_rdlock(bnode->lock);
            if (bnode->bdata && bnode->isopen)
                  ret = bnode->bdata;
            pthread_rwlock_unlock(bnode->lock);
      }

      return ret;
}

bool
have_seen_bufnum(unsigned const bufnum)
{
      buffer_node *bnode = find_buffer_node(bufnum);
      return bnode != NULL;
}

void
get_initial_lines(Buffer *bdata)
{
      pthread_mutex_lock(&bdata->lock.total);
      b_list *tmp = nvim_buf_get_lines(bdata->num);
      if (bdata->lines->qty == 1)
            ll_delete_node(bdata->lines, bdata->lines->head);
      ll_insert_blist_after(bdata->lines, bdata->lines->head, tmp, 0, (-1));

      talloc_free(tmp);
      atomic_store(&bdata->initialized, true);
      pthread_mutex_unlock(&bdata->lock.total);
}

void
clear_bnode(void *vdata, bool blocking)
{
      buffer_node *bnode = vdata;
      if (bnode && bnode->bdata)
            clear_highlight(bnode->bdata, blocking);
}

/*======================================================================================
 * /-----------------------------------------\
 * |Create, initialize, or find a project dir|
 * \-----------------------------------------/
 *=====================================================================================*/

static bool     check_norecurse_directories(bstring const *dir) __attribute__((__pure__));
static bstring *check_project_directories(bstring *dir, Filetype const *ft);
static Top_Dir *check_open_topdirs(Buffer const *bdata, bstring const *base);
static void     get_tag_filename(bstring *gzfile, bstring const *base, Buffer *bdata);

static inline void ensure_cache_directory(char const *dir);
static inline void set_vim_tags_opt(char const *fname);

/*
 * This struct is primarily for filetypes that use ctags. It is convenient to run the
 * program once for a project tree and use the results for all files belonging to it, so
 * we run ctags on the `top_dir' of the tree. This routine populates that structure.
 */
static Top_Dir *
init_topdir(Buffer *bdata)
{
      bstring   *dir = check_project_directories(b_strcpy(bdata->name.path), bdata->ft);
      bool const recurse = check_norecurse_directories(dir);
      bstring   *base    = (recurse) ? dir : b_strcpy(bdata->name.full);

      assert(top_dirs != NULL && base != NULL);

      /* Search to see if this topdir is already open. */
      Top_Dir *tdir = check_open_topdirs(bdata, base);
      if (tdir) {
            talloc_free(base);
            ++tdir->refs;
            //talloc_reference(bdata, tdir);
            return tdir;
      }

      SHOUT("Initializing project directory \"%s\"", BS(dir));

      bstring *tnam = nvim_call_function(B("tempname"), E_STRING).ptr;
      bstring *cdir = b_strcpy(settings.cache_dir);

      tdir = talloc_zero(CTX, Top_Dir);
      tdir->tmpfd     = safe_open(BS(tnam), O_CREAT | O_RDWR | O_EXCL | O_BINARY | O_CLOEXEC, 0600);
      tdir->gzfile    = talloc_move(tdir, &cdir);
      tdir->pathname  = talloc_move(tdir, &dir);
      tdir->tmpfname  = talloc_move(tdir, &tnam);
      tdir->ftid      = bdata->ft->id;
      tdir->index     = top_dirs->qty;
      tdir->recurse   = recurse;
      tdir->refs      = 1;
      tdir->timestamp = (time_t)0;
      tdir->pathname->flags |= BSTR_DATA_FREEABLE;

      if (tdir->tmpfd == (-1))
            errx(1, "Failed to open temporary file");

      talloc_set_destructor(tdir, destroy_topdir);

      /* Make sure the ctags cache directory exists. */
      ensure_cache_directory(BS(settings.cache_dir));
      set_vim_tags_opt(BS(tdir->tmpfname));
      get_tag_filename(tdir->gzfile, base, bdata);
      ll_append(top_dirs, tdir);
      if (!recurse)
            talloc_free(base);

      return tdir;
}

static Top_Dir *
check_open_topdirs(Buffer const *bdata, bstring const *base)
{
      Top_Dir *ret = NULL;
      pthread_mutex_lock(&top_dirs->lock);

      LL_FOREACH_F (top_dirs, node) {
            Top_Dir *cur = node->data;

            if (!cur || !cur->pathname || cur->ftid != bdata->ft->id)
                  continue;
            if (b_iseq(cur->pathname, base)) {
                  ECHO("Using already initialized project directory \"%s\"", cur->pathname);
                  ret = cur;
                  break;
            }
      }

      pthread_mutex_unlock(&top_dirs->lock);
      return ret;
}

/*--------------------------------------------------------------------------------------*/

/*
 * For filetypes that use ctags to find highlight candidates we normally need to
 * run the program recursively on a directory. For some directories this is
 * undesirable (eg. $HOME, /, /usr/include, or something like C:\, etc) because
 * it will take a long time. Check the user provided list of such directories
 * and do not recurse ctags if we find a match.
 *
 * This only has to be done once when opening a project, so it won't hurt much to just
 * use a simple linear search.
 */
static bool
check_norecurse_directories(bstring const *const dir)
{
      for (unsigned i = 0; i < settings.norecurse_dirs->qty; ++i)
            if (b_iseq(dir, settings.norecurse_dirs->lst[i]))
                  return false;

      return true;
}

static void
get_tag_filename(bstring *gzfile, bstring const *base, Buffer *const bdata)
{
      b_catlit(gzfile, SEPSTR "tags" SEPSTR);

      /* Calling b_conchar lots of times is less efficient than just writing
       * to the string, but for an small operation like this it is better to
       * be safe than sorry. */
      for (unsigned i = 0; i < base->slen && i < gzfile->mlen; ++i) {
            if (base->data[i] == SEPCHAR || DOSCHECK(base->data[i]))
                  b_catlit(gzfile, "__");
            else
                  b_catchar(gzfile, base->data[i]);
      }

      switch (settings.comp_type) {
      case COMP_LZMA:
#ifdef LZMA_SUPPORT
            b_sprintfa(gzfile, ".%s.tags.xz", &bdata->ft->vim_name);
            break;
#endif
      case COMP_GZIP:
            b_sprintfa(gzfile, ".%s.tags.gz", &bdata->ft->vim_name);
            break;
      case COMP_NONE:
      default:
            b_sprintfa(gzfile, ".%s.tags", &bdata->ft->vim_name);
            break;
      }
}

static inline void
ensure_cache_directory(char const *dir)
{
      /* Make sure the ctags cache directory exists. */
      struct stat st;
      if (stat(dir, &st) != 0)
            if (mkdir(dir, 0755) != 0)
                  err(1, "Failed to create cache directory");
}

static inline void
set_vim_tags_opt(char const *fname)
{
      char     buf[2048];
      bstring *oldval = nvim_get_option(B("tags"), E_STRING).ptr;

      int n = snprintf(buf, 2048, "%s,%s", BS(oldval), fname);

      if (!nvim_set_option(B("tags"), btp_fromblk(buf, n)))
            shout("Nvim rejected my option :(");

      talloc_free(oldval);
}

/*--------------------------------------------------------------------------------------*/

static b_list *
get_project_directory_candidates(FILE *fp, bstring const *dir, Filetype const *ft)
{
      b_list * candidates = b_list_create();
      bstring *tmp;

      for (tmp = NULL; (tmp = B_GETS(fp, '\n', false)); talloc_free(tmp)) {
            int64_t n = b_strchr(tmp, '\t');
            if (n < 0)
                  continue;
            if (n > UINT_MAX)
                  errx(1, "Index %" PRId64 " is too large.", n);

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

            if (strstr(BS(dir), BS(tmp))) {
                  b_list_append(candidates, tmp);
                  tmp = NULL;
            }
      }

      return candidates;
}

/*
 * Check the file `tag-highlight.txt' for any directories the user has specified
 * to be `project' directories. Anything under them in the directory tree will
 * use that directory as its base.
 */
static bstring *
check_project_directories(bstring *dir, Filetype const *ft)
{
#ifdef _WIN32
      FILE *fp = fopen(BS(settings.settings_file), "rb");
#else
      /* "reb" -> 'e' = O_CLOEXEC. Nobody ever told me about that. */
      FILE *fp = fopen(BS(settings.settings_file), "rbem");
#endif
      if (!fp)
            return dir;
      b_list *candidates = get_project_directory_candidates(fp, dir, ft);
      fclose(fp);

      if (candidates->qty == 0) {
            b_list_destroy(candidates);
            return dir;
      }

      /* Find the longest match */
      bstring *longest = (candidates->lst[0]);
      for (unsigned i = 0; i < candidates->qty; ++i)
            if (candidates->lst[i]->slen > longest->slen)
                  longest = candidates->lst[i];


      talloc_steal(CTX, longest);
      talloc_free(candidates);
      talloc_free(dir);

      return longest;
}

/*======================================================================================
 * /--------------------------------------\
 * |Create, initialize, or find a filetype|
 * \--------------------------------------/
 *=====================================================================================*/

static void      get_ignored_tags(Filetype *ft);
static void      get_tags_from_restored_groups(Filetype *ft, b_list *restored_groups);
static bstring * get_restore_cmds(b_list *restored_groups);
static cmd_info *get_cmd_info(Filetype *ft);

/*
 * Populate the non-static portions of the filetype structure, including a list of
 * special tags and groups of tags that should be ignored when highlighting.
 */
static void
init_filetype(Filetype *ft)
{
      if (ft->initialized)
            return;
      pthread_mutex_lock(&ftdata_mutex);

      ft->initialized = true;
      ft->order = nvimext_get_var_fmt(E_STRING, PKG "%s#order", BTS(ft->vim_name)).ptr;
      mpack_array *tmp =
          mpack_dict_get_key(settings.ignored_tags, E_MPACK_ARRAY, &ft->vim_name).ptr;

      if (!ft->order)
            ft->order = b_create(0);

      talloc_steal(ft, ft->order);

      if (tmp) {
            ft->ignored_tags = mpack_array_to_blist(tmp, true);
            B_LIST_SORT_FAST(ft->ignored_tags);
            talloc_steal(ft, ft->ignored_tags);
      } else {
            ft->ignored_tags = NULL;
      }

      ft->restore_cmds = NULL;
      get_ignored_tags(ft);

      mpack_dict *equiv = nvimext_get_var_fmt(E_MPACK_DICT, PKG "%s#equivalent", BTS(ft->vim_name)).ptr;
      if (equiv) {
            ft->equiv = b_list_create_alloc(equiv->qty);

            for (unsigned i = 0, j = equiv->qty; i < j; ++i) {
                  mpack_dict_ent *ent   = equiv->lst[i];
                  bstring *       toadd = ent->key->str;
                  b_concat(toadd, ent->value->str);
                  b_list_append(ft->equiv, toadd);
                  ent->key->str = NULL;
                  TALLOC_FREE(ent);
            }

            TALLOC_FREE(equiv);
            talloc_steal(ft, ft->equiv);
      } else {
            ft->equiv = NULL;
      }

      ft->cmd_info = get_cmd_info(ft);
      talloc_steal(ft, ft->cmd_info);

      pthread_mutex_unlock(&ftdata_mutex);
}

/*--------------------------------------------------------------------------------------*/

static void
get_ignored_tags(Filetype *ft)
{
      mpack_dict *tmp = nvim_get_var(B(PKG "restored_groups"), E_MPACK_DICT).ptr;
      b_list *    restored_groups = mpack_dict_get_key(tmp, E_STRLIST, &ft->vim_name).ptr;

      if (restored_groups) {
            talloc_steal(CTX, restored_groups);

            if (ft->has_parser) {
                  get_tags_from_restored_groups(ft, restored_groups);
                  B_LIST_SORT_FAST(ft->ignored_tags);
                  talloc_steal(ft, ft->ignored_tags);
            } else {
                  ft->restore_cmds = get_restore_cmds(restored_groups);
                  talloc_steal(ft, ft->restore_cmds);
            }

            talloc_free(restored_groups);
      }

      talloc_free(tmp);
      ft->restore_cmds_initialized = true;
}

static void
get_tags_from_restored_groups(Filetype *ft, b_list *restored_groups)
{
      if (!ft->ignored_tags) {
            ft->ignored_tags = b_list_create();
            talloc_steal(ft, ft->ignored_tags);
      }

      for (unsigned i = 0; i < restored_groups->qty; ++i) {
            char  cmd[8192];
            char *ptr;
            size_t const len = snprintf(cmd, 8192, "syntax list %s", BS(restored_groups->lst[i]));
            /* bstring *output  = nvim_command_output(btp_fromblk(cmd, len), E_STRING).ptr; */
            bstring *output  = nvim_exec(btp_fromblk(cmd, len), true, E_STRING).ptr;

            if (!output || !(ptr = strstr(BS(output), "xxx")))
                  goto next;

            if (!b_starts_with(output, B("match /")))
            {
                  bstring cpy = BSTR_WRITEABLE_STATIC_INIT;
                  cpy.data = (uchar *)(ptr += 4); /* Add 4 to skip the "xxx " */
                  cpy.slen = output->slen - (unsigned)PSUB(ptr, output->data);

                  bstring line = BSTR_WRITEABLE_STATIC_INIT;

                  while (b_memsep(&line, &cpy, '\n') > 0) {
                        while (isblank(*line.data)) {
                              ++line.data;
                              --line.slen;
                        }
                        if (b_starts_with(&line, B("links to ")))
                              break;

                        bstring tok = BSTR_STATIC_INIT;

                        while (b_memsep(&tok, &line, ' ')) {
                              bstring *toadd = b_fromblk(tok.data, tok.slen);
                              // toadd->flags  |= BSTR_MASK_USR1 | BSTR_DATA_FREEABLE | BSTR_FREEABLE;
                              b_list_append(ft->ignored_tags, toadd);
                        }
                  }

            }

      next:
            talloc_free(output);
      }
}

static bstring *
get_restore_cmds(b_list *restored_groups)
{
      assert(restored_groups != NULL);
      b_list *allcmds = b_list_create_alloc(restored_groups->qty);

      for (unsigned i = 0; i < restored_groups->qty; ++i) {
            bstring *cmd    = b_sprintf("syntax list %s", restored_groups->lst[i]);

            /* bstring *output = nvim_command_output(cmd, E_STRING).ptr; */
            bstring *output = nvim_exec(cmd, true, E_STRING).ptr;

            talloc_free(cmd);
            if (!output)
                  continue;

            char *ptr = strstr(BS(output), "xxx");
            if (!ptr) {
                  talloc_free(output);
                  continue;
            }

            cmd = b_alloc_null(64U + output->slen);
            ptr += 4;
            ALWAYS_ASSERT(!isblank(*ptr));
            b_sprintfa(cmd, "syntax clear %s | ", restored_groups->lst[i]);

            b_list *toks = b_list_create();

            /* Only syntax keywords can replace previously supplied items,
             * so just ignore any match groups. */
            if (strncmp(ptr, SLS("match /")) != 0) {
                  char *tmp;
                  char  link_name[1024];
                  b_sprintfa(cmd, "syntax keyword %s ", restored_groups->lst[i]);

                  while ((tmp = strchr(ptr, '\n'))) {
                        b_list_append(toks, b_fromblk(ptr, PSUB(tmp, ptr)));
                        while (isblank(*++tmp))
                              ;
                        ptr = tmp;
                        if (strncmp(ptr, "links to ", 9) == 0 && !(strchr(ptr, '\n') + 1))
                              break;
                  }

                  b_list_remove_dups(&toks);
                  for (unsigned x = 0; x < toks->qty; ++x)
                        b_append_all(cmd, toks->lst[x], B(" "));

                  b_list_destroy(toks);

                  size_t const n = strlcpy(link_name, (ptr + 9), sizeof(link_name));
                  ALWAYS_ASSERT(n > 0);
                  b_sprintfa(cmd, " | hi! link %s %n", restored_groups->lst[i], link_name);

                  b_list_append(allcmds, cmd);
            }

            talloc_free(output);
      }

      bstring *ret = b_join(allcmds, B(" | "));
      b_list_destroy(allcmds);
      return ret;
}

static cmd_info *
get_cmd_info(Filetype *ft)
{
      unsigned const   ngroups = ft->order->slen;
      struct cmd_info *info    = talloc_array(NULL, struct cmd_info, ngroups);

      for (unsigned i = 0; i < ngroups; ++i) {
            int const   ch = ft->order->data[i];
            mpack_dict *dict = nvimext_get_var_fmt(E_MPACK_DICT, PKG "%s#%c",
                                                   BTS(ft->vim_name), ch).ptr;

            info[i].kind  = ch;
            info[i].group = mpack_dict_get_key(dict, E_STRING, B("group")).ptr;
            info[i].num   = ngroups;

            talloc_steal(info, info[i].group);
            talloc_free(dict);
      }

      return info;
}

/*--------------------------------------------------------------------------------------
 * /===========\
 * |Destructors|
 * \===========/
 *-------------------------------------------------------------------------------------*/

void
(destroy_buffer)(Buffer *bdata, unsigned const flags)
{
      extern bool process_exiting;
      extern void destroy_clangdata(Buffer * bdata);
      assert(bdata != NULL);

      if (!process_exiting && (flags & DES_BUF_SHOULD_CLEAR))
            clear_highlight(bdata);


      if (flags & DES_BUF_DESTROY_NODE) {
            buffer_node *bnode = find_buffer_node(bdata->num);
            assert(bnode != NULL);
            pthread_rwlock_wrlock(bnode->lock);
            bnode->isopen = false;
            bnode->bdata  = NULL;
            pthread_rwlock_unlock(bnode->lock);
      }

      unsigned cnt;
      atomic_store(&bdata->initialized, false);
      while ((cnt = p99_futex_load(&bdata->lock.num_workers)) > 0) {
            (void)cnt;
            usleep(1);
#if 0
            for (unsigned i = 0; i < cnt; ++i) {
                  if (bdata->lock.pids[i] > 0) {
                        //pthread_cancel(bdata->lock.pids[i]);
                        //bdata->lock.pids[i] = 0;
                        //p99_count_dec(&bdata->lock.num_workers);
                  }
            }
#endif
      }

      pthread_mutex_lock(&bdata->lock.lang_mtx);
      //pthread_mutex_lock(&bdata->lock.total);

      if (--bdata->topdir->refs == 0) {
            echo("Destroying topdir (%s)", BS(bdata->topdir->pathname));
            TALLOC_FREE(bdata->topdir);
      }

      if (bdata->ft->is_c) {
            if (bdata->headers)
                  TALLOC_FREE(bdata->headers);
      } else if (bdata->ft->id == FT_GO) {
            golang_clear_data(bdata);
      }

      else if (bdata->calls)
            mpack_destroy_arg_array(bdata->calls);

      /* pthread_mutex_unlock(&bdata->lock.total); */
      /* pthread_mutex_destroy(&bdata->lock.total); */
      pthread_mutex_unlock(&bdata->lock.lang_mtx);
      pthread_mutex_destroy(&bdata->lock.lang_mtx);

      //p99_futex_wakeup(&destruction_futex[bdata->num]);
      if (flags & DES_BUF_TALLOC_FREE) {
            talloc_set_destructor(bdata, NULL);
            warnd("freeing");
            talloc_free(bdata);
      }
}

static int
destroy_buffer_wrapper(Buffer *bdata)
{
      destroy_buffer(bdata, DES_BUF_SHOULD_CLEAR | DES_BUF_TALLOC_FREE);
      return 0;
}

static int
destroy_buffer_node(buffer_node *bnode)
{
      ALWAYS_ASSERT(bnode != NULL);

      if (bnode->bdata)
            destroy_buffer(bnode->bdata, DES_BUF_SHOULD_CLEAR | DES_BUF_TALLOC_FREE);
      if (bnode->lock) {
            pthread_rwlock_destroy(bnode->lock);
            TALLOC_FREE(bnode->lock);
      }
      TALLOC_FREE(bnode);
      return 0;
}

static int
destroy_topdir(Top_Dir *topdir)
{
      pthread_mutex_lock(&wtf_mutex);
      close(topdir->tmpfd);
      unlink(BS(topdir->tmpfname));
      bool found = false;

      LL_FOREACH_F (top_dirs, node) {
            if (node->data == topdir) {
                  // talloc_steal(NULL, node->data);
                  // node->data = NULL;
                  ll_delete_node(top_dirs, node);
                  found = true;
                  break;
            }
      }

      if (!found)
            errx(1, "Topdir \"%s\" not found in list?!?", BS(topdir->pathname));
      // TALLOC_FREE(topdir);
      pthread_mutex_unlock(&wtf_mutex);
      return 0;
}
