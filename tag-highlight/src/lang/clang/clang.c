#include "Common.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "lang/clang/intern.h"
#include "util/find.h"
#include "clang-c/Index.h"
#include <sys/stat.h>

/* 
 * Here follows some of the sort of code that took so long to get working and
 * was written so haphazardly that one just doesn't ever want to look at ever
 * again, let alone document it, let alone *refactor* it. Ugh.
 */

#define TUFLAGS                                          \
        (  CXTranslationUnit_DetailedPreprocessingRecord \
         | CXTranslationUnit_Incomplete                  \
         | CXTranslationUnit_PrecompiledPreamble         \
         | CXTranslationUnit_CreatePreambleOnFirstParse  \
         | CXTranslationUnit_KeepGoing                   \
      /* | CXTranslationUnit_IncludeAttributedTypes */   \
      /* | CXTranslationUnit_VisitImplicitAttributes */  \
         | CXTranslationUnit_SkipFunctionBodies          \
         | CXTranslationUnit_LimitSkipFunctionBodiesToPreamble \
        )

#define INIT_ARGV (32)
#define CTX       clang_talloc_ctx_

#if 0
#ifndef TIME_CLANG
#  undef TIMER_START
#  undef TIMER_REPORT_RESTART
#  undef TIMER_REPORT
#  define TIMER_START(...)
#  define TIMER_REPORT_RESTART(...)
#  define TIMER_REPORT(...)
#endif
#endif

static const char *const gcc_sys_dirs[] = {GCC_ALL_INCLUDE_DIRECTORIES};
static pthread_mutex_t   lc_mutex       = PTHREAD_MUTEX_INITIALIZER;

char  libclang_tmp_path[PATH_MAX + 1];
void *clang_talloc_ctx_ = NULL;

static translationunit_t *init_compilation_unit(Buffer *bdata, bstring *buf);
static translationunit_t *recover_compilation_unit(Buffer *bdata, bstring *buf);
static CXCompileCommands  get_clang_compile_commands_for_file(CXCompilationDatabase *db, Buffer *bdata);

static int          destroy_struct_translationunit(translationunit_t *stu);
static int          do_destroy_clangdata(clangdata_t *cdata);
static str_vector  *get_backup_commands(Buffer *bdata);
static str_vector  *get_compile_commands(Buffer *bdata);
static token_t     *get_token_data(translationunit_t *stu, CXToken *tok, CXCursor *cursor);
static void         tokenize_range(translationunit_t *stu, CXFile *file, int64_t first, int64_t last);
static inline void  lines2bytes(Buffer *bdata, int64_t *startend, int first, int last);

__attribute__((__constructor__))
static void clang_initializer(void)
{
        pthread_mutex_init(&lc_mutex);
}

/*======================================================================================*/

void
(libclang_highlight)(Buffer *bdata, int const first, int const last, int const type)
{
        if (!bdata || !atomic_load(&bdata->initialized))
                return;
        assert(P99_EQ_ANY(bdata->ft->id, FT_C, FT_CXX));

        mpack_arg_array   *calls;
        translationunit_t *stu;
        int64_t   startend[2];
        bstring  *joined  = NULL;

        uint32_t cnt_val = p99_count_inc(&bdata->lock.num_workers);
        if (cnt_val > 3) {
                p99_count_dec(&bdata->lock.num_workers);
                return;
        }

        pthread_mutex_lock(&bdata->lock.lang_mtx);

#if 0
        struct timer tm;
        TIMER_START(&tm);
#endif

        pthread_mutex_lock(&bdata->lock.total);
        joined = ll_join_bstrings(bdata->lines, '\n');
        pthread_mutex_unlock(&bdata->lock.total);
        
        if (last == (-1)) {
                startend[0] = 0;
                startend[1] = joined->slen;
        } else {
                lines2bytes(bdata, startend, first, last);
        }

        if (type == HIGHLIGHT_REDO) {
                if (bdata->clangdata)
                        destroy_clangdata(bdata);
                stu = init_compilation_unit(bdata, joined);
        } else {
                stu = (bdata->clangdata) ? recover_compilation_unit(bdata, joined)
                                         : init_compilation_unit(bdata, joined);
        }

        CLD(bdata)->mainfile = clang_getFile(CLD(bdata)->tu, BS(bdata->name.full));
        tokenize_range(stu, &CLD(bdata)->mainfile, startend[0], startend[1]);

        calls = create_nvim_calls(bdata, stu);
        nvim_call_atomic(calls);
        talloc_free(calls);
        talloc_free(stu);

#if 0
        TIMER_REPORT(&tm, "clang parse");
#endif

        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&bdata->lock.lang_mtx);
}

void *
highlight_c_pthread_wrapper(void *vdata)
{
        Buffer *bdata = vdata;
        libclang_highlight(bdata, 0, -1, HIGHLIGHT_NORMAL);
        pthread_exit();
}

static inline void
lines2bytes(Buffer *bdata, int64_t *startend, int const first, int const last)
{
        int64_t startbyte = 0;
        int64_t endbyte   = 0;
        int64_t i         = 0;

        LL_FOREACH_F (bdata->lines, node) {
                bstring *line = node->data;
                if (i < first)
                        endbyte = (startbyte += line->slen + 1);
                else if (i < last + 1)
                        endbyte += line->slen + 1;
                else
                        break;
                ++i;
        }

        startend[0] = startbyte;
        startend[1] = endbyte;
}

/*======================================================================================*/

static noreturn void
handle_libclang_error(Buffer *bdata, unsigned const err)
{
        extern void exit_cleanup(void);

        shout("Libclang error (%u). Unfortunately this is fatal at the moment.", err);
        /* longjmp(bdata->jbuf, 1); */
        /* quick_exit(1); */
        exit(1);
}

static translationunit_t *
recover_compilation_unit(Buffer *bdata, bstring *buf)
{
        b_regularize_path_sep(bdata->name.full, '\\');
        struct CXUnsavedFile unsaved = {.Filename = BS(bdata->name.full),
                                        .Contents = BS(buf), .Length = buf->slen};

        int ret = clang_reparseTranslationUnit(CLD(bdata)->tu, 1U, &unsaved,
                                               clang_defaultReparseOptions(CLD(bdata)->tu));
        if (ret != 0)
                handle_libclang_error(bdata, ret);

        translationunit_t *stu = talloc(CTX, translationunit_t);
        stu->tu  = CLD(bdata)->tu;
        stu->buf = talloc_move(stu, &buf);
        stu->ftid = bdata->ft->id;
        talloc_set_destructor(stu, destroy_struct_translationunit);

        return stu;
}

static translationunit_t *
init_compilation_unit(Buffer *bdata, bstring *buf)
{
        str_vector *comp_cmds = get_compile_commands(bdata);
        b_regularize_path_sep(bdata->name.full, '\\');
        struct CXUnsavedFile unsaved = {.Filename = BS(bdata->name.full),
                                        .Contents = BS(buf), .Length = buf->slen};

        /* argv_dump(stderr, comp_cmds); */

        clangdata_t *cld = talloc_zero(bdata, clangdata_t);
        bdata->clangdata = cld;
        cld->bdata       = bdata;
        cld->idx         = clang_createIndex(0, 0);
        cld->argv        = comp_cmds;

        talloc_set_destructor(cld, do_destroy_clangdata);

        enum CXErrorCode clerror = clang_parseTranslationUnit2(
            cld->idx, BS(bdata->name.full), (char const **)comp_cmds->lst,
            (int)comp_cmds->qty, &unsaved, 1, TUFLAGS, &cld->tu
        );

        if (!cld->tu || clerror != 0)
                handle_libclang_error(bdata, clerror);

        translationunit_t *stu = talloc(CTX, translationunit_t);
        stu->buf  = talloc_move(stu, &buf);
        stu->tu   = cld->tu;
        stu->idx  = cld->idx;
        stu->ftid = bdata->ft->id;
        talloc_set_destructor(stu, destroy_struct_translationunit);

        return stu;
}

/*======================================================================================*/

#ifdef DOSISH

static char *
stupid_windows_bullshit(char const *const path)
{
        size_t const len = strlen(path);
        if (len < 3 || !isalpha(path[1]) || path[2] != '/')
                return NULL;

        char *ret = malloc(len + 3);
        char *ptr = ret;
        *ptr++    = path[1];
        *ptr++    = ':';
        *ptr++    = '\\';
        for (char const *sptr = path+3; *sptr; ++sptr)
                *ptr++ = (*sptr == '/') ? '\\' : *sptr;
        *ptr = '\0';

        return ret;
}

static inline void fixup_path_sep(char *orig)
{
        for (char *ptr = orig; *ptr; ++ptr)
                if (*ptr == '/')
                        *ptr = '\\';
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
handle_win32_command_script(Buffer *bdata, char const *directory, char const *cstr, str_vector *ret)
{
        char searchbuf[8192];
        {
                char        ch;
                char       *sptr = searchbuf;
                char const *optr = cstr + 1;

                *sptr++ = '.';
                *sptr++ = '*';

                while ((ch = *optr++))
                        *sptr++ = (char)((ch == '\\') ? '/' : ch);
                *sptr = '\0';
        }

        /* bstring *file = find_file(BS(bdata->topdir->pathname), searchbuf, FIND_FIRST); */
        struct stat st;
        bstring *file = b_fromcstr(directory);
        b_catchar(file, '/');
        b_catcstr(file, searchbuf);

        if (stat(BS(file), &st) == 0) {
                bstring *contents = b_quickread("%s", BS(file));
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

                        unquote((bstring[]){{.data = (uchar *)tok, .slen = len, .mlen = 0,  .flags = 0}});
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

static inline void
handle_include_compile_command(str_vector *lst, char const *s, CXString directory, bool is_i)
{
        if (isalpha(s[0]) && s[1] == ':' && (s[2] == '/' || s[2] == '\\')) {
                if (is_i)
                        argv_append(lst, talloc_asprintf(NULL, "-I%s", s), false);
                else
                        argv_append(lst, s, true);
        } else if (s[0] == '/') {
                if (is_i) {
                        char *tmp = stupid_windows_bullshit(s);
                        argv_append(lst, talloc_asprintf(NULL, "-I%s", tmp), false);
                        free(tmp);
                } else {
                        argv_append(lst, stupid_windows_bullshit(s), false);
                }
        } else {
                if (is_i)
                        argv_append(lst, talloc_asprintf(NULL, "-I%s\\%s", CS(directory), s), false);
                else
                        argv_append(lst, talloc_asprintf(NULL, "%s\\%s", CS(directory), s), false);
        }
        fixup_path_sep(lst->lst[lst->qty - 1]);
}

#else /* ! defined DOSISH */

static inline void
handle_include_compile_command(str_vector *lst, char const *cstr, CXString directory, bool is_i)
{
        if (cstr[0] == '/') {
                if (is_i)
                        argv_append(lst, talloc_asprintf(NULL, "-I%s", cstr), false);
                else
                        argv_append(lst, cstr, true);
        } else {
                if (is_i)
                        argv_append(lst, talloc_asprintf(NULL, "-I%s/%s", CS(directory), cstr), false);
                else
                        argv_append(lst, talloc_asprintf(NULL, "%s/%s", CS(directory), cstr), false);
        }
}

#endif /* defined DOSISH */

/*======================================================================================*/

enum allow_next_arg { NE_NORMAL, NE_DISALLOW, NE_LANG_ALLOW, NE_FILE_ALLOW, NE_FILE_ALLOW_I, };

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

        unsigned const ncmds = clang_CompileCommands_getSize(cmds);
        str_vector    *ret   = argv_create(INIT_ARGV);

        ECHO("Found compilation database for %s -> %u commands\n", bdata->topdir->pathname, ncmds);

        for (unsigned i = 0; i < ncmds; ++i) {
                CXCompileCommand command   = clang_CompileCommands_getCommand(cmds, i);
                CXString         directory = clang_CompileCommand_getDirectory(command);
                unsigned const   nargs     = clang_CompileCommand_getNumArgs(command);
                int              arg_allow = NE_NORMAL;

                for (unsigned x = 0; x < nargs; ++x) {
                        int next_arg_allow = NE_NORMAL;
                        CXString    tmp  = clang_CompileCommand_getArg(command, x);
                        char const *cstr = CS(tmp);

                        if (STREQ(cstr, "-Xclang")) {
                                /* Really annoying. Just pretend this one never happened. */
                                next_arg_allow = arg_allow;
                        } else if (arg_allow != NE_NORMAL) {
                                switch (arg_allow) {
                                case NE_FILE_ALLOW:
                                        handle_include_compile_command(ret, cstr, directory, false);
                                        break;
                                case NE_FILE_ALLOW_I:
                                        handle_include_compile_command(ret, cstr, directory, true);
                                        break;
                                case NE_LANG_ALLOW:
                                        argv_append(ret, cstr, true);
                                        break;
                                case NE_DISALLOW:
                                default:
                                        break;
                                }
                        } else if (cstr[0] == '-' && cstr[1]) {
                                if (STREQ(cstr+1, "I")) {
                                        next_arg_allow = NE_FILE_ALLOW_I;
                                } else if (P99_STREQ_ANY(cstr+1, "isystem", "include", "include-pch")) {
                                        argv_append(ret, cstr, true);
                                        next_arg_allow = NE_FILE_ALLOW;
                                } else if (STREQ(cstr+1, "x")) {
                                        argv_append(ret, cstr, true);
                                        next_arg_allow = NE_LANG_ALLOW;
                                } else if (P99_STREQ_ANY(cstr+1, "o", "c")) {
                                        next_arg_allow = NE_DISALLOW;
                                } else if (P99_STREQ_ANY(cstr+1, "MMD", "MP", "MD", "MT", "MF")
#if 0
                                           || P99_STRNEQL_ANY(cstr+1, "-driver-mode", "march")
#endif
                                           )
                                {
                                        /* Nothing */
                                } else {
                                        switch (cstr[1]) {
                                        case 'W':
                                                break;
#if 0
                                        case 'f':
                                        case 'c':
                                        case 'o':
#endif
                                        case 'I':
                                                handle_include_compile_command(ret, cstr+2, directory, true);
                                                break;
                                        default:
                                                argv_append(ret, cstr, true);
                                        }
                                }
#ifdef DOSISH
                        } else if (cstr[0] == '@') {
                                handle_win32_command_script(bdata, CS(directory), cstr+1, ret);
#endif
                        }

                        clang_disposeString(tmp);
                        arg_allow = next_arg_allow;
                }

                clang_disposeString(directory);
        }

        for (size_t i = 0; i < ARRSIZ(gcc_sys_dirs); ++i)
                argv_append(ret, gcc_sys_dirs[i], true);

        /* If we don't remove clang's max error limit then it will crash if it
         * reaches it. This happens fairly often when editing a file. */
        argv_append(ret, "-ferror-limit=0", true);

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
                argv_append(ret, gcc_sys_dirs[i], true);
        argv_append(ret, "-I.", true);
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
        bstring          *newnam = b_strcpy(bdata->name.full);
        CXCompileCommands comp   = clang_CompilationDatabase_getCompileCommands(*db, BS(newnam));
        unsigned const    num    = clang_CompileCommands_getSize(comp);
        b_destroy(newnam);

        if (num == 0) {
                ECHO("Looking for backup files...");
                clang_CompileCommands_dispose(comp);

                bstring *file = find_file(BS(bdata->name.path),
                                          (bdata->ft->id == FT_C) ? ".*\\.c$"
                                                                  : ".*\\.(cpp|cc|cxx|c++)$",
                                          FIND_FIRST);

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

static int
destroy_struct_translationunit(translationunit_t *stu)
{
        if (stu->cxtokens && stu->num)
                clang_disposeTokens(stu->tu, stu->cxtokens, stu->num);

        genlist_destroy(stu->tokens);
        talloc_free(stu->buf);
        talloc_free(stu->cxcursors);
        talloc_free(stu);
        return 0;
}

static int
do_destroy_clangdata(clangdata_t *cdata)
{
        if (cdata->argv)
                TALLOC_FREE(cdata->argv);

        clang_disposeTranslationUnit(cdata->tu);
        clang_disposeIndex(cdata->idx);

        return 0;
}

void
destroy_clangdata(Buffer *bdata)
{
        clangdata_t *cdata = bdata->clangdata;
        if (!cdata)
                return;
        do_destroy_clangdata(cdata);
        bdata->clangdata = NULL;
}

/*======================================================================================*/

static bool
punctuation_sanity_check(CXCursor *cursor)
{
        CXString spell = clang_getCursorSpelling(*cursor);
        bool     ret   = (strncmp(CS(spell), "operator", 8) == 0);
        clang_disposeString(spell);
        return ret;
}

static token_t *
get_token_data(translationunit_t *stu, CXToken *tok, CXCursor *cursor)
{
        token_t          *ret;
        resolved_range_t  res;
        CXTokenKind       tokkind = clang_getTokenKind(*tok);

        if (!(tokkind == CXToken_Identifier ||
              (tokkind == CXToken_Punctuation && stu->ftid == FT_CXX &&
               punctuation_sanity_check(cursor))) ||
            !resolve_range(clang_getTokenExtent(stu->tu, *tok), &res))
        {
                return NULL;
        }

        ret             = talloc_size(CTX, offsetof(token_t, raw) + res.len + 1LLU);
        ret->token      = *tok;
        ret->cursor     = *cursor;
        ret->cursortype = clang_getCursorType(*cursor);
        ret->tokenkind  = tokkind;
        ret->line       = res.line  - 1;
        ret->col1       = res.start - 1;
        ret->col2       = res.end   - 1;
        ret->offset     = res.offset1;
        ret->len        = res.len;

        memcpy(ret->raw, stu->buf->data + res.offset1, res.len);

        ret->raw[res.len] = '\0';
        ret->text.data    = (unsigned char *)ret->raw;
        ret->text.slen    = res.len;
        ret->text.mlen    = res.len + 1;
        ret->text.flags   = 0;

        return ret;
}

static void
tokenize_range(translationunit_t *stu,
               CXFile            *file,
               int64_t const      first,
               int64_t const      last)
{
        token_t       *t;
        CXToken       *toks = NULL;
        unsigned       num  = 0;
        CXSourceRange  rng  = clang_getRange(
            clang_getLocationForOffset(stu->tu, *file, (unsigned)first),
            clang_getLocationForOffset(stu->tu, *file, (unsigned)last)
        );

        clang_tokenize(stu->tu, rng, &toks, &num);
        CXCursor *cursors = talloc_array(stu, CXCursor, num);
        clang_annotateTokens(stu->tu, toks, num, cursors);

        stu->cxtokens  = toks;
        stu->cxcursors = cursors;
        stu->num       = num;
        stu->tokens    = genlist_create_alloc(stu, num / 2);

        for (unsigned i = 0; i < num; ++i)
                if ((t = get_token_data(stu, &toks[i], &cursors[i])))
                        genlist_append(stu->tokens, t);
}
