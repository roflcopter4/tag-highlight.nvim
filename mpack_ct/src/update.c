#include "util.h"

#include "data.h"
#include "highlight.h"
#include "mpack.h"

#undef nvim_get_var_l
#define nvim_get_var_l(VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var(sockfd, B(PKG "#" VARNAME_), (EXPECT_), (KEY_), (FATAL_))

#define PKG "tag_highlight"
/* #define PKG "mytags" */

struct cmd_info {
        int kind;
        bstring *group;
        bstring *prefix;
        bstring *suffix;
};

static void handle_kind(bstring *cmd, unsigned i, const struct ftdata_s *ft,
                        const struct taglist  *tags, const struct cmd_info *info);

static void hardcocks(struct bufdata *bdata, struct taglist *tags);
static void update_from_cache(struct bufdata *bdata);

static b_list *copy_blist(const b_list *list);
static b_list * clone_blist(const b_list *list);

extern FILE *cmdlog;

void
update_highlight(const int bufnum, struct bufdata *bdata)
{
        static bool in_progress = false;
        if (in_progress)
                return;
        in_progress = true;
        bdata       = null_find_bufdata(bufnum, bdata);

        if (bdata->cmd_cache) {
                update_from_cache(bdata);
                in_progress = false;
                return;
        }

        bstring *joined = strip_comments(bdata);
        b_list  *toks   = tokenize(bdata, joined);

#if 0
        b_list *orig = bdata->topdir->tags;
        b_list *mustfree = copy_blist(bdata->topdir->tags);
        b_list *clonelist = clone_blist(mustfree);
        bdata->topdir->tags = mustfree;
#endif

        FILE *ass = fopen("/home/bml/final_output.log", "a");
        for (uint i = 0; i < toks->qty; ++i) 
                b_fputs(ass, toks->lst[i], B("\n"));
        fclose(ass);

        struct taglist *tags = findemtagers(bdata, toks);
        if (tags) {
                hardcocks(bdata, tags);

                for (unsigned i = 0; i < tags->qty; ++i) {
                        b_destroy(tags->lst[i]->b);
                        xfree(tags->lst[i]);
                }
                xfree(tags->lst);
                xfree(tags);
        }

        b_list_destroy(toks);
        b_destroy(joined);

#if 0
        b_list_destroy(mustfree);
        b_list_destroy(clonelist);
        bdata->topdir->tags = orig;
#endif

        in_progress = false;

}


static void
hardcocks(struct bufdata *bdata, struct taglist *tags)
{
        const unsigned ngroups = bdata->ft->order->slen;
        struct cmd_info info[ngroups];

        if (bdata->cmd_cache)
                b_list_destroy(bdata->cmd_cache);
        bdata->cmd_cache = b_list_create();

        for (unsigned i = 0; i < ngroups; ++i) {
                const int ch     = bdata->ft->order->data[i];
                dictionary *dict = nvim_get_var_fmt(sockfd, MPACK_DICT, NULL, 1, PKG"#%s#%c",
                                                    BTS(bdata->ft->vim_name), ch);

                info[i].kind   = ch;
                info[i].group  = dict_get_key(dict, MPACK_STRING, B("group"), 1);
                info[i].prefix = dict_get_key(dict, MPACK_STRING, B("prefix"), 0);
                info[i].suffix = dict_get_key(dict, MPACK_STRING, B("suffix"), 0);

                b_writeprotect(info[i].group);
                b_writeprotect(info[i].prefix);
                b_writeprotect(info[i].suffix);

                destroy_dictionary(dict);

                b_writeallow(info[i].group);
                b_writeallow(info[i].prefix);
                b_writeallow(info[i].suffix);
        }

        nvim_command(sockfd, B("ownsyntax"), 1);

        for (unsigned i = 0; i < ngroups; ++i) {
                unsigned ctr = 0;

                for (; ctr < tags->qty; ++ctr)
                        if (tags->lst[ctr]->kind == info[i].kind)
                                break;

                if (ctr == tags->qty) {
                        nvprintf("Kind \"%c\" is empty, skipping.\n", info[i].kind);
                } else {
                        bstring *cmd = b_alloc_null(0x4000);
                        handle_kind(cmd, ctr, bdata->ft, tags, &info[i]);

                        /* nvprintf("Running command \"%s\"\n", BS(cmd)); */
                        fprintf(cmdlog, "%s\n\n", BS(cmd));
                        nvim_command(sockfd, cmd, 1);
                        b_add_to_list(bdata->cmd_cache, cmd);
                        /* b_destroy(cmd); */
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


static void
handle_kind(bstring               *cmd,
            unsigned               i,
            const struct ftdata_s *ft,
            const struct taglist  *tags,
            const struct cmd_info *info)
{
        char group_id[256];
        snprintf(group_id, 256, "_tag_highlight_%s_%c_%s", BTS(ft->vim_name), info->kind, BS(info->group));

        b_concat(cmd, B("silent! syntax clear "));
        b_catcstr(cmd, group_id);
        b_concat(cmd, B(" | "));

        if (info->prefix || info->suffix) {
                bstring *prefix = (info->prefix) ? info->prefix : B("\\C\\<");
                bstring *suffix = (info->suffix) ? info->suffix : B("\\>");

                b_formata(cmd, "syntax match %s /%s\\%%(%s", group_id, BS(prefix), BS(tags->lst[i++]->b));

                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i) {
                        if (!b_iseq(tags->lst[i]->b, tags->lst[i-1]->b)) {
                                b_concat(cmd, B("\\|"));
                                b_concat(cmd, tags->lst[i]->b);
                        }
                }

                b_formata(cmd, "\\)%s/ display | hi def link %s %s", BS(suffix), group_id, BS(info->group));
        } else {
                b_formata(cmd, "syntax keyword %s %s ", group_id, BS(tags->lst[i++]->b));

                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i) {
                        if (!b_iseq(tags->lst[i]->b, tags->lst[i-1]->b)) {
                                b_concat(cmd, tags->lst[i]->b);
                                b_conchar(cmd, ' ');
                        }
                }

                b_formata(cmd, "display | hi def link %s %s", group_id, BS(info->group));
        }
}


/*============================================================================*/


void
clear_highlight(const int bufnum, struct bufdata *bdata)
{
        bdata        = null_find_bufdata(bufnum, bdata);
        bstring *cmd = b_alloc_null(8192);

        for (unsigned i = 0; i < bdata->ft->order->slen; ++i) {
                const int   ch   = bdata->ft->order->data[i];
                dictionary *dict = nvim_get_var_fmt(sockfd, MPACK_DICT, NULL, 1, PKG "#%s#%c",
                                                    BTS(bdata->ft->vim_name), ch);
                bstring *group = dict_get_key(dict, MPACK_STRING, B("group"), 1);

                b_formata(cmd, "silent! syntax clear _tag_highlight_%s_%c_%s",
                          BTS(bdata->ft->vim_name), ch, BS(group));

                if (i < (bdata->ft->order->slen - 1))
                        b_concat(cmd, B(" | "));

                destroy_dictionary(dict);
                /* b_destroy(group); */
        }

        nvim_command(sockfd, cmd, 1);
        b_free(cmd);
}


/*============================================================================*/


static void
update_from_cache(struct bufdata *bdata)
{
        echo("Updating from cache.");
        nvim_command(sockfd, B("ownsyntax"), 1);

        for (unsigned i = 0; i < bdata->cmd_cache->qty; ++i)
                nvim_command(sockfd, bdata->cmd_cache->lst[i], 1);
}


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
                /* b_writeallow(ret->lst[ret->qty]); */
                ++ret->qty;
        }

        return ret;
}
