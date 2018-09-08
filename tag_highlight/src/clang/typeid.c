#include "util/util.h"

#include "util/list.h"
#include "clang.h"
#include "libclang.h"

#include "nvim_api/api.h"
#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"

static void do_typeswitch(struct bufdata           *bdata,
                          struct atomic_call_array *calls,
                          struct token             *tok,
                          struct cmd_info          *info,
                          const b_list             *enumerators);
static void add_hl_call(struct atomic_call_array *calls,
                        const int                 bufnum,
                        const int                 hl_id,
                        const bstring            *group,
                        const struct token       *tok);
static void
add_clr_call(struct atomic_call_array *calls,
             const int bufnum,
             const int hl_id,
             const int line,
             const int end);

static bool check_skip(struct bufdata *bdata, struct token *tok);

static struct atomic_call_array *new_call_array(void);
static const bstring *find_group(struct filetype *ft, const struct cmd_info *info, unsigned num, const int ctags_kind);
static void clear_nvim_highlights(struct bufdata *bdata);

#define TLOC(TOK) ((TOK)->line), ((TOK)->col), ((TOK)->col + (TOK)->line)
#define ADD_CALL(CH)                                                         \
        do {                                                                 \
                if (!(group = find_group(bdata->ft, info, info->num, (CH)))) \
                        goto done;                                           \
                add_hl_call(calls, bdata->num, bdata->hl_id, group, tok);    \
        } while (0)

/*======================================================================================*/

void
type_id(struct bufdata         *bdata,
        struct translationunit *stu,
        const b_list           *enumerators,
        struct cmd_info        *info,
        const int               line,
        const int               end)
{
        struct atomic_call_array *calls = new_call_array();

        if (!info)
                errx(1, "Invalid");
                /* *info = getinfo(bdata); */

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(0, bdata->num, 0, NULL, 0, 0, 0);

        add_clr_call(calls, bdata->num, bdata->hl_id, line, end);

        for (unsigned i = 0; i < stu->tokens->qty; ++i) {
                struct token *tok = stu->tokens->lst[i];
                if (check_skip(bdata, tok))
                        continue;
                do_typeswitch(bdata, calls, tok, info, enumerators);
        }

        nvim_call_atomic(0, calls);
        destroy_call_array(calls);
}

/*======================================================================================*/


static void do_typeswitch(struct bufdata           *bdata,
                          struct atomic_call_array *calls,
                          struct token             *tok,
                          struct cmd_info          *info,
                          const b_list             *enumerators)
{
        static thread_local unsigned lastline = UINT_MAX;

        const bstring *group;
        int            ctagskind = 0;

        if (lastline != tok->line) {
                add_clr_call(calls, bdata->num, (-1), tok->line, tok->line/*  + 1 */);
                lastline = tok->line;
        }

        switch (tok->cursor.kind) {
        case CXCursor_TypedefDecl:
                /* An actual typedef */
                ADD_CALL('t');
                break;
        case CXCursor_TypeRef:
                /* Referance to a typedef. */
                ADD_CALL('t');
                break;
        case CXCursor_MemberRef:
                /* A reference to a member of a struct, union, or class in
                 * non-expression context such as a designated initializer. */
                ADD_CALL('m');
                break;
        case CXCursor_MemberRefExpr:
                /* Ordinary reference to a struct/class member. */
                ADD_CALL('m');
                break;
        case CXCursor_Namespace:
                ADD_CALL('n');
                break;
        case CXCursor_NamespaceRef:
                ADD_CALL('n');
                break;
        case CXCursor_StructDecl:
                ADD_CALL('s');
                break;
        case CXCursor_UnionDecl:
                ADD_CALL('u');
                break;
        case CXCursor_ClassDecl:
                ADD_CALL('c');
                break;
        case CXCursor_EnumDecl:
                /* An enumeration. */
                ADD_CALL('g');
                break;
        case CXCursor_EnumConstantDecl:
                /** An enumerator constant. */
                ADD_CALL('e');
                break;
        case CXCursor_FieldDecl:
                /* A field or non-static data member (C++) in a struct, union, or class. */
                ADD_CALL('m');
                break;
        case CXCursor_FunctionDecl:
                ADD_CALL('f');
                break;
        case CXCursor_CallExpr:
                /* An expression that calls a function. */
                ADD_CALL('f');
                break;
#if 0
        case CXCursor_MacroDefinition:
                if (!(group = find_group(bdata->ft, info, num, 'd')))
                        break;
                add_hl_call(calls, bdata->num, bdata->hl_id, group, tok);
                break;
#endif
        case CXCursor_MacroExpansion:
                ADD_CALL('d');
                break;
        case CXCursor_DeclRefExpr:
                /* Possibly the most generic kind, this could refer to many things. */
                switch (tok->cursortype.kind) {
                case CXType_FunctionProto:
                        ADD_CALL('f');
                        goto done;
                case CXType_Int: {
                        bstring *tmp = btp_fromcstr(tok->raw);
                        if (B_LIST_BSEARCH_FAST(enumerators, tmp))
                                ADD_CALL('e');
                        goto done;
                }
                default:
                        break;
                }

        default:
                break;
        }
done:;
}


/*======================================================================================*/

static bool
check_skip(struct bufdata *bdata, struct token *tok)
{
        bstring *tmp = btp_fromcstr(tok->raw);
        return B_LIST_BSEARCH_FAST(bdata->ft->ignored_tags, tmp);
}

/*======================================================================================*/

#define INIT_ACALL_SIZE (128)
extern FILE *cmd_log;

static struct atomic_call_array *
new_call_array(void)
{
        struct atomic_call_array *calls = xmalloc(sizeof(struct atomic_call_array));
        calls->mlen = INIT_ACALL_SIZE;
        calls->fmt  = nmalloc(calls->mlen, sizeof(char *));
        calls->args = nmalloc(calls->mlen, sizeof(union atomic_call_args *));
        calls->qty  = 0;
        return calls;
}

static void
add_hl_call(struct atomic_call_array *calls,
            const int                 bufnum,
            const int                 hl_id,
            const bstring            *group,
            const struct token       *tok)
{
        assert(calls);
        if (calls->qty >= calls->mlen-1) {
                calls->mlen *= 2;
                calls->fmt  = nrealloc(calls->fmt, calls->mlen, sizeof(char *));
                calls->args = nrealloc(calls->args, calls->mlen, sizeof(union atomic_call_args *));
        }

        calls->fmt[calls->qty]         = strdup("s[dd,s,ddd]");
        calls->args[calls->qty]        = nmalloc(7, sizeof(union atomic_call_args));
        calls->args[calls->qty][0].str = b_lit2bstr("nvim_buf_add_highlight");
        calls->args[calls->qty][1].num = bufnum;
        calls->args[calls->qty][2].num = hl_id;
        calls->args[calls->qty][3].str = b_strcpy(group);
        calls->args[calls->qty][4].num = tok->line;
        calls->args[calls->qty][5].num = tok->col1;
        calls->args[calls->qty][6].num = tok->col2;

        if (cmd_log)
                fprintf(cmd_log, "nvim_buf_add_highlight(%d, %d, %s, %d, %d, %d)\n",
                        bufnum, hl_id, BS(group), tok->line, tok->col1, tok->col2);
        ++calls->qty;
}

static void
add_clr_call(struct atomic_call_array *calls,
             const int bufnum,
             const int hl_id,
             const int line,
             const int end)
{
        assert(calls);
        if (calls->qty >= calls->mlen-1) {
                calls->mlen *= 2;
                calls->fmt  = nrealloc(calls->fmt, calls->mlen, sizeof(char *));
                calls->args = nrealloc(calls->args, calls->mlen, sizeof(union atomic_call_args *));
        }

        calls->fmt[calls->qty]         = strdup("s[dddd]");
        calls->args[calls->qty]        = nmalloc(5, sizeof(union atomic_call_args));
        calls->args[calls->qty][0].str = b_lit2bstr("nvim_buf_clear_highlight");
        calls->args[calls->qty][1].num = bufnum;
        calls->args[calls->qty][2].num = hl_id;
        calls->args[calls->qty][3].num = line;
        calls->args[calls->qty][4].num = end;

        if (cmd_log)
                fprintf(cmd_log, "nvim_buf_clear_highlight(%d, %d, %d, %d)\n", bufnum, hl_id, line, end);
        ++calls->qty;
}

static void
clear_nvim_highlights(struct bufdata *bdata)
{
        /* struct atomic_call_array *calls = new_call_array(); */

        /* for (unsigned i = 0; i < bdata->hl->qty; ++i) {

        } */

        nvim_buf_clear_highlight(0, bdata->num, (-1), 0, (-1));
}

static const bstring *
find_group(struct filetype *ft, const struct cmd_info *info, const unsigned num, const int ctags_kind)
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
