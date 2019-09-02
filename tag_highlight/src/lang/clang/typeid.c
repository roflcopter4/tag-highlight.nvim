#include "Common.h"
#include "highlight.h"

#include "lang/clang/clang.h"
#include "lang/clang/intern.h"
#include "lang/lang.h"
#include "util/list.h"

static void do_typeswitch(Buffer                  *bdata,
                          struct mpack_arg_array  *calls,
                          struct token            *tok,
                          struct cmd_info         *info,
                          enum CXCursorKind *const last_kind);

static bool tok_in_skip_list(Buffer *bdata, struct token *tok);
/* static void report_cursor(FILE *fp, CXCursor cursor); */
/* static void tokvisitor(struct token *tok); */

#define TLOC(TOK) ((TOK)->line), ((TOK)->col), ((TOK)->col + (TOK)->line)

#define ADD_CALL(CH)                                                                   \
        do {                                                                           \
                if ((group = find_group(bdata->ft, info, info->num, (CH))))            \
                        add_hl_call(calls, bdata->num, bdata->hl_id, group,            \
                                    (line_data[]){{tok->line, tok->col1, tok->col2}}); \
        } while (0)

/*======================================================================================*/

#define B_QUICK_DUMP(LST, FNAME)                                                                \
        do {                                                                                    \
                FILE *fp = safe_fopen_fmt("%s/%s.log", "wb", BS(settings.cache_dir), (FNAME)); \
                b_list_dump(fp, LST);                                                           \
                fclose(fp);                                                                     \
        } while (0)

mpack_arg_array *
create_nvim_calls(Buffer *bdata, struct translationunit *stu)
{
        enum CXCursorKind last  = 1;
        mpack_arg_array  *calls = new_arg_array();

        if (!CLD(bdata)->info)
                errx(1, "Invalid");

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        /* B_QUICK_DUMP(bdata->ft->ignored_tags, "ignored"); */

        /* lc_index_file(bdata, stu, calls); */

        for (unsigned i = 0; i < stu->tokens->qty; ++i) {
                struct token *tok = stu->tokens->lst[i];
                if ((int64_t)tok->line == INT64_C(-1))
                        continue;
                if (tok_in_skip_list(bdata, tok)) {
                        /* SHOUT("Skipping verboten token \"%s\"\n", BTS(tok->text)); */
                        continue;
                }

#if 0
                if (b_strstr(&tok->text, B("main"), 0) >= 0) {
                        SHOUT("Allowed verboten token \"%s\"\n", BTS(tok->text));
                }
#endif

                /* if ((int)tok->line == (-1) || tok_in_skip_list(bdata, tok)) */
                        /* continue; */
                /* tokvisitor(tok); */
                do_typeswitch(bdata, calls, tok, CLD(bdata)->info, &last);
        }

        lc_index_file(bdata, stu, calls);

        return calls;
}

/*======================================================================================*/


static void do_typeswitch(Buffer                   *bdata,
                          struct mpack_arg_array   *calls,
                          struct token             *tok,
                          struct cmd_info          *info,
                          enum CXCursorKind *const  last_kind)
{
        const bstring *group;
        CXCursor       cursor = tok->cursor;
        int goto_safety_count = 0;
        /* static atomic_flag flg    = ATOMIC_FLAG_INIT; */

retry:
        if (goto_safety_count < 2)
                ++goto_safety_count;
        else
                return;
        switch (cursor.kind) {
        case CXCursor_TypedefDecl:
                /* An actual typedef */
                ADD_CALL(CTAGS_TYPE);
                break;
        case CXCursor_TypeAliasDecl:
                ADD_CALL(CTAGS_TYPE);
                break;
        case CXCursor_TypeRef: {
                /* Some object that is a reference or instantiation of a type. */

                /* In C++, classes and structs both appear under TypeRef. To
                 * differentiate them from other types we need to find the
                 * declaration cursor and identify its type. The easiest way to do
                 * this is to change the value of `cursor' and jump back to the top. */
                if (bdata->ft->id != FT_C) {
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
                break;
        }
        case CXCursor_MemberRef:
                /* A reference to a member of a struct, union, or class in
                 * non-expression context such as a designated initializer. */
                ADD_CALL(CTAGS_MEMBER);
                break;
        case CXCursor_MemberRefExpr: {
                /* Ordinary reference to a struct/class member. */
                CXType deftype = clang_getCanonicalType(tok->cursortype);
                if (bdata->ft->id != FT_C && deftype.kind == CXType_Unexposed)
                        ADD_CALL(CTAGS_FUNCTION);
                else
                        ADD_CALL(CTAGS_MEMBER);
                break;
        }
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
                ADD_CALL(CTAGS_CLASS);
                /* ADD_CALL(CTAGS_TYPE); */
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
        /* P99_AVOID */ {
                /* CXCursor tmp = clang_getCursorReferenced(cursor); */
                /* CXString str = clang_getTypeSpelling(clang_getCursorType(tmp)); */

                /* CXType   curs_type = clang_getCursorType(cursor); */
                /* CXType   can_type  = clang_getCanonicalType(curs_type);  */
                /* CXString str       = clang_getTypeSpelling(can_type);    */
                /* echo("Got cursor of kind \"%s\" in the macro", CS(str)); */
                /* clang_disposeString(str);                                */
                /* CXCursor new       = clang_getCursorLexicalParent(cursor); */
                CXType   curs_type = clang_getCursorType(cursor);
                CXString str1      = clang_getTypeSpelling(curs_type);
                CXString str2      = clang_getCursorKindSpelling(cursor.kind);
                CXString str3      = clang_getTypeKindSpelling(curs_type.kind);
                echo("got \"%s\" - kind: \"%s\" - typekind: \"%s\" -- \"%s\"\n", CS(str1), CS(str2), CS(str3), tok->raw);
                free_cxstrings(str1, str2, str3);

                /* if (!clang_Cursor_isNull(new)) {      */
                /*         echo("Not null, retying.\n"); */
                /*         cursor = new;                 */
                /*         goto retry;                   */
                /* }                                     */
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
#if 0
                case CXType_Int: {
                        bstring *tmp = &tok->text;
                        if (B_LIST_BSEARCH_FAST(enumerators, tmp))
                                ADD_CALL(CTAGS_ENUMCONST);
                        break;
                }
#endif
                default:
                        break;
                }

                break;

        case CXCursor_ParmDecl:
                break;

        default:
                break;
        }

        *last_kind = cursor.kind;
}

/*======================================================================================*/

static bool
tok_in_skip_list(Buffer *bdata, struct token *tok)
{
        bstring *tmp = &tok->text;
        return B_LIST_BSEARCH_FAST(bdata->ft->ignored_tags, tmp) != NULL;
}

#if 0
static void
report_parent(FILE *fp, CXCursor cursor)
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
report_cursor(FILE *fp, CXCursor cursor)
{
        CXType   cursor_type    = clang_getCursorType(cursor);
        CXString typespell      = clang_getTypeSpelling(cursor_type);
        CXString typekindrepr   = clang_getTypeKindSpelling(cursor_type.kind);
        CXString curs_kindspell = clang_getCursorKindSpelling(cursor.kind);

        fprintf(fp, "  cursor: %-17s - %-30s - %s\n",
                CS(typekindrepr), CS(curs_kindspell), CS(typespell));

        free_cxstrings(typespell, typekindrepr, curs_kindspell);
}
#endif

#if 0
static void
tokvisitor(struct token *tok)
{
        extern char     libclang_tmp_path[SAFE_PATH_MAX + 1];
        static unsigned n = 0;

        CXString typespell      = clang_getTypeSpelling(tok->cursortype);
        CXString typekindrepr   = clang_getTypeKindSpelling(tok->cursortype.kind);
        CXString curs_kindspell = clang_getCursorKindSpelling(tok->cursor.kind);
        FILE    *toklog         = safe_fopen_fmt("%s/toks.log", "ab", libclang_tmp_path);

        fprintf(toklog, "\033[31;1m%4u: %4u\033[0m => \033[1m%-46s\033[0m - %-17s - %-30s - %s\n",
                n++, tok->line, tok->raw,
                CS(typekindrepr), CS(curs_kindspell), CS(typespell));

        report_parent(toklog, tok->cursor);
        fclose(toklog);
        free_cxstrings(typespell, typekindrepr, curs_kindspell);
}
#endif
