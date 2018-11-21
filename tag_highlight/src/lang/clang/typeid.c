#include "clang.h"
#include "intern.h"
#include "util/list.h"

static void do_typeswitch(struct bufdata           *bdata,
                          struct nvim_arg_array    *calls,
                          struct token             *tok,
                          struct cmd_info          *info,
                          const b_list             *enumerators,
                          enum CXCursorKind *const  last_kind);

static bool tok_in_skip_list(struct bufdata *bdata, struct token *tok);
static void tokvisitor(struct token *tok);

#define TLOC(TOK) ((TOK)->line), ((TOK)->col), ((TOK)->col + (TOK)->line)
#define ADD_CALL(CH)                                                                \
        do {                                                                        \
                if ((group = find_group(bdata->ft, info, info->num, (CH))))         \
                        add_hl_call(calls, bdata->num, bdata->hl_id, group,         \
                                    &(line_data){tok->line, tok->col1, tok->col2}); \
        } while (0)

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
                          struct nvim_arg_array    *calls,
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
