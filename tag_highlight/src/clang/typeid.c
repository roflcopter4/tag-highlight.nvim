#include "tag_highlight.h"
#include "intern.h"

#include "clang.h"
#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "util/list.h"

static void do_typeswitch(struct bufdata           *bdata,
                          struct nvim_arg_array *calls,
                          struct token             *tok,
                          struct cmd_info          *info,
                          const b_list             *enumerators,
                          enum CXCursorKind *const  last_kind);
static void add_hl_call(struct nvim_arg_array *calls,
                        const int                 bufnum,
                        const int                 hl_id,
                        const bstring            *group,
                        const struct token       *tok);
static void
add_clr_call(struct nvim_arg_array *calls,
             const int bufnum,
             const int hl_id,
             const int line,
             const int end);

static bool tok_in_skip_list(struct bufdata *bdata, struct token *tok);
static void tokvisitor(struct token *tok);

static struct nvim_arg_array *new_arg_array(void);
static const bstring *find_group(struct filetype *ft, const struct cmd_info *info, unsigned num, const int ctags_kind);

#define TLOC(TOK) ((TOK)->line), ((TOK)->col), ((TOK)->col + (TOK)->line)
#define ADD_CALL(CH)                                                              \
        do {                                                                      \
                if ((group = find_group(bdata->ft, info, info->num, (CH))))       \
                        add_hl_call(calls, bdata->num, bdata->hl_id, group, tok); \
        } while (0)
#define STRDUP(STR)                                                     \
        __extension__({                                                 \
                static const char strng_[]   = ("" STR "");             \
                char *            strng_cpy_ = xmalloc(sizeof(strng_)); \
                memcpy(strng_cpy_, strng_, sizeof(strng_));             \
                strng_cpy_;                                             \
        })

#if 0
enum equiv_ctags_type {
        CTAGS_CLASS     = 'c',
        CTAGS_ENUM      = 'g',
        CTAGS_ENUMCONST = 'e',
        CTAGS_FUNCTION  = 'f',
        CTAGS_GLOBALVAR = 'v',
        CTAGS_MEMBER    = 'm',
        CTAGS_NAMESPACE = 'n',
        CTAGS_PREPROC   = 'd',
        CTAGS_STRUCT    = 's',
        CTAGS_TYPE      = 't',
        CTAGS_UNION     = 'u',
};
#endif
#define CTAGS_CLASS     'c'
#define CTAGS_ENUM      'g'
#define CTAGS_ENUMCONST 'e'
#define CTAGS_FUNCTION  'f'
#define CTAGS_GLOBALVAR 'v'
#define CTAGS_MEMBER    'm'
#define CTAGS_NAMESPACE 'n'
#define CTAGS_PREPROC   'd'
#define CTAGS_STRUCT    's'
#define CTAGS_TYPE      't'
#define CTAGS_UNION     'u'
#define EXTENSION_TEMPLATE 'T'

/*======================================================================================*/

#define B_QUICK_DUMP(LST, FNAME)                                                                \
        do {                                                                                    \
                FILE *fp = safe_fopen_fmt("%s/.tag_highlight_log/%s.log", "wb", HOME, (FNAME)); \
                b_list_dump(fp, LST);                                                           \
                fclose(fp);                                                                     \
        } while (0)

nvim_arg_array *
type_id(struct bufdata *bdata, struct translationunit *stu)
{
        enum CXCursorKind last  = 1;
        nvim_arg_array  *calls = new_arg_array();

        if (!CLD(bdata)->info)
                errx(1, "Invalid");

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(,bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        B_QUICK_DUMP(bdata->ft->ignored_tags, "ignored");

        for (unsigned i = 0; i < stu->tokens->qty; ++i) {
                struct token *tok = stu->tokens->lst[i];
                if ((int)tok->line == (-1) || tok_in_skip_list(bdata, tok))
                        continue;
                /* tokvisitor(tok); */
                do_typeswitch(bdata, calls, tok, CLD(bdata)->info, CLD(bdata)->enumerators, &last);
        }

        return calls;
}

/*======================================================================================*/


static void do_typeswitch(struct bufdata           *bdata,
                          struct nvim_arg_array *calls,
                          struct token             *tok,
                          struct cmd_info          *info,
                          const b_list             *enumerators,
                          enum CXCursorKind *const  last_kind)
{
        CXCursor       cursor = tok->cursor;
        const bstring *group;

retry:
        switch (cursor.kind) {
        case CXCursor_TypedefDecl:
                /* An actual typedef */
                ADD_CALL(CTAGS_TYPE);
                break;
        case CXCursor_TypeRef: {
                /* Some object that is a reference or instantiation of a type. */

                /* In C++, classes and structs both appear under TypeRef. To
                 * differentiate them from other types we need to find the
                 * declaration cursor and identify its type. The easiest way to do
                 * this is to change the value of `cursor' and jump back to the top. */
                if (bdata->ft->id == FT_CPP) {
                        /* CXType curs_type = clang_getCursorType(tok->cursor); */
                        CXCursor newcurs = clang_getTypeDeclaration(tok->cursortype);

                        if (clang_isCursorDefinition(cursor)) {
                                ADD_CALL(CTAGS_CLASS);
                                break;
                        }
#if 0
                        if (clang_equalCursors(newcurs, cursor) && newcurs.kind == CXCursor_ClassDecl) {
                                ADD_CALL(CTAGS_CLASS);
                                break;
                        }
#endif
                        if (newcurs.kind != CXCursor_NoDeclFound) {
                                cursor = newcurs;
                                goto retry;
                        }
                }

                ADD_CALL(CTAGS_TYPE);
        } break;
        case CXCursor_MemberRef:
                /* A reference to a member of a struct, union, or class in
                 * non-expression context such as a designated initializer. */
                ADD_CALL(CTAGS_MEMBER);
                break;
        case CXCursor_MemberRefExpr: {
                /* Ordinary reference to a struct/class member. */
                CXType deftype = clang_getCanonicalType(tok->cursortype);
                /* CXCursor def = clang_getCursorDefinition(cursor); */
                /* CXType   deftype = clang_getCursorType(def); */
                /* CXString curskind = clang_getCursorKindSpelling(def.kind); */
                /* echo("Got curs_kind %s... for %s\n", CS(curskind), tok->raw); */
                /* CXString type = clang_getTypeSpelling(deftype);
                CXString typekind = clang_getTypeKindSpelling(deftype.kind);
                echo("And %s - %s - %d for %s\n", CS(type), CS(typekind), deftype.kind, tok->raw); */
                /* free_cxstrings(curskind, type); */
                if (deftype.kind == CXType_Unexposed)
                        ADD_CALL(CTAGS_FUNCTION);
                else
                        ADD_CALL(CTAGS_MEMBER);
        } break;
        /* case CXCursor_Namespace: */
        case CXCursor_NamespaceRef:
                ADD_CALL(CTAGS_NAMESPACE);
                break;
        case CXCursor_StructDecl:
                ADD_CALL(CTAGS_STRUCT);
                break;
        case CXCursor_UnionDecl:
                ADD_CALL(CTAGS_UNION);
                break;
        case CXCursor_ClassDecl:
                /* ADD_CALL(CTAGS_CLASS); */
                ADD_CALL(CTAGS_TYPE);
                break;
        case CXCursor_EnumDecl:
                /* An enumeration. */
                ADD_CALL(CTAGS_ENUM);
                break;
        case CXCursor_EnumConstantDecl:
                /** An enumerator constant. */
                ADD_CALL(CTAGS_ENUMCONST);
                break;
        case CXCursor_FieldDecl:
                /* A field or non-static data member (C++) in a struct, union, or class. */
                ADD_CALL(CTAGS_MEMBER);
                break;
        case CXCursor_FunctionDecl:
        case CXCursor_CXXMethod:
        case CXCursor_CallExpr:
                /* An expression that calls a function. */
                ADD_CALL(CTAGS_FUNCTION);
                break;

        /* --- Mainly C++ Stuff --- */
        case CXCursor_Constructor:
        case CXCursor_Destructor:
                ADD_CALL(CTAGS_CLASS);
                break;
        case CXCursor_TemplateTypeParameter:
                ADD_CALL(CTAGS_TYPE);
        case CXCursor_NonTypeTemplateParameter:
                break;
        case CXCursor_ConversionFunction:
        case CXCursor_FunctionTemplate:
                ADD_CALL(CTAGS_FUNCTION);
                break;
        case CXCursor_ClassTemplate:
                ADD_CALL(CTAGS_CLASS);
                break;
        case CXCursor_TemplateRef:
                ADD_CALL(EXTENSION_TEMPLATE);
                break;
        /* ------------------------ */

        /* Macros are trouble */
        case CXCursor_MacroDefinition:
#if 0
        P99_AVOID {
                /* CXCursor tmp = clang_getCursorReferenced(cursor); */
                /* CXString str = clang_getTypeSpelling(clang_getCursorType(tmp)); */

                CXType   curs_type = clang_getCursorType(cursor);
                CXType   can_type  = clang_getCanonicalType(curs_type);
                CXString str       = clang_getTypeSpelling(can_type);
                echo("Got cursor of kind \"%s\" in the macro", CS(str));
                clang_disposeString(str);
        }
#endif
                if (*last_kind == CXCursor_PreprocessingDirective)
                        ADD_CALL(CTAGS_PREPROC);
                break;

        case CXCursor_MacroExpansion:
                ADD_CALL(CTAGS_PREPROC);
                break;

        case CXCursor_DeclRefExpr:
                /* Possibly the most generic kind, this could refer to many things. */
                switch (tok->cursortype.kind) {
                case CXType_Enum:
                        ADD_CALL(CTAGS_ENUMCONST);
                        break;
                case CXType_FunctionProto:
                        ADD_CALL(CTAGS_FUNCTION);
                        break;
                case CXType_Int: {
                        bstring *tmp = &tok->text;
                        unsigned i;
                        /* B_LIST_FOREACH(enumerators, cur, i)
                                if (b_iseq(cur, tmp)) {
                                        ADD_CALL(CTAGS_ENUMCONST);
                                        break;
                                } */

                        if (B_LIST_BSEARCH_FAST(enumerators, tmp))
                                ADD_CALL(CTAGS_ENUMCONST);
                } break;
                default:
                        break;
                }
                break;
        default:
                break;
        }

        *last_kind = cursor.kind;
}


/*======================================================================================*/

static bool
tok_in_skip_list(struct bufdata *bdata, struct token *tok)
{
        bstring *tmp = btp_fromcstr(tok->raw);
        return B_LIST_BSEARCH_FAST(bdata->ft->ignored_tags, tmp);
}

static void
report_parent(CXCursor cursor, FILE *fp)
{
        CXCursor parent         = clang_getCursorSemanticParent(cursor);
        CXType   parent_type    = clang_getCursorType(parent);
        CXString typespell      = clang_getTypeSpelling(parent_type);
        CXString typekindrepr   = clang_getTypeKindSpelling(parent_type.kind);
        CXString curs_kindspell = clang_getCursorKindSpelling(parent.kind);

        fprintf(fp, "  parent: %-17s - %-30s - %s\n",
                CS(typekindrepr), CS(curs_kindspell), CS(typespell));

        free_cxstrings(typespell, typekindrepr, curs_kindspell);
}

static void
tokvisitor(struct token *tok)
{
        extern char     libclang_tmp_path[SAFE_PATH_MAX + 1];
        static unsigned n = 0;

        CXString typespell      = clang_getTypeSpelling(tok->cursortype);
        CXString typekindrepr   = clang_getTypeKindSpelling(tok->cursortype.kind);
        CXString curs_kindspell = clang_getCursorKindSpelling(tok->cursor.kind);
        FILE    *toklog         = safe_fopen_fmt("%s/toks.log", "ab", libclang_tmp_path);

        fprintf(toklog, "%4u: %4u => %-50s - %-17s - %-30s - %s\n",
                n++, tok->line, tok->raw,
                CS(typekindrepr), CS(curs_kindspell), CS(typespell));

        report_parent(tok->cursor, toklog);
        fclose(toklog);
        free_cxstrings(typespell, typekindrepr, curs_kindspell);
}

/*======================================================================================*/

#define INIT_ACALL_SIZE (128)
extern FILE *cmd_log;

static struct nvim_arg_array *
new_arg_array(void)
{
        struct nvim_arg_array *calls = xmalloc(sizeof(struct nvim_arg_array));
        calls->mlen = INIT_ACALL_SIZE;
        calls->fmt  = nmalloc(calls->mlen, sizeof(char *));
        calls->args = nmalloc(calls->mlen, sizeof(nvim_argument *));
        calls->qty  = 0;
        return calls;
}

static void
add_hl_call(struct nvim_arg_array *calls,
            const int                 bufnum,
            const int                 hl_id,
            const bstring            *group,
            const struct token       *tok)
{
        assert(calls);
        if (calls->qty >= calls->mlen-1) {
                calls->mlen *= 2;
                calls->fmt  = nrealloc(calls->fmt, calls->mlen, sizeof(char *));
                calls->args = nrealloc(calls->args, calls->mlen, sizeof(nvim_argument *));
        }

        calls->fmt[calls->qty]         = STRDUP("s[dd,s,ddd]");
        calls->args[calls->qty]        = nmalloc(7, sizeof(nvim_argument));
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
add_clr_call(struct nvim_arg_array *calls,
             const int bufnum,
             const int hl_id,
             const int line,
             const int end)
{
        assert(calls);
        if (calls->qty >= calls->mlen-1) {
                calls->mlen *= 2;
                calls->fmt  = nrealloc(calls->fmt, calls->mlen, sizeof(char *));
                calls->args = nrealloc(calls->args, calls->mlen, sizeof(nvim_argument *));
        }

        calls->fmt[calls->qty]         = STRDUP("s[dddd]");
        calls->args[calls->qty]        = nmalloc(5, sizeof(nvim_argument));
        calls->args[calls->qty][0].str = b_lit2bstr("nvim_buf_clear_highlight");
        calls->args[calls->qty][1].num = bufnum;
        calls->args[calls->qty][2].num = hl_id;
        calls->args[calls->qty][3].num = line;
        calls->args[calls->qty][4].num = end;

        if (cmd_log)
                fprintf(cmd_log, "nvim_buf_clear_highlight(%d, %d, %d, %d)\n",
                        bufnum, hl_id, line, end);
        ++calls->qty;
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
