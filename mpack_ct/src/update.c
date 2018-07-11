#include "util.h"

#include "contrib/bsd_funcs.h"
#include "data.h"
#include "highlight.h"
#include "mpack.h"

#undef nvim_get_var_l
#define nvim_get_var_l(VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var(0, B(PKG "#" VARNAME_), (EXPECT_), (KEY_), (FATAL_))
#define SLS(CSTRING_) (CSTRING_), (sizeof(CSTRING_) - 1)

struct cmd_info {
        int kind;
        bstring *group;
        bstring *prefix;
        bstring *suffix;
};

static int  handle_kind(bstring *cmd, unsigned i, const struct ftdata_s *ft,
                        const struct taglist  *tags, const struct cmd_info *info);
static void update_commands(struct bufdata *bdata, struct taglist *tags);
static void update_from_cache(struct bufdata *bdata);
static bstring *get_restore_cmds(b_list *restored_groups);
UNUSED static b_list *copy_blist(const b_list *list);
UNUSED static b_list *clone_blist(const b_list *list);

extern FILE *cmdlog;


/*======================================================================================*/


void
update_highlight(const int bufnum, struct bufdata *bdata)
{
        static bool in_progress = false;
        if (in_progress)
                return;
        in_progress = true;
        bdata       = null_find_bufdata(bufnum, bdata);

        if (!bdata->ft->restore_cmds_initialized) {
                b_list *restored_groups = blist_from_var(0, "restored_groups",
                                                         &bdata->ft->vim_name, 0);
                if (restored_groups) {
                        bdata->ft->restore_cmds = get_restore_cmds(restored_groups);
                        b_list_destroy(restored_groups);
                }
                bdata->ft->restore_cmds_initialized = true;
        }

        if (bdata->cmd_cache) {
                update_from_cache(bdata);
                in_progress = false;
                return;
        }

        bstring *joined = strip_comments(bdata);
        b_list  *toks   = tokenize(bdata, joined);

        FILE *ass = fopen("/home/bml/final_output.log", "a");
        for (uint i = 0; i < toks->qty; ++i) 
                b_fputs(ass, toks->lst[i], B("\n"));
        fclose(ass);

        struct taglist *tags = findemtagers(bdata, toks);
        if (tags) {
                update_commands(bdata, tags);

                for (unsigned i = 0; i < tags->qty; ++i) {
                        b_destroy(tags->lst[i]->b);
                        xfree(tags->lst[i]);
                }
                xfree(tags->lst);
                xfree(tags);
        }

        b_list_destroy(toks);
        b_destroy(joined);

        if (bdata->ft->restore_cmds) {
                fprintf(cmdlog, "%s\n\n", BS(bdata->ft->restore_cmds));
                nvim_command(0, bdata->ft->restore_cmds, 1);
        }

        in_progress = false;
}


static void
update_commands(struct bufdata *bdata, struct taglist *tags)
{
        const unsigned ngroups = bdata->ft->order->slen;
        struct cmd_info info[ngroups];

        if (bdata->cmd_cache)
                b_list_destroy(bdata->cmd_cache);
        bdata->cmd_cache = b_list_create();

        for (unsigned i = 0; i < ngroups; ++i) {
                const int     ch   = bdata->ft->order->data[i];
                mpack_dict_t *dict = nvim_get_var_fmt(0, MPACK_DICT, NULL, 1, PKG "#%s#%c",
                                                      BTS(bdata->ft->vim_name), ch);

                info[i].kind   = ch;
                info[i].group  = dict_get_key(dict, MPACK_STRING, B("group"), 1);
                info[i].prefix = dict_get_key(dict, MPACK_STRING, B("prefix"), 0);
                info[i].suffix = dict_get_key(dict, MPACK_STRING, B("suffix"), 0);

                b_writeprotect(info[i].group);
                b_writeprotect(info[i].prefix);
                b_writeprotect(info[i].suffix);
                destroy_mpack_dict(dict);
                b_writeallow(info[i].group);
                b_writeallow(info[i].prefix);
                b_writeallow(info[i].suffix);
        }

        nvim_command(0, B("ownsyntax"), 1);

        for (unsigned i = 0; i < ngroups; ++i) {
                unsigned ctr = 0;
                for (; ctr < tags->qty; ++ctr)
                        if (tags->lst[ctr]->kind == info[i].kind)
                                break;

                if (ctr != tags->qty) {
                        bstring *cmd = b_alloc_null(0x4000);
                        handle_kind(cmd, ctr, bdata->ft, tags, &info[i]);
                        fprintf(cmdlog, "%s\n\n", BS(cmd));
                        nvim_command(0, cmd, 1);
                        b_add_to_list(bdata->cmd_cache, cmd);
                }

                b_destroy(info[i].group);
                if (info[i].prefix)
                        b_destroy(info[i].prefix);
                if (info[i].suffix)
                        b_destroy(info[i].suffix);
        }
        fputs("\n\n\n\n", cmdlog);
        fflush(cmdlog);
}


static int
handle_kind(bstring *cmd, unsigned i,
            const struct ftdata_s *ft,
            const struct taglist  *tags,
            const struct cmd_info *info)
{
        bstring *group_id = b_format("_tag_highlight_%s_%c_%s", BTS(ft->vim_name),
                                     info->kind, BS(info->group));

        b_append_all(cmd, B(" "), B("silent! syntax clear"), group_id, B("|"));

        if (info->prefix || info->suffix) {
                bstring *prefix = (info->prefix) ?: B("\\C\\<");
                bstring *suffix = (info->suffix) ?: B("\\>");

                b_append_all(cmd, NULL, B("syntax match "), group_id, B(" /"),
                             prefix, B("\\%("), tags->lst[i++]->b);

                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                        if (!b_iseq(tags->lst[i]->b, tags->lst[i-1]->b))
                                b_append_all(cmd, NULL, B("\\|"), tags->lst[i]->b);

                b_append_all(cmd, NULL, B("\\)"), suffix, B("/ display | hi def link "),
                             group_id, B(" "), info->group);
        } else {
                b_append_all(cmd, B(" "), B("syntax keyword"), group_id, tags->lst[i++]->b);

                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                        if (!b_iseq(tags->lst[i]->b, tags->lst[i-1]->b))
                                b_append_all(cmd, B(" "), tags->lst[i]->b);

                b_append_all(cmd, B(" "), B("display | hi def link"), group_id, info->group);
        }

        b_destroy(group_id);
        return i;
}


/*======================================================================================*/


void
clear_highlight(const int bufnum, struct bufdata *bdata)
{
        bdata        = null_find_bufdata(bufnum, bdata);
        bstring *cmd = b_alloc_null(8192);

        for (unsigned i = 0; i < bdata->ft->order->slen; ++i) {
                const int     ch    = bdata->ft->order->data[i];
                mpack_dict_t *dict  = nvim_get_var_fmt(0, MPACK_DICT, NULL, 1, PKG "#%s#%c",
                                                     BTS(bdata->ft->vim_name), ch);
                bstring      *group = dict_get_key(dict, MPACK_STRING, B("group"), 1);

                b_formata(cmd, "silent! syntax clear _tag_highlight_%s_%c_%s",
                          BTS(bdata->ft->vim_name), ch, BS(group));

                if (i < (bdata->ft->order->slen - 1))
                        b_concat(cmd, B(" | "));

                destroy_mpack_dict(dict);
        }

        nvim_command(0, cmd, 1);
        b_free(cmd);
}


/*======================================================================================*/


static bstring *
get_restore_cmds(b_list *restored_groups)
{
        assert(restored_groups);
        b_list *allcmds = b_list_create_alloc(restored_groups->qty);

        for (unsigned i = 0; i < restored_groups->qty; ++i) {
                bstring *cmd = b_format("syntax list %s", BS(restored_groups->lst[i]));
                bstring *output = nvim_command_output(0, cmd, MPACK_STRING, NULL, 0);
                b_destroy(cmd);
                if (!output)
                        continue;

                cmd       = b_alloc_null(output->slen);
                char *ptr = strstr(BS(output), "xxx");
                ptr += 4;
                assert(!isblank(*ptr));

                /* Only syntax keywords can replace previously supplied items,
                 * so just ignore any match groups. */
                if (strncmp(ptr, SLS("match /")) != 0) {
                        char *tmp;
                        char link_name[1024];
                        b_append_all(cmd, B(" "), B("syntax keyword"), restored_groups->lst[i]);

                        while ((tmp = strchr(ptr, '\n'))) {
                                b_catblk(cmd, ptr, (tmp - ptr));
                                b_conchar(cmd, ' ');
                                while (isblank(*++tmp))
                                        ;
                                if (strncmp((ptr = tmp), "links to ", 9) == 0)
                                        if (!((tmp = strchr(ptr, '\n')) + 1))
                                                break;
                        }

                        strlcpy(link_name, (ptr += 9), 1024);
                        b_append_all(cmd, B(" "), B("| hi! link "),
                                     restored_groups->lst[i], bt_fromcstr(link_name));

                        b_add_to_list(allcmds, cmd);
                }

                b_free(output);
        }

        bstring *ret = b_join(allcmds, B(" | "));
        b_list_destroy(allcmds);

        return ret;
}


static void
update_from_cache(struct bufdata *bdata)
{
        echo("Updating from cache.");
        nvim_command(0, B("ownsyntax"), 1);

        for (unsigned i = 0; i < bdata->cmd_cache->qty; ++i)
                nvim_command(0, bdata->cmd_cache->lst[i], 1);

        if (bdata->ft->restore_cmds)
                nvim_command(0, bdata->ft->restore_cmds, 1);
}


/*======================================================================================*/


static b_list *
copy_blist(const b_list *list)
{
        b_list *ret = b_list_create_alloc(list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                ret->lst[ret->qty] = b_strcpy(list->lst[i]);
                b_writeallow(ret->lst[ret->qty]);
                ++ret->qty;
        }

        return ret;
}


static b_list *
clone_blist(const b_list *list)
{
        b_list *ret = b_list_create_alloc(list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                ret->lst[ret->qty] = b_clone_swap(list->lst[i]);
                ++ret->qty;
        }

        return ret;
}
