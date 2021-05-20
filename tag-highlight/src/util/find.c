#include "Common.h"
#include "find.h"
#include <sys/stat.h>

#if defined(DOSISH) || defined(MINGW)
//#  include <direct.h>
#  define B_FILE_EQ(FILE1_, FILE2_) (b_iseq_caseless((FILE1_), (FILE2_)))
#  define SEPSTR "\\"
#else
#  define B_FILE_EQ(FILE1_, FILE2_) (b_iseq((FILE1_), (FILE2_)))
#  define SEPSTR "/"
#  include <sys/wait.h>
#endif

#define CNULL ((char *)0)

static bstring *run_find(const char *path, const char *search);
/* static void unquote(bstring *orig); */

/*======================================================================================*/

void *
find_file(const char *path, const char *search, const enum find_flags flags)
{
        bstring *result = run_find(path, search);
        if (!result)
                return NULL;

        switch (flags) {
        case FIND_SHORTEST: {
                b_list  *split    = b_split_char(result, '\n', true);
                bstring *shortest = split->lst[0];

                B_LIST_FOREACH (split, str, i)
                        if (str->slen < shortest->slen)
                                shortest = str;

                b_writeprotect(shortest);
                b_list_destroy(split);
                b_writeallow(shortest);
                b_chomp(shortest);

                return shortest;
        }
        case FIND_FIRST: {
                bstring *first;
                unsigned pos = b_strchr(result, '\n');
                if (pos > 0) {
                        first = b_fromblk(result->data, pos);
                        b_destroy(result);
                } else
                        first = result;

                return first;
        }
        case FIND_SPLIT: // b_list *
                return b_split_char(result, '\n', true);
        default:
        case FIND_LITERAL:
                return result;
        }
}

/*======================================================================================*/

static bstring *
run_find(const char *path, const char *search)
{
        int status = 0;
#ifdef HAVE_FORK
        int pipefds[2] = {0, 0};
#  ifdef HAVE_PIPE2
        if (pipe2(pipefds, O_NONBLOCK|O_CLOEXEC) == (-1))
                err(1, "Pipe failed");
#  elif !defined(DOSISH)
        if (pipe(pipefds) == (-1))
                err(1, "Pipe failed");
#  endif
        const int flgs[2] = {fcntl(pipefds[0], F_GETFD), fcntl(pipefds[1], F_GETFD)};
        fcntl(pipefds[0], F_SETFD, flgs[0]|O_NONBLOCK|O_CLOEXEC);
        fcntl(pipefds[1], F_SETFD, flgs[1]|O_NONBLOCK|O_CLOEXEC);


        const int pid = fork();

        if (pid == 0) {
                if (dup2(pipefds[1], 1) == (-1))
                        err(1, "dup2() failed");

                char **tmp = (char *[]){"fd", "-au", (char *)search, (char *)path,  CNULL};
                execvp("fd", tmp);

                tmp = (char *[]){"find", (char *)path, "-regex", (char *)search, CNULL};
                if (execvp("find", tmp) == (-1))
                        err(1, "exec failed");
        }

        close(pipefds[1]);
        waitpid(pid, &status, 0);

        if ((status >>= 8) != 0)
                errx(status, "Command failed with status %d", status);

        bstring *ret = b_read_fd(pipefds[0]);
        close(pipefds[0]);

        if (ret->slen == 0)
                b_destroy(ret);
#else
        char buf[8192], tmpbuf[SAFE_PATH_MAX];
        strcpy_s(tmpbuf, SAFE_PATH_MAX, ".find_tmp_XXXXXX");
        tmpnam_s(tmpbuf, SAFE_PATH_MAX);
        snprintf(buf, 8192, "find \"%s\" -regex \"%s\" > \"%s\"", path, search, tmpbuf);
        status = system(buf);
        if ((status >>= 8) != 0)
                errx(status, "Command failed with status %d", status);
        bstring *ret = b_quickread("%s", tmpbuf);
        unlink(tmpbuf);
#endif

        return ret;
}

#if 0
static void
unquote(bstring *str)
{
        /* bstring *ret = b_alloc_null(str->slen + 1); */
        uint8_t  buf[str->slen + 1];
        unsigned x = 0;

        for (unsigned i = 0; i < str->slen; ++i) {
                const uint8_t ch = str->data[str->slen];
                if (ch != '"' && ch != '\'')
                        buf[x++] = ch;
        }

        if (x != str->slen) {
                memcpy(str->data, buf, x);
                str->data[x] = '\0';
                str->slen    = x;
        }
}
#endif
