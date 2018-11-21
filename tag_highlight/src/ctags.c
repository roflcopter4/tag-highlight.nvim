#include "Common.h"
#include <dirent.h>
#if defined(DOSISH) || defined(MINGW)
//#  include <direct.h>
#else
#  include <spawn.h>
#  include <sys/wait.h>
#endif

#include "highlight.h"
#include "util/archive.h"

static inline void write_gzfile(struct top_dir *topdir);
static int         exec_ctags(struct bufdata *bdata, b_list *headers, enum update_taglist_opts opts);

/*======================================================================================*/


bool
run_ctags(struct bufdata *bdata, const enum update_taglist_opts opts)
{
        assert(bdata != NULL && bdata->topdir != NULL);
        if (!bdata->lines || !bdata->initialized) {
                ECHO("File is empty, cannot run ctags");
                return false;
        }

        /* Wipe any cached commands if they exist. */
        if (!bdata->ft->is_c && bdata->calls) {
                _nvim_destroy_arg_array(bdata->calls);
                bdata->calls = NULL;
        }

        int status = exec_ctags(bdata, ((bdata->ft->is_c) ? bdata->headers : NULL), opts);

        if (status != 0)
                warnx("ctags failed with status \"%d\"\n", status);
        else
                ECHO("Ctags finished successfully.");

        return (status == 0);
}


int
get_initial_taglist(struct bufdata *bdata)
{
        struct stat  st;
        struct timer *t  = TIMER_INITIALIZER;
        int          ret = 0;
        TIMER_START(t);
        bdata->topdir->tags = b_list_create();

        if (have_seen_file(bdata->name.full)) {
                ECHO("Seen file before, running ctags in case there was just a "
                     "momentary disconnect on write...");
                goto force_ctags;
        }

        /* Read the compressed tags file if it exists, otherwise we run ctags
         * and read the file it creates. If there is a read error in the saved
         * file, run ctags as a backup. */
        if (stat(BS(bdata->topdir->gzfile), &st) == 0) {
                ret += getlines(bdata->topdir->tags, settings.comp_type,
                                bdata->topdir->gzfile);
                if (ret) {
                        for (unsigned i = 0; i < bdata->topdir->tags->qty; ++i)
                                b_write(bdata->topdir->tmpfd,
                                        bdata->topdir->tags->lst[i], B("\n"));
                } else {
                        warnx("Could not read file. Running ctags.");
                        if (!bdata->initialized)
                                return 0;
                        echo("linecount -> %d", bdata->lines->qty);
                        goto force_ctags;
                }
        } else {
                if (errno == ENOENT)
                        ECHO("File \"%s\" not found, running ctags.\n",
                             bdata->topdir->gzfile);
                else
                        warn("Unexpected io error");
        force_ctags:
                run_ctags(bdata, UPDATE_TAGLIST_FORCE);
                write_gzfile(bdata->topdir);

                if (stat(BS(bdata->topdir->gzfile), &st) != 0)
                        err(1, "Failed to stat gzfile");

                ret += getlines(bdata->topdir->tags, COMP_NONE, bdata->topdir->tmpfname);
        }

        TIMER_REPORT(t, "initial taglist");
        return ret;
}


int
update_taglist(struct bufdata *bdata, const enum update_taglist_opts opts)
{
        if (opts == UPDATE_TAGLIST_NORMAL && bdata->ctick == bdata->last_ctick) {
                ECHO("ctick unchanged");
                return false;
        }
        bool          ret = true;
        struct timer *t   = TIMER_INITIALIZER;
        TIMER_START(t);

        bdata->last_ctick = bdata->ctick;
        if (!run_ctags(bdata, opts))
                warnx("Ctags exited with errors; trying to continue anyway.");

        if (!getlines(bdata->topdir->tags, COMP_NONE, bdata->topdir->tmpfname)) {
                ECHO("Failed to read file");
                ret = false;
        } else {
                write_gzfile(bdata->topdir);
        }

        TIMER_REPORT(t, "update taglist");
        return ret;
}


static inline void
write_gzfile(struct top_dir *topdir)
{
        ECHO("Compressing tagfile.");
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
        ECHO("Finished compressing tagfile!");
}


/*======================================================================================*/


#ifdef DOSISH
static int
exec_ctags(struct bufdata *bdata, b_list *headers, const enum update_taglist_opts opts)
{
        bstring *cmd = b_fromcstr_alloc(2048, "ctags ");

        for (unsigned i = 0; i < settings.ctags_args->qty; ++i)
                b_sprintfa(cmd, "\"%s\" ", settings.ctags_args->lst[i]);

        if (headers) {
                B_LIST_SORT(headers);
                bstring *tmp = b_join_quote(headers, B(" "), '"');
                b_sprintfa(cmd, " \"-f%s\" \"%s\" %s",
                           bdata->topdir->tmpfname, bdata->name.full, tmp);

                b_destroy(tmp);
                b_list_destroy(headers);
        } else if (bdata->topdir->recurse && !bdata->ft->is_c) {
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
/*
 * In a unix environment, let's avoid the headache of quoting arguments by
 * calling fork/exec, even at the cost of a bunch of extra allocations and copying.
 */
static int
exec_ctags(struct bufdata *bdata, b_list *headers, const enum update_taglist_opts opts)
{
        unsigned i;
        str_vector *argv = argv_create(128);
        argv_append(argv, BS(settings.ctags_bin), true);

        B_LIST_FOREACH(settings.ctags_args, arg, i) {
                if (arg)
                        argv_append(argv, b_bstr2cstr(arg, 0), false);
        }

        assert(bdata->topdir->tmpfname != NULL &&
               bdata->topdir->tmpfname->data != NULL &&
               bdata->topdir->tmpfname->data[0] != '\0');
        argv_fmt(argv, "-f%s", BS(bdata->topdir->tmpfname));

        if (opts != UPDATE_TAGLIST_FORCE_LANGUAGE &&
            bdata->topdir->recurse && !bdata->ft->is_c)
        {
                argv_fmt(argv, "--languages=%s", BS(&bdata->ft->ctags_name));
                argv_append(argv, "-R", true);
                argv_append(argv, b_bstr2cstr(bdata->topdir->pathname, 0), false);
        } else {
                if (bdata->ft->is_c)
                        argv_append(argv, "--languages=c,c++", true);
                else
                        argv_fmt(argv, "--language-force=%s", BS(&bdata->ft->ctags_name));
                argv_append(argv, b_bstr2cstr(bdata->name.full, 0), false);
                if (headers)
                        B_LIST_FOREACH (headers, bstr, i)
                                argv_append(argv, b_bstr2cstr(bstr, 0), false);
        }

        argv_append(argv, (const char *)0, false);

#ifdef DEBUG
        {
                bstring *cmd = b_alloc_null(2048);
                for (char **tmp = argv->lst; *tmp; ++tmp)
                        b_sprintfa(cmd, "\"%n\", ", *tmp);
                cmd->data[cmd->slen -= 2] = '\0';
                ECHO("Running command 'ctags' with args [%s]\n", cmd);
                b_destroy(cmd);

                FILE *fp = safe_fopen_fmt(
                    "%s/.tag_highlight_log/ctags_arguments.log", "wb", HOME);
                fprintf(fp, "%s\n", BS(settings.ctags_bin));
                for (char **tmp = argv->lst; *tmp; ++tmp)
                        fprintf(fp, "%s\n", *tmp);
                fclose(fp);
        }
#endif
        int pid, status;

#ifdef HAVE_POSIX_SPAWNP
        if (posix_spawnp(&pid, BS(settings.ctags_bin), NULL, NULL, argv->lst, environ) != 0)
                err(1, "Exec failed");
#else
        if ((pid = fork()) == 0)
                if (execvp(BS(settings.ctags_bin), argv->lst) != 0)
                        err(1, "Exec failed");
#endif

        waitpid(pid, &status, 0);
        status <<= 8;
        argv_destroy(argv);
        echo("Status is %d", status);
        return status;
}
#endif
