#include "util/util.h"

#include "find.h"
#include <sys/stat.h>

#if defined(DOSISH) || defined(MINGW)
#  include <direct.h>
#  define B_FILE_EQ(FILE1_, FILE2_) (b_iseq_caseless((FILE1_), (FILE2_)))
#  define SEPSTR "\\"
#else
#  define B_FILE_EQ(FILE1_, FILE2_) (b_iseq((FILE1_), (FILE2_)))
#  define SEPSTR "/"
#  include <sys/wait.h>
#endif

#define CNULL ((char *)0)

static bstring *run_find(const char *path, const char *search);
static bstring *read_fd(const int fd);
static b_list  *split_find(const char *path, const char *search);
static void     dump_char_list(const char *desc, char **list);

/*======================================================================================*/

void *
find_file(const char *path, const char *search, const enum find_flags flags)
{
        bstring *result = run_find(path, search);
        if (!result)
                return NULL;

        switch (flags) {
        case FIND_SHORTEST: {
                unsigned i;
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
                bstring *tmp;
                unsigned pos = b_strchr(result, '\n');
                if (pos > 0) {
                        tmp = b_fromblk(result->data, pos);
                        b_destroy(result);
                } else
                        tmp = result;

                return tmp;
        }
        case FIND_SPLIT:
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
        int pipefds[2] = {0, 0};
        int status     = 0;

        /* if (pipe(pipefds) == (-1)) */
                /* err(1, "Pipe failed"); */
        if (pipe2(pipefds, O_NONBLOCK|O_CLOEXEC) == (-1))
                err(1, "Pipe failed");

        /* const int flgs[2] = {fcntl(pipefds[0], F_GETFD), fcntl(pipefds[1], F_GETFD)};
        fcntl(pipefds[0], F_SETFD, flgs[0]|O_NONBLOCK|O_CLOEXEC);
        fcntl(pipefds[1], F_SETFD, flgs[1]|O_NONBLOCK|O_CLOEXEC); */

        const int pid = fork();

        if (pid == 0) {
                if (dup2(pipefds[1], 1) == (-1))
                        err(1, "dup2() failed");

                char **tmp = (char *[]){"fd", "-au", (char *)search, (char *)path,  CNULL};
                execvp("fd", tmp);

                tmp = (char *[]){"find", (char *)path, "-name", (char *)search, CNULL};
                if (execvp("find", tmp) == (-1))
                        err(1, "exec failed");
        }

        close(pipefds[1]);
        waitpid(pid, &status, 0);

        if ((status >>= 8) != 0)
                errx(status, "Command failed with status %d", status);

        bstring *ret = read_fd(pipefds[0]);
        close(pipefds[0]);

        if (ret->slen == 0)
                b_destroy(ret);

        return ret;
}

#define INIT_READ ((size_t)(8192llu))
#ifdef DOSISH
#  define SSIZE_T size_t
#else
#  define SSIZE_T ssize_t
#endif

static bstring *
read_fd(const int fd)
{
        bstring *ret = b_alloc_null(INIT_READ + 1u);

        for (;;) {
                SSIZE_T nread = read(fd, (ret->data + ret->slen), INIT_READ);
                if (nread > 0) {
                        ret->slen += nread;
                        if ((size_t)nread < INIT_READ) {
                                /* eprintf("breaking\n"); */
                                break;
                        }
                        b_growby(ret, INIT_READ);
                } else {
                        break;
                }
        }

        ret->data[ret->slen] = '\0';
        return ret;
}

static b_list *
split_find(const char *path, const char *search)
{
        bstring *result = run_find(path, search);
        if (!result)
                return NULL;
        return b_split_char(result, '\n', true);
}

static void
dump_char_list(const char *desc, char **list)
{
        char *tmp;
        fputs(desc, stderr);

        while ((tmp = *list++))
                fprintf(stderr, "%s, ", tmp);
        fputc('\n', stderr);
}
