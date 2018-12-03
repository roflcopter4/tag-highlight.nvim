#include "Common.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "lang/ctags_scan/scan.h"

extern int highlight_go(Buffer *bdata);

#undef nvim_get_var_l
#define nvim_get_var_l(VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var(0, B(PKG VARNAME_), (EXPECT_), (KEY_), (FATAL_))

struct cmd_info {
        int      kind;
        bstring *group;
        bstring *prefix;
        bstring *suffix;
};

static mpack_arg_array *update_commands(Buffer *bdata, struct taglist *tags);
static void     update_from_cache(Buffer *bdata);
static void     add_cmd_call(mpack_arg_array **calls, bstring *cmd);
static void     update_c_like(Buffer *bdata, int type);
static void     update_other(Buffer *bdata);
static int      handle_kind(bstring *cmd, unsigned i, const struct filetype *ft,
                            const struct taglist  *tags, const struct cmd_info *info);

#ifdef DEBUG
extern FILE *cmd_log;
#  define LOGCMD(...) fprintf(cmd_log, __VA_ARGS__)
#else
#  define LOGCMD(...)
#endif

/*======================================================================================*/
#define BBS(BSTR) ((char *)((BSTR)->data))

void
(update_highlight)(Buffer *bdata, const int type)
{
        struct timer *t = TIMER_INITIALIZER;
        TIMER_START(t);

        if (!bdata || !bdata->topdir || !bdata->lines || bdata->lines->qty <= 1)
                return;

        ECHO("Updating commands for bufnum %d", bdata->num);
        pthread_mutex_lock(&bdata->lock.update);

        if (bdata->ft->has_parser) {
                if (bdata->ft->is_c) {
                        update_c_like(bdata, type);
                } else if (bdata->ft->id == FT_GO) {
                        int ret = highlight_go(bdata);
                        switch (ret) {
                        case 0:
                                break;
                        case ENOENT: case ENOEXEC:
                                echo("Parser not found, falling back to ctags: %s\n",
                                     strerror(ret));
                                goto parser_failed;
                        default:
                                echo("Unexpected parser error: %s\n", strerror(ret));
                                goto parser_failed;
                        }
                        
                } else {
                        errx(1, "Impossible filetype.");
                }
        } else {
        parser_failed:
                if (bdata->calls)
                        update_from_cache(bdata);
                else
                        update_other(bdata);
        }

        pthread_mutex_unlock(&bdata->lock.update);
        TIMER_REPORT(t, "update highlight");
}

static void
update_c_like(Buffer *bdata, const int type)
{
        libclang_highlight(bdata,,,type);
        if (!bdata->topdir->tags && !bdata->initialized) {
                update_taglist(bdata, UPDATE_TAGLIST_FORCE);
                bdata->initialized = true;
        }
}

static void
update_from_cache(Buffer *bdata)
{
        ECHO("Updating from cache.");
        nvim_call_atomic(,bdata->calls);
        if (bdata->ft->restore_cmds)
                nvim_command(,bdata->ft->restore_cmds);
}

static void
update_other(Buffer *bdata)
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
                        mpack_destroy_arg_array(bdata->calls);
                bdata->calls = update_commands(bdata, tags);
                nvim_call_atomic(,bdata->calls);

                for (unsigned i = 0; i < tags->qty; ++i) {
                        b_destroy(tags->lst[i]->b);
                        xfree(tags->lst[i]);
                }
                xfree(tags->lst);
                xfree(tags);

                if (bdata->ft->restore_cmds) {
                        LOGCMD("%s\n\n", BS(bdata->ft->restore_cmds));
                        nvim_command(,bdata->ft->restore_cmds);
                }
        }

        b_list_destroy(toks);
        b_destroy(joined);

        if (retry) {
                ECHO("Nothing whatsoever found. Re-running ctags with the "
                     "'--language-force' option.");
                update_taglist(bdata, UPDATE_TAGLIST_FORCE_LANGUAGE);
                retry = false;
                goto retry;
        }
}

/*======================================================================================*/

static mpack_arg_array *
update_commands(Buffer *bdata, struct taglist *tags)
{
        const unsigned   ngroups = bdata->ft->order->slen;
        struct cmd_info *info    = nalloca(ngroups, sizeof(*info));

        for (unsigned i = 0; i < ngroups; ++i) {
                const int     ch   = bdata->ft->order->data[i];
                mpack_dict_t *dict = nvim_get_var_fmt(
                        E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;

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

        mpack_arg_array *calls = NULL;
        add_cmd_call(&calls, b_fromlit("ownsyntax"));

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


#define SYN_MATCH_START   "syntax match %s /%s\\%%(%s"
#define SYN_MATCH_END     "\\)%s/ containedin=ALLBUT,%s display | hi def link %s %s"
#define SYN_KEYWORD_START " syntax keyword %s %s "
#define SYN_KEYWORD_END   "display | hi def link %s %s"

static int
handle_kind(bstring *cmd, unsigned i,
            const struct filetype *ft,
            const struct taglist  *tags,
            const struct cmd_info *info)
{
        bstring *group_id = b_sprintf("_tag_highlight_%s_%c_%s",
                                      &ft->vim_name, info->kind, info->group);
        b_sprintfa(cmd, "silent! syntax clear %s | ", group_id);

        bstring *global_allbut = nvim_get_var(,B("tag_highlight#allbut"), E_STRING).ptr;
        bstring *ft_allbut     = nvim_get_var_fmt(E_STRING, "tag_highlight#%s#allbut", BTS(ft->vim_name)).ptr;
        if (ft_allbut) {
                b_concat_all(global_allbut, B(","), ft_allbut);
                b_destroy(ft_allbut);
        }

/* #if 0 */
        if (info->prefix || info->suffix) {
/* #endif */
                bstring *prefix = (info->prefix) ? info->prefix : B("\\C\\<");
                bstring *suffix = (info->suffix) ? info->suffix : B("\\>");

                b_sprintfa(cmd, SYN_MATCH_START, group_id, prefix, tags->lst[i++]->b);
                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                        b_sprintfa(cmd, "\\|%s", tags->lst[i]->b);
                b_sprintfa(cmd, SYN_MATCH_END, suffix, global_allbut, group_id, info->group);

                b_destroy(global_allbut);
/* #if 0 */
        } else {
                b_sprintfa(cmd, SYN_KEYWORD_START, group_id, tags->lst[i++]->b);
                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                        b_sprintfa(cmd, "%s ", tags->lst[i]->b);
                b_sprintfa(cmd, SYN_KEYWORD_END, group_id, info->group);
        }
/* #endif */

        b_destroy(group_id);
        return i;
}

/*======================================================================================*/

void
update_line(Buffer *bdata, const int first, const int last)
{
        libclang_highlight(bdata, first, last);
}

void
(clear_highlight)(Buffer *bdata)
{
        if (!bdata)
                return;

        if (bdata->ft->order && !(bdata->ft->has_parser)) {
                bstring *cmd = b_alloc_null(8192);

                for (unsigned i = 0; i < bdata->ft->order->slen; ++i) {
                        const int     ch   = bdata->ft->order->data[i];
                        mpack_dict_t *dict = nvim_get_var_fmt(
                                E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;
                        bstring *group = dict_get_key(dict, E_STRING, B("group")).ptr;

                        b_sprintfa(cmd, "silent! syntax clear _tag_highlight_%s_%c_%s",
                                   &bdata->ft->vim_name, ch, group);

                        if (i < (bdata->ft->order->slen - 1))
                                b_catlit(cmd, " | ");

                        destroy_mpack_dict(dict);
                }

                nvim_command(0, cmd);
                b_destroy(cmd);
        }

        if (bdata->hl_id > 0) {
                nvim_buf_clear_highlight(0, bdata->num, bdata->hl_id, 0, (-1));
        }
}

/*======================================================================================*/

static void
add_cmd_call(mpack_arg_array **calls, bstring *cmd)
{
#define CALLS (*calls)
        if (!*calls) {
                CALLS        = xmalloc(sizeof(mpack_arg_array));
                CALLS->qty   = 0;
                CALLS->mlen  = 16;
                CALLS->fmt   = xcalloc(CALLS->mlen, sizeof(char *));
                CALLS->args  = xcalloc(CALLS->mlen, sizeof(mpack_argument *));
        } else if (CALLS->qty >= CALLS->mlen-1) {
                CALLS->mlen *= 2;
                CALLS->fmt   = nrealloc(CALLS->fmt,  CALLS->mlen, sizeof(char *));
                CALLS->args  = nrealloc(CALLS->args, CALLS->mlen, sizeof(mpack_argument *));
        }

        CALLS->args[CALLS->qty]        = nmalloc(2, sizeof(mpack_argument));
        CALLS->fmt[CALLS->qty]         = STRDUP("s[s]");
        CALLS->args[CALLS->qty][0].str = b_fromlit("nvim_command");
        CALLS->args[CALLS->qty][1].str = cmd;

        ++CALLS->qty;
#undef CALLS
}

