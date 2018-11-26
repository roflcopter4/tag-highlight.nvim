#include "Common.h"
#include "highlight.h"

#include "lang/clang/clang.h"
#include "lang/clang/intern.h"
#include "util/find.h"

#ifdef DOSISH
#  define at_quick_exit(a)
#  define quick_exit(a) _Exit(a)
#endif

#define TUFLAGS                                          \
        (  CXTranslationUnit_DetailedPreprocessingRecord \
         | CXTranslationUnit_KeepGoing                   \
         | CXTranslationUnit_PrecompiledPreamble         \
         /* | CXTranslationUnit_Incomplete */                  \
         | CXTranslationUnit_CreatePreambleOnFirstParse  \
         /* | CXTranslationUnit_IncludeAttributedTypes */ )
#define INIT_ARGV   (32)
#define DUMPDATASIZ ((int64_t)((double)SAFE_PATH_MAX * 1.5))

#ifdef DOSISH
#  define DUMPDATA()
#else
#  define DUMPDATA()                                                             \
        do {                                                                     \
                char dump[DUMPDATASIZ];                                          \
                snprintf(dump, DUMPDATASIZ, "%s/XXXXXX.log", libclang_tmp_path); \
                const int dumpfd = mkstemps(dump, 4);                            \
                argv_dump_fd(dumpfd, comp_cmds);                                 \
                b_list_dump_fd(dumpfd, enumlist.enumerators);                    \
                close(dumpfd);                                                   \
        } while (0)
#endif

static const char *const gcc_sys_dirs[] = {GCC_ALL_INCLUDE_DIRECTORIES};
char              libclang_tmp_path[SAFE_PATH_MAX];

struct enum_data {
        CXTranslationUnit tu;
        b_list           *enumerators;
        const bstring    *buf;
};

static void                    destroy_struct_translationunit(struct translationunit *stu);
static CXCompileCommands       get_clang_compile_commands_for_file(CXCompilationDatabase *db, Buffer *bdata);
static str_vector             *get_backup_commands(Buffer *bdata);
static str_vector             *get_compile_commands(Buffer *bdata);
static struct token           *get_token_data(CXTranslationUnit *tu, CXToken *tok, CXCursor *cursor);
static struct translationunit *init_compilation_unit(Buffer *bdata, bstring *buf);
static struct translationunit *recover_compilation_unit(Buffer *bdata, bstring *buf);
static inline void             lines2bytes(Buffer *bdata, int64_t *startend, int first, int last);
static void                    tagfinder(struct enum_data *data, CXCursor cursor);
static void                    tokenize_range(struct translationunit *stu, CXFile *file, int64_t first, int64_t last);
static enum CXChildVisitResult visit_continue(CXCursor cursor, CXCursor parent, void *client_data);

#ifndef TIME_CLANG
#  undef TIMER_START
#  undef TIMER_REPORT_RESTART
#  undef TIMER_REPORT
#  define TIMER_START(...)
#  define TIMER_REPORT_RESTART(...)
#  define TIMER_REPORT(...)
#endif

/*======================================================================================*/

void
(libclang_highlight)(Buffer *bdata, const int first, const int last, const int type)
{
        if (!bdata || !bdata->initialized)
                return;
        if (!P44_EQ_ANY(bdata->ft->id, FT_C, FT_CXX))
                return;

        pthread_mutex_lock(&bdata->lock.ctick);
        const unsigned new = atomic_load(&bdata->ctick);
        const unsigned old = atomic_exchange(&bdata->last_ctick, new);

        if (type == HIGHLIGHT_NORMAL && new > 0 && old >= new) {
                pthread_mutex_unlock(&bdata->lock.ctick);
                return;
        }
        pthread_mutex_unlock(&bdata->lock.ctick);

        struct translationunit *stu;
        int64_t                 startend[2];
        static pthread_mutex_t  lc_mutex = PTHREAD_MUTEX_INITIALIZER;
        bstring                *joined   = NULL;
        unsigned                cnt_val  = p99_count_inc(&bdata->lock.num_workers);

        if (cnt_val >= 2) {
                p99_count_dec(&bdata->lock.num_workers);
                return;
        }

        pthread_mutex_lock(&lc_mutex);
        {
                pthread_mutex_lock(&bdata->lines->lock);
                joined = ll_join(bdata->lines, '\n');
                if (last == (-1)) {
                        startend[0] = 0;
                        startend[1] = joined->slen;
                } else {
                        lines2bytes(bdata, startend, first, last);
                }
                pthread_mutex_unlock(&bdata->lines->lock);
        }

        if (type == HIGHLIGHT_REDO) {
                if (bdata->clangdata)
                        destroy_clangdata(bdata);
                stu = init_compilation_unit(bdata, joined);
        } else {
                stu = (bdata->clangdata) ? recover_compilation_unit(bdata, joined)
                                         : init_compilation_unit(bdata, joined);
        }

        CLD(bdata)->mainfile = clang_getFile(CLD(bdata)->tu, CLD(bdata)->tmp_name);

        tokenize_range(stu, &CLD(bdata)->mainfile, startend[0], startend[1]);
        mpack_arg_array *calls = type_id(bdata, stu);
        nvim_call_atomic(,calls);

        mpack_destroy_arg_array(calls);
        destroy_struct_translationunit(stu);
        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&lc_mutex);
}

void *
libclang_threaded_highlight(void *vdata)
{
        libclang_highlight((Buffer *)vdata);
        pthread_exit(NULL);
}

static inline void
lines2bytes(Buffer *bdata, int64_t *startend, const int first, const int last)
{
        int64_t startbyte = 0, endbyte = 0, i = 0;

        LL_FOREACH_F (bdata->lines, node) {
                if (i < first)
                        endbyte = (startbyte += node->data->slen + 1);
                else if (i < last + 1)
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
recover_compilation_unit(Buffer *bdata, bstring *buf)
{
        struct translationunit *stu = xmalloc(sizeof(struct translationunit));
        stu->tu  = CLD(bdata)->tu;
        stu->buf = buf;

        const int fd = open(CLD(bdata)->tmp_name, O_WRONLY|O_TRUNC, 0600);
        if (b_write(fd, stu->buf, B("\n")) != 0)
                err(1, "Write failed");
        close(fd);

        int ret = clang_reparseTranslationUnit(CLD(bdata)->tu, 0, NULL, TUFLAGS);
        if (ret != 0)
                errx(1, "libclang error: %d", ret);

        return stu;
}

static struct translationunit *
init_compilation_unit(Buffer *bdata, bstring *buf)
{
        if (!*libclang_tmp_path)
                get_tmp_path(libclang_tmp_path);

        int          tmpfd, tmplen;
        char         tmp[SAFE_PATH_MAX];
        str_vector  *comp_cmds = get_compile_commands(bdata);
#ifdef HAVE_MKOSTEMPS
        tmplen = snprintf(tmp, SAFE_PATH_MAX, "%s/XXXXXX%s", libclang_tmp_path, BS(bdata->name.base));
        tmpfd  = mkostemps(tmp, (int)bdata->name.base->slen, O_DSYNC);
#else
        bstring *tmp_file;
        tmpfd = _nvim_get_tmpfile(,&tmp_file, btp_fromcstr(bdata->name.suffix));
        memcpy(tmp, tmp_file->data, (tmplen = (int)tmp_file->slen) + 1);
        b_destroy(tmp_file);
#endif
        
        if (b_write(tmpfd, buf, B("\n")) != 0)
                err(1, "Write error");
        close(tmpfd);

#ifdef DEBUG
        argv_dump(stderr, comp_cmds);
#endif

        bdata->clangdata = xmalloc(sizeof(struct clangdata));
        CLD(bdata)->idx  = clang_createIndex(0, 0);
        CLD(bdata)->tu   = NULL;

        unsigned clerror = clang_parseTranslationUnit2(CLD(bdata)->idx, tmp, (const char **)comp_cmds->lst,
                                                       (int)comp_cmds->qty, NULL, 0, TUFLAGS, &CLD(bdata)->tu);
        if (!CLD(bdata)->tu || clerror != 0)
                errx(1, "libclang error: %d", clerror);

        struct translationunit *stu = xmalloc(sizeof(struct translationunit));
        stu->buf = buf;
        stu->tu  = CLD(bdata)->tu;

        /* Get all enumerators in the translation unit separately, because clang
         * doesn't expose them as such, only as normal integers (in C). */
        struct enum_data enumlist = {CLD(bdata)->tu, b_list_create(), buf};
        clang_visitChildren(clang_getTranslationUnitCursor(CLD(bdata)->tu), visit_continue, &enumlist);
        B_LIST_SORT_FAST(enumlist.enumerators);

        DUMPDATA();

        CLD(bdata)->enumerators = enumlist.enumerators;
        CLD(bdata)->argv        = comp_cmds;
        CLD(bdata)->info        = getinfo(bdata);
        memcpy(CLD(bdata)->tmp_name, tmp, (size_t)tmplen + UINTMAX_C(1));

        return stu;
}

/*======================================================================================*/

#ifdef DOSISH
static char *
stupid_windows_bullshit(const char *const path)
{
        const size_t len = strlen(path);
        if (len < 5 || path[4] != '/')
                return NULL;

        char *ret = xmalloc(len + 7);
        char *ptr = ret;
        *ptr++    = '-';
        *ptr++    = 'I';
        *ptr++    = path[3];
        *ptr++    = ':';
        *ptr++    = '\\';
        *ptr++    = '\\';
        for (const char *sptr = path+5; *sptr; ++sptr)
                *ptr++ = (*sptr == '/') ? '\\' : *sptr;
        *ptr      = '\0';

        return ret;
}

static void
handle_win32_command_script(Buffer *bdata, const char *cstr, str_vector *ret)
{
        char        ch;
        char        searchbuf[2048];
        const char *optr = cstr + 1;
        char       *sptr = searchbuf;
        *sptr++          = '.';
        *sptr++          = '*';

        while ((ch = *optr++))
                *sptr++ = (char)((ch == '\\') ? '/' : ch);
        *sptr = '\0';

        bstring *file = find_file(BS(bdata->topdir->pathname), searchbuf, FIND_FIRST);
        if (file) {
                bstring *contents = b_quickread("%s", BS(b_regularize_path(file)));
                assert(contents);
                eprintf("Read %s\n", BS(contents));
                char *dataptr = (char *)contents->data;

                while ((sptr = strsep(&dataptr, " \n\t"))) {
                        if (sptr[0] == '-') {
                                if (sptr[2] == '/')
                                        argv_append(ret, stupid_windows_bullshit(sptr), false);
                                else
                                        argv_append(ret, sptr, true);
                        }
                }

                b_free(contents);
                b_free(file);
        }
}
#endif

static str_vector *
get_compile_commands(Buffer *bdata)
{
        CXCompilationDatabase_Error cberr;
        CXCompilationDatabase       db =
                clang_CompilationDatabase_fromDirectory(BS(bdata->topdir->pathname), &cberr);
        if (cberr != 0) {
                clang_CompilationDatabase_dispose(db);
                warn("Couldn't locate compilation database in \"%s\".",
                     BS(bdata->topdir->pathname));
                return get_backup_commands(bdata);
        }

        CXCompileCommands cmds = get_clang_compile_commands_for_file(&db, bdata);
        if (!cmds) {
                clang_CompilationDatabase_dispose(db);
                return get_backup_commands(bdata);
        }

        const unsigned ncmds = clang_CompileCommands_getSize(cmds);
        str_vector    *ret   = argv_create(INIT_ARGV);

        for (size_t i = 0; i < ARRSIZ(gcc_sys_dirs); ++i)
                argv_append(ret, gcc_sys_dirs[i], false);
        argv_append(ret, "-ferror-limit=0", true);

        for (unsigned i = 0; i < ncmds; ++i) {
                CXCompileCommand command = clang_CompileCommands_getCommand(cmds, i);
                const unsigned   nargs   = clang_CompileCommand_getNumArgs(command);
                bool             fileok  = false;

                for (unsigned x = 0; x < nargs; ++x) {
                        bool        next_fileok = false;
                        CXString    tmp         = clang_CompileCommand_getArg(command, x);
                        const char *cstr        = CS(tmp);

                        if (strcmp(cstr, "-o") == 0)
                                ++x;
                        else if (cstr[0] == '-') {
                                if (P44_STREQ_ANY(cstr+1, "I", "isystem", "include", "x"))
                                        next_fileok = true;
#ifdef DOSISH
                                if (cstr[2] == '/') {
                                        char *fixed_path = stupid_windows_bullshit(cstr);
                                        if (fixed_path)
                                                argv_append(ret, fixed_path, false);
                                } else
#endif
                                if (cstr[1] != 'f' && cstr[1] != 'c' && cstr[1] != 'W')
                                        argv_append(ret, cstr, true);

#ifdef DOSISH
                        } else if (cstr[0] == '@') {
                                handle_win32_command_script(bdata, cstr, ret);
#endif
                        } else if (fileok)
                                argv_append(ret, cstr, true);
                                
                        clang_disposeString(tmp);
                        fileok = next_fileok;
                }
        }

        argv_fmt(ret, "-I%s", BS(bdata->name.path));
        argv_append(ret, "-stdlib=libstdc++", true);

        clang_CompileCommands_dispose(cmds);
        clang_CompilationDatabase_dispose(db);
        return ret;
}

static str_vector *
get_backup_commands(Buffer *bdata)
{
        str_vector *ret = argv_create(INIT_ARGV);
        for (size_t i = 0; i < ARRSIZ(gcc_sys_dirs); ++i)
                argv_append(ret, gcc_sys_dirs[i], false);
        argv_fmt(ret, "-I%s", BS(bdata->name.path));
        argv_fmt(ret, "-I%s", BS(bdata->topdir->pathname));
        argv_append(ret, "-stdlib=libstdc++", true);

        return ret;
}

static CXCompileCommands
get_clang_compile_commands_for_file(CXCompilationDatabase *db, Buffer *bdata)
{
        CXCompileCommands comp = clang_CompilationDatabase_getCompileCommands(*db, BS(bdata->name.full));
        const unsigned    num  = clang_CompileCommands_getSize(comp);
        ECHO("num is %u\n", num);

        if (num == 0) {
                ECHO("Looking for backup files...");
                clang_CompileCommands_dispose(comp);
                bstring *file = find_file(BS(bdata->name.path), (bdata->ft->id == FT_C)
                                          ? ".*\\.c$" : ".*\\.(cpp|cc|cxx|c++)$", FIND_FIRST);
                if (file) {
                        ECHO("Found %s!\n", file);
                        comp = clang_CompilationDatabase_getCompileCommands(*db, BS(file));
                        b_free(file);
                } else {
                        ECHO("Found nothing.\n");
                        comp = NULL;
                }
        }
        return comp;
}

/*======================================================================================*/

static void
destroy_struct_translationunit(struct translationunit *stu)
{
        if (stu->cxtokens && stu->num)
                clang_disposeTokens(stu->tu, stu->cxtokens, stu->num);
        genlist_destroy(stu->tokens);
        b_free(stu->buf);
        xfree(stu->cxcursors);
        xfree(stu);
}

void
destroy_clangdata(Buffer *bdata)
{
        struct clangdata *cdata = bdata->clangdata;
        if (!cdata)
                return;
        b_list_destroy(cdata->enumerators);

        for (unsigned i = ARRSIZ(gcc_sys_dirs); i < cdata->argv->qty; ++i)
                xfree(cdata->argv->lst[i]);
        xfree(cdata->argv->lst);
        xfree(cdata->argv);

        if (cdata->info) {
                for (unsigned i = 0, e = cdata->info[0].num; i < e; ++i)
                        b_free(cdata->info[i].group);
                xfree(cdata->info);
        }

        clang_disposeTranslationUnit(cdata->tu);
        clang_disposeIndex(cdata->idx);
        xfree(cdata);
        bdata->clangdata = NULL;
}

/*======================================================================================*/

static void
tagfinder(struct enum_data *data, CXCursor cursor)
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

static struct token *
get_token_data(CXTranslationUnit *tu, CXToken *tok, CXCursor *cursor)
{
        struct token         *ret;
        struct resolved_range res;
        CXTokenKind tokkind = clang_getTokenKind(*tok);

        if (tokkind != CXToken_Identifier ||
            !resolve_range(clang_getTokenExtent(*tu, *tok), &res))
                return NULL;

        CXString dispname = clang_getCursorDisplayName(*cursor);
        size_t   len      = strlen(CS(dispname)) + UINTMAX_C(1);
        ret               = xmalloc(offsetof(struct token, raw) + len);
        ret->token        = *tok;
        ret->cursor       = *cursor;
        ret->cursortype   = clang_getCursorType(*cursor);
        ret->tokenkind    = tokkind;
        ret->line         = res.line  - 1;
        ret->col1         = res.start - 1;
        ret->col2         = res.end   - 1;
        ret->len          = res.end   - res.start;

        memcpy(ret->raw, CS(dispname), len);
        ret->text.data  = (unsigned char *)ret->raw;
        ret->text.slen  = len-1;
        ret->text.mlen  = len;
        ret->text.flags = 0;

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
            clang_getLocationForOffset(stu->tu, *file, (unsigned)first),
            clang_getLocationForOffset(stu->tu, *file, (unsigned)last)
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
