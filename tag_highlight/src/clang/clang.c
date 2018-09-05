#include "util/util.h"

#include "clang.h"
#include "libclang.h"

#include "api.h"
#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include <spawn.h>
#include <stddef.h>
#include <wait.h>

#define auto_type               __extension__ __auto_type
#define TMPCHARSIZ             (512)
#define CS(CXSTR)              (clang_getCString(CXSTR))
#define TUFLAGS                                          \
        (  CXTranslationUnit_DetailedPreprocessingRecord \
         | CXTranslationUnit_KeepGoing                   \
         | CXTranslationUnit_PrecompiledPreamble)

typedef struct argument_vector str_vector;

struct mydata {
        CXTranslationUnit tu;
        b_list *enumerators;
        const bstring *buf;
};

struct clangdata {
        CXIndex            idx;
        CXTranslationUnit  tu;
        b_list            *enumerators;
        str_vector        *argv;
        struct cmd_info   *info;
        char               tmp_name[TMPCHARSIZ];
};

static char tmp_path[PATH_MAX + 1];

static void tokenize_file(struct translationunit *stu, CXFile *file);
static struct token   *get_token_data(CXTranslationUnit *tu, CXToken *tok, CXCursor *cursor, const bstring *buf);
static _Bool           get_range(CXSourceRange r, unsigned values[4]);
static void            get_tmp_path(void);

static struct translationunit *libclang_get_compilation_unit(struct bufdata *bdata);
static void destroy_translationunit(struct translationunit *stu);
static enum CXChildVisitResult visit_continue(CXCursor cursor, UNUSED CXCursor parent, void *client_data);

static str_vector *get_compile_commands(struct bufdata *bdata);
static str_vector *get_backup_commands(struct bufdata *bdata);
static void safe_argv_destroy(str_vector *argv);

/*======================================================================================*/

void
libclang_get_hl_commands(struct bufdata *bdata)
{
        if (bdata->clangdata)
                destroy_clangdata(bdata->clangdata);
        struct translationunit *stu = libclang_get_compilation_unit(bdata);
        destroy_translationunit(stu);
}

static struct translationunit *
libclang_get_compilation_unit(struct bufdata *bdata)
{
        static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
        if (!bdata)
                return NULL;
        pthread_mutex_lock(&mut);
        if (!*tmp_path)
                get_tmp_path();

        str_vector *compile_commands = get_compile_commands(bdata);
        argv_dump(stderr, compile_commands);

        bstring *buf = ll_join(bdata->lines, '\n');
        char     tmp[TMPCHARSIZ];
        snprintf(tmp, TMPCHARSIZ, "%s/XXXXXX.c", tmp_path);
        const int tmpfd = mkostemps(tmp, 2, O_DSYNC);
        b_write(tmpfd, buf, B("\n"));
        close(tmpfd);

        struct translationunit *ret = xmalloc(sizeof(struct translationunit));
        ret->idx = clang_createIndex(1, 0);
        ret->tu  = NULL;
        ret->buf = buf;

        enum CXErrorCode clerror = clang_parseTranslationUnit2(
            ret->idx, tmp, (const char *const*)compile_commands->lst,
            compile_commands->qty, NULL, 0, TUFLAGS, &ret->tu);

        if (!ret->tu || clerror != 0)
                errx(1, "libclang error: %d", clerror);

        struct mydata enumlist = { ret->tu, b_list_create(), buf };
        clang_visitChildren(clang_getTranslationUnitCursor(ret->tu),
                            visit_continue, &enumlist);
        B_LIST_SORT_FAST(enumlist.enumerators);

        {
                char dump[TMPCHARSIZ];
                snprintf(dump, TMPCHARSIZ, "%s/XXXXXX.log", tmp_path);
                const int dumpfd = mkstemps(dump, 4);
                argv_dump_fd(dumpfd, compile_commands);
                b_list_dump_fd(dumpfd, enumlist.enumerators);
                close(dumpfd);
        }

        CXFile file = clang_getFile(ret->tu, tmp);
        tokenize_file(ret, &file);
        bdata->clangdata = xmalloc(sizeof(struct clangdata));

        ((struct clangdata *)bdata->clangdata)->info = NULL;
        typeswitch(bdata, ret, enumlist.enumerators,
                   &((struct clangdata *)bdata->clangdata)->info);

        *(struct clangdata *)(bdata->clangdata) = (struct clangdata){
            .idx = ret->idx, .tu = ret->tu, .argv = compile_commands,
            .enumerators = enumlist.enumerators};
        strcpy(((struct clangdata *)(bdata->clangdata))->tmp_name, tmp);

        /* b_list_destroy(enumlist.enumerators); */
        /* safe_argv_destroy(compile_commands); */
        pthread_mutex_unlock(&mut);
        return ret;
}

/*======================================================================================*/

static void
tokenize_file(struct translationunit *stu, CXFile *file)
{
        CXToken        *toks = NULL;
        unsigned        num  = 0;
        CXSourceRange   rng  = clang_getRange(
            clang_getLocationForOffset(stu->tu, *file, 0),
            clang_getLocationForOffset(stu->tu, *file, stu->buf->slen));

        clang_tokenize(stu->tu, rng, &toks, &num);
        CXCursor *cursors = nmalloc(num, sizeof(CXCursor));
        clang_annotateTokens(stu->tu, toks, num, cursors);

        stu->cxtokens  = toks;
        stu->cxcursors = cursors;
        stu->num       = num;
        stu->tokens    = genlist_create_alloc(num / 2);

        eprintf("Succesfully parsed %u tokens!\n", num);

        for (unsigned i = 0; i < num; ++i) {
                struct token *t = get_token_data(&stu->tu, &toks[i], &cursors[i], stu->buf);
                if (t)
                        genlist_append(stu->tokens, t);
        }
}

static struct token *
get_token_data(CXTranslationUnit *tu, CXToken *tok, CXCursor *cursor, const bstring *buf)
{
        struct token *ret;
        unsigned      values[4];
        CXTokenKind   tokkind = clang_getTokenKind(*tok);

        if (tokkind != CXToken_Identifier ||
            !get_range(clang_getTokenExtent(*tu, *tok), values))
                return NULL;

        ret             = xmalloc(offsetof(struct token, raw) + values[2] + 1);
        ret->token      = *tok;
        ret->cursor     = *cursor;
        ret->cursortype = clang_getCursorType(*cursor);
        ret->tokenkind  = clang_getTokenKind(*tok);
        ret->line       = values[0] - 1;
        ret->col1       = values[1] - 1;
        ret->col2       = values[2] - 1;
        ret->len        = values[2] - values[1];

        memcpy(ret->raw, buf->data + values[3], ret->len);
        ret->raw[ret->len] = '\0';
        return ret;
}

static _Bool
get_range(CXSourceRange r, unsigned values[4])
{
        CXSourceLocation start = clang_getRangeStart(r);
        CXSourceLocation end   = clang_getRangeEnd(r);
        unsigned         ranges[2][3];

        clang_getExpansionLocation(start, NULL, &ranges[0][0], &ranges[0][1], &ranges[0][2]);
        clang_getExpansionLocation(end,   NULL, &ranges[1][0], &ranges[1][1], NULL);

        if (ranges[0][0] != ranges[1][0])
                return false;

        values[0] = ranges[0][0];
        values[1] = ranges[0][1];
        values[2] = ranges[1][1];
        values[3] = ranges[0][2];
        return true;
}

/*======================================================================================*/

static void tokenize_range(struct translationunit *stu, CXFile *file, int first, int last);

void
libclang_update_line(struct bufdata *bdata, int first, int last)
{
        static pthread_mutex_t line_mutex;
        pthread_mutex_lock(&line_mutex);
        struct translationunit *stu      = xmalloc(sizeof(struct translationunit));
        struct clangdata       *scdata      = bdata->clangdata;
        stu->buf = ll_join(bdata->lines, '\n');
        stu->tu  = scdata->tu;

        int fd = open(scdata->tmp_name, O_WRONLY|O_TRUNC, 0600);
        b_write(fd, stu->buf);
        close(fd);

        int ret = clang_reparseTranslationUnit(
            scdata->tu, 0, NULL, clang_defaultReparseOptions(scdata->tu));
        CXFile   file      = clang_getFile(scdata->tu, scdata->tmp_name);
        int64_t  startbyte = 0;
        int64_t  endbyte   = 0;
        unsigned i         = 0;

        LL_FOREACH_F(bdata->lines, node) {
                if (i < first)
                        endbyte = (startbyte += node->data->slen + 1);
                else if (i < last+1)
                        endbyte += node->data->slen + 1;
                else
                        break;
                ++i;
        }

        tokenize_range(stu, &file, startbyte, endbyte);
        typeswitch_2(bdata, stu, scdata->enumerators, &scdata->info, first, last);
        destroy_translationunit(stu);
        pthread_mutex_unlock(&line_mutex);
}

static _Bool
get_range2(CXSourceRange r, unsigned values[4])
{
        CXSourceLocation start = clang_getRangeStart(r);
        CXSourceLocation end   = clang_getRangeEnd(r);
        unsigned         ranges[2][3];

        clang_getExpansionLocation(start, NULL, &ranges[0][0], &ranges[0][1], &ranges[0][2]);
        clang_getExpansionLocation(end,   NULL, &ranges[1][0], &ranges[1][1], NULL);

        if (ranges[0][0] != ranges[1][0])
                return false;

        values[0] = ranges[0][0];
        values[1] = ranges[0][1];
        values[2] = ranges[1][1];
        values[3] = ranges[0][2];
        return true;
}

static struct token *
get_token_data2(CXTranslationUnit *tu, CXToken *tok, CXCursor *cursor, const bstring *buf)
{
        struct token *ret;
        unsigned      values[4];
        CXTokenKind   tokkind = clang_getTokenKind(*tok);

        if (tokkind != CXToken_Identifier ||
            !get_range2(clang_getTokenExtent(*tu, *tok), values))
                return NULL;

        CXString  dispname = clang_getCursorDisplayName(*cursor);

        /* ret             = xmalloc(offsetof(struct token, raw) + values[2] + 1); */
        ret             = xmalloc(offsetof(struct token, raw) + strlen(CS(dispname)) + 1);
        ret->token      = *tok;
        ret->cursor     = *cursor;
        ret->cursortype = clang_getCursorType(*cursor);
        ret->tokenkind  = clang_getTokenKind(*tok);
        ret->line       = values[0] - 1;
        ret->col1       = values[1] - 1;
        ret->col2       = values[2] - 1;
        ret->len        = values[2] - values[1];

        /* memcpy(ret->raw, buf->data + values[3], ret->len); */
        /* memcpy(ret->raw, CS(dispname), strlen(CS())); */
        strcpy(ret->raw, CS(dispname));
        /* ret->raw[ret->len] = '\0'; */
        return ret;
}

static void
tokenize_range(struct translationunit *stu, CXFile *file, const int first, const int last)
{
        CXToken        *toks = NULL;
        unsigned        num  = 0;
        CXSourceRange   rng  = clang_getRange(clang_getLocationForOffset(stu->tu, *file, first),
                                              clang_getLocationForOffset(stu->tu, *file, last));

        clang_tokenize(stu->tu, rng, &toks, &num);
        CXCursor *cursors = nmalloc(num, sizeof(CXCursor));
        clang_annotateTokens(stu->tu, toks, num, cursors);

        stu->cxtokens  = toks;
        stu->cxcursors = cursors;
        stu->num       = num;
        stu->tokens    = genlist_create_alloc(num / 2);

        for (unsigned i = 0; i < num; ++i) {
                struct token *t = get_token_data2(&stu->tu, &toks[i], &cursors[i], stu->buf);
                if (t)
                        genlist_append(stu->tokens, t);
        }
}



/*======================================================================================*/

#define CMD_SIZ      (4096)
#define STATUS_SHIFT (8)

static void clean_tmpdir(void)
{
#ifdef DOSISH
        char cmd[CMD_SIZ];
        snprintf(cmd, CMD_SIZ, "rm -rf \"%s\"", tmp_path);
        int status = system(cmd) << STATUS_SHIFT;
        if (status != 0)
                err(1, "rm failed with status %d", status);
#else
        int         status = 0;
        int         pid    = 0;
        char *const argv[] = {"rm", "-rf", tmp_path, (char *)0};
        const int   ret    = posix_spawnp(&pid, "rm", NULL, NULL, argv, environ);

        if (ret != 0)
                err(1, "Posix spawn failed: %d", ret);
        waitpid(pid, &status, 0);
        if ((status <<= STATUS_SHIFT) != 0)
                err(1, "rm failed with status %d", status);
#endif
}

static void
get_tmp_path(void)
{
        memcpy(tmp_path, SLS("/mnt/ramdisk/tag_highlight_XXXXXX"));
        errno = 0;
        if (!mkdtemp(tmp_path))
                err(1, "mkdtemp failed");
        atexit(clean_tmpdir);
}

/*======================================================================================*/

#define destroy_cxstrings(...) (destroy_cxstrings__(__VA_ARGS__, (CXString *)NULL))
static void destroy_cxstrings__(CXString *str, ...);

void
tokvisitor(struct token *tok)
{
        static char     logpth[PATH_MAX + 1];
        static unsigned n = 0;

        CXString typespell     = clang_getTypeSpelling(tok->cursortype);
        CXString typekindrepr  = clang_getTypeKindSpelling(tok->cursortype.kind);
        CXString curs_kindspel = clang_getCursorKindSpelling(tok->cursor.kind);
        FILE    *toklog        = safe_fopen_fmt("%s/toks.log", "ab", tmp_path);

        fprintf(toklog, "%4u: %-50s - %-17s - %-30s - %s\n",
                n++, tok->raw, CS(typekindrepr), CS(curs_kindspel), CS(typespell));

        fclose(toklog);
        destroy_cxstrings(&typespell, &typekindrepr, &curs_kindspel);
}

static void
destroy_cxstrings__(CXString *str, ...)
{
        if (!str)
                abort();
        va_list ap;
        va_start(ap, str);
        do
                clang_disposeString(*str);
        while ((str = va_arg(ap, CXString *)));
        va_end(ap);
}

void
destroy_clangdata(void *data)
{
        struct clangdata *cdata = data;
        b_list_destroy(cdata->enumerators);
        safe_argv_destroy(cdata->argv);
        clang_disposeTranslationUnit(cdata->tu);
        clang_disposeIndex(cdata->idx);
        /* free(data); */
}

static void
destroy_translationunit(struct translationunit *stu)
{
        clang_disposeTokens(stu->tu, stu->cxtokens, stu->num);
        genlist_destroy(stu->tokens);
        free(stu->cxcursors);
        b_free(stu->buf);
        free(stu);
}

static void
safe_argv_destroy(str_vector *argv)
{
        argv->qty -= 3;
        argv_destroy(argv);
}

/*========================================================================================*/

static void
tagfinder(struct mydata *data, CXCursor cursor)
{
        if (cursor.kind != CXCursor_EnumConstantDecl)
                return;

        CXSourceLocation loc   = clang_getCursorLocation(cursor);
        CXToken         *cxtok = clang_getToken(data->tu, loc);
        if (!cxtok)
                return;

        if (clang_getTokenKind(*cxtok) == CXToken_Identifier) {
                CXString  dispname = clang_getCursorDisplayName(cursor);
                bstring  *str      = b_fromcstr(CS(dispname));
                if (str && str->slen > 0)
                        b_list_append(&data->enumerators, str);
                clang_disposeString(dispname);
        }

        clang_disposeTokens(data->tu, cxtok, 1);
}

static enum CXChildVisitResult
visit_continue(CXCursor cursor, UNUSED CXCursor parent, void *client_data)
{
        tagfinder(client_data, cursor);
        return CXChildVisit_Recurse;
}

/*======================================================================================*/

static str_vector *
get_compile_commands(struct bufdata *bdata)
{
        CXCompilationDatabase_Error cberr;
        CXCompilationDatabase       db = clang_CompilationDatabase_fromDirectory(
            BS(bdata->topdir->pathname), &cberr);
        if (cberr != 0) {
                clang_CompilationDatabase_dispose(db);
                warn("Couldn't locate compilation database.");
                return get_backup_commands(bdata);
        }

        CXCompileCommands cmds = clang_CompilationDatabase_getCompileCommands(
            db, BS(bdata->filename));
        const unsigned ncmds = clang_CompileCommands_getSize(cmds);
        str_vector    *ret   = argv_create(32);

        for (unsigned i = 0; i < ncmds; ++i) {
                CXCompileCommand command = clang_CompileCommands_getCommand(cmds, i);
                const unsigned   nargs   = clang_CompileCommand_getNumArgs(command);

                for (unsigned x = 0; x < nargs; ++x) {
                        CXString    tmp  = clang_CompileCommand_getArg(command, x);
                        const char *cstr = CS(tmp);
                        bstring     bstr = bt_fromcstr(cstr);
                        bstring    *base = b_basename(&bstr);

                        if (strncmp(cstr, "-o", 2) == 0) {
                                if (bstr.slen == 2)
                                        ++x;
                        } else if (!b_iseq(&bstr, bdata->filename) &&
                                   !b_iseq(&bstr, bdata->basename) &&
                                   !b_iseq(base, bdata->basename))
                        {
                                argv_append(ret, cstr, true);
                        }

                        b_free(base);
                        clang_disposeString(tmp);
                }
        }

        argv_fmt(ret, "-I%s", BS(bdata->pathname));
        for (unsigned i = 0; i < 3; ++i)
                argv_append(ret, clang_paths[i], false);

        clang_CompileCommands_dispose(cmds);
        clang_CompilationDatabase_dispose(db);
        return ret;
}

static str_vector *
get_backup_commands(struct bufdata *bdata)
{
        str_vector *ret = argv_create(8);
        argv_append(ret, "clang", true);
        argv_fmt(ret, "-I%s", BS(bdata->pathname));
        argv_fmt(ret, "-I%s", BS(bdata->topdir->pathname));

        for (unsigned i = 0; i < 3; ++i)
                argv_append(ret, clang_paths[i], false);

        return ret;
}
