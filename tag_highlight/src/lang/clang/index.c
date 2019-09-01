#include "clang.h"
#include "intern.h"
#include <stdarg.h>

/* static int my_abortQuery(CXClientData data, void *reserved); */
/* static void my_diagnostic(CXClientData data, CXDiagnosticSet dgset, void *reserved); */
/* static CXIdxClientFile my_enteredMainFile(CXClientData data, CXFile file, void *reserved); */
/* static CXIdxClientFile my_ppIncludedFile(CXClientData data, const CXIdxIncludedFileInfo *info); */
/* static CXIdxClientASTFile my_importedASTFile(CXClientData data, const CXIdxImportedASTFileInfo *info); */
/* static CXIdxClientContainer my_startedTranslationUnit(CXClientData data, void *reserved); */
/* static void my_indexDeclaration(CXClientData raw_data, const CXIdxDeclInfo *clang_info); */
static void my_indexEntityReference(CXClientData raw_data, const CXIdxEntityRefInfo *info);

static const char *const idx_entity_kind_repr[] = {
    "Unexposed",
    "Typedef",
    "Function",
    "Variable",
    "Field",
    "EnumConstant",
    "ObjCClass",
    "ObjCProtocol",
    "ObjCCategory",
    "ObjCInstanceMethod",
    "ObjCClassMethod",
    "ObjCProperty",
    "ObjCIvar",
    "Enum",
    "Struct",
    "Union",
    "CXXClass",
    "CXXNamespace",
    "CXXNamespaceAlias",
    "CXXStaticVariable",
    "CXXStaticMethod",
    "CXXInstanceMethod",
    "CXXConstructor",
    "CXXDestructor",
    "CXXConversionFunction",
    "CXXTypeAlias",
    "CXXInterface",
};

#define ADD_CALL(CH)                                                                   \
        do {                                                                           \
                const bstring *group;                                                  \
                if ((group = find_group(data->bdata->ft, data->cdata->info,            \
                                        data->cdata->info->num, (CH))))                \
                        add_hl_call(data->calls, data->bdata->num, data->bdata->hl_id, \
                                    group, &line_data);                                \
        } while (0)

/*======================================================================================*/

struct idx_data {
        Buffer                 *bdata;
        struct clangdata       *cdata;
        struct translationunit *stu;
        mpack_arg_array        *calls;
        genlist                *tok_list;
        FILE                   *fp;
        unsigned                cnt;
};

#define LOG(...)    fprintf(data->fp, __VA_ARGS__)
#define MY_IDX_MASK (O_TRUNC|O_CREAT|O_WRONLY), (0644)
#define DAT(data)      ((struct idx_data *)(data))

void
lc_index_file(Buffer *bdata, struct translationunit *stu, mpack_arg_array *calls)
{
        FILE *fp = safe_fopen_fmt("%s/idx.log", "wb", BS(settings.cache_dir));

        struct idx_data  data = {bdata, CLD(bdata), stu, calls, NULL /* genlist_create() */, fp, 0};
        CXIndexAction    iact = clang_IndexAction_create(data.cdata->idx);

        IndexerCallbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.indexEntityReference = &my_indexEntityReference;
#if 0
        IndexerCallbacks cb   = {/* .abortQuery             = &my_abortQuery, */
                                 /* .diagnostic             = &my_diagnostic, */
                                 /* .enteredMainFile        = &my_enteredMainFile, */
                                 /* .ppIncludedFile         = &my_ppIncludedFile, */
                                 /* .importedASTFile        = &my_importedASTFile, */
                                 /* .startedTranslationUnit = &my_startedTranslationUnit, */
                                 /* .indexDeclaration       = &my_indexDeclaration, */
                                 .indexEntityReference   = &my_indexEntityReference};
#endif

#if 0
        if (!bdata->headers)
                bdata->headers = b_list_create();
#endif

        const int r = clang_indexTranslationUnit(iact, &data, &cb, sizeof(cb),
                                                 CXIndexOpt_IndexFunctionLocalSymbols,
                                                 data.cdata->tu);
        if (r != 0)
                errx(1, "Clang failed with error %data", r);

#if 0
        stu->tokens = data.tok_list;
        mpack_arg_array *calls = type_id(bdata, stu);
        nvim_call_atomic(calls);
        mpack_destroy_arg_array(calls);
#endif

#if 0
        if (bdata->headers->qty > 0)
                b_list_remove_dups(&bdata->headers);
#endif

        clang_IndexAction_dispose(iact);
        fclose(fp);
}

/*======================================================================================*/

static struct token *
mktok(const CXCursor *cursor, const CXString *dispname, const struct resolved_range *rng)
{
        size_t        len = strlen(CS(*dispname)) + 1LLU;
        struct token *ret = malloc(offsetof(struct token, raw) + len);
        ret->cursor       = *cursor;
        ret->cursortype   = clang_getCursorType(*cursor);
        ret->line         = rng->line - 1;
        ret->col1         = rng->start - 1;
        ret->col2         = rng->end - 1;
        ret->len          = rng->end - rng->start;

        memcpy(ret->raw, CS(*dispname), len);

        return ret;
}

static void log_idx_location(const CXCursor         cursor,
                             const CXIdxEntityInfo *ref,
                             const CXIdxLoc         loc,
                             struct idx_data       *data)
{
        struct { CXFile file; unsigned line, column, offset; } linfo;
        clang_indexLoc_getFileLocation(loc, NULL, &linfo.file, &linfo.line,
                                       &linfo.column, &linfo.offset);
        if (!clang_File_isEqual(linfo.file, CLD(data->bdata)->mainfile))
                return;

        CXType   curstype       = clang_getCursorType(cursor);
        CXString dispname       = clang_getCursorDisplayName(cursor);
        CXString curstype_spell = clang_getTypeSpelling(curstype);
        CXString typekind_spell = clang_getTypeKindSpelling(curstype.kind);
        CXString fname          = clang_getFileName(linfo.file);

        LOG("The cursor dispname is \"%s\" - and it is of type \"%s\" - typekind \"%s\"\n"
            "Entity info: kind=(%s), name=(%s), USR=(%s)\n",
            CS(dispname), CS(curstype_spell), CS(typekind_spell),
            idx_entity_kind_repr[ref->kind], ref->name, ref->USR);

        struct resolved_range rng = {0, 0, 0, 0, 0, 0, NULL};
        resolve_range(clang_getCursorExtent(cursor), &rng);
        /* genlist_append(data->tok_list, mktok(&cursor, &dispname, rng)); */

#if 0
        CXCursor parent          = clang_getCursorSemanticParent(cursor);
        CXType   pcurstype       = clang_getCursorType(parent);
        CXString pdispname       = clang_getCursorDisplayName(parent);
        CXString pkind_spell     = clang_getCursorKindSpelling(parent.kind);
        CXString pcurstype_spell = clang_getTypeSpelling(pcurstype);
        CXString ptypekind_spell = clang_getTypeKindSpelling(pcurstype.kind);

        LOG("The cursor's parent's dispname is \"%s\" - and it is of type \"%s\" - typekind \"%s\" - kind \"%s\"\n",
            CS(pdispname), CS(pcurstype_spell), CS(ptypekind_spell), CS(pkind_spell));
#endif

        if (clang_File_isEqual(data->cdata->mainfile, rng.file)) {
                LOG("Also the above is on line: %u, column: %u to %u - offset %u.\n"
                    "That is ,      %s -- line: %u, column: %u       - offset %u.\n"
                    "---------------------------------------------\n",
                    rng.line, rng.start, rng.end, rng.offset1/* , BS(tmp) */,
                    CS(fname), linfo.line, linfo.column, linfo.offset);
        }

        LOG("---------------------------------------------\n");
        free_cxstrings(dispname, curstype_spell, typekind_spell, fname);
        /* free_cxstrings(pdispname, pcurstype_spell, ptypekind_spell); */
}

/*======================================================================================*/

#define LINE_DATA_FAIL    (0)
#define LINE_DATA_SUCEESS (1)

static int
get_line_data (const struct translationunit *stu,
               const CXCursor                cursor,
               const CXIdxLoc                loc,
               struct idx_data              *data,
               struct line_data             *line_data)
{
        struct { CXFile file; unsigned line, column, offset; } fileinfo;
        clang_indexLoc_getFileLocation(loc, NULL, &fileinfo.file, &fileinfo.line,
                                       &fileinfo.column, &fileinfo.offset);

        if (!clang_File_isEqual(fileinfo.file, data->cdata->mainfile))
                return LINE_DATA_FAIL;

        struct resolved_range rng = {0, 0, 0, 0, 0, 0, NULL};
        resolve_range(clang_getCursorExtent(cursor), &rng);

        if (rng.line == 0 || rng.start == rng.end || rng.end == 0)
                return LINE_DATA_FAIL;

        /*
         * Whenever a cursor appears within a macro definition clang will consider that
         * cursor to appear in the source wherever the macro is used. Since it's not
         * literally in the original source file, clang returns the location as being the
         * whole macro call. This would mean highlighting the whole call very incorrectly
         * in the color of a token that isn't there. So we check the actual text at the
         * given range to see if it is the same as the actual text of the cursor itself.
         *
         * This took like 2 days to figure out and solve. I swear I'm an idiot.
         */
        CXString   dispname = clang_getCursorDisplayName(cursor);
        bstring    realtok  = bt_fromblk(&stu->buf->data[rng.offset1], rng.len);
        const bool eq       = b_iseq_cstr(&realtok, CS(dispname));
        clang_disposeString(dispname);
        if (!eq)
                return LINE_DATA_FAIL;

        line_data->line  = rng.line - 1;
        line_data->start = rng.start - 1;
        line_data->end   = rng.end - 1;

        return LINE_DATA_SUCEESS;
}

/*======================================================================================*/

#if 0
static int
my_abortQuery(UNUSED CXClientData data, UNUSED void *reserved)
{
        /* LOG("Abort query called, ignoring!\n"); */
        return 0;
}

static void
my_diagnostic(CXClientData data, UNUSED CXDiagnosticSet dgset, UNUSED void *reserved)
{
        LOG("Diagnostic called, ignoring!\n");
}

static CXIdxClientFile
my_enteredMainFile(CXClientData data, UNUSED CXFile file, UNUSED void *reserved)
{
        LOG("Entered the main file. Good I guess?");
        return NULL;
}
#endif

#if 0
static CXIdxClientFile
my_ppIncludedFile(CXClientData data, const CXIdxIncludedFileInfo *info)
{
        LOG("I suppose we've run across an included file:  ");
        if (info->isAngled)
                LOG("The file is <%s>\n", info->filename);
        else
                LOG("The file is \"%s\"\n", info->filename);

        CXString fullname = clang_getFileName(info->file);
        b_list_append(&DAT(data)->bdata->headers, b_fromcstr(CS(fullname)));
        clang_disposeString(fullname);

        return NULL;
}
#endif

#if 0
static CXIdxClientASTFile
my_importedASTFile(CXClientData data, UNUSED const CXIdxImportedASTFileInfo *info)
{
        LOG("Somehow we stumbled into a PCH.\n");
        return NULL;
}

static CXIdxClientContainer
my_startedTranslationUnit(CXClientData data, UNUSED void *reserved)
{
        LOG("We've just started a translation unit.\n");
        return NULL;
}
#endif

#if 0
static void
my_indexDeclaration(CXClientData rawdata, const CXIdxDeclInfo *info)
{
        struct idx_data *data = rawdata;
        LOG("This node is an \033[1;32mINDEX DECLARATION\033[0m.\n");
        log_idx_location(info->cursor, info->entityInfo, info->loc, data);
}
#endif

static void
my_indexEntityReference(CXClientData raw_data, const CXIdxEntityRefInfo *info)
{
        struct idx_data  *data = raw_data;
        struct line_data  line_data;

        if (!get_line_data(data->stu, info->cursor, info->loc, data, &line_data))
                return;

        switch (info->referencedEntity->kind) {
        case CXIdxEntity_EnumConstant:
                ADD_CALL(CTAGS_ENUMCONST);
                break;
#if 0
        case CXIdxEntity_CXXClass:
                if (data->bdata->ft->id == FT_CXX)
                        ADD_CALL(CTAGS_CLASS);
                break;
#endif
        case CXIdxEntity_CXXTypeAlias:
                if (data->bdata->ft->id == FT_CXX)
                        ADD_CALL(CTAGS_TYPE);
                break;
        case CXIdxEntity_CXXNamespaceAlias:
                if (data->bdata->ft->id == FT_CXX)
                        ADD_CALL(CTAGS_NAMESPACE);
                break;
        default:
                return;
        }

#if 0
        LOG("This node is an \033[1;36mENTITY REFERENCE\033[0m.\n");
        log_idx_location(info->cursor, info->referencedEntity, info->loc, data);
#endif
}
