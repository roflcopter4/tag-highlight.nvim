#include "Common.h"
#include <stddef.h>
#include <sys/stat.h>

#if defined(DOSISH) || defined(MINGW)
# include <direct.h>
#else
# include <dirent.h>
# include <spawn.h>
# include <sys/wait.h>
#endif

#include "highlight.h"
#include "util/archive.h"

static int run_ctags (Buffer *bdata, enum update_taglist_opts opts);
static int exec_ctags(Buffer *bdata, b_list *headers, enum update_taglist_opts opts);
static bstring *exec_ctags_pipe(Buffer *bdata, b_list *headers, enum update_taglist_opts opts, int *status);
static inline void write_gzfile_from_buffer(struct top_dir const *topdir, bstring const *buf);
static inline void write_gzfile(struct top_dir *topdir);


/*======================================================================================*/


static int
run_ctags(Buffer *bdata, enum update_taglist_opts const opts)
{
      if (!nvim_get_var(B("tag_highlight#run_ctags"), E_BOOL).num) {
            warnd("ctags is disabled. Not running.");
            return (-1);
      }

      assert(bdata != NULL && bdata->topdir != NULL);
      if (!bdata->lines) {
            warnd("File is empty, cannot run ctags");
            return false;
      }

      /* Wipe any cached commands if they exist. */
      if (!bdata->ft->has_parser && bdata->calls)
            TALLOC_FREE(bdata->calls);

      int status = exec_ctags(bdata, bdata->ft->is_c ? bdata->headers : NULL, opts);

      if (status != 0)
            warnx("ctags failed with status \"%d\"\n", status);

      return (status == 0);
}


static bstring *
run_ctags_pipe(Buffer *bdata, enum update_taglist_opts const opts)
{
      if (!nvim_get_var(B("tag_highlight#run_ctags"), E_BOOL).num) {
            warnd("ctags is disabled. Not running.");
            return NULL;
      }

      assert(bdata != NULL && bdata->topdir != NULL);
      if (!bdata->lines) {
            warnd("File is empty, cannot run ctags");
            return false;
      }

      /* Wipe any cached commands if they exist. */
      if (!bdata->ft->has_parser && bdata->calls)
            TALLOC_FREE(bdata->calls);

      int      status = 0;
      bstring *ret = exec_ctags_pipe(bdata, bdata->ft->is_c ? bdata->headers : NULL, opts, &status);

      if (status != 0)
            warnx("ctags failed with status \"%d\"\n", status);

      return ret;
}


int
get_initial_taglist(Buffer *bdata)
{
      struct stat     st;
      struct top_dir *top = bdata->topdir;
      int             ret = 0;

      if (top->tags)
            return update_taglist(bdata, UPDATE_TAGLIST_FORCE);

      top->tags = b_list_create();
      talloc_steal(top, top->tags);

      /* Read the compressed tags file if it exists, otherwise we run ctags
       * and read the file it creates. If there is a read error in the saved
       * file, run ctags as a backup. */
      if (stat(BS(top->gzfile), &st) == 0)
      {
            if (top->timestamp < st.st_mtime)
            {
                  ret += getlines(top->tags, settings.comp_type, top->gzfile);

                  if (ret) {
                        if (ftruncate(top->tmpfd, 0) == (-1))
                              err(1, "ftruncate()");
                        for (unsigned i = 0; i < top->tags->qty; ++i)
                              b_write(top->tmpfd, top->tags->lst[i], B("\n"));
                  } else {
                        warnd("Could not read file. Running ctags.");
                        warnd("linecount -> %d", bdata->lines->qty);
                        goto force_ctags;
                  }
            }
      } else {
            int tmp = errno;
            if (tmp) {
                  if (tmp == ENOENT)
                        warnd("File \"%.*s\" not found, running ctags.\n", BSC(top->gzfile));
                  else
                        warn("Unexpected io error");
            }
      force_ctags:
            ; bstring *out = run_ctags_pipe(bdata, UPDATE_TAGLIST_FORCE);
            if (!out) {
                  return 1;
            }
            write_gzfile_from_buffer(top, out);
            b_write(bdata->topdir->tmpfd, out);
#if 0
            if (run_ctags(bdata, UPDATE_TAGLIST_FORCE) < 0) {
                  warnd("No ctags.");
                  return 1;
            }
            write_gzfile(top);
#endif

            if (stat(BS(top->gzfile), &st) != 0)
                  err(1, "Failed to stat gzfile \"%s\"", BS(top->gzfile));

#if 0
            ret += getlines(top->tags, COMP_NONE, top->tmpfname);
#endif
            ret += getlines_from_buffer(top->tags, out);
            talloc_free(out);
      }

      top->timestamp = (time_t)st.st_mtime;
      return ret;
}


int
update_taglist(Buffer *bdata, enum update_taglist_opts const opts)
{
      register unsigned ctick = p99_futex_load(&bdata->ctick);
#if 0
      if (opts == UPDATE_TAGLIST_NORMAL && ctick == bdata->last_ctick) {
            ECHO("ctick unchanged");
            return false;
      }
#endif
      bool ret;
      pthread_mutex_lock(&bdata->lock.total);

      atomic_store(&bdata->last_ctick, ctick);

#if 0
      int val = run_ctags(bdata, opts);
      if (val > 0)
            warnx("Ctags exited with errors; trying to continue anyway.");
      else if (val == (-1)) {
            ret = false;
            goto skip;
      }
#endif

      bstring *out = run_ctags_pipe(bdata, opts);
      if (!out) {
            ret = false;
            goto skip;
      }

      talloc_free(bdata->topdir->tags);
      bdata->topdir->tags = b_list_create();
      talloc_steal(bdata->topdir, bdata->topdir->tags);

      if (ftruncate(bdata->topdir->tmpfd, 0) == (-1))
            err(1, "ftruncate()");
      b_write(bdata->topdir->tmpfd, out);

      ret = getlines_from_buffer(bdata->topdir->tags, out);
      talloc_free(out);

#if 0
      if (!getlines(bdata->topdir->tags, COMP_NONE, bdata->topdir->tmpfname)) {
            ECHO("Failed to read file");
            ret = false;
      } else {
            write_gzfile(bdata->topdir);
      }
#endif

skip:
      pthread_mutex_unlock(&bdata->lock.total);
      return ret;
}


/*--------------------------------------------------------------------------------------*/


static inline void
write_gzfile_from_buffer(struct top_dir const *topdir, bstring const *buf)
{
      //write_plain_from_buffer(topdir, buf);
      switch (settings.comp_type) {
      case COMP_NONE:
            write_plain_from_buffer(topdir, buf);
            break;
      case COMP_LZMA:
#ifdef LZMA_SUPPORT
            write_lzma_from_buffer(topdir, buf);
            break;
#endif
      case COMP_GZIP:
            write_gzip_from_buffer(topdir, buf);
            break;
      default:
            abort();
      }
}

static inline void
write_gzfile(struct top_dir *topdir)
{
      switch (settings.comp_type) {
      case COMP_NONE:
            write_plain(topdir);
            break;
      case COMP_LZMA:
#ifdef LZMA_SUPPORT
            write_lzma(topdir);
            break;
#endif
      case COMP_GZIP:
            write_gzip(topdir);
            break;
      default:
            abort();
      }
}


/*======================================================================================*/


#ifdef DOSISH

static int
exec_ctags(Buffer *bdata, b_list *headers, UNUSED enum update_taglist_opts const opts)
{
      bstring *cmd = b_fromcstr_alloc(2048, "ctags ");

      for (unsigned i = 0; i < settings.ctags_args->qty; ++i)
            b_sprintfa(cmd, "\"%s\" ", settings.ctags_args->lst[i]);

      if (headers) {
            B_LIST_SORT(headers);
            bstring *tmp = b_join_quote(headers, B(" "), '"');
            b_sprintfa(cmd, " \"-f%s\" \"%s\" %s", bdata->topdir->tmpfname,
                       bdata->name.full, tmp);

            talloc_free(tmp);
            talloc_free(headers);
      } else if (bdata->topdir->recurse) {
            b_sprintfa(cmd, " \"--languages=%s\" -R \"-f%s\" \"%s\"", &bdata->ft->ctags_name,
                       bdata->topdir->tmpfname, bdata->topdir->pathname);
      } else {
            b_sprintfa(cmd, " \"-f%s\" \"%s\"", bdata->topdir->tmpfname, bdata->name.full);
      }

      warnd("Running ctags command `CMD.EXE /c %s`", cmd);

      /* Yes, this directly uses unchecked user input in a call to system().
       * Frankly, if somehow someone takes over a user's vimrc then they're
       * already compromised, and if the user wants to attack their own
       * system for some reason then they can be my guest. */
      return system(BS(cmd));
}

/*--------------------------------------------------------------------------------------*/
#else // DOSISH

static        str_vector *get_ctags_argv(Buffer *bdata, b_list *headers, enum update_taglist_opts opts);
static inline str_vector *get_ctags_argv_init(Buffer *bdata);
static inline void get_ctags_argv_recursion(Buffer *bdata, str_vector *argv);
static inline void get_ctags_argv_other(Buffer *bdata, b_list *headers, str_vector *argv);
static inline void get_ctags_argv_lang(Buffer *bdata, str_vector *argv, bool force);

static bstring *
exec_ctags_pipe(Buffer *bdata, b_list *headers, enum update_taglist_opts const opts, int *status)
{
      str_vector *argv = get_ctags_argv(bdata, headers, opts);
      /* argv_dump(stderr, argv); */
      bstring *out = get_command_output(BS(settings.ctags_bin), argv->lst, NULL, status);

      if (status != 0)
            warnx("ctags returned with status %d", ((*status &0xFF00) >> 8));

      return out;
}

/*
 * In a unix environment, let's avoid the headache of quoting arguments by
 * calling fork/exec, even at the cost of a bunch of extra allocations and copying.
 *
 * What a mess though.
 */
static int
exec_ctags(Buffer *bdata, b_list *headers, enum update_taglist_opts const opts)
{
      int         pid, status;
      str_vector *argv = get_ctags_argv(bdata, headers, opts);
      warnd("Running ctags:");

# ifdef HAVE_POSIX_SPAWNP
      if (posix_spawnp(&pid, BS(settings.ctags_bin), NULL, NULL, argv->lst, environ) != 0)
            err(1, "posix_spawnp failed");
# else
      if ((pid = fork()) == 0)
            if (execvpe(BS(settings.ctags_bin), argv->lst, environ) != 0)
                  err(1, "execvpe failed");
# endif

      waitpid(pid, &status, 0);
      talloc_free(argv);
      return (status > 0) ? ((status & 0xFF00) >> 8) : status;
}

# define B2C(var) b_bstr2cstr((var), 0)

/* 
 * This function was an unreadable mess so I split it up. Hopefully this hasn't
 * made it even worse.
 */
static str_vector *
get_ctags_argv(Buffer *bdata, b_list *headers,
               enum update_taglist_opts const opts)
{
      assert(bdata->topdir->tmpfname != NULL &&
             bdata->topdir->tmpfname->data != NULL &&
             bdata->topdir->tmpfname->data[0] != '\0');

      str_vector *argv = get_ctags_argv_init(bdata);

      if (bdata->topdir->recurse && opts != UPDATE_TAGLIST_FORCE_LANGUAGE)
            get_ctags_argv_recursion(bdata, argv);
      else
            get_ctags_argv_other(bdata, headers, argv);

      get_ctags_argv_lang(bdata, argv, opts == UPDATE_TAGLIST_FORCE_LANGUAGE);
      argv_append(argv, (char const *)0, false);

      return argv;
}

static inline str_vector *
get_ctags_argv_init(Buffer *bdata)
{
      str_vector *argv = argv_create(128);

      argv_append(argv, B2C(settings.ctags_bin), false);
      argv_append(argv, "--pattern-length-limit=1", true);
      argv_append(argv, "--output-format=e-ctags", true);
      argv_append(argv, "-f-", true);
#if 0
      argv_append_fmt(argv, "-f%s", BS(bdata->topdir->tmpfname));
#endif

      B_LIST_FOREACH (settings.ctags_args, arg, i)
            if (arg)
                  argv_append(argv, B2C(arg), false);

      return argv;
}

static inline void
get_ctags_argv_recursion(Buffer *bdata, str_vector *argv)
{
      argv_append(argv, "-R", true);
      argv_append(argv, B2C(bdata->topdir->pathname), false);
}

static inline void
get_ctags_argv_other(Buffer *bdata, b_list *headers, str_vector *argv)
{
      argv_append(argv, B2C(bdata->name.full), false);

      if (headers)
            B_LIST_FOREACH (headers, bstr, i)
                  argv_append(argv, B2C(bstr), false);
}

static inline void
get_ctags_argv_lang(Buffer *bdata, str_vector *argv, bool const force)
{
      if (bdata->ft->is_c)
            argv_append(argv, "--languages=c,c++", true);
      else if (force)
            argv_append_fmt(argv, "--language-force=%s", BTS(bdata->ft->ctags_name));
      else
            argv_append_fmt(argv, "--languages=%s", BTS(bdata->ft->ctags_name));
}

#endif // DOSISH
/*--------------------------------------------------------------------------------------*/
