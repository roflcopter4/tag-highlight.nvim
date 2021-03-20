#include "Common.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "lang/ctags_scan/scan.h"

extern int highlight_go(Buffer *bdata);

struct cmd_call_info {
        int      kind;
        bstring *group;
        bstring *prefix;
        bstring *suffix;
};

static mpack_arg_array *update_commands(Buffer *bdata, struct taglist *tags);
static void update_from_cache(Buffer *bdata);
static void add_cmd_call(mpack_arg_array **calls, bstring *cmd);
static void update_c_like(Buffer *bdata, int type);
static void update_other(Buffer *bdata);
static int  handle_kind(bstring *cmd, unsigned i,   struct filetype const *ft,
                        struct taglist const *tags, struct cmd_call_info const *info);

#if defined DEBUG && defined DEBUG_LOGS
extern FILE *cmd_log;
#  define LOGCMD(...) fprintf(cmd_log, __VA_ARGS__)
#else
#  define LOGCMD(...)
#endif

#define CTX update_top_talloc_ctx_
void *update_top_talloc_ctx_ = NULL;

/*======================================================================================*/

void
(update_highlight)(Buffer *bdata, enum update_highlight_type const type)
{
        //struct timer t[1] = {STRUCT_TIMER_INITIALIZER};

        if (!bdata || !bdata->topdir || !bdata->lines)
                return;

        //pthread_mutex_lock(&bdata->lock.total);
        //echo("Updating commands for bufnum %d", bdata->num);
        //TIMER_START(t);

        if (bdata->ft->has_parser) {
                int ret = 0;

                if (bdata->ft->is_c) {
                        update_c_like(bdata, type);
                } else if (bdata->ft->id == FT_GO) {
                        ret = highlight_go(bdata);
                } else {
                        errx(1, "Impossible filetype.");
                }

                switch (ret) {
                case 0: /* OK */
                case -1:
                        /* Something went wrong, but we'll assume the error was due to an
                         * incomplete line the user is typing and try again soon. */
                        break;
                case ENOENT:
                case ENOEXEC:
                        SHOUT("Parser not found, falling back to ctags: %s\n", strerror(ret));
                        goto parser_failed;
                default:
                        SHOUT("Unexpected parser error: (%d), %s\n",
                                        ret, (ret > 0 ? strerror(ret) : "CUSTOM"));
                        goto parser_failed;
                }
        } else {
        parser_failed:
                if (bdata->calls)
                        update_from_cache(bdata);
                else
                        update_other(bdata);
        }

        //TIMER_REPORT(t, "update highlight");
        //pthread_mutex_unlock(&bdata->lock.total);
}

static void
update_c_like(Buffer *bdata, int const type)
{
        libclang_highlight(bdata,,,type);
        if (bdata->initialized && type != HIGHLIGHT_NORMAL)
                update_taglist(bdata, UPDATE_TAGLIST_FORCE);
}

static void
update_from_cache(Buffer *bdata)
{
        pthread_mutex_lock(&bdata->lock.total);
        echo("Updating from cache.");
        nvim_call_atomic(bdata->calls);
        if (bdata->ft->restore_cmds)
                nvim_command(bdata->ft->restore_cmds);
        pthread_mutex_unlock(&bdata->lock.total);
}

static void
update_other(Buffer *bdata)
{
        bstring        *joined;
        b_list         *toks;
        struct taglist *tags;
        bool            retry = true;

        pthread_mutex_lock(&bdata->lock.total);
retry:
        joined = strip_comments(bdata);
        toks   = tokenize(bdata, joined); 
        tags   = process_tags(bdata, toks);

        if (tags) {
                retry = false;
                echo("Got %u total tags\n", tags->qty);
                if (bdata->calls)
                        talloc_free(bdata->calls);
                bdata->calls = update_commands(bdata, tags);
                talloc_steal(bdata, bdata->calls);
                nvim_call_atomic(bdata->calls);

                for (unsigned i = 0; i < tags->qty; ++i) {
                        b_free(tags->lst[i]->b);
                        talloc_free(tags->lst[i]);
                }
                talloc_free(tags->lst);
                talloc_free(tags);

                if (bdata->ft->restore_cmds) {
                        LOGCMD("%s\n\n", BS(bdata->ft->restore_cmds));
                        nvim_command(bdata->ft->restore_cmds);
                }
        }

        b_list_destroy(toks);
        b_free(joined);

        if (retry) {
                echo("Nothing whatsoever found. Re-running ctags with the "
                     "'--language-force' option.");
                update_taglist(bdata, UPDATE_TAGLIST_FORCE_LANGUAGE);
                retry = false;
                goto retry;
        }

        pthread_mutex_unlock(&bdata->lock.total);
}

/*======================================================================================*/

static mpack_arg_array *
update_commands(Buffer *bdata, struct taglist *tags)
{
        unsigned const        ngroups = bdata->ft->order->slen;
        struct cmd_call_info *info =
            talloc_array(CTX, struct cmd_call_info, ngroups);

        for (unsigned i = 0; i < ngroups; ++i) {
                int const   ch   = bdata->ft->order->data[i];
                mpack_dict *dict = nvim_get_var_fmt(
                        E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;

                info[i].kind   = ch;
                info[i].group  = mpack_dict_get_key(dict, E_STRING, B("group")).ptr;
                info[i].prefix = mpack_dict_get_key(dict, E_STRING, B("prefix")).ptr;
                info[i].suffix = mpack_dict_get_key(dict, E_STRING, B("suffix")).ptr;

                talloc_steal(info, info[i].group);
                talloc_steal(info, info[i].prefix);
                talloc_steal(info, info[i].suffix);
                talloc_free(dict);
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
        }


#if defined DEBUG && defined DEBUG_LOGS
        fputs("\n\n\n\n", cmd_log);
        fflush(cmd_log);
#endif

        talloc_free(info);
        return calls;
}

static int
handle_kind(bstring *cmd, unsigned i,
            struct filetype      const *ft,
            struct taglist       const *tags,
            struct cmd_call_info const *info)
{
        bstring *group_id = b_sprintf("_tag_highlight_%s_%c_%s",
                                      &ft->vim_name, info->kind, info->group);
        b_sprintfa(cmd, "silent! syntax clear %s | ", group_id);

        bstring *global_allbut = nvim_get_var(B("tag_highlight#allbut"), E_STRING).ptr;
        bstring *ft_allbut = nvim_get_var_fmt(E_STRING, "tag_highlight#%s#allbut",
                                              BTS(ft->vim_name)).ptr;

#ifdef DEBUG
        talloc_steal(CTX, global_allbut);
        talloc_steal(CTX, global_allbut);
#endif

        if (ft_allbut) {
                if (global_allbut) {
                        b_append_all(global_allbut, B(","), ft_allbut);
                        b_free(ft_allbut);
                } else {
                        global_allbut = ft_allbut;
                }
        }

        if (info->prefix || info->suffix) {
                bstring const *const prefix = (info->prefix) ? info->prefix : B("\\C\\<");
                bstring const *const suffix = (info->suffix) ? info->suffix : B("\\>");

                b_sprintfa(cmd, "syntax match %s /%s\\%%(%s",
                           group_id, prefix, tags->lst[i++]->b);
                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                        b_sprintfa(cmd, "\\|%s", tags->lst[i]->b);
                if (global_allbut)
                        b_sprintfa(cmd, "\\)%s/ containedin=ALLBUT,%s display | hi def link %s %s",
                                   suffix, global_allbut, group_id, info->group);
                else
                        b_sprintfa(cmd, "\\)%s/ display | hi def link %s %s",
                                   suffix, group_id, info->group);
        } else {
                b_sprintfa(cmd, " syntax keyword %s %s ",
                           group_id, tags->lst[i++]->b);
                for (; (i < tags->qty) && (tags->lst[i]->kind == info->kind); ++i)
                        b_sprintfa(cmd, "%s ", tags->lst[i]->b);
                b_sprintfa(cmd, "display | hi def link %s %s",
                           group_id, info->group);
        }


        b_free(global_allbut);
        b_free(group_id);
        return ((i > 0) ? (int)i : 0);
}

/*======================================================================================*/

void
update_line(Buffer *bdata, int const first, int const last)
{
        libclang_highlight(bdata, first, last);
}

void
(clear_highlight)(Buffer *bdata, bool const blocking)
{
        if (!bdata)
                return;
        pthread_mutex_lock(&bdata->lock.total);

        if (bdata->ft->order && !(bdata->ft->has_parser)) {
                bstring *cmd = b_alloc_null(8192);

                for (unsigned i = 0; i < bdata->ft->order->slen; ++i) {
                        int const   ch   = bdata->ft->order->data[i];
                        mpack_dict *dict = nvim_get_var_fmt(E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;
                        bstring *group   = mpack_dict_get_key(dict, E_STRING, B("group")).ptr;

                        b_sprintfa(cmd, "silent! syntax clear _tag_highlight_%s_%c_%s",
                                   &bdata->ft->vim_name, ch, group);

                        if (i < (bdata->ft->order->slen - 1))
                                b_catlit(cmd, " | ");

                        b_free(group);
                        talloc_free(dict);
                }

                nvim_command(cmd);
                b_free(cmd);
        }

        if (bdata->hl_id > 0)
                nvim_buf_clear_highlight(bdata->num, bdata->hl_id, 0, (-1), blocking);

        pthread_mutex_unlock(&bdata->lock.total);
}

/*======================================================================================*/

static void
add_cmd_call(mpack_arg_array **calls, bstring *cmd)
{
#define CL (*calls)
        if (!CL) {
                CL        = talloc(CTX, mpack_arg_array);
                CL->qty   = 0;
                CL->mlen  = 16;
                CL->fmt   = talloc_zero_array(CL, char *, CL->mlen);
                CL->args  = talloc_zero_array(CL, mpack_argument *, CL->mlen);
        } else if (CL->qty >= CL->mlen-1) {
                CL->mlen *= 2;
                CL->fmt   = talloc_realloc(CL, CL->fmt, char *, CL->mlen);
                CL->args  = talloc_realloc(CL, CL->args, mpack_argument *, CL->mlen);
        }

        bstring *tmp = b_fromlit("nvim_command");
        CL->fmt[CL->qty]         = talloc_strdup(CL->fmt, "s[s]");
        CL->args[CL->qty]        = talloc_array(CL->args, mpack_argument, 2);
        CL->args[CL->qty][0].str = talloc_move(CL->args[CL->qty], &tmp);
        CL->args[CL->qty][1].str = talloc_move(CL->args[CL->qty], &cmd);

        ++CL->qty;
#undef CL
}
