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
         | CXTranslationUnit_Incomplete                  \
         | CXTranslationUnit_CreatePreambleOnFirstParse  \
         | CXTranslationUnit_IncludeAttributedTypes )
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

#if 0
struct enum_data {
        CXTranslationUnit tu;
        b_list           *enumerators;
        const bstring    *buf;
};
#endif

static void                    destroy_struct_translationunit(struct translationunit *stu);
static CXCompileCommands       get_clang_compile_commands_for_file(CXCompilationDatabase *db, Buffer *bdata);
static str_vector             *get_backup_commands(Buffer *bdata);
static str_vector             *get_compile_commands(Buffer *bdata);
static struct token           *get_token_data(CXTranslationUnit *tu, CXToken *tok, CXCursor *cursor);
static struct translationunit *init_compilation_unit(Buffer *bdata, bstring *buf);
static struct translationunit *recover_compilation_unit(Buffer *bdata, bstring *buf);
static inline void             lines2bytes(Buffer *bdata, int64_t *startend, int first, int last);
static void                    tokenize_range(struct translationunit *stu, CXFile *file, int64_t first, int64_t last);
/* static void                    tagfinder(strucgt enum_data *data, CXCursor cursor); */
/* static enum CXChildVisitResult visit_continue(CXCursor cursor, CXCursor parent, void *client_data); */

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
        static pthread_mutex_t lc_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        int64_t   startend[2];
        uint32_t  cnt_val = p99_count_inc(&bdata->lock.num_workers);
        bstring  *joined  = NULL;

        if (cnt_val >= 2) {
                p99_count_dec(&bdata->lock.num_workers);
                return;
        }

        /* pthread_mutex_lock(&bdata->lock.update); */
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
        mpack_arg_array *calls = create_nvim_calls(bdata, stu);
        nvim_call_atomic(calls);

        mpack_destroy_arg_array(calls);
        destroy_struct_translationunit(stu);
        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&lc_mutex);
        /* pthread_mutex_lock(&bdata->lock.update); */
}

void *
libclang_threaded_highlight(void *vdata)
{
        libclang_highlight((Buffer *)vdata, 0, -1, HIGHLIGHT_NORMAL);
        pthread_exit();
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
        struct translationunit *stu = malloc(sizeof(struct translationunit));
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
        tmpfd = _nvim_get_tmpfile(&tmp_file, btp_fromcstr(bdata->name.suffix));
        memcpy(tmp, tmp_file->data, (tmplen = (int)tmp_file->slen) + 1);
        b_destroy(tmp_file);
#endif
        
        if (b_write(tmpfd, buf, B("\n")) != 0)
                err(1, "Write error");
        close(tmpfd);

#ifdef DEBUG
        argv_dump(stderr, comp_cmds);
#endif

        bdata->clangdata = malloc(sizeof(struct clangdata));
        CLD(bdata)->idx  = clang_createIndex(0, 0);
        CLD(bdata)->tu   = NULL;

        unsigned clerror = clang_parseTranslationUnit2(CLD(bdata)->idx, tmp, (const char **)comp_cmds->lst,
                                                       (int)comp_cmds->qty, NULL, 0, TUFLAGS, &CLD(bdata)->tu);
        if (!CLD(bdata)->tu || clerror != 0)
                errx(1, "libclang error: %d", clerror);

        struct translationunit *stu = malloc(sizeof(struct translationunit));
        stu->buf = buf;
        stu->tu  = CLD(bdata)->tu;

#if 0
        /* Get all enumerators in the translation unit separately, because clang
         * doesn't expose them as such, only as normal integers (in C). */
        struct enum_data enumlist = {CLD(bdata)->tu, b_list_create(), buf};
        clang_visitChildren(clang_getTranslationUnitCursor(CLD(bdata)->tu), visit_continue, &enumlist);
        B_LIST_SORT_FAST(enumlist.enumerators);

        DUMPDATA();

        CLD(bdata)->enumerators = enumlist.enumerators;
#endif

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
        if (len < 3 || !isalpha(path[1]) || path[2] != '/')
                return NULL;

        char *ret = malloc(len + 3);
        char *ptr = ret;
        *ptr++    = path[1];
        *ptr++    = ':';
        *ptr++    = '\\';
        for (const char *sptr = path+3; *sptr; ++sptr)
                *ptr++ = (*sptr == '/') ? '\\' : *sptr;
        *ptr = '\0';

        return ret;
}

static void
unquote(bstring *str)
{
        uint8_t  buf[str->slen + 1];
        unsigned x = 0;

        for (unsigned i = 0; i < str->slen; ++i)
                if (str->data[i] != '"' && str->data[i] != '\'')
                        buf[x++] = str->data[i];

        if (x != str->slen) {
                memcpy(str->data, buf, x);
                str->data[x] = '\0';
                str->slen    = x;
        }
}

static void
handle_win32_command_script(Buffer *bdata, const char *cstr, str_vector *ret)
{
        char        ch;
        char        searchbuf[8192];
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
                char *tok = strstr((char *)contents->data, "-I");

                eprintf("Found %s\n", tok);

                while (tok) {
                        size_t len;
                        char  *next = strstr(tok+2, "-I");

                        if (next) {
                                *(next-1) = '\0';
                                len       = PSUB(next-1, tok);
                        } else {
                                next = strchrnul(tok+2, '\n');
                                if (*(next-1) == '\r')
                                        --next;
                                *next = '\0';
                                len   = PSUB(next, tok);
                        }

                        unquote((bstring[]){{len, 0, (uchar *)tok, 0}});
                        argv_append(ret, "-I", true);
                        tok += 2;
                        char *abspath;
                        char buf[PATH_MAX];

                        if (tok[0] == '/') {
                                char *tmp = stupid_windows_bullshit(tok);
                                abspath = realpath(tmp, buf);
                                free(tmp);
                        } else {
                                abspath = realpath(tok, buf);
                        }

                        eprintf("Path is %s\n", abspath);
                        argv_append(ret, abspath, true);
                        tok = (*next) ? next : NULL;
                }

                b_destroy(contents);
                b_destroy(file);
        }
}

#  define ARGV_APPEND_FILE(VAR, STR)                                     \
        do {                                                             \
                if ((STR)[0] == '/') {                                   \
                        char *fixed_path = stupid_windows_bullshit(STR); \
                        if (fixed_path)                                  \
                                argv_append((VAR), fixed_path, false);   \
                } else {                                                 \
                        argv_append((VAR), (STR), true);                 \
                }                                                        \
        } while (0)

static inline void
handle_include_compile_command(str_vector *lst, const char *cstr, CXString directory)
{
        bool  do_fix_path = cstr[0] == '/';
        char *fixed_path  = (do_fix_path) ? fixed_path = stupid_windows_bullshit(STR)
                                          : fixed_path = cstr;

        if (isalpha(fixed_path[0]) && fixed_path[1] == ':') {
                argv_append(lst, fixed_path, do_fix_path);
        } else {
                char *buf = NULL;
                asprintf(&buf, "%s\\%s", CS(directory), fixed_path);
                argv_append(lst, buf, false);
                if (do_fix_path)
                        free(do_fix_path);
        }
}

#else /* ! defined DOSISH */

#  define ARGV_APPEND_FILE(VAR, STR) \
        argv_append((VAR), (STR), true)

static inline void
handle_include_compile_command(str_vector *lst, const char *cstr, CXString directory)
{
        if (cstr[0] == '/') {
                argv_append(lst, cstr, true);
        } else {
                char *buf = NULL;
                asprintf(&buf, "%s/%s", CS(directory), cstr);
                argv_append(lst, buf, false);
        }
}
#endif

/*======================================================================================*/

enum allow_next_arg { NE_NORMAL, NE_DISALLOW, NE_FILE_ALLOW, NE_LANG_ALLOW };

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
                warnx("Using backup commands.\n");
                clang_CompilationDatabase_dispose(db);
                return get_backup_commands(bdata);
        }

        const unsigned ncmds = clang_CompileCommands_getSize(cmds);
        str_vector    *ret   = argv_create(INIT_ARGV);

        for (size_t i = 0; i < ARRSIZ(gcc_sys_dirs); ++i)
                argv_append(ret, gcc_sys_dirs[i], false);

        /* If we don't remove clang's max error limit then it will crash if it
         * reaches it. This happens fairly often when editing a file. */
        argv_append(ret, "-ferror-limit=0", true);

        extern FILE *clang_log_file;

        for (unsigned i = 0; i < ncmds; ++i) {
                CXCompileCommand command   = clang_CompileCommands_getCommand(cmds, i);
                CXString         directory = clang_CompileCommand_getDirectory(command);
                const unsigned   nargs     = clang_CompileCommand_getNumArgs(command);
                int              arg_allow = NE_NORMAL;

                for (unsigned x = 0; x < nargs; ++x) {
                        int next_arg_allow = NE_NORMAL;
                        CXString    tmp  = clang_CompileCommand_getArg(command, x);
                        const char *cstr = CS(tmp);

                        if (arg_allow != NE_NORMAL) {
                                switch (arg_allow) {
                                case NE_FILE_ALLOW:
                                        handle_include_compile_command(ret, cstr+2, directory);
                                        break;
                                case NE_LANG_ALLOW:
                                        argv_append(ret, cstr, true);
                                        break;
                                case NE_DISALLOW:
                                default:
                                        break;
                                }
                        } else if (cstr[0] == '-' && cstr[1]) {
                                if (P44_STREQ_ANY(cstr+1, "I", "isystem", "include")) {
                                        argv_append(ret, cstr, true);
                                        next_arg_allow = NE_FILE_ALLOW;
                                } else if (strcmp(cstr+1, "x") == 0) {
                                        argv_append(ret, cstr, true);
                                        next_arg_allow = NE_LANG_ALLOW;
                                } else if (strcmp(cstr+1, "o") == 0) {
                                        next_arg_allow = NE_DISALLOW;
                                } else if (P44_STREQ_ANY(cstr+1, "MMD", "MP", "MD", "MT", "MF")) {
                                        /* Nothing */
                                } else {
                                        switch (cstr[1]) {
                                        case 'f': case 'c': case 'W': case 'o':
                                                break;
                                        case 'I':
                                                argv_append(ret, "-I", true);
                                                handle_include_compile_command(ret, cstr+2, directory);
                                                break;
                                        default:
                                                argv_append(ret, cstr, true);
                                        }
                                }
#ifdef DOSISH
                        } else if (cstr[0] == '@') {
                                handle_win32_command_script(bdata, cstr, ret);
#endif
                        }
                                
                        clang_disposeString(tmp);
                        arg_allow = next_arg_allow;
                }

                clang_disposeString(directory);
        }

        argv_append(ret, "-I", true);
        argv_append(ret, BS(bdata->name.path), true);

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
        argv_append(ret, "-I", true);
        argv_append(ret, BS(bdata->name.path), true);
        argv_append(ret, "-I", true);
        argv_append(ret, BS(bdata->topdir->pathname), true);
        argv_append(ret, "-stdlib=libstdc++", true);

        return ret;
}

static CXCompileCommands
get_clang_compile_commands_for_file(CXCompilationDatabase *db, Buffer *bdata)
{
        bstring *newnam = b_strcpy(bdata->name.full);
        b_regularize_path(newnam);
        CXCompileCommands comp = clang_CompilationDatabase_getCompileCommands(*db, BS(newnam));
        const unsigned    num  = clang_CompileCommands_getSize(comp);
        b_destroy(newnam);

        if (num == 0) {
                ECHO("Looking for backup files...");
                clang_CompileCommands_dispose(comp);
                bstring *file = find_file(BS(bdata->name.path), (bdata->ft->id == FT_C)
                                          ? ".*\\.c$" : ".*\\.(cpp|cc|cxx|c++)$", FIND_FIRST);
                if (file) {
                        ECHO("Found %s!\n", file);
                        comp = clang_CompilationDatabase_getCompileCommands(*db, BS(file));
                        b_destroy(file);
                } else {
                        ECHO("Found nothing.\n");
                        comp = NULL;
                }
        }
        return comp;
}

CXCompilationDatabase
find_compilation_database(Buffer *bdata)
{
        CXCompilationDatabase_Error cberr;
        CXCompilationDatabase       db =
                clang_CompilationDatabase_fromDirectory(BS(bdata->topdir->pathname), &cberr);
        if (cberr != 0) {
                clang_CompilationDatabase_dispose(db);
                warn("Couldn't locate compilation database in \"%s\".",
                     BS(bdata->topdir->pathname));
                return NULL;
        }
        ECHO("Found db at '%s'\n", bdata->topdir->pathname);
        return db;
}

/*======================================================================================*/

static void
destroy_struct_translationunit(struct translationunit *stu)
{
        if (stu->cxtokens && stu->num)
                clang_disposeTokens(stu->tu, stu->cxtokens, stu->num);
        genlist_destroy(stu->tokens);
        b_destroy(stu->buf);
        free(stu->cxcursors);
        free(stu);
}

void
destroy_clangdata(Buffer *bdata)
{
        struct clangdata *cdata = bdata->clangdata;
        if (!cdata)
                return;

        for (unsigned i = ARRSIZ(gcc_sys_dirs); i < cdata->argv->qty; ++i)
                free(cdata->argv->lst[i]);
        free(cdata->argv->lst);
        free(cdata->argv);

        if (cdata->info) {
                for (unsigned i = 0, e = cdata->info[0].num; i < e; ++i)
                        b_destroy(cdata->info[i].group);
                free(cdata->info);
        }

        clang_disposeTranslationUnit(cdata->tu);
        clang_disposeIndex(cdata->idx);
        free(cdata);
        bdata->clangdata = NULL;
}

/*======================================================================================*/

#if 0
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
#endif

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

        /* CXString dispname = clang_getCursorDisplayName(*cursor); */
        CXString dispname = clang_getCursorSpelling(*cursor);
        size_t   len      = strlen(CS(dispname)) + UINTMAX_C(1);
        ret               = malloc(offsetof(struct token, raw) + len);
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
