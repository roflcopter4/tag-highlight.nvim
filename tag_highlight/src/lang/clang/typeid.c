#include "Common.h"
#include "highlight.h"

#include "lang/clang/clang.h"
#include "lang/clang/intern.h"
#include "lang/lang.h"
#include "util/list.h"

static void do_typeswitch(Buffer            *bdata,
                          mpack_arg_array   *calls,
                          token_t           *tok,
                          enum CXCursorKind *last_kind);

static bool tok_in_skip_list(Buffer *bdata, token_t *tok) __attribute__((pure));
/* static void report_cursor(FILE *fp, CXCursor cursor, int ch); */

#define TLOC(TOK) ((TOK)->line), ((TOK)->col), ((TOK)->col + (TOK)->line)

#define ADD_CALL(CH)                                                                   \
        do {                                                                           \
                const bstring *group = find_group(bdata->ft, (CH));                    \
                if (group) {                                                           \
                        /*report_cursor(fp, cursor, (CH)); */                          \
                        add_hl_call(calls, bdata->num, bdata->hl_id, group,            \
                                    (line_data[]){{tok->line, tok->col1, tok->col2}}); \
                }                                                                      \
        } while (0)

/*======================================================================================*/

mpack_arg_array *
create_nvim_calls(Buffer *bdata, translationunit_t *stu)
{
        enum CXCursorKind last  = 1;
        mpack_arg_array  *calls = new_arg_array();

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        for (unsigned i = 0; i < stu->tokens->qty; ++i) {
                token_t *tok = stu->tokens->lst[i];
                if (((int)tok->line) == -1)
                        continue;
                if (tok_in_skip_list(bdata, tok))
                        continue;

                do_typeswitch(bdata, calls, tok, &last);
        }

        lc_index_file(bdata, stu, calls);

        return calls;
}

/*======================================================================================*/

static bool
sanity_check_name(token_t *tok, CXCursor cursor)
{
        CXCursor newcurs = clang_getTypeDeclaration(tok->cursortype);
        CXString name1   = clang_getCursorSpelling(cursor);
        CXString name2   = clang_getCursorDisplayName(newcurs);
        bool     tmp     = STREQ(tok->raw, CS(name1)) || STREQ(tok->raw, CS(name2));
        free_cxstrings(name1, name2);
        return tmp;
}

static void
do_typeswitch(Buffer                   *bdata,
              mpack_arg_array          *calls,
              token_t                  *tok,
              enum CXCursorKind *const  last_kind)
{
        CXCursor cursor       = tok->cursor;
        int goto_safety_count = 0;

#if 0
        if (bdata->ft->id == FT_CXX && !sanity_check_name(tok, cursor))
                goto skip;
#endif

retry:
        if (goto_safety_count < 2)
                ++goto_safety_count;
        else
                goto skip;

        /*
         * XXX
         * Frequently things like C++ attributes get colored the same as whatever is
         * next to them. I don't know why clang does this, and the only way around it
         * is to examine the actual token itself. It's probably not worth the bother.
         * Attributes can just be added to the list of ignored tags.
         */

        switch (cursor.kind) {
        /* An actual typedef */
        case CXCursor_TypedefDecl:
                ADD_CALL(CTAGS_TYPE);
                break;

        case CXCursor_TypeAliasDecl:
                ADD_CALL(CTAGS_TYPE);
                break;

        /* Some object that is a reference or instantiation of a type. */
        case CXCursor_TypeRef: {
                /*
                 * In C++, classes and structs both appear under TypeRef. To
                 * differentiate them from other types we need to find the
                 * declaration cursor and identify its type. The easiest way to do
                 * this is to change the value of `cursor' and jump back to the top.
                 */
                if (bdata->ft->id == FT_CXX) {
                        CXCursor newcurs = clang_getTypeDeclaration(tok->cursortype);
                        if (newcurs.kind != CXCursor_NoDeclFound) {
                                cursor = newcurs;
                                goto retry;
                        }
                }

                ADD_CALL(CTAGS_TYPE);
                break;
        }

        /* A reference to a member of a struct, union, or class in
         * non-expression context such as a designated initializer. */
        case CXCursor_MemberRef:
                ADD_CALL(CTAGS_MEMBER);
                break;

        /* Ordinary reference to a struct/class member. */
        case CXCursor_MemberRefExpr: {
                CXType deftype = clang_getCanonicalType(tok->cursortype);
                if (bdata->ft->id != FT_C && deftype.kind == CXType_Unexposed)
                        ADD_CALL(EXTENSION_METHOD);
                else
                        ADD_CALL(CTAGS_MEMBER);
                break;
        }

        case CXCursor_Namespace:
                if (goto_safety_count == 1 && !sanity_check_name(tok, cursor))
                        break;
        case CXCursor_NamespaceRef:
                ADD_CALL(CTAGS_NAMESPACE);
                break;

        case CXCursor_StructDecl:
                ADD_CALL(CTAGS_STRUCT);
                break;

        case CXCursor_UnionDecl:
                ADD_CALL(CTAGS_UNION);
                break;

        case CXCursor_ClassDecl: {
                if (goto_safety_count == 1 && !sanity_check_name(tok, cursor))
                        break;
                ADD_CALL(CTAGS_CLASS);
                break;
        }

        /* An enumeration. */
        case CXCursor_EnumDecl:
                ADD_CALL(CTAGS_ENUM);
                break;

        /** An enumerator constant. */
        case CXCursor_EnumConstantDecl:
                ADD_CALL(CTAGS_ENUMCONST);
                break;

        /* A field or non-static data member (C++) in a struct, union, or class. */
        case CXCursor_FieldDecl:
                ADD_CALL(CTAGS_MEMBER);
                break;

        case CXCursor_CXXMethod:
                ADD_CALL(EXTENSION_METHOD);
                break;

        /* An expression that calls a function. */
        case CXCursor_FunctionDecl:
        case CXCursor_CallExpr:
                ADD_CALL(CTAGS_FUNCTION);
                break;

        /* --- Mainly C++ Stuff --- */
        case CXCursor_Constructor:
        case CXCursor_Destructor:
                ADD_CALL(CTAGS_CLASS);
                break;

        case CXCursor_TemplateTypeParameter:
                ADD_CALL(CTAGS_TYPE);
                break;

        case CXCursor_NonTypeTemplateParameter:
                NOP;
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

        /* Macros are trouble */
        case CXCursor_MacroDefinition:
                if (*last_kind == CXCursor_PreprocessingDirective)
                        ADD_CALL(CTAGS_PREPROC);
                break;

        case CXCursor_MacroExpansion:
                ADD_CALL(CTAGS_PREPROC);
                break;

        /* Possibly the most generic kind, this could refer to many things. */
        case CXCursor_DeclRefExpr:
                switch (tok->cursortype.kind) {
                case CXType_Enum:
                        ADD_CALL(CTAGS_ENUMCONST);
                        break;
                case CXType_FunctionProto:
                        ADD_CALL(CTAGS_FUNCTION);
                        break;
                default:
                        break;
                }

                break;

        case CXCursor_ParmDecl:
        default:
                break;
        }

skip:
        *last_kind = cursor.kind;
}

/*======================================================================================*/

static bool
tok_in_skip_list(Buffer *bdata, token_t *tok)
{
        bstring *tmp = &tok->text;
        return B_LIST_BSEARCH_FAST(bdata->ft->ignored_tags, tmp) != NULL;
}

#if 0

                assert (cursur != tok->cursor);
#if 0
                CXString name = clang_getCursorDisplayName(cursor);
                bstring  tmp  = bt_fromblk(stu->buf->data + tok->offset, tok->len);
                bool     ret  = b_iseq_cstr(&tmp, CS(name));
                clang_disposeString(name);
#endif
                resolved_range_t rng;
                memset(&rng, 0, sizeof rng);
                if (resolve_range(clang_getCursorExtent(cursor), &rng))
                {
                        CXString    name = clang_getCursorDisplayName(cursor);
                        char const *tmp  = (char *)stu->buf->data + rng.offset1;
                        int const   ret  = strncmp(tmp, CS(name), rng.len);
                        clang_disposeString(name);

#define QM "\e[1;36m\"\e[0m"

                        fprintf(sfp, QM"%.*s"QM" \e[1;31mvs\e[0m "QM"%s"QM"\n\n", rng.len, tmp, CS(name));

                        if (ret != 0)
                                return;
                }
        }

static void
report_parent(FILE *fp, CXCursor cursor)
{
        CXCursor parent         = clang_getCursorLexicalParent(cursor);
        CXType   parent_type    = clang_getCursorType(parent);
        CXString typespell      = clang_getTypeSpelling(parent_type);
        CXString typekindrepr   = clang_getTypeKindSpelling(parent_type.kind);
        CXString curs_kindspell = clang_getCursorKindSpelling(parent.kind);

        fprintf(fp, "     parent: %-17s - %-30s - %s\n",
                CS(typekindrepr), CS(curs_kindspell), CS(typespell));

        free_cxstrings(typespell, typekindrepr, curs_kindspell);
}

static void
do_report_cursor(FILE *fp, CXCursor cursor, int const ch)
{
        CXType   cursor_type    = clang_getCursorType(cursor);
        CXString typespell      = clang_getTypeSpelling(cursor_type);
        CXString typekindspell  = clang_getTypeKindSpelling(cursor_type.kind);
        CXString kindspell      = clang_getCursorKindSpelling(cursor.kind);

        fprintf(fp, "(%c)  cursor: %-17s - %-30s - %s\n", ch ?: '0',
                CS(typekindspell), CS(kindspell), CS(typespell));

        free_cxstrings(typespell, typekindspell, kindspell);
}

static void
report_cursor(FILE *fp, CXCursor cursor, int const ch)
{
        do_report_cursor(fp, cursor, ch);
        /* fputc('\n', fp); */
}
#endif
