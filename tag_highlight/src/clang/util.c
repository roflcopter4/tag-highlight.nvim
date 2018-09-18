#include "util/util.h"

#include "clang.h"
#include "intern.h"


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

#if 0
void
locate_extent(bstring *dest, struct bufdata *bdata, const struct resolved_range *const res)
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