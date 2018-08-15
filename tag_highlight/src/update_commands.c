#include "util.h"

#include "data.h"
#include "highlight.h"
#include "mpack.h"

#undef nvim_get_var_l
#define nvim_get_var_l(VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var(0, B(PKG "#" VARNAME_), (EXPECT_), (KEY_), (FATAL_))

struct cmd_info {
        int kind;
        bstring *group;
        bstring *prefix;
        bstring *suffix;
};

static int  handle_kind(bstring *cmd, unsigned i, const struct ftdata_s *ft,
                        const struct taglist  *tags, const struct cmd_info *info);
static struct atomic_call_array *update_commands(struct bufdata *bdata,
                                                 struct taglist *tags);
static void update_from_cache(struct bufdata *bdata);
static bstring *get_restore_cmds(b_list *restored_groups);
static void add_cmd_call(struct atomic_call_array **calls, bstring *cmd);


extern pthread_mutex_t update_mutex;
static unsigned usable = 0;

#ifdef DEBUG
extern FILE *cmd_log;
#  define LOGCMD(...) fprintf(cmd_log, __VA_ARGS__)
#else
#  define LOGCMD(...)
#endif


/*======================================================================================*/


void
update_highlight(const int bufnum, struct bufdata *bdata)
{
        pthread_mutex_lock(&update_mutex);
        bdata = null_find_bufdata(bufnum, bdata);
        echo("Updating commands for bufnum %d", bufnum);

        if (!bdata->ft->restore_cmds_initialized) {
                mpack_dict_t *tmp = nvim_get_var(
                        0, B("tag_highlight#restored_groups"), E_MPACK_DICT).ptr;

                b_list *restored_groups = dict_get_key(
                        tmp, E_STRLIST, &bdata->ft->vim_name).ptr;

                if (restored_groups)
                        b_list_writeprotect(restored_groups);
                destroy_mpack_dict(tmp);
                if (restored_groups) {
                        b_list_writeallow(restored_groups);
                        bdata->ft->restore_cmds = get_restore_cmds(restored_groups);
                        b_list_destroy(restored_groups);
                }
                bdata->ft->restore_cmds_initialized = true;
        }

        if (bdata->cmd_cache) {
                update_from_cache(bdata);
                pthread_mutex_unlock(&update_mutex);
                return;
        }

        bstring *joined = strip_comments(bdata);
        b_list  *toks   = tokenize(bdata, joined);

        struct taglist *tags = process_tags(bdata, toks);
        if (tags) {
                usable = 0;
                echo("Got %u total tags\n", tags->qty);

                if (bdata->calls)
                        destroy_call_array(bdata->calls);
                bdata->calls = update_commands(bdata, tags);
                echo("Got %u usable tags for buffer %d\n", usable, bdata->num);
                nvim_call_atomic(0, bdata->calls);

                for (unsigned i = 0; i < tags->qty; ++i) {
                        b_destroy(tags->lst[i]->b);
                        xfree(tags->lst[i]);
                }
                xfree(tags->lst);
                xfree(tags);

                if (bdata->ft->restore_cmds) {
                        LOGCMD("%s\n\n", BS(bdata->ft->restore_cmds));
                        nvim_command(0, bdata->ft->restore_cmds);
                }
        } else {
                echo("Nothing whatsoever found...");
        }

        b_list_destroy(toks);
        b_destroy(joined);
        pthread_mutex_unlock(&update_mutex);
}


static struct atomic_call_array *
update_commands(struct bufdata *bdata, struct taglist *tags)
{
        const unsigned   ngroups = bdata->ft->order->slen;
        struct cmd_info *info    = nalloca(ngroups, sizeof(*info));

        if (bdata->cmd_cache)
                b_list_destroy(bdata->cmd_cache);
        bdata->cmd_cache = b_list_create();

        for (unsigned i = 0; i < ngroups; ++i) {
                const int     ch   = bdata->ft->order->data[i];
                mpack_dict_t *dict = nvim_get_var_fmt(0, E_MPACK_DICT,
                                                      PKG "#%s#%c",
                                                      BTS(bdata->ft->vim_name), ch).ptr;

                info[i].kind   = ch;
                info[i].group  = dict_get_key(dict, E_STRING, B("group")).ptr;
                info[i].prefix = dict_get_key(dict, E_STRING, B("prefix")).ptr;
                info[i].suffix = dict_get_key(dict, E_STRING, B("suffix")).ptr;

                b_writeprotect(info[i].group);
                b_writeprotect(info[i].prefix);
                b_writeprotect(info[i].suffix);
                destroy_mpack_dict(dict);
                b_writeallow(info[i].group);
                b_writeallow(info[i].prefix);
                b_writeallow(info[i].suffix);
        }

        struct atomic_call_array *calls = NULL;
        add_cmd_call(&calls, b_lit2bstr("ownsyntax"));

        for (unsigned i = 0; i < ngroups; ++i) {
                unsigned ctr = 0;
                for (; ctr < tags->qty; ++ctr)
                        if (tags->lst[ctr]->kind == info[i].kind)
                                break;

                if (ctr != tags->qty) {
                        bstring *cmd = b_alloc_null(0x4000);
                        handle_kind(cmd, ctr, bdata->ft, tags, &info[i]);
                        LOGCMD("%s\n\n", BS(cmd));
                        add_cmd_call(&calls, cmd);
                }

                b_destroy(info[i].group);
                if (info[i].prefix)
                        b_destroy(info[i].prefix);
                if (info[i].suffix)
                        b_destroy(info[i].suffix);
        }

#ifdef DEBUG
        fputs("\n\n\n\n", cmd_log);
        fflush(cmd_log);
#endif

        return calls;
}


static int
handle_kind(bstring *cmd, unsigned i,
            const struct ftdata_s *ft,
            const struct taglist  *tags,
            const struct cmd_info *info)
{
        bstring *group_id = b_sprintf(B("_tag_highlight_%s_%c_%s"),
                                      &ft->vim_name, info->kind, info->group);
        b_sprintfa(cmd, B("silent! syntax clear %s | "), group_id);

        if (info->prefix || info->suffix) {
                bstring *prefix = (info->prefix) ? info->prefix : B("\\C\\<");
                bstring *suffix = (info->suffix) ? info->suffix : B("\\>");

                b_sprintfa(cmd, B("syntax match %s /%s\\%%(%s"),
                                 group_id, prefix, tags->lst[i++]->b);

                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                {
                        if (!b_iseq(tags->lst[i]->b, tags->lst[i-1]->b)) {
                                b_sprintfa(cmd, B("\\|%s"), tags->lst[i]->b);
                                ++usable;
                        }
                }

                b_sprintfa(cmd, B("\\)%s/ display | hi def link %s %s"),
                            suffix, group_id, info->group);
        } else {
                b_sprintfa(cmd, B(" syntax keyword %s %s "),
                            group_id, tags->lst[i++]->b);

                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                {
                        if (!b_iseq(tags->lst[i]->b, tags->lst[i-1]->b)) {
                                b_sprintfa(cmd, B("%s "), tags->lst[i]->b);
                                ++usable;
                        }
                }

                b_sprintfa(cmd, B("display | hi def link %s %s"),
                            group_id, info->group);
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
                const int     ch   = bdata->ft->order->data[i];
                mpack_dict_t *dict = nvim_get_var_fmt(0, E_MPACK_DICT,
                                                      PKG "#%s#%c",
                                                      BTS(bdata->ft->vim_name), ch).ptr;
                bstring *group = dict_get_key(dict, E_STRING, B("group")).ptr;

                b_sprintfa(cmd, B("silent! syntax clear _tag_highlight_%s_%c_%s"),
                            &bdata->ft->vim_name, ch, group);

                if (i < (bdata->ft->order->slen - 1))
                        b_concat(cmd, B(" | "));

                destroy_mpack_dict(dict);
        }

        nvim_command(0, cmd);
        b_free(cmd);
}


/*======================================================================================*/


static bstring *
get_restore_cmds(b_list *restored_groups)
{
        assert(restored_groups);
        b_list *allcmds = b_list_create_alloc(restored_groups->qty);

        for (unsigned i = 0; i < restored_groups->qty; ++i) {
                bstring *cmd    = b_sprintf(B("syntax list %s"), restored_groups->lst[i]);
                bstring *output = nvim_command_output(0, cmd, E_STRING).ptr;
                b_destroy(cmd);
                if (!output)
                        continue;

                cmd       = b_alloc_null(64u + output->slen);
                char *ptr = strstr(BS(output), "xxx");
                if (!ptr) {
                        b_free(output);
                        continue;
                }

                ptr += 4;
                assert(!isblank(*ptr));
                b_sprintfa(cmd, B("syntax clear %s | "), restored_groups->lst[i]);

                b_list *toks = b_list_create();

                /* Only syntax keywords can replace previously supplied items,
                 * so just ignore any match groups. */
                if (strncmp(ptr, SLS("match /")) != 0) {
                        char *tmp;
                        char link_name[1024];
                        b_sprintfa(cmd, B("syntax keyword %s "), restored_groups->lst[i]);

                        while ((tmp = strchr(ptr, '\n'))) {
                                b_list_append(&toks, b_fromblk(ptr, PSUB(tmp, ptr)));
                                while (isblank(*++tmp))
                                        ;
                                if (strncmp((ptr = tmp), "links to ", 9) == 0)
                                        if (!((tmp = strchr(ptr, '\n')) + 1))
                                                break;
                        }

                        b_list_remove_dups(&toks);
                        for (unsigned x = 0; x < toks->qty; ++x) {
                                b_concat(cmd, toks->lst[x]);
                                b_conchar(cmd, ' ');
                        }
                        b_list_destroy(toks);

                        const size_t n = strlcpy(link_name, (ptr += 9), 1024);
                        assert(n > 0);
                        b_sprintfa(cmd, B(" | hi! link %s %s"),
                                    restored_groups->lst[i], btp_fromcstr(link_name));

                        b_list_append(&allcmds, cmd);
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
        nvim_call_atomic(0, bdata->calls);
        if (bdata->ft->restore_cmds)
                nvim_command(0, bdata->ft->restore_cmds);
}


/*======================================================================================*/


static void
add_cmd_call(struct atomic_call_array **calls, bstring *cmd)
{
        assert(calls);
        if (!*calls) {
                (*calls)        = xmalloc(sizeof **calls);
                (*calls)->mlen  = 16;
                (*calls)->fmt   = xcalloc(sizeof(char *), (*calls)->mlen);
                (*calls)->args  = xcalloc(sizeof(union atomic_call_args *),
                                          (*calls)->mlen);
                (*calls)->qty   = 0;
        } else if ((*calls)->qty >= (*calls)->mlen-1) {
                (*calls)->mlen *= 2;
                (*calls)->fmt   = nrealloc((*calls)->fmt,
                                           sizeof(char *),
                                           (*calls)->mlen);
                (*calls)->args  = nrealloc((*calls)->args,
                                           sizeof(union atomic_call_args *),
                                           (*calls)->mlen);
        }

        (*calls)->args[(*calls)->qty]        = nmalloc(sizeof(union atomic_call_args), 2);
        (*calls)->fmt[(*calls)->qty]         = strdup("s[s]");
        (*calls)->args[(*calls)->qty][0].str = b_lit2bstr("nvim_command");
        (*calls)->args[(*calls)->qty][1].str = cmd;

        ++(*calls)->qty;
}
