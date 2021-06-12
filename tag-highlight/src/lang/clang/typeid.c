#include "Common.h"
#include "highlight.h"

#include "lang/clang/clang.h"
#include "lang/clang/intern.h"
#include "lang/lang.h"
#include "util/list.h"
#include "clang-c/Index.h"

static void do_typeswitch(Buffer *bdata, mpack_arg_array *calls,
                          token_t *tok, enum CXCursorKind *last_kind);

static bool tok_in_skip_list(Buffer *bdata, token_t *tok) __attribute__((pure));
static bool sanity_check_name(token_t *tok, CXCursor cursor);

/*======================================================================================*/

//static thread_local FILE *dump_fp = NULL;

mpack_arg_array *
create_nvim_calls(Buffer *bdata, translationunit_t *stu)
{
        enum CXCursorKind last  = 1;
        mpack_arg_array  *calls = new_arg_array();

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        //dump_fp = safe_fopen_fmt("%s/garbage.log", "wb", BS(settings.cache_dir));

        for (unsigned i = 0; i < stu->tokens->qty; ++i) {
                token_t *tok = stu->tokens->lst[i];
                if (((int)tok->line) == -1)
                        continue;
                if (tok_in_skip_list(bdata, tok))
                        continue;

                do_typeswitch(bdata, calls, tok, &last);
        }

        //fclose(dump_fp);

        lc_index_file(bdata, stu, calls);

        return calls;
}

/*======================================================================================*/

#define ADD_CALL(CH)                                                                   \
        do {                                                                           \
                const bstring *group = find_group(bdata->ft, (CH));                    \
                if (group)                                                             \
                        add_hl_call(calls, bdata->num, bdata->hl_id, group,            \
                                    (line_data[]){{tok->line, tok->col1, tok->col2}}); \
        } while (0)

static void
do_typeswitch(Buffer *bdata, mpack_arg_array *calls,
              token_t *tok, enum CXCursorKind *last_kind)
{
        CXCursor cursor       = tok->cursor;
        int goto_safety_count = 0;

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

        case CXCursor_ClassDecl:
                if (goto_safety_count == 1 && !sanity_check_name(tok, cursor))
                        break;
                ADD_CALL(CTAGS_CLASS);
                break;

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
                ADD_CALL(EXTENSION_METHOD);
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
                        if (tok->tokenkind == CXToken_Punctuation)
                                ADD_CALL(EXTENSION_OVERLOADEDOP);
                        else
                                ADD_CALL(CTAGS_FUNCTION);
                        break;
                default:;
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

#if 0
static void braindead(token_t *tok)
{
        CXString spell        = clang_getCursorSpelling(tok->cursor);
        CXType   what         = clang_getCursorType(tok->cursor);
        CXString typespell    = clang_getCursorKindSpelling(tok->cursor.kind);
        CXString whatspell    = clang_getTypeSpelling(what);
        CXString tkspell      = clang_getTypeKindSpelling(what.kind);
        CXCursor ref          = clang_getCursorReferenced(tok->cursor);
        CXString refspell     = clang_getCursorSpelling(ref);
        CXString reftypespell = clang_getCursorKindSpelling(ref.kind);

        fprintf(dump_fp, "Have (%s): (`%s`  TYPE  `%s`) : TK = `%s` **** references `%s`  -->  `%s   -->  `%s`\n",
                tok->raw, CS(spell), CS(typespell), CS(tkspell), CS(refspell), CS(reftypespell), CS(whatspell));

        free_cxstrings(whatspell, spell, typespell, tkspell, refspell, reftypespell);
}
#endif
