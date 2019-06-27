#include "clang.h"
#include "intern.h"
#if defined HAVE_POSIX_SPAWNP
#  include <spawn.h>
#  include <wait.h>
#elif defined HAVE_FORK
#  include <wait.h>
#endif

extern void get_tmp_path(char *buf);
static void clean_tmpdir();

static const char *cleanup_path;

//========================================================================================

bool
resolve_range(CXSourceRange r, struct resolved_range *res)
{
        CXSourceLocation start = clang_getRangeStart(r);
        CXSourceLocation end   = clang_getRangeEnd(r);
        unsigned         rng[2][3];
        CXFile           file;

        clang_getExpansionLocation(start, &file, &rng[0][0], &rng[0][1], &rng[0][2]);
        clang_getExpansionLocation(end,   NULL,  &rng[1][0], &rng[1][1], NULL);

        if (rng[0][0] != rng[1][0])
                return false;

        res->line   = rng[0][0];
        res->start  = rng[0][1];
        res->end    = rng[1][1];
        res->offset = rng[0][2];
        res->file   = file;
        return true;
}

#define CMD_SIZ      (4096)
#define STATUS_SHIFT (8)
#define TMP_LOCATION "/mnt/ramdisk"

void
get_tmp_path(char *buf)
{
#ifndef USE_RAMDISK
        bstring *name = nvim_call_function(,B("tempname"), E_STRING).ptr;
        memcpy(buf, name->data, name->slen+1);
        b_destroy(name);
        mkdir(buf, 0700);
#else
        memcpy(buf, SLS(TMP_LOCATION "/tag_highlight_XXXXXX"));
        errno = 0;
        if (!mkdtemp(buf))
                err(1, "mkdtemp failed");
        at_quick_exit(clean_tmpdir);
        atexit(clean_tmpdir);
#endif
        cleanup_path = buf;
}

static void
clean_tmpdir(void)
{
#if defined(HAVE_POSIX_SPAWNP) || defined(HAVE_FORK)
        int  status, pid;
        char cmd[CMD_SIZ];
        snprintf(cmd, CMD_SIZ, "rm -rf %s/tag_highlight*", TMP_LOCATION);
        char *const argv[] = {"sh", "-c", cmd, (char *)0};

#  ifdef HAVE_POSIX_SPAWNP
        posix_spawnp(&pid, "sh", NULL, NULL, argv, environ);
#  else
        if ((pid = fork()) == 0)
                execvpe("sh", argv, environ);
#  endif
        waitpid(pid, &status, 0);
#endif
}

#if 0
void
locate_extent(bstring *dest, Buffer *bdata, const struct resolved_range *const res)
{
#if 0
        ll_node *node = ll_at((linked_list *)lst, res->line);
        assert(node && node->data && node->data->data && node->data->slen >= res->end);
        memcpy(dest->data, node->data->data + res->start, dest->slen);
        dest->data[dest->slen] = '\0';
#endif
        dest->slen = res->end - res->start;
        b_list *lines = nvim_buf_get_lines(0, bdata->num, res->line+1, res->line+2);

        b_list_dump_nvim(lines);
        
        if (!dest->data || !lines || lines->qty < 1)
                errx(1, "No data.");
        bstring *str = lines->lst[0];

        if (str->slen < res->end) {
                warnx("Line length (%u) is too short (< %u)", str->slen, res->end);
                b_list_destroy(lines);
                P99_THROW(EINVAL);
        }

        memcpy(dest->data, str->data + res->start, dest->slen);
        dest->data[dest->slen] = '\0';
        b_list_destroy(lines);
}
#endif
