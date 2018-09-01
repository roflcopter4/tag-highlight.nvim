#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <math.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "top.h"

#define CS(CXSTR)   (clang_getCString(CXSTR))
#define ARRSIZ(ARR) (sizeof(ARR) / sizeof((ARR)[0]))
#define MKARGV(...) \
        (ARRSIZ(((const char *[]){__VA_ARGS__}))), ((const char *[]){__VA_ARGS__})

#define PARSEFILE(IDX, FNAM, ARR) \
        clang_createTranslationUnitFromSourceFile((IDX), (FNAM), ARRSIZ(ARR), (ARR), 0, 0)

#define destroy_cxstrings(...) (destroy_cxstrings__(__VA_ARGS__, (CXString *)NULL))

static CXCompilationDatabase   get_database(const char *file);
static void                    visitor(CXCursor cursor);
static enum CXChildVisitResult visit_continue(CXCursor cursor, CXCursor parent, CXClientData client_data);
static enum CXVisitorResult    visit_range(void *context, CXCursor c, CXSourceRange r);
static void tokenize_file(CXTranslationUnit *tu, CXFile *file, size_t filesize);

static void tokenize_tmp(const char *const filename);
static void destroy_cxstrings__(CXString *str, ...);

static const char *const tagargs[] = {
    "-DHAVE_CONFIG_H",
    "-D_GNU_SOURCE",
    "-std=gnu11",
    "-I/usr/include",
    "-I/usr/lib64/clang/8.0.0/include",
    "-I/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include",
    "-I/usr/lib64/gcc/x86_64-pc-linux-gnu/8.2.0/include-fixed",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/build",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/contrib/jsmn",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/bstring",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/archive",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/mpack",
    "-I/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/util",
};

static const char *const src_file = "/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/mpack/mpack.c";


int
main(UNUSED int argc, UNUSED char *argv[])
{
#if 0
        char              buf[8192];
        CXIndex           Idx = clang_createIndex(1, 1);
        CXTranslationUnit TU2 = NULL;

        clang_parseTranslationUnit2(Idx, src_file, tagargs, ARRSIZ(tagargs), NULL, 0,
                   CXTranslationUnit_PrecompiledPreamble|CXTranslationUnit_DetailedPreprocessingRecord, &TU2);
        if (!TU2)
                errx(1, "clang error");

        struct stat st;
        stat(src_file, &st);
        auto_type file = clang_getFile(TU2, src_file);
#endif

        /* tokenize_file(&TU2, &file, st.st_size); */
        /* tokenize_tmp(src_file); */

        /* clang_visitChildren(clang_getTranslationUnitCursor(TU2), visit_continue, NULL); */
        /* clang_disposeTranslationUnit(TU2); */

        /* clang_disposeIndex(Idx); */
        return 0;
}

static CXCompilationDatabase
get_database(const char *file)
{
        CXCompilationDatabase_Error error;
        CXCompilationDatabase ret = clang_CompilationDatabase_fromDirectory(file, &error);

        if (error != 0)
                errx(1, "clang");

        return ret;
}


static void
visitor(CXCursor cursor)
{
        static long n = 0;
        if (clang_Cursor_isMacroBuiltin(cursor))
                return;

        CXSourceLocation loc = clang_getCursorLocation(cursor);
        if (clang_Location_isFromMainFile(loc))
                return;

        CXType   type          = clang_getCursorType(cursor);
        CXString typespell     = clang_getTypeSpelling(type);
        CXString typekindrepr  = clang_getTypeKindSpelling(type.kind);
        CXString curs_spelling = clang_getCursorSpelling(cursor);
        CXString curs_kindspel = clang_getCursorKindSpelling(cursor.kind);
        CXString dispname      = clang_getCursorDisplayName(cursor);
        CXString pretty        = clang_getCursorPrettyPrinted(cursor, NULL);
        CXFile   file          = NULL;
        unsigned location[3]   = {0, 0, 0};

        clang_getExpansionLocation(loc, &file, &location[0], &location[1], &location[2]);
        CXString    filename = clang_getFileName(file);
        const char *realname = CS(dispname);
        if (!*realname)
                realname = CS(curs_spelling);

        printf("%4ld: %-30s %-30s - %-17s - %-20s - %s\n", n++, CS(dispname),
               CS(curs_spelling), CS(typekindrepr), CS(curs_kindspel), CS(typespell));

        /* const char *s = clang_getCString(pretty);
        if (*s)
                printf("%s\n", s); */
        /* printf("%s\n", repr_CXCursorKind(cursor.kind)); */
        /* printf("%s\n", clang_getCString(curs_spelling)); */
        /* printf("%s\n", clang_getCString(typespell)); */

        destroy_cxstrings(&pretty, &typespell, &curs_spelling, &typekindrepr, &filename, &dispname);

        /* clang_visitChildren(cursor, visitor, NULL); */
        /* return CXChildVisit_Continue; */
        /* return CXChildVisit_Recurse; */
}

static enum CXChildVisitResult
visit_continue(CXCursor cursor, UNUSED CXCursor parent, UNUSED CXClientData client_data)
{
        visitor(cursor);
        return CXChildVisit_Recurse;
}

static enum CXVisitorResult
visit_range(UNUSED void *context, UNUSED CXCursor c, CXSourceRange r)
{
        CXSourceLocation start          = clang_getRangeStart(r);
        CXSourceLocation end            = clang_getRangeEnd(r);
        CXFile           file[2]        = {NULL, NULL};
        unsigned         location[2][3] = {{0, 0, 0}, {0, 0, 0}};

        clang_getExpansionLocation(start, &file[0], &location[0][0], &location[0][1], &location[0][2]);
        clang_getExpansionLocation(end,   &file[1], &location[1][0], &location[1][1], &location[1][2]);

        CXString filename[2] = {clang_getFileName(file[0]), clang_getFileName(file[1])};

        printf("Found in File \"%s\" from line %u, column %u, offset %u\n"
               "         File \"%s\" to   line %u, column %u, offset %u\n",
               clang_getCString(filename[0]), location[0][0], location[0][1], location[0][2],
               clang_getCString(filename[1]), location[1][0], location[1][1], location[1][2]);

        clang_disposeString(filename[0]);
        clang_disposeString(filename[1]);

        return CXVisit_Continue;
}

static void
tokenize_file(CXTranslationUnit *tu, CXFile *file, const size_t filesize)
{
        CXSourceLocation loc1 = clang_getLocationForOffset(*tu, *file, 0);
        CXSourceLocation loc2 = clang_getLocationForOffset(*tu, *file, filesize);
        CXSourceRange    rng  = clang_getRange(loc1, loc2);
        CXToken         *toks = NULL;
        unsigned         num  = 0;

        clang_tokenize(*tu, rng, &toks, &num);
        CXCursor *cursors = calloc(num, sizeof(CXCursor));
        clang_annotateTokens(*tu, toks, num, cursors);

        for (unsigned i = 0; i < num; ++i)
                visitor(cursors[i]);

        clang_disposeTokens(*tu, toks, num);
        free(cursors);
}

static void
tokenize_tmp(const char *const filename)
{
        struct stat st;
        stat(filename, &st);
        char *buf = malloc(st.st_size + 1);
        int   fd  = open(filename, O_RDONLY);
        read(fd, buf, st.st_size);
        buf[st.st_size] = '\0';
        close(fd);

        char temp[16]; 
        strcpy(temp, "/tmp/XXXXXX.c");
        const int tmpfd = mkstemps(temp, 2);
        write(tmpfd, buf, st.st_size);

        printf("Opened tmp file %s at %d\n", temp, tmpfd);

        CXIndex           idx = clang_createIndex(1, 1);
        CXTranslationUnit tu = NULL;

        clang_parseTranslationUnit2(idx, temp, tagargs, ARRSIZ(tagargs), NULL, 0,
                   CXTranslationUnit_PrecompiledPreamble|CXTranslationUnit_DetailedPreprocessingRecord, &tu);
        if (!tu)
                errx(1, "clang error");

        auto_type file = clang_getFile(tu, temp);
        tokenize_file(&tu, &file, st.st_size);

        clang_disposeTranslationUnit(tu);
        clang_disposeIndex(idx);
        close(tmpfd);
        unlink(temp);
        free(buf);
}


static void
destroy_cxstrings__(CXString *str, ...)
{
        int cnt = 0;
        if (!str)
                abort();
        va_list ap;
        va_start(ap, str);

        do {
                ++cnt;
                clang_disposeString(*str);
        } while ((str = va_arg(ap, CXString *)));

        fprintf(stderr, "Disposed of %d strings\n", cnt);
        va_end(ap);
}
