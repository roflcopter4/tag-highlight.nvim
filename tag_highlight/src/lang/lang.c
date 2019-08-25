#include "lang/lang.h"

/*======================================================================================*/

const bstring *
find_group(struct filetype *ft, const struct cmd_info *info,
           const unsigned num, const int ctags_kind)
{
        if (b_strchr(ft->order, ctags_kind) < 0)
                return NULL;
        const bstring *ret = NULL;

        for (unsigned i = 0; i < num; ++i) {
                if (info[i].kind == ctags_kind) {
                        ret = info[i].group;
                        break;
                }
        }

        return ret;
}

struct cmd_info *
getinfo(Buffer *bdata)
{
        const unsigned   ngroups = bdata->ft->order->slen;
        struct cmd_info *info    = nmalloc(ngroups, sizeof(*info));

        for (unsigned i = 0; i < ngroups; ++i) {
                const int     ch   = bdata->ft->order->data[i];
                mpack_dict_t *dict = nvim_get_var_fmt(
                        E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;

                info[i].kind  = ch;
                info[i].group = dict_get_key(dict, E_STRING, B("group")).ptr;
                info[i].num   = ngroups;

                b_writeprotect(info[i].group);
                destroy_mpack_dict(dict);
                b_writeallow(info[i].group);
        }

        return info;
}

void
destroy_struct_info(struct cmd_info *info)
{
        if (info) {
                for (unsigned i = 0, e = info[0].num; i < e; ++i)
                        b_destroy(info[i].group);
                xfree(info);
        }
}

/*======================================================================================*/

#define INIT_ACALL_SIZE (128)

struct mpack_arg_array *
new_arg_array(void)
{
        struct mpack_arg_array *calls = xmalloc(sizeof(struct mpack_arg_array));
        calls->mlen = INIT_ACALL_SIZE;
        calls->fmt  = nmalloc(calls->mlen, sizeof(char *));
        calls->args = nmalloc(calls->mlen, sizeof(mpack_argument *));
        calls->qty  = 0;
        return calls;
}

void
add_hl_call(struct mpack_arg_array *calls,
            const int                 bufnum,
            const int                 hl_id,
            const bstring            *group,
            const struct line_data   *data)
{
        assert(calls);
        if (calls->qty >= calls->mlen-1) {
                calls->mlen *= 2;
                calls->fmt  = nrealloc(calls->fmt, calls->mlen, sizeof(char *));
                calls->args = nrealloc(calls->args, calls->mlen, sizeof(mpack_argument *));
        }

        calls->fmt[calls->qty]         = STRDUP("s[dd,s,ddd]");
        calls->args[calls->qty]        = nmalloc(7, sizeof(mpack_argument));
        calls->args[calls->qty][0].str = b_fromlit("nvim_buf_add_highlight");
        calls->args[calls->qty][1].num = bufnum;
        calls->args[calls->qty][2].num = hl_id;
        calls->args[calls->qty][3].str = b_strcpy(group);
        calls->args[calls->qty][4].num = data->line;
        calls->args[calls->qty][5].num = data->start;
        calls->args[calls->qty][6].num = data->end;

        if (cmd_log)
                fprintf(cmd_log, "nvim_buf_add_highlight(%d, %d, %s, %u, %u, %u)\n",
                        bufnum, hl_id, BS(group), data->line, data->start, data->end);
        ++calls->qty;
}

void
add_clr_call(struct mpack_arg_array *calls,
             const int bufnum,
             const int hl_id,
             const int line,
             const int end)
{
        assert(calls);
        if (calls->qty >= calls->mlen-1) {
                calls->mlen *= 2;
                calls->fmt  = nrealloc(calls->fmt, calls->mlen, sizeof(char *));
                calls->args = nrealloc(calls->args, calls->mlen, sizeof(mpack_argument *));
        }

        calls->fmt[calls->qty]         = STRDUP("s[dddd]");
        calls->args[calls->qty]        = nmalloc(5, sizeof(mpack_argument));
        calls->args[calls->qty][0].str = b_fromlit("nvim_buf_clear_highlight");
        calls->args[calls->qty][1].num = bufnum;
        calls->args[calls->qty][2].num = hl_id;
        calls->args[calls->qty][3].num = line;
        calls->args[calls->qty][4].num = end;

        if (cmd_log)
                fprintf(cmd_log, "nvim_buf_clear_highlight(%d, %d, %d, %d)\n",
                        bufnum, hl_id, line, end);
        ++calls->qty;
}
