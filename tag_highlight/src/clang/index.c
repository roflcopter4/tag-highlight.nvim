#include "util/util.h"

#include "clang.h"
#include "clang_intern.h"
#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include <stdarg.h>

#define UU     __attribute__((__unused__))

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
        struct bufdata *bdata;
        FILE     *fp;
        unsigned  cnt;
};

#define LOG(...)    fprintf(((struct idx_data *)data)->fp, __VA_ARGS__)
#define MY_IDX_MASK (O_TRUNC|O_CREAT|O_WRONLY), (0644)

void
lc_index_file(struct bufdata *bdata)
{
        FILE *fp = safe_fopen_fmt("%s/.tag_highlight_log/idx.log", "wb", HOME);
        struct idx_data  data = {bdata, fp, 0};
        CXIndexAction    iact = clang_IndexAction_create(CLD(bdata)->idx);
        IndexerCallbacks cb   = {.abortQuery             = &my_abortQuery,
                                 .diagnostic             = &my_diagnostic,
                                 .enteredMainFile        = &my_enteredMainFile,
                                 .ppIncludedFile         = &my_ppIncludedFile,
                                 .importedASTFile        = &my_importedASTFile,
                                 .startedTranslationUnit = &my_startedTranslationUnit,
                                 .indexDeclaration       = &my_indexDeclaration,
                                 .indexEntityReference   = &my_indexEntityReference};

        int r = clang_indexTranslationUnit(iact, &data, &cb, sizeof(cb), 0, CLD(bdata)->tu);
        if (r != 0)
                errx(1, "Clang failed with error %d", r);

        fclose(fp);
        clang_IndexAction_dispose(iact);
}

/*======================================================================================*/



/*======================================================================================*/

static int
my_abortQuery(UNUSED CXClientData data, UNUSED void *reserved)
{
        /* LOG("Abort query called, ignoring!\n"); */
        return 0;
}

static void
my_diagnostic(UNUSED CXClientData data, UNUSED CXDiagnosticSet dgset, UNUSED void *reserved)
{
        LOG("Diagnostic called, ignoring!\n");
}

static CXIdxClientFile
my_enteredMainFile(UNUSED CXClientData data, UNUSED CXFile file, UNUSED void *reserved)
{
        LOG("Entered the main file. Good I guess?");
        return NULL;
}

static CXIdxClientFile
my_ppIncludedFile(UNUSED CXClientData data, const CXIdxIncludedFileInfo *info)
{
        LOG("I suppose we've run across an included file:  ");
        if (info->isAngled)
                LOG("The file is <%s>\n", info->filename);
        else
                LOG("The file is \"%s\"\n", info->filename);
        return NULL;
}

static CXIdxClientASTFile
my_importedASTFile(UNUSED CXClientData data, UNUSED const CXIdxImportedASTFileInfo *info)
{
        LOG("Somehow we stumbled into a PCH.\n");
        return NULL;
}

static CXIdxClientContainer
my_startedTranslationUnit(UNUSED CXClientData data, UNUSED void *reserved)
{
        LOG("We've just started a translation unit.\n");
        return NULL;
}

static void
my_indexDeclaration(CXClientData data, const CXIdxDeclInfo *info)
{
        LOG("This node is an \e[1;32mINDEX DECLARATION\e[0m.\n");

        CXType   curstype       = clang_getCursorType(info->cursor);
        CXString dispname       = clang_getCursorDisplayName(info->cursor);
        CXString curstype_spell = clang_getTypeSpelling(curstype);
        CXString typekind_spell = clang_getTypeKindSpelling(curstype.kind);

        LOG("The cursor dispname is \"%s\" - and it is of type \"%s\" - typekind \"%s\"\n"
            "Entity info: kind=(%s), name=(%s), USR=(%s)\n"
            "---------------------------------------------\n",
            CS(dispname), CS(curstype_spell), CS(typekind_spell),
            idx_entity_kind_repr[info->entityInfo->kind],
            info->entityInfo->name, info->entityInfo->USR);
        free_cxstrings(&dispname, &curstype_spell, &typekind_spell);
}

static void
my_indexEntityReference(CXClientData data, const CXIdxEntityRefInfo *info)
{
        LOG("This node is an \e[1;36mENTITY REFERENCE\e[0m.\n");

        CXType   curstype       = clang_getCursorType(info->cursor);
        CXString dispname       = clang_getCursorDisplayName(info->cursor);
        CXString curstype_spell = clang_getTypeSpelling(curstype);
        CXString typekind_spell = clang_getTypeKindSpelling(curstype.kind);

        LOG("The cursor dispname is \"%s\" - and it is of type \"%s\" - typekind \"%s\"\n"
            "Entity info: kind=(%s), name=(%s), USR=(%s)\n"
            "---------------------------------------------\n",
            CS(dispname), CS(curstype_spell), CS(typekind_spell),
            idx_entity_kind_repr[info->referencedEntity->kind],
            info->referencedEntity->name, info->referencedEntity->USR);
        free_cxstrings(&dispname, &curstype_spell, &typekind_spell);
}
