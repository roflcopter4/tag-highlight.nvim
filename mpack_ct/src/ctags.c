#include "util.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "data.h"
#include "highlight.h"
#include "mpack.h"

extern int sockfd;


/*============================================================================*/


bool
run_ctags(const int bufnum, struct bufdata *bdata)
{
        if (!bdata) {
                assert(bufnum > 0);
                bdata = find_buffer(bufnum);
        }

        assert(bdata != NULL);
        bstring *cmd = b_fromcstr_alloc(2048, "ctags ");

        for (unsigned i = 0; i < settings.ctags_args->qty; ++i) {
                b_concat(cmd, settings.ctags_args->lst[i]);
                b_conchar(cmd, ' ');
        }

        b_formata(cmd, " -R -f%s '%s'", BS(bdata->tmpfname), BS(bdata->topdir));

        nvprintf("Running ctags command \"\"%s\"\"\n", BS(cmd));

        int status = system(BS(cmd));
        if (status != 0)
                nvprintf("ctags failed with status \"%d\"\n", status);
        else
                echo("Ctags finished successfully.");
        b_destroy(cmd);

        return (status == 0);
}


b_list *
get_archived_tags(struct bufdata *bdata)
{
        b_list *ret = b_list_create_alloc(512);
        nvprintf("Opening archive file \"%s\".\n", BS(bdata->gzfile));
        assert(getlines(
                   ret, settings.compression_type,
                   B("/home/bml/.vim_tags/"
                     "__home__bml__.vim__dein__repos__github.com__roflcopter4__"
                     "tag-highlight.nvim__mpack_ct.tags.xz")) == 1);
        return ret;
}


bool
check_gzfile(struct bufdata *bdata)
{
        struct stat st;
        errno    = 0;
        bool ret = (stat(BS(bdata->gzfile), &st) == 0);

        if (!ret) {
                if (errno == ENOENT)
                        echo("File not found, running ctags.");
                else
                        warn("Unexpected io error");

                ret = run_ctags(0, bdata);
        }

        return ret;
}
