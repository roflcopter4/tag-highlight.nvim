#include "tag_highlight.h"

#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "clang/clang.h"

#include <lzma.h>

#undef nvim_get_var_l
#define nvim_get_var_l(VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var(0, B(PKG VARNAME_), (EXPECT_), (KEY_), (FATAL_))

struct cmd_info {
        int kind;
        bstring *group;
        bstring *prefix;
        bstring *suffix;
};

static nvim_call_array *update_commands(struct bufdata *bdata, struct taglist *tags);
static void     update_from_cache(struct bufdata *bdata);
static bstring *get_restore_cmds(b_list *restored_groups);
static void     add_cmd_call(nvim_call_array **calls, bstring *cmd);
static void     get_tags_from_restored_groups(struct bufdata *bdata, b_list *restored_groups);
static void     get_ignored_tags(struct bufdata *bdata);
static void     update_c_like(struct bufdata *bdata, bool force);
static void     update_other(struct bufdata *bdata);
static int      handle_kind(bstring *cmd, unsigned i, const struct filetype *ft,
                            const struct taglist  *tags, const struct cmd_info *info);

extern pthread_mutex_t update_mutex;
#ifdef DEBUG
extern FILE *cmd_log;
#  define LOGCMD(...) fprintf(cmd_log, __VA_ARGS__)
#else
#  define LOGCMD(...)
#endif

/*======================================================================================*/
#define BBS(BSTR) ((char *)((BSTR)->data))

void
(update_highlight)(const int bufnum, struct bufdata *bdata, const bool force)
{
        struct timer t;
        pthread_mutex_lock(&update_mutex);
        TIMER_START(t);

        bdata = null_find_bufdata(bufnum, bdata);
        if (!bdata->topdir || !bdata->lines || bdata->lines->qty <= 1) {
                pthread_mutex_unlock(&update_mutex);
                return;
        }
        ECHO("Updating commands for bufnum %d", bdata->num);

        if (!bdata->ft->restore_cmds_initialized) 
                get_ignored_tags(bdata);

        if (bdata->ft->is_c)
                update_c_like(bdata, force);
        else if (bdata->calls)
                update_from_cache(bdata);
        else
                update_other(bdata);

        TIMER_REPORT(t, "update highlight");
        pthread_mutex_unlock(&update_mutex);
}

static void
update_c_like(struct bufdata *bdata, const bool force)
{
        echo("File is c, not doing things.");
        libclang_highlight(bdata, 0, (-1), force);
        if (!bdata->topdir->tags && !bdata->initialized) {
                update_taglist(bdata, UPDATE_TAGLIST_FORCE);
                bdata->initialized = true;
        }
}

static void
update_from_cache(struct bufdata *bdata)
{
        ECHO("Updating from cache.");
        nvim_call_atomic(0, bdata->calls);
        if (bdata->ft->restore_cmds)
                nvim_command(0, bdata->ft->restore_cmds);
}

static void
update_other(struct bufdata *bdata)
{
        bstring        *joined;
        b_list         *toks;
        struct taglist *tags;
        bool            retry = true;
retry:
        joined = strip_comments(bdata);
        toks   = tokenize(bdata, joined); 
        tags   = process_tags(bdata, toks);

        if (tags) {
                retry = false;
                ECHO("Got %u total tags\n", tags->qty);
                if (bdata->calls)
                        destroy_call_array(bdata->calls);
                bdata->calls = update_commands(bdata, tags);
                nvim_call_atomic(0, bdata->calls);

                for (unsigned i = 0; i < tags->qty; ++i) {
                        b_free(tags->lst[i]->b);
                        xfree(tags->lst[i]);
                }
                xfree(tags->lst);
                xfree(tags);

                if (bdata->ft->restore_cmds) {
                        LOGCMD("%s\n\n", BS(bdata->ft->restore_cmds));
                        nvim_command(0, bdata->ft->restore_cmds);
                }
        }

        b_list_destroy(toks);
        b_free(joined);

        if (retry) {
                ECHO("Nothing whatsoever found. Re-running ctags with the "
                     "'--language-force' option.");
                update_taglist(bdata, UPDATE_TAGLIST_FORCE_LANGUAGE);
                retry = false;
                goto retry;
        }
}

/*======================================================================================*/

static nvim_call_array *
update_commands(struct bufdata *bdata, struct taglist *tags)
{
        const unsigned   ngroups = bdata->ft->order->slen;
        struct cmd_info *info    = nalloca(ngroups, sizeof(*info));

        for (unsigned i = 0; i < ngroups; ++i) {
                const int     ch   = bdata->ft->order->data[i];
                mpack_dict_t *dict = nvim_get_var_fmt(
                        0, E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;

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

        nvim_call_array *calls = NULL;
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

                b_free(info[i].group);
                if (info[i].prefix)
                        b_free(info[i].prefix);
                if (info[i].suffix)
                        b_free(info[i].suffix);
        }

#ifdef DEBUG
        fputs("\n\n\n\n", cmd_log);
        fflush(cmd_log);
#endif

        return calls;
}


#define SYN_MATCH_START   "syntax match %s /%s\\%%(%s"
#define SYN_MATCH_END     "\\)%s/ display | hi def link %s %s"
#define SYN_KEYWORD_START " syntax keyword %s %s "
#define SYN_KEYWORD_END   "display containedin=ALLBUT,String | hi def link %s %s"

static int
handle_kind(bstring *cmd, unsigned i,
            const struct filetype *ft,
            const struct taglist  *tags,
            const struct cmd_info *info)
{
        bstring *group_id = b_sprintf("_tag_highlight_%s_%c_%s",
                                      &ft->vim_name, info->kind, info->group);
        b_sprintfa(cmd, "silent! syntax clear %s | ", group_id);

        if (info->prefix || info->suffix) {
                bstring *prefix = (info->prefix) ? info->prefix : B("\\C\\<");
                bstring *suffix = (info->suffix) ? info->suffix : B("\\>");

                b_sprintfa(cmd, SYN_MATCH_START, group_id, prefix, tags->lst[i++]->b);
                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                        b_sprintfa(cmd, "\\|%s", tags->lst[i]->b);
                b_sprintfa(cmd, SYN_MATCH_END, suffix, group_id, info->group);
        }
        else {
                b_sprintfa(cmd, SYN_KEYWORD_START, group_id, tags->lst[i++]->b);
                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                        b_sprintfa(cmd, "%s ", tags->lst[i]->b);
                b_sprintfa(cmd, SYN_KEYWORD_END, group_id, info->group);
        }

        b_free(group_id);
        return i;
}

/*======================================================================================*/

void
update_line(struct bufdata *bdata, const int first, const int last)
{
        /* libclang_update_line(bdata, first+1, last+1); */
        /* echo("first: %d, last: %d\n", first, last); */
        /* libclang_update_line(bdata, first, last); */
        /* libclang_get_hl_commands(bdata); */
        libclang_highlight(bdata, first, last);
}

void
(clear_highlight)(const int bufnum, struct bufdata *bdata)
{
        bstring *cmd = b_alloc_null(8192);
        bdata        = null_find_bufdata(bufnum, bdata);

        for (unsigned i = 0; i < bdata->ft->order->slen; ++i) {
                const int     ch   = bdata->ft->order->data[i];
                mpack_dict_t *dict = nvim_get_var_fmt(
                        0, E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;
                bstring *group = dict_get_key(dict, E_STRING, B("group")).ptr;

                b_sprintfa(cmd, "silent! syntax clear _tag_highlight_%s_%c_%s",
                           &bdata->ft->vim_name, ch, group);

                if (i < (bdata->ft->order->slen - 1))
                        b_catlit(cmd, " | ");

                destroy_mpack_dict(dict);
        }

        nvim_command(0, cmd);

        if (bdata->hl_id > 0) {
                nvim_buf_clear_highlight(0, bdata->num, bdata->hl_id, 0, (-1));
        }

        b_free(cmd);
}

/*======================================================================================*/

static void
get_tags_from_restored_groups(struct bufdata *bdata, b_list *restored_groups)
{
        if (!bdata->ft->ignored_tags)
                bdata->ft->ignored_tags = b_list_create();

        for (unsigned i = 0; i < restored_groups->qty; ++i) {
                char         cmd[2048];
                const size_t len = snprintf(cmd, 2048, "syntax list %s",
                                            BS(restored_groups->lst[i]));
                bstring *output = nvim_command_output(0, btp_fromblk(cmd, len), E_STRING).ptr;
                if (!output)
                        continue;

                const char *ptr = strstr(BS(output), "xxx");
                if (!ptr) {
                        b_free(output);
                        continue;
                }
                ptr += 4;
                bstring tmp = bt_fromblk(ptr, output->slen - PSUB(ptr, output->data));
                b_writeallow(&tmp);

                if (strncmp(ptr, SLS("match /")) != 0) {
                        bstring *line = &(bstring){0, 0, NULL, BSTR_WRITE_ALLOWED};

                        while (b_memsep(line, &tmp, '\n')) {
                                while (isblank(*line->data)) {
                                        ++line->data;
                                        --line->slen;
                                }
                                if (strncmp(BS(line), SLS("links to ")) == 0)
                                        break;

                                bstring *tok = &(bstring){0, 0, NULL, 0};

                                while (b_memsep(tok, line, ' ')) {
                                        bstring *toadd = b_fromblk(tok->data, tok->slen);
                                        toadd->flags  |= BSTR_MASK_USR1;
                                        b_list_append(&bdata->ft->ignored_tags, toadd);
                                }
                        }
                }

                b_free(output);
        }

        B_LIST_SORT_FAST(bdata->ft->ignored_tags);
}

static bstring *
get_restore_cmds(b_list *restored_groups)
{
        assert(restored_groups);
        b_list *allcmds = b_list_create_alloc(restored_groups->qty);

        for (unsigned i = 0; i < restored_groups->qty; ++i) {
                bstring *cmd    = b_sprintf("syntax list %s", restored_groups->lst[i]);
                bstring *output = nvim_command_output(0, cmd, E_STRING).ptr;
                b_free(cmd);
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
                b_sprintfa(cmd, "syntax clear %s | ", restored_groups->lst[i]);

                b_list *toks = b_list_create();

                /* Only syntax keywords can replace previously supplied items,
                 * so just ignore any match groups. */
                if (strncmp(ptr, SLS("match /")) != 0) {
                        char *tmp;
                        char link_name[1024];
                        b_sprintfa(cmd, "syntax keyword %s ", restored_groups->lst[i]);

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
                        b_sprintfa(cmd, " | hi! link %s %s",
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
get_ignored_tags(struct bufdata *bdata)
{
        mpack_dict_t *tmp = nvim_get_var(0, B("tag_highlight#restored_groups"), E_MPACK_DICT).ptr;
        b_list *restored_groups = dict_get_key(tmp, E_STRLIST, &bdata->ft->vim_name).ptr;

        if (restored_groups)
                b_list_writeprotect(restored_groups);
        destroy_mpack_dict(tmp);

        if (restored_groups) {
                b_list_writeallow(restored_groups);
                if (bdata->ft->id == FT_C || bdata->ft->id == FT_CPP) {
                        get_tags_from_restored_groups(bdata, restored_groups);
#ifdef DEBUG
                        b_list_dump(cmd_log, bdata->ft->ignored_tags);
#endif
                } else
                        bdata->ft->restore_cmds = get_restore_cmds(restored_groups);

                b_list_destroy(restored_groups);
        }

        bdata->ft->restore_cmds_initialized = true;
}

/*======================================================================================*/

static void
add_cmd_call(nvim_call_array **calls, bstring *cmd)
{
        assert(calls);
        if (!*calls) {
                (*calls)        = xmalloc(sizeof **calls);
                (*calls)->mlen  = 16;
                (*calls)->fmt   = xcalloc((*calls)->mlen, sizeof(char *));
                (*calls)->args  = xcalloc((*calls)->mlen, sizeof(union atomic_call_args *));
                (*calls)->qty   = 0;
        } else if ((*calls)->qty >= (*calls)->mlen-1) {
                (*calls)->mlen *= 2;
                (*calls)->fmt   = nrealloc((*calls)->fmt,  (*calls)->mlen, sizeof(char *));
                (*calls)->args  = nrealloc((*calls)->args, (*calls)->mlen, sizeof(union atomic_call_args *));
        }

        (*calls)->args[(*calls)->qty]        = nmalloc(2, sizeof(union atomic_call_args));
        (*calls)->fmt[(*calls)->qty]         = strdup("s[s]");
        (*calls)->args[(*calls)->qty][0].str = b_lit2bstr("nvim_command");
        (*calls)->args[(*calls)->qty][1].str = cmd;

        ++(*calls)->qty;
}
