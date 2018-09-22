#include "util/util.h"

#include "clang.h"
#include "data.h"
#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include <stdarg.h>

static int my_abortQuery(CXClientData data, void *reserved);
static void my_diagnostic(CXClientData data, CXDiagnosticSet dgset, void *reserved);
static CXIdxClientFile my_enteredMainFile(CXClientData data, CXFile file, void *reserved);
static CXIdxClientFile my_ppIncludedFile(CXClientData data, const CXIdxIncludedFileInfo *info);
static CXIdxClientASTFile my_importedASTFile(CXClientData data, const CXIdxImportedASTFileInfo *info);
static CXIdxClientContainer my_startedTranslationUnit(CXClientData data, void *reserved);
static void my_indexDeclaration(CXClientData data, const CXIdxDeclInfo *info);
static void my_indexEntityReference(CXClientData data, const CXIdxEntityRefInfo *info);

/*======================================================================================*/

struct idx_data {
        struct bufdata   *bdata;
        struct clangdata *cdata;
        FILE             *fp;
        unsigned          cnt;
};

#define LOG(...)    fprintf(((struct idx_data *)data)->fp, __VA_ARGS__)
#define MY_IDX_MASK (O_TRUNC|O_CREAT|O_WRONLY), (0644)
#define DAT(D)      ((struct idx_data *)(D))

void
lc_index_file(struct bufdata *bdata)
{
        FILE *fp = safe_fopen_fmt("%s/.tag_highlight_log/idx.log", "wb", HOME);
        struct idx_data  data = {bdata, CLD(bdata), fp, 0};
        CXIndexAction    iact = clang_IndexAction_create(CLD(bdata)->idx);
        IndexerCallbacks cb   = {/* .abortQuery             = &my_abortQuery, */
                                 /* .diagnostic             = &my_diagnostic, */
                                 /* .enteredMainFile        = &my_enteredMainFile, */
                                 .ppIncludedFile         = &my_ppIncludedFile,
                                 /* .importedASTFile        = &my_importedASTFile, */
                                 /* .startedTranslationUnit = &my_startedTranslationUnit, */
                                 .indexDeclaration       = &my_indexDeclaration,
                                 .indexEntityReference   = &my_indexEntityReference};

        if (!bdata->headers)
                bdata->headers = b_list_create();

        const int r = clang_indexTranslationUnit(iact, &data, &cb, sizeof(cb),
                                                 CXIndexOpt_IndexFunctionLocalSymbols,
                                                 CLD(bdata)->tu);
        if (r != 0)
                errx(1, "Clang failed with error %d", r);

        if (bdata->headers->qty > 0)
                b_list_remove_dups(&bdata->headers);
        fclose(fp);
        clang_IndexAction_dispose(iact);
}

/*======================================================================================*/

#define log_idx_location(...) P99_CALL_DEFARG(log_idx_location, 3, __VA_ARGS__)
#define log_idx_location_defarg_2() data

static void
(log_idx_location)(const CXCursor cursor, const CXIdxEntityInfo *ref, struct idx_data *data)
{
        CXType   curstype       = clang_getCursorType(cursor);
        CXString dispname       = clang_getCursorDisplayName(cursor);
        CXString curstype_spell = clang_getTypeSpelling(curstype);
        CXString typekind_spell = clang_getTypeKindSpelling(curstype.kind);

        LOG("The cursor dispname is \"%s\" - and it is of type \"%s\" - typekind \"%s\"\n"
            "Entity info: kind=(%s), name=(%s), USR=(%s)\n",
            CS(dispname), CS(curstype_spell), CS(typekind_spell),
            idx_entity_kind_repr[ref->kind], ref->name, ref->USR);
        free_cxstrings(dispname, curstype_spell, typekind_spell);

        struct resolved_range *rng = &(struct resolved_range){0, 0, 0, 0, 0};
        resolve_range(clang_getCursorExtent(cursor), rng);

        if (clang_File_isEqual(data->cdata->mainfile, rng->file)) {
                LOG("Also the above is on line %u, from col %u to %u - offset %u.\n"
                    /* "Real extent: \"%s\"\n" */
                    "---------------------------------------------\n",
                    rng->line, rng->start, rng->end, rng->offset/* , BS(tmp) */);

                /* nvim_buf_add_highlight(0, data->bdata->num, -1, B("Keyword"), rng->line-1, rng->start-1, rng->end-1); */
        }

        LOG("---------------------------------------------\n");
}

/*======================================================================================*/

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

static void
my_indexDeclaration(CXClientData data, const CXIdxDeclInfo *info)
{
        LOG("This node is an \033[1;32mINDEX DECLARATION\033[0m.\n");
        log_idx_location(info->cursor, info->entityInfo);
}

static void
my_indexEntityReference(CXClientData data, const CXIdxEntityRefInfo *info)
{
        LOG("This node is an \x1B[1;36mENTITY REFERENCE\x1B[0m.\n");
        log_idx_location(info->cursor, info->referencedEntity);
}
