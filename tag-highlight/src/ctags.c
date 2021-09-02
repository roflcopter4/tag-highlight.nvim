#include "Common.h"
#include <sys/stat.h>

#if defined(DOSISH) || defined(MINGW)
#  include <direct.h>
#else
#  include <dirent.h>
#  include <spawn.h>
#  include <sys/wait.h>
#endif

#include "highlight.h"
#include "util/archive.h"

static inline void write_gzfile(struct top_dir *topdir);
static int         exec_ctags(Buffer *bdata, b_list *headers, enum update_taglist_opts opts);

/*======================================================================================*/


bool
run_ctags(Buffer *bdata, enum update_taglist_opts const opts)
{
        assert(bdata != NULL && bdata->topdir != NULL);
        if (!bdata->lines || !bdata->initialized) {
                ECHO("File is empty, cannot run ctags");
                return false;
        }

        /* Wipe any cached commands if they exist. */
        if (!bdata->ft->has_parser && bdata->calls) {
                talloc_free(bdata->calls);
                bdata->calls = NULL;
        }

        int status = exec_ctags(bdata,
                                bdata->ft->is_c ? bdata->headers : NULL,
                                opts);

        if (status != 0)
                shout("ctags failed with status \"%d\"\n", status);

        return (status == 0);
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
        if (stat(BS(top->gzfile), &st) == 0) {
                if (top->timestamp < st.st_mtime) {
                        ret += getlines(top->tags, settings.comp_type, top->gzfile);
                        if (ret) {
                                if (ftruncate(top->tmpfd, 0) == (-1))
                                        err(1, "ftruncate()");
                                for (unsigned i = 0; i < top->tags->qty; ++i)
                                        b_write(top->tmpfd, top->tags->lst[i], B("\n"));
                        } else {
                                warnx("Could not read file. Running ctags.");
                                if (!bdata->initialized)
                                        return 0;
                                ECHO("linecount -> %d", bdata->lines->qty);
                                goto force_ctags;
                        }
                }
        } else {
                if (errno == ENOENT)
                        ECHO("File \"%s\" not found, running ctags.\n", top->gzfile);
                else
                        warn("Unexpected io error");
        force_ctags:
                run_ctags(bdata, UPDATE_TAGLIST_FORCE);
                write_gzfile(top);

                if (stat(BS(top->gzfile), &st) != 0)
                        err(1, "Failed to stat gzfile \"%s\"", BS(top->gzfile));

                ret += getlines(top->tags, COMP_NONE, top->tmpfname);
        }

        top->timestamp = (time_t)st.st_mtime;
        return ret;
}


int
update_taglist(Buffer *bdata, enum update_taglist_opts const opts)
{
        register unsigned ctick = p99_futex_load(&bdata->ctick);
        if (opts == UPDATE_TAGLIST_NORMAL && ctick == bdata->last_ctick) {
                ECHO("ctick unchanged");
                return false;
        }
        bool ret = true;
        pthread_mutex_lock(&bdata->lock.total);

        atomic_store_explicit(&bdata->last_ctick, ctick, memory_order_relaxed);

        if (!run_ctags(bdata, opts))
                warnx("Ctags exited with errors; trying to continue anyway.");

        talloc_free(bdata->topdir->tags);
        bdata->topdir->tags = b_list_create();
        talloc_steal(bdata->topdir, bdata->topdir->tags);

        if (!getlines(bdata->topdir->tags, COMP_NONE, bdata->topdir->tmpfname)) {
                ECHO("Failed to read file");
                ret = false;
        } else {
                write_gzfile(bdata->topdir);
        }

        pthread_mutex_unlock(&bdata->lock.total);
        return ret;
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
                b_sprintfa(cmd, " \"-f%s\" \"%s\" %s",
                           bdata->topdir->tmpfname, bdata->name.full, tmp);

                talloc_free(tmp);
                talloc_free(headers);
        } else if (bdata->topdir->recurse) {
                b_sprintfa(cmd, " \"--languages=%s\" -R \"-f%s\" \"%s\"",
                           &bdata->ft->ctags_name, bdata->topdir->tmpfname,
                           bdata->topdir->pathname);
        } else {
                b_sprintfa(cmd, " \"-f%s\" \"%s\"",
                           bdata->topdir->tmpfname, bdata->name.full);
        }

        ECHO("Running ctags command `CMD.EXE /c %s`", cmd);

        /* Yes, this directly uses unchecked user input in a call to system().
         * Frankly, if somehow someone takes over a user's vimrc then they're
         * already compromised, and if the user wants to attack their own
         * system for some reason then they can be my guest. */
        return system(BS(cmd));
}
#else
static inline str_vector *get_ctags_argv(Buffer *bdata, b_list *headers, enum update_taglist_opts opts);

/*
 * In a unix environment, let's avoid the headache of quoting arguments by
 * calling fork/exec, even at the cost of a bunch of extra allocations and copying.
 *
 * What a mess though.
 */
static int
exec_ctags(Buffer *bdata, b_list *headers, enum update_taglist_opts const opts)
{
        int pid, status;
        str_vector *argv = get_ctags_argv(bdata, headers, opts);
        warnd("Running ctags:");

#ifdef HAVE_POSIX_SPAWNP
        if (posix_spawnp(&pid, BS(settings.ctags_bin), NULL, NULL, argv->lst, environ) != 0)
                err(1, "Exec failed");
#else
        if ((pid = fork()) == 0)
                if (execvpe(BS(settings.ctags_bin), argv->lst, environ) != 0)
                        err(1, "Exec failed");
#endif

        waitpid(pid, &status, 0);
        talloc_free(argv);
        return (status > 0) ? ((status & 0xFF00) >> 8)
                            : status;
}


static inline str_vector *
get_ctags_argv(Buffer *bdata, b_list *headers, enum update_taglist_opts const opts)
{
        str_vector *argv = argv_create(128);
        argv_append(argv, BS(settings.ctags_bin), true);

        B_LIST_FOREACH (settings.ctags_args, arg, i)
                if (arg)
                        argv_append(argv, b_bstr2cstr(arg, 0), false);

        argv_append(argv, "--pattern-length-limit=0", true);
        assert(bdata->topdir->tmpfname != NULL &&
               bdata->topdir->tmpfname->data != NULL &&
               bdata->topdir->tmpfname->data[0] != '\0');

        argv_append_fmt(argv, "-f%s", BS(bdata->topdir->tmpfname));

        if (opts != UPDATE_TAGLIST_FORCE_LANGUAGE && bdata->topdir->recurse)
        {
                if (bdata->ft->is_c)
                        argv_append(argv, "--languages=c,c++", true);
                else
                        argv_append_fmt(argv, "--languages=%s", BS(&bdata->ft->ctags_name));

                argv_append(argv, "-R", true);
                argv_append(argv, b_bstr2cstr(bdata->topdir->pathname, 0), false);
        } else {
                if (bdata->ft->is_c)
                        argv_append(argv, "--languages=c,c++", true);
                else
                        argv_append_fmt(argv, "--language-force=%s", BS(&bdata->ft->ctags_name));

                argv_append(argv, b_bstr2cstr(bdata->name.full, 0), false);
                if (headers)
                        B_LIST_FOREACH (headers, bstr, i)
                                argv_append(argv, b_bstr2cstr(bstr, 0), false);
        }

        argv_append(argv, (char const *)0, false);

        return argv;
}

#endif
