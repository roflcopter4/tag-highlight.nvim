#include "Common.h"
#include "highlight.h"

#include "lang/clang/clang.h"
#include "lang/clang/intern.h"
#include "lang/lang.h"
#include "util/list.h"
#include "clang-c/Index.h"


static void do_typeswitch(Buffer            *bdata,
                          mpack_arg_array   *calls,
                          token_t           *tok,
                          enum CXCursorKind *last_kind);

static void translationunit_visitor(Buffer *bdata, translationunit_t *stu);

static bool tok_in_skip_list(Buffer *bdata, token_t *tok) __attribute__((pure));
static bool sanity_check_name(token_t *tok, CXCursor cursor);
static bool is_really_template_parameter(token_t *tok, CXCursor cursor);

UNUSED static void braindead(token_t *tok, int ngotos, CXCursor *provided);
UNUSED static void slightly_less_braindead(CXFile file, CXCursor cursor);

/*======================================================================================*/

static thread_local FILE *dump_fp = NULL;

mpack_arg_array *
create_nvim_calls(Buffer *bdata, translationunit_t *stu)
{
      enum CXCursorKind last  = 1;
      mpack_arg_array  *calls = new_arg_array();

      if (bdata->hl_id == 0)
            bdata->hl_id = nvim_buf_add_highlight(bdata->num);
      else
            add_clr_call(calls, (int)bdata->num, bdata->hl_id, 0, -1);

      dump_fp = safe_fopen_fmt("wbe", "%s/garbage.log", BS(settings.cache_dir));

      //translationunit_visitor(bdata, stu);

      for (unsigned i = 0; i < stu->tokens->qty; ++i) {
            token_t *tok = stu->tokens->lst[i];
            if (((int)tok->line) == -1) {
                  fprintf(dump_fp, "Token \"%s\" isn't even in the damned file?!\n", tok->raw);
                  continue;
            }
            if (tok_in_skip_list(bdata, tok)) {
                  fprintf(dump_fp, "Token \"%s\" is to be skipped.\n", tok->raw);
                  continue;
            }

            do_typeswitch(bdata, calls, tok, &last);
      }

      (void)fclose(dump_fp);

      lc_index_file(bdata, stu, calls);

      return calls;
}

/*======================================================================================*/

#if 0
#define ADD_CALL(CH)                                                             \
      do {                                                                       \
            const bstring *group = find_group(bdata->ft, (CH));                  \
            if (group)                                                           \
                  add_hl_call(calls, bdata->num, bdata->hl_id, group,            \
                              (line_data[]){{tok->line, tok->col1, tok->col2}}); \
      } while (0)
#endif

#define ADD_CALL(CH) (call_group = (CH))

static void
do_typeswitch(Buffer            *bdata,
              mpack_arg_array   *calls,
              token_t           *tok,
              enum CXCursorKind *last_kind)
{
      CXCursor          cursor = tok->cursor;

      int  goto_safety_count = 0;
      int  call_group        = 0;
      bool in_template       = false;

      //braindead(tok, 0);

retry:
      if (goto_safety_count < 3)
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
      case CXCursor_TypeAliasDecl:
            ADD_CALL(CTAGS_TYPE);
            break;

      case CXCursor_CXXBaseSpecifier:
            ADD_CALL(CTAGS_TYPE);
            warnd("Got a CXXBaseSpecifier!");
            //braindead(tok, goto_safety_count, &cursor);
            break;

      /* Some object that is a reference or instantiation of a type. */
      case CXCursor_TypeRef:
      {
            /*
             * In C++, classes and structs both appear under TypeRef. To
             * differentiate them from other types we need to find the
             * declaration cursor and identify its type. The easiest way to do
             * this is to change the value of `cursor' and jump back to the top.
             */
            if (bdata->ft->id == FT_CXX) {
                  /* CXCursor newcurs = clang_getTypeDeclaration(tok->cursortype); */
                  /* CXType type      = clang_getCursorType(cursor);    */
                  /* CXCursor newcurs = clang_getTypeDeclaration(type); */
                  /* CXCursor newcurs = clang_getCursorDefinition(cursor); */
                  CXCursor newcurs = clang_getCursorReferenced(cursor);

                  switch (newcurs.kind) {
                  case CXCursor_NoDeclFound:
                        break;
                  case CXCursor_TemplateTypeParameter:
                        ADD_CALL(EXTENSION_TEMPLATE_TYPE_PARAM);
                        break;
                  case CXCursor_NonTypeTemplateParameter:
                        ADD_CALL(EXTENSION_TEMPLATE_NONTYPE_PARAM);
                        in_template = true;
                        __attribute__((fallthrough));
                  default:
                        cursor = newcurs;
                        goto retry;
                  }
            } else {
                  ADD_CALL(CTAGS_TYPE);
            }

            break;
      }

      /* A reference to a member of a struct, union, or class in
       * non-expression context such as a designated initializer. */
      case CXCursor_MemberRef:
            ADD_CALL(CTAGS_MEMBER);
            break;

      /* Ordinary reference to a struct/class member. */
      case CXCursor_MemberRefExpr: {
            CXType deftype = clang_getCanonicalType(clang_getCursorType(cursor));
            if (bdata->ft->id == FT_CXX && deftype.kind == CXType_Unexposed) {
                  cursor = clang_getCursorReferenced(cursor);
                  goto retry;
            } else {
                  ADD_CALL(CTAGS_MEMBER);
            }
            break;
      }

      case CXCursor_Namespace:
            if (goto_safety_count > 0 && !sanity_check_name(tok, cursor))
                  break;
            __attribute__((fallthrough));
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
            if (goto_safety_count > 0 && !sanity_check_name(tok, cursor))
                  break;
            ADD_CALL(CTAGS_CLASS);
            break;

      /* An enumeration. */
      case CXCursor_EnumDecl:
            ADD_CALL(CTAGS_ENUM);
            break;

      /** An enumerator constant. */
      case CXCursor_EnumConstantDecl:
      case CXCursor_FlagEnum:
            ADD_CALL(CTAGS_ENUMCONST);
            break;

      /* A field or non-static data member (C++) in a struct, union, or class. */
      case CXCursor_FieldDecl:
            if (goto_safety_count > 0 && is_really_template_parameter(tok, cursor)) {
                  ADD_CALL(EXTENSION_TEMPLATE_TYPE_PARAM);
                  break;
            }
            ADD_CALL(CTAGS_MEMBER);
            break;

      case CXCursor_CXXMethod:
            if (goto_safety_count > 0 && is_really_template_parameter(tok, cursor)) {
                  ADD_CALL(EXTENSION_TEMPLATE_TYPE_PARAM);
                  break;
            }
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
            ADD_CALL(EXTENSION_TEMPLATE_TYPE_PARAM);
            break;

      case CXCursor_ClassTemplatePartialSpecialization:
            ADD_CALL(EXTENSION_I_DONT_KNOW);
            break;

      case CXCursor_NonTypeTemplateParameter:
      {
            CXCursor ref  = clang_getCursorReferenced(cursor);
            CXType   what = clang_getCursorType(cursor);
            if (what.kind == CXType_Int && clang_equalCursors(cursor, ref)) {
                  CXString spell = clang_getCursorSpelling(ref);
                  if (STREQ(tok->raw, CS(spell)))
                        ADD_CALL(EXTENSION_TEMPLATE_TYPE_PARAM);
                  else
                        ADD_CALL(EXTENSION_TYPE_KEYWORD);
                  clang_disposeString(spell);
            } else {
                  ADD_CALL(EXTENSION_TEMPLATE_NONTYPE_PARAM);
            }
            break;
      }

      case CXCursor_ConversionFunction:
      case CXCursor_FunctionTemplate:
            ADD_CALL(CTAGS_FUNCTION);
            break;

      case CXCursor_ClassTemplate:
            if (goto_safety_count > 0 && !sanity_check_name(tok, cursor))
                  break;
            ADD_CALL(EXTENSION_TEMPLATE);
            break;

      case CXCursor_TemplateRef:
            if (!sanity_check_name(tok, cursor))
                  break;
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

      case CXCursor_OverloadedDeclRef:
            ADD_CALL(EXTENSION_OVERLOADED_DECL);
            break;

      //case CXCursor_DeclStmt:

      /* Possibly the most generic kind, this could refer to many things. */
      case CXCursor_DeclRefExpr:
            switch (clang_getCursorType(cursor).kind) {
            case CXType_Typedef:
                  if (bdata->ft->id == FT_CXX) {
                        CXCursor ref = clang_getCursorReferenced(cursor);
                        switch (ref.kind) {
                        case CXCursor_NonTypeTemplateParameter:
                              ADD_CALL(EXTENSION_TEMPLATE_TYPE_PARAM);
                              break;
                        case CXCursor_TypeRef:
                              ADD_CALL(CTAGS_TYPE);
                              break;
                        default:
                              break;
                        }
                  }

                  break;

            case CXType_Enum:
                  ADD_CALL(CTAGS_ENUMCONST);
                  break;

            case CXType_FunctionProto:
                  if (tok->tokenkind == CXToken_Punctuation)
                        ADD_CALL(EXTENSION_OVERLOADEDOP);
                  else
                        ADD_CALL(CTAGS_FUNCTION);
                  break;

            case CXType_UShort ... CXType_ULongLong:
            case CXType_Short ... CXType_LongLong:
            {
                  CXCursor ref = clang_getCursorReferenced(cursor);
                  switch (ref.kind) {
                  case CXCursor_EnumConstantDecl:
                        ADD_CALL(CTAGS_ENUMCONST);
                        break;
                  case CXCursor_NonTypeTemplateParameter:
                        ADD_CALL(EXTENSION_TEMPLATE_NONTYPE_PARAM);
                        break;
                  default:;
                  }
                  break;
            }

            default:
                  break;
            }

            if (in_template) {
                  ADD_CALL(EXTENSION_TEMPLATE_TYPE_PARAM);
                  break;
            }

            break;

      case CXCursor_ParmDecl: {
            if (goto_safety_count > 0 && is_really_template_parameter(tok, cursor)) {
                  ADD_CALL(EXTENSION_TEMPLATE_TYPE_PARAM);
                  break;
            }
      }     break;

      case CXCursor_CompoundStmt: {
            CXType type = clang_getCursorType(cursor);
            cursor = clang_getTypeDeclaration(type);
            goto retry;
      }

      case CXCursor_VariableRef: {
            CXCursor tmp = clang_getCursorReferenced(cursor);
            if (tmp.kind == CXCursor_FieldDecl)
                  ADD_CALL(CTAGS_MEMBER);
      }     break;

      case CXCursor_TemplateTemplateParameter:
            warnd("Got a TemplateTemplateParameter!");
            //braindead(tok, goto_safety_count, &cursor);
            break;

      default:
            //if (goto_safety_count > 0)
            //      braindead(tok, goto_safety_count, &cursor);
            break;
      }

      if (call_group) {
            const bstring *group = find_group(bdata->ft, call_group);
            if (group)
                  add_hl_call(calls, bdata->num, bdata->hl_id, group,
                              (line_data[]){{tok->line, tok->col1, tok->col2}});
      }

skip:
#ifdef DEBUG
      //braindead(tok, goto_safety_count, &cursor);
#endif
      *last_kind = cursor.kind;
}

/*======================================================================================*/

static enum CXChildVisitResult cursor_visitor(CXCursor cursor, CXCursor parent, CXClientData client_data);

struct translationunit_visitor_data {
      Buffer            *bdata;
      translationunit_t *stu;
};

static void
translationunit_visitor(Buffer *bdata, translationunit_t *stu)
{
      struct translationunit_visitor_data data = {bdata, stu};
      CXCursor tu_cursor = clang_getTranslationUnitCursor(stu->tu);
      clang_visitChildren(tu_cursor, &cursor_visitor, &data);
}

static enum CXChildVisitResult
cursor_visitor(CXCursor cursor, UNUSED CXCursor parent, CXClientData client_data)
{
#if 0
      if (clang_Cursor_isNull(cursor))
            return CXChildVisit_Continue;
#endif

      struct translationunit_visitor_data *tu_data = client_data;
      UNUSED Buffer                       *bdata   = tu_data->bdata;
      UNUSED translationunit_t            *stu     = tu_data->stu;

      CXFile base_file = clang_getFile(stu->tu, BS(bdata->name.full));
      slightly_less_braindead(base_file, cursor);
      //clang_visitChildren(cursor, &cursor_visitor, client_data);

      return CXChildVisit_Recurse;
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

static bool
is_really_template_parameter(token_t *tok, CXCursor cursor)
{
      CXString   spell = clang_getCursorSpelling(cursor);
      bool const ret   = !STREQ(tok->raw, CS(spell));
      clang_disposeString(spell);
      return ret;
}

static void
braindead(token_t *tok, int ngotos, CXCursor *provided)
{
        CXString spell        = clang_getCursorSpelling(tok->cursor);
        CXType   what         = clang_getCursorType(tok->cursor);
        CXString whatspell    = clang_getTypeSpelling(what);
        CXString typespell    = clang_getCursorKindSpelling(tok->cursor.kind);
        CXString tkspell      = clang_getTypeKindSpelling(what.kind);

        CXCursor decl = clang_getTypeDeclaration(what);
        CXString declspell = clang_getCursorSpelling(decl);
        CXString decldisp  = clang_getCursorDisplayName(decl);

        CXCursor ref          = clang_getCursorReferenced(tok->cursor);
        CXString refspell     = clang_getCursorSpelling(ref);
        CXString reftypespell = clang_getCursorKindSpelling(ref.kind);
        CXType   refwhat      = clang_getCursorType(ref);
        CXString refwhatspell = clang_getTypeSpelling(refwhat);
        CXString refwhatkindspell = clang_getTypeKindSpelling(refwhat.kind);

        CXCursor def          = clang_getCursorDefinition(tok->cursor);
        CXString defspell     = clang_getCursorSpelling(def);
        CXString deftypespell = clang_getCursorKindSpelling(def.kind);
        CXString defwhatspell = clang_getTypeSpelling(clang_getCursorType(def));

        fprintf(dump_fp, "Have (%s [%d]): spell(`%s`) (WHAT  `%s`  TYPE  `%s`) > TK=(`%s`) > decl:(`%s` - `%s`)  ** references ** S:(%s) W:(%s) T:(%s) TK:(%s)  **DEF**  S:(%s) W:(%s) T:(%s)  ===>  %u:[%u, %u]\n",
                tok->raw, ngotos, CS(spell), CS(whatspell), CS(typespell), CS(tkspell), CS(declspell), CS(decldisp),
                CS(refspell), CS(refwhatspell), CS(reftypespell), CS(refwhatkindspell),
                CS(defspell), CS(defwhatspell), CS(deftypespell),
                tok->line + 1U, tok->col1, tok->col2);

        free_cxstrings(whatspell, spell, typespell, tkspell,
                       refspell, reftypespell, refwhatspell, refwhatkindspell,
                       defspell, deftypespell, defwhatspell,
                       declspell, decldisp
        );

        if (provided && ngotos > 1) {
              CXString provspell     = clang_getCursorSpelling(*provided);
              CXString provtypespell = clang_getCursorKindSpelling(provided->kind);
              CXType   provwhat      = clang_getCursorType(*provided);
              CXString provwhatspell = clang_getTypeSpelling(provwhat);
              CXString provwhatkindspell = clang_getTypeKindSpelling(provwhat.kind);

              fprintf(dump_fp, "\tProvided cursor: S:(%s) W:(%s) T:(%s) TK:(%s)\n", CS(provspell), CS(provwhatspell), CS(provtypespell), CS(provwhatkindspell));
        }
}



static void
slightly_less_braindead(CXFile file, CXCursor cursor)
{
        resolved_range_t range = {0U, 0U, 0U, 0U, 0U, 0U, NULL};
        resolve_range(clang_getCursorExtent(cursor), &range);
        if (!clang_File_isEqual(file, range.file))
              return;

        CXString spell        = clang_getCursorSpelling(cursor);
        CXType   what         = clang_getCursorType(cursor);
        CXString whatspell    = clang_getTypeSpelling(what);
        CXString typespell    = clang_getCursorKindSpelling(cursor.kind);
        CXString tkspell      = clang_getTypeKindSpelling(what.kind);

        CXCursor decl      = clang_getTypeDeclaration(what);
        CXString declspell = clang_getCursorSpelling(decl);
        CXString decldisp  = clang_getCursorDisplayName(decl);

        CXCursor ref              = clang_getCursorReferenced(cursor);
        CXString refspell         = clang_getCursorSpelling(ref);
        CXString reftypespell     = clang_getCursorKindSpelling(ref.kind);
        CXType   refwhat          = clang_getCursorType(ref);
        CXString refwhatspell     = clang_getTypeSpelling(refwhat);
        CXString refwhatkindspell = clang_getTypeKindSpelling(refwhat.kind);

        CXCursor def          = clang_getCursorDefinition(cursor);
        CXString defspell     = clang_getCursorSpelling(def);
        CXString deftypespell = clang_getCursorKindSpelling(def.kind);
        CXString defwhatspell = clang_getTypeSpelling(clang_getCursorType(def));

        fprintf(dump_fp,
                "\033[1;32m" "Have:" "\033[0m"
                "\033[1;33m" " spell(" "\033[0m"
                "\033[0;33m" "%s" "\033[0m"
                "\033[1;33m" ")" "\033[0m"
                " (WHAT  `%s`  TYPE  `%s`) > TK=(`%s`) > decl:(`%s` - `%s`)  "
                "\033[1;36m" "** references **" "\033[0m"
                " S:(%s) W:(%s) T:(%s) TK:(%s)  "
                "\033[1;36m" "**DEF**" "\033[0m"
                "  S:(%s) W:(%s) T:(%s)  "
                "\033[1;34m" "===>" "\033[0m"
                "  %u[%u:%u]"
                "\n",
                CS(spell), CS(whatspell), CS(typespell), CS(tkspell), CS(declspell), CS(decldisp),
                CS(refspell), CS(refwhatspell), CS(reftypespell), CS(refwhatkindspell),
                CS(defspell), CS(defwhatspell), CS(deftypespell),
                range.line, range.start, range.end
       );

        free_cxstrings(whatspell, spell, typespell, tkspell,
                       refspell, reftypespell, refwhatspell, refwhatkindspell,
                       defspell, deftypespell, defwhatspell,
                       declspell, decldisp
        );
}
