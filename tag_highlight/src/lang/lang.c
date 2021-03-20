#include "lang/lang.h"

/*======================================================================================*/

__attribute__((__pure__))
const bstring *
find_group(struct filetype *ft, struct cmd_info const *info,
           unsigned const num, int const ctags_kind)
{
        if (b_strchr(ft->order, ctags_kind) < 0)
                return NULL;
        bstring const *ret = NULL;

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
        unsigned const   ngroups = bdata->ft->order->slen;
        struct cmd_info *info    = talloc_array(NULL, struct cmd_info, ngroups);

        for (unsigned i = 0; i < ngroups; ++i) {
                int const   ch   = bdata->ft->order->data[i];
                mpack_dict *dict = nvim_get_var_fmt(
                        E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;

                info[i].kind  = ch;
                info[i].group = mpack_dict_get_key(dict, E_STRING, B("group")).ptr;
                info[i].num   = ngroups;

                talloc_steal(info, info[i].group);
                talloc_free(dict);
        }

        return info;
}

/*======================================================================================*/

#define INIT_ACALL_SIZE (128)

struct mpack_arg_array *
new_arg_array(void)
{
        struct mpack_arg_array *calls = talloc(NULL, struct mpack_arg_array);
        calls->mlen = INIT_ACALL_SIZE;
        calls->fmt  = talloc_array(calls, char *, calls->mlen);
        calls->args = talloc_array(calls, mpack_argument *, calls->mlen);
        calls->qty  = 0;
        return calls;
}

void
add_hl_call(struct mpack_arg_array *calls,
            int const               bufnum,
            int const               hl_id,
            bstring const          *group,
            struct line_data const *data)
{
        assert(calls);
        if (calls->qty >= calls->mlen-1) {
                calls->mlen *= 2;
                calls->fmt  = talloc_realloc(calls, calls->fmt, char *, calls->mlen);
                calls->args = talloc_realloc(calls, calls->args, mpack_argument *, calls->mlen);
        }

        mpack_argument *arg = talloc_array(calls->args, mpack_argument, 7);
        arg[0].str = b_fromlit("nvim_buf_add_highlight");
        arg[1].num = bufnum;
        arg[2].num = hl_id;
        arg[3].str = b_strcpy(group);
        arg[4].num = data->line;
        arg[5].num = data->start;
        arg[6].num = data->end;

        talloc_steal(arg, arg[0].str);
        talloc_steal(arg, arg[3].str);

        calls->fmt[calls->qty]  = talloc_strdup(calls->fmt, "s[dd,s,ddd]");
        calls->args[calls->qty] = arg;
        ++calls->qty;

        if (cmd_log)
                fprintf(cmd_log, "nvim_buf_add_highlight(%d, %d, %s, %u, %u, %u)\n",
                        bufnum, hl_id, BS(group), data->line, data->start, data->end);
}

void
add_clr_call(struct mpack_arg_array *calls,
             int const bufnum,
             int const hl_id,
             int const line,
             int const end)
{
        assert(calls);
        if (calls->qty >= calls->mlen-1) {
                calls->mlen *= 2;
                calls->fmt  = talloc_realloc(calls, calls->fmt, char *, calls->mlen);
                calls->args = talloc_realloc(calls, calls->args, mpack_argument *, calls->mlen);
        }

        mpack_argument *arg = talloc_array(calls->args, mpack_argument, 5);
        arg[0].str = b_fromlit("nvim_buf_clear_highlight");
        arg[1].num = bufnum;
        arg[2].num = hl_id;
        arg[3].num = line;
        arg[4].num = end;

        talloc_steal(arg, arg[0].str);

        calls->fmt[calls->qty]  = talloc_strdup(calls->fmt, "s[dddd]");
        calls->args[calls->qty] = arg;
        ++calls->qty;

        if (cmd_log)
                fprintf(cmd_log, "nvim_buf_clear_highlight(%d, %d, %d, %d)\n",
                        bufnum, hl_id, line, end);
}
