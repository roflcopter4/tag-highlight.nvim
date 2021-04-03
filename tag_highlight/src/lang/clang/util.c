#include "clang.h"
#include "intern.h"

//========================================================================================

bool
resolve_range(CXSourceRange r, resolved_range_t *res)
{
        CXSourceLocation start = clang_getRangeStart(r);
        CXSourceLocation end   = clang_getRangeEnd(r);
        CXFile           file;
        unsigned         line2;

        clang_getFileLocation(start, &file, &res->line, &res->start, &res->offset1);
        clang_getFileLocation(end,   NULL,  &line2,     &res->end,   &res->offset2);

        res->len = res->offset2 - res->offset1;

        return (res->line == line2);
}

#define TMP_LOCATION "/mnt/ramdisk"

void
get_tmp_path(char *buf)
{
#ifndef USE_RAMDISK
        bstring *name = nvim_call_function(B("tempname"), E_STRING).ptr;
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
}
