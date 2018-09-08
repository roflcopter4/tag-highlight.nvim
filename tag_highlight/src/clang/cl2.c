#include "util/util.h"

#include "clang.h"
#include "data.h"
#include "highlight.h"
#include "libclang.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include <spawn.h>
#include <stddef.h>
#include <wait.h>

#define CLD(s)    ((struct clangdata *)((s)->clangdata))
#define TMPSIZ    (512)
#define CS(CXSTR) (clang_getCString(CXSTR))
#define TUFLAGS                                          \
        (  CXTranslationUnit_DetailedPreprocessingRecord \
         | CXTranslationUnit_KeepGoing                   \
         | CXTranslationUnit_PrecompiledPreamble         \
         | CXTranslationUnit_Incomplete)

#define DUMPDATA()                                                 \
        do {                                                       \
                char dump[TMPSIZ];                                 \
                snprintf(dump, TMPSIZ, "%s/XXXXXX.log", tmp_path); \
                const int dumpfd = mkstemps(dump, 4);              \
                argv_dump_fd(dumpfd, comp_cmds);                   \
                b_list_dump_fd(dumpfd, enumlist.enumerators);      \
                close(dumpfd);                                     \
        } while (0)


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
        char               tmp_name[TMPSIZ];
};

static char tmp_path[PATH_MAX + 1];

static inline void lines2bytes(struct bufdata *bdata, int64_t *startend, int first, int last);

static struct translationunit *init_compilation_unit(struct bufdata *bdata);
static struct translationunit *recover_compilation_unit(struct bufdata *bdata);
static struct cmd_info *       getinfo(const struct bufdata *bdata);
static str_vector *            get_compile_commands(struct bufdata *bdata);
static str_vector *            get_backup_commands(struct bufdata *bdata);
static void                    get_tmp_path(void);
static void                    clean_tmpdir(void);
static void                    destroy_struct_translationunit(struct translationunit *stu);
static void                    tagfinder(struct mydata *data, CXCursor cursor);
static enum CXChildVisitResult visit_continue(CXCursor cursor, CXCursor parent, void *client_data);

static void          tokenize_range(struct translationunit *stu, CXFile *file, int64_t first, int64_t last);
static bool          get_range(CXSourceRange r, unsigned values[4]);
static struct token *get_token_data(CXTranslationUnit *tu, CXToken *tok, CXCursor *cursor);

/*======================================================================================*/

void
libclang_highlight(struct bufdata *bdata, const int first, const int last)
{
        static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&mut);

        struct translationunit *stu = (bdata->clangdata)
                                          ? recover_compilation_unit(bdata)
                                          : init_compilation_unit(bdata);
        int64_t  startend[2];
        CXFile   file = clang_getFile(CLD(bdata)->tu, CLD(bdata)->tmp_name);

        if (last == (-1)) {
                startend[0] = 0;
                startend[1] = stu->buf->slen;
        } else {
                lines2bytes(bdata, startend, first, last);
        }

        /* eprintf("Scanninf from line %d to %d (aka %ld to %ld)\n", first, last, startend[0], startend[1]); */

        tokenize_range(stu, &file, startend[0], startend[1]);
        type_id(bdata, stu, CLD(bdata)->enumerators, CLD(bdata)->info, first, last);
        destroy_struct_translationunit(stu);

        pthread_mutex_unlock(&mut);
}

static inline void
lines2bytes(struct bufdata *bdata, int64_t *startend, const int first, const int last)
{
        int64_t  startbyte = 0, endbyte = 0;
        unsigned i = 0;

        LL_FOREACH_F(bdata->lines, node) {
                if (i < first)
                        endbyte = (startbyte += node->data->slen + 1);
                else if (i < last+1)
                        endbyte += node->data->slen + 1;
                else
                        break;
                ++i;
        }

        startend[0] = startbyte;
        startend[1] = endbyte;
}

/*======================================================================================*/

static struct translationunit *
recover_compilation_unit(struct bufdata *bdata)
{
        struct translationunit *stu = xmalloc(sizeof(struct translationunit));
        stu->idx = CLD(bdata)->idx;
        stu->tu  = CLD(bdata)->tu;
        stu->buf = ll_join(bdata->lines, '\n');

        const int fd = open(CLD(bdata)->tmp_name, O_WRONLY|O_TRUNC, 0600);
        b_write(fd, stu->buf, B("\n"));
        close(fd);

        int ret = clang_reparseTranslationUnit(CLD(bdata)->tu, 0, NULL, TUFLAGS);
        if (ret != 0)
                errx(1, "libclang error: %d", ret);

        return stu;
}

static struct translationunit *
init_compilation_unit(struct bufdata *bdata)
{
        if (!*tmp_path)
                get_tmp_path();

        char         tmp[TMPSIZ];
        bstring     *buf       = ll_join(bdata->lines, '\n');
        str_vector  *comp_cmds = get_compile_commands(bdata);
        const size_t tmplen    = snprintf(tmp, TMPSIZ, "%s/XXXXXX.c", tmp_path);
        const int    tmpfd     = mkostemps(tmp, 2, O_DSYNC);
        b_write(tmpfd, buf, B("\n"));
        close(tmpfd);

        argv_dump(stderr, comp_cmds);

        struct translationunit *stu = xmalloc(sizeof(struct translationunit));
        stu->idx = clang_createIndex(1, 0);
        stu->tu  = NULL;
        stu->buf = buf;

        int clerror = clang_parseTranslationUnit2(stu->idx, tmp, (const char **)comp_cmds->lst,
                                                  comp_cmds->qty, NULL, 0, TUFLAGS, &stu->tu);

        if (!stu->tu || clerror != 0)
                errx(1, "libclang error: %d", clerror);

        /* Get all enumerators in the translation unit separately, because clang
         * doesn't expose them as such, only as normal integers. */
        struct mydata enumlist = {stu->tu, b_list_create(), buf};
        clang_visitChildren(clang_getTranslationUnitCursor(stu->tu),
                            visit_continue, &enumlist);
        B_LIST_SORT_FAST(enumlist.enumerators);

        DUMPDATA();

        bdata->clangdata = xmalloc(sizeof(struct clangdata));
        CLD(bdata)->idx         = stu->idx;
        CLD(bdata)->tu          = stu->tu;
        CLD(bdata)->enumerators = enumlist.enumerators;
        CLD(bdata)->argv        = comp_cmds;
        CLD(bdata)->info        = getinfo(bdata);
        memcpy(CLD(bdata)->tmp_name, tmp, tmplen + 1);

        return stu;
}

static struct cmd_info *
getinfo(const struct bufdata *bdata)
{
        const unsigned   ngroups = bdata->ft->order->slen;
        struct cmd_info *info    = nmalloc(ngroups, sizeof(*info));

        for (unsigned i = 0; i < ngroups; ++i) {
                const int     ch   = bdata->ft->order->data[i];
                mpack_dict_t *dict = nvim_get_var_fmt(
                        0, E_MPACK_DICT, PKG "%s#%c", BTS(bdata->ft->vim_name), ch).ptr;

                info[i].kind  = ch;
                info[i].group = dict_get_key(dict, E_STRING, B("group")).ptr;
                info[i].num   = ngroups;

                b_writeprotect(info[i].group);
                destroy_mpack_dict(dict);
                b_writeallow(info[i].group);
        }

        return info;
}

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
            db, BS(bdata->name.full));
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
                        } else if (!b_iseq(&bstr, bdata->name.full) &&
                                   !b_iseq(&bstr, bdata->name.base) &&
                                   !b_iseq(base,  bdata->name.path)) {
                                argv_append(ret, cstr, true);
                        }

                        b_free(base);
                        clang_disposeString(tmp);
                }
        }

        argv_fmt(ret, "-I%s", BS(bdata->name.path));
        for (unsigned i = 0; i < 3; ++i)
                argv_append(ret, clang_paths[i], true);

        clang_CompileCommands_dispose(cmds);
        clang_CompilationDatabase_dispose(db);
        return ret;
}

static str_vector *
get_backup_commands(struct bufdata *bdata)
{
        str_vector *ret = argv_create(8);
        argv_append(ret, "clang", true);
        argv_fmt(ret, "-I%s", BS(bdata->name.path));
        argv_fmt(ret, "-I%s", BS(bdata->topdir->pathname));

        /* for (unsigned i = 0; i < 3; ++i)
                argv_append(ret, clang_paths[i], false); */

        return ret;
}

/*======================================================================================*/

#define CMD_SIZ      (4096)
#define STATUS_SHIFT (8)

static void
get_tmp_path(void)
{
        memcpy(tmp_path, SLS("/mnt/ramdisk/tag_highlight_XXXXXX"));
        errno = 0;
        if (!mkdtemp(tmp_path))
                err(1, "mkdtemp failed");
        atexit(clean_tmpdir);
}

static void
clean_tmpdir(void)
{
#ifndef HAVE_POSIX_SPAWNP
#error asswipe
        char cmd[CMD_SIZ];
        snprintf(cmd, CMD_SIZ, "rm -rf \"%s\"", tmp_path);
        int status = system(cmd) << STATUS_SHIFT;
        if (status != 0)
                err(1, "rm failed with status %d", status);
#else
        int status, pid, ret;
        char *const argv[] = {"rm", "-rf", tmp_path, (char *)0};

        if ((ret = posix_spawnp(&pid, "rm", NULL, NULL, argv, environ)) != 0)
                err(1, "Posix spawn failed: %d", ret);

        waitpid(pid, &status, 0);
        if ((status <<= STATUS_SHIFT) != 0)
                err(1, "rm failed with status %d", status);
#endif
}

static void
destroy_struct_translationunit(struct translationunit *stu)
{
        clang_disposeTokens(stu->tu, stu->cxtokens, stu->num);
        free(stu->cxcursors);
        genlist_destroy(stu->tokens);
        b_free(stu->buf);
        free(stu);
}

void
destroy_clangdata(struct bufdata *bdata)
{
        struct clangdata *cdata = bdata->clangdata;
        if (!cdata)
                return;
        b_list_destroy(cdata->enumerators);
        argv_destroy(cdata->argv);

        for (unsigned i = 0, e = cdata->info[0].num; i < e; ++i)
                b_free(cdata->info[i].group);

        free(cdata->info);
        clang_disposeTranslationUnit(cdata->tu);
        clang_disposeIndex(cdata->idx);
        free(cdata);
        bdata->clangdata = NULL;
}

/*======================================================================================*/

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

static bool
get_range(CXSourceRange r, unsigned values[4])
{
        CXSourceLocation start = clang_getRangeStart(r);
        CXSourceLocation end   = clang_getRangeEnd(r);
        unsigned         rng[2][3];

        clang_getExpansionLocation(start, NULL, &rng[0][0], &rng[0][1], &rng[0][2]);
        clang_getExpansionLocation(end,   NULL, &rng[1][0], &rng[1][1], NULL);

        if (rng[0][0] != rng[1][0])
                return false;

        values[0] = rng[0][0];
        values[1] = rng[0][1];
        values[2] = rng[1][1];
        values[3] = rng[0][2];
        return true;
}

static struct token *
get_token_data(CXTranslationUnit *tu, CXToken *tok, CXCursor *cursor)
{
        struct token *ret;
        unsigned      values[4];
        CXTokenKind   tokkind = clang_getTokenKind(*tok);

        if (tokkind != CXToken_Identifier ||
            !get_range(clang_getTokenExtent(*tu, *tok), values))
                return NULL;

        CXString  dispname = clang_getCursorDisplayName(*cursor);

        const size_t n  = strlen(CS(dispname)) + 1llu;
        ret             = xmalloc(offsetof(struct token, raw) + n);
        ret->token      = *tok;
        ret->cursor     = *cursor;
        ret->cursortype = clang_getCursorType(*cursor);
        ret->tokenkind  = clang_getTokenKind(*tok);
        ret->line       = values[0] - 1;
        ret->col1       = values[1] - 1;
        ret->col2       = values[2] - 1;
        ret->len        = values[2] - values[1];

        memcpy(ret->raw, CS(dispname), n);
        clang_disposeString(dispname);
        return ret;
}

static void
tokenize_range(struct translationunit *stu, CXFile *file, const int64_t first, const int64_t last)
{
        struct token   *t;
        CXToken        *toks = NULL;
        unsigned        num  = 0;
        CXSourceRange   rng  = clang_getRange(
            clang_getLocationForOffset(stu->tu, *file, first),
            clang_getLocationForOffset(stu->tu, *file, last)
        );

        clang_tokenize(stu->tu, rng, &toks, &num);
        CXCursor *cursors = nmalloc(num, sizeof(CXCursor));
        clang_annotateTokens(stu->tu, toks, num, cursors);

        stu->cxtokens  = toks;
        stu->cxcursors = cursors;
        stu->num       = num;
        stu->tokens    = genlist_create_alloc(num / 2);

        for (unsigned i = 0; i < num; ++i)
                if ((t = get_token_data(&stu->tu, &toks[i], &cursors[i])))
                        genlist_append(stu->tokens, t);
}

/*======================================================================================*/
// vim: tw=90 sts=8 sw=8 expandtab
