#include "util.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "archive/archive_util.h"
#include "data.h"
#include "highlight.h"
#include "mpack.h"


static inline void write_gzfile(struct top_dir *topdir);


/*============================================================================*/


bool
run_ctags(struct bufdata *bdata, struct top_dir *topdir)
{
        assert(topdir != NULL);
        if (bdata->cmd_cache) {
                b_list_destroy(bdata->cmd_cache);
                bdata->cmd_cache = NULL;
        }

        bstring *cmd = b_fromcstr_alloc(2048, "ctags ");

        for (unsigned i = 0; i < settings.ctags_args->qty; ++i) {
                b_concat(cmd, settings.ctags_args->lst[i]);
                b_conchar(cmd, ' ');
        }

        if (topdir->recurse)
                b_formata(cmd, " -R -f%s '%s'", BS(topdir->tmpfname), BS(topdir->pathname));
        else {
                echo("Not recursing!!!");
                b_formata(cmd, " -f%s '%s'", BS(topdir->tmpfname), BS(bdata->filename));
        }

        nvprintf("Running ctags command \"\"%s\"\"\n", BS(cmd));

        /* Yes, this directly uses unchecked user input in a call to system().
         * Frankly, if somehow someone takes over a user's vimrc then they're
         * already compromised, and if the user wants to attack their own
         * system for some reason then they can be my guest. */
        int status = system(BS(cmd));

        if (status != 0)
                nvprintf("ctags failed with status \"%d\"\n", status);
        else
                echo("Ctags finished successfully.");
        b_destroy(cmd);

        return (status == 0);
}


void
get_initial_taglist(struct bufdata *bdata, struct top_dir *topdir)
{
        struct stat st;
        errno = 0;
        topdir->tags = b_list_create();

        if (stat(BS(topdir->gzfile), &st) == 0) {
                getlines(topdir->tags, settings.comp_type, topdir->gzfile);
                for (unsigned i = 0; i < topdir->tags->qty; ++i)
                        b_write(topdir->tmpfd, topdir->tags->lst[i], B("\n"));
        } else {
                if (errno == ENOENT)
                        nvprintf("File \"%s\" not found, running ctags.\n",
                                 BS(topdir->gzfile));
                else
                        warn("Unexpected io error");

                run_ctags(bdata, topdir);
                write_gzfile(topdir);
                errno = 0;

                if (stat(BS(topdir->gzfile), &st) != 0)
                        err(1, "Failed to stat gzfile");

                getlines(topdir->tags, COMP_NONE, topdir->tmpfname);
        }
}


bool
update_taglist(struct bufdata *bdata)
{
        if (bdata->ctick == bdata->last_ctick)
                return false;

        bdata->last_ctick = bdata->ctick;
        assert(run_ctags(bdata, bdata->topdir));

        getlines(bdata->topdir->tags, COMP_NONE, bdata->topdir->tmpfname);
        write_gzfile(bdata->topdir);

        return true;
}


/*============================================================================*/


static inline void
write_gzfile(struct top_dir *topdir)
{
        echo("Compressing tagfile.");
        switch (settings.comp_type) {
        case COMP_NONE: write_plain(topdir); break;
        case COMP_GZIP: write_gzip(topdir);  break;
        case COMP_LZMA: write_lzma(topdir);  break;
        /* case COMP_LZMA: lazy_write_lzma(topdir);  break; */

        default: abort();
        }
        echo("Finished compressing tagfile!");
}
