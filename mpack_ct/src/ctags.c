#include "mytags.h"
#include <fcntl.h>

#include "data.h"
#include "mpack.h"
#include "tags.h"

extern int sockfd;


/*============================================================================*/


bool
run_ctags(const int bufnum)
{
        struct bufdata *bdata = get_bufdata(sockfd, bufnum);

        bstring *cmd = b_fromcstr_alloc(2048, "ctags ");
        for (unsigned i = 0; i < settings.ctags_args->qty; ++i) {
                b_concat(cmd, settings.ctags_args->lst[i]);
                b_conchar(cmd, ' ');
        }

        /* b_formata(cmd, " -R -f- '%s' | sort >&%d", BS(bdata->topdir), bdata->tmpfd); */
        b_formata(cmd, " -R -f%s '%s'", BS(bdata->tmpfname), BS(bdata->topdir));

        nvprintf("Running ctags command \"\"%s\"\"\n", BS(cmd));

        int status = system(BS(cmd));
        if (status != 0)
                warn("ctags failed with status \"%d\"", status);

        b_destroy(cmd);

        return (status == 0);
}


b_list *
get_archived_tags(struct bufdata *bdata)
{
        b_list *ret = b_list_create_alloc(512);
        assert(getlines(ret, settings.compression_type, bdata->gzfile) == 1);
        return ret;
}
