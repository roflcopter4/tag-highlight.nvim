#include "Common.h"
#include "lang/lang.h"
#include "lang/golang/golang.h"

#include "contrib/p99/p99_count.h"

#ifndef WEXITSTATUS
# define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
#endif
#ifdef DEBUG
static const char is_debug[] = "1";
#else
static const char is_debug[] = "0";
#endif

#define READ_FD  (0)
#define WRITE_FD (1)

static pthread_mutex_t golang_init_mtx;

__attribute__((__constructor__(10000)))
static void golang_sock_init(void)
{
      pthread_mutexattr_t attr;
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      pthread_mutex_init(&golang_init_mtx, &attr);
}

/*======================================================================================*/


static pid_t start_binary(Buffer *bdata);

__attribute__((__artificial__))
static inline int golang_clear_data_wrapper(void *vdata)
{
      golang_clear_data(vdata);
      return 0;
}

void
golang_clear_data(Buffer *bdata)
{
        pthread_mutex_lock(&golang_init_mtx);
        struct golang_data *gd = bdata->godata.sock_info;
        bool is_initialized;

        if ((is_initialized = atomic_load(&bdata->godata.initialized)) && gd) {
#ifdef DOSISH
                bool b;
                b = TerminateProcess(gd->hProcess, 0);
                assert(b);
                b = CloseHandle(gd->read_handle);
                assert(b);
                b = CloseHandle(gd->write_handle);
                assert(b);
#else
                int chstat = 0;
                eprintf("Killing %d\n", gd->pid);
                kill(gd->pid, SIGTERM);
                waitpid(gd->pid, &chstat, 0);
                warnx("Child exited with status %d -> %d", chstat, WEXITSTATUS(chstat));
                close(gd->read_fd);
                close(gd->write_fd);
#endif
                talloc_free(gd);
                bdata->godata.sock_info = NULL;
        } else {
              warnx("Attempt to close uninitialized buffer! (%u): %p, %d", bdata->num, gd, is_initialized);
        }

        pthread_mutex_unlock(&golang_init_mtx);
}

void
golang_buffer_init(Buffer *bdata)
{
        pthread_mutex_lock(&golang_init_mtx);
        bdata->godata.sock_info = talloc_zero(bdata, struct golang_data);
        //talloc_set_destructor(bdata->godata.sock_info, golang_clear_data_wrapper);
        start_binary(bdata);
        atomic_store(&bdata->godata.initialized, true);
        pthread_mutex_unlock(&golang_init_mtx);
}

/*======================================================================================*/

#ifdef DOSISH

static bstring *read_pipe(HANDLE hand);
static void write_buffer(HANDLE hand, bstring const *buf);

bstring *golang_recv_msg(struct golang_data const *gd)
{
        return read_pipe(gd->read_handle);
}

void golang_send_msg(struct golang_data const *gd, bstring const *const msg)
{
        write_buffer(gd->write_handle, msg);
}


static pid_t
start_binary(Buffer *bdata)
{
        PROCESS_INFORMATION pi;
        HANDLE hand[2];
        bstring const *go_binary = settings.go_binary;
        char *const argv[] = {
                BS(go_binary),
                (char *)program_invocation_short_name,
                (char *)is_debug,
                BS(bdata->name.full),
                BS(bdata->name.path),
                BS(bdata->topdir->pathname),
                (char *)0
        };
        struct golang_data *gd = bdata->godata.sock_info;

        bstring *commandline = b_create(128);
        for (char **s = (char **)argv; *s; ++s) {
                b_catchar(commandline, '"');
                b_catcstr(commandline, *s);
                b_catlit(commandline, "\" ");
        }

        win32_start_process_with_pipe(BS(go_binary), BS(commandline), hand, &pi);
        b_free(commandline);

#if 0
        if ((fds[0] = _open_osfhandle((intptr_t)hand[0], 0)) == (-1))
                errx(1, "_open_osfhandle failed");
        if ((fds[1] = _open_osfhandle((intptr_t)hand[1], 0)) == (-1))
                errx(1, "_open_osfhandle failed");
#endif

        gd->write_handle = hand[WRITE_FD];
        gd->read_handle = hand[READ_FD];
        gd->hProcess = pi.hProcess;

        return 0;
}


static void
write_buffer(HANDLE hand, bstring const *const buf)
{
        bool  st;
        DWORD n;
        {
                uint64_t w = (uint64_t)buf->slen;
                st = WriteFile(hand, (void *)&w, sizeof(uint64_t), &n, NULL);
                if (!st || n != sizeof(uint64_t))
                        win32_error_exit(1, "WriteFile", GetLastError());
        }

        st = WriteFile(hand, buf->data, buf->slen, &n, NULL);

        if (!st || n != buf->slen)
                WIN32_ERROR_EXIT(1, "WriteFile -> %u != %u", n, buf->slen);
}

static bstring *
read_pipe(HANDLE hand)
{
        DWORD    nread;
        uint32_t num2read;
        {
                char buf[16], *p;

                bool st = ReadFile(hand, buf, 10, &nread, NULL);

                if (!st || nread != 10)
                        WIN32_ERROR_EXIT(1, "nread (%lu) != 10", nread);

                buf[10]  = '\0';
                num2read = strtoull(buf, &p, 10);
#ifndef NDEBUG
                if (p != buf + 10)
                        errx(1, "Invalid input %d, %s (%s)", num2read, buf, p);
#endif
        }

        bstring *ret = b_create(num2read + 1);
        do {
                 if (!ReadFile(hand, ret->data + ret->slen, num2read, &nread, NULL))
                         WIN32_ERROR_EXIT(1, "ReadFile (nread: %lu)", nread);
                 ret->slen += nread;
        } while (ret->slen != num2read);

        if (ret->slen != num2read)
              errx(1, "Win32 ReadFile failed without indicating an error?! "
                      "-> %u != %u", ret->slen, num2read);

        ret->data[ret->slen] = '\0';
        return ret;
}

/*--------------------------------------------------------------------------------------*/
#else
/*--------------------------------------------------------------------------------------*/

static bstring *read_pipe(int read_fd);
static void write_buffer(int fd, bstring const *buf);

bstring *golang_recv_msg(struct golang_data const *gd)
{
        return read_pipe(gd->read_fd);
}

void golang_send_msg(struct golang_data const *gd, bstring const *const msg)
{
        write_buffer(gd->write_fd, msg);
}

#include <sys/wait.h>

/* If you're lazy and you know it clap your hands CLAP CLAP */
static void openpipe(int fds[2]);

static pid_t
start_binary(Buffer *bdata)
{
        bstring const *go_binary = settings.go_binary;

        int fds[2][2], pid;
        openpipe(fds[0]);
        openpipe(fds[1]);

        char repr[2][16];
        sprintf(repr[0], "%d", fds[0][READ_FD]);
        sprintf(repr[1], "%d", fds[1][WRITE_FD]);

        char *const argv[] = {
                BS(go_binary),
                (char *)program_invocation_short_name,
                (char *)is_debug,
                BS(bdata->name.full),
                BS(bdata->name.path),
                BS(bdata->topdir->pathname),
                (char *)0
        };
        struct golang_data *gd = bdata->godata.sock_info;

        if ((pid = fork()) == 0) {
                if (dup2(fds[0][READ_FD],  STDIN_FILENO) != 0)
                        err(1, "dup2() failed (somehow?!)\n");
                if (dup2(fds[1][WRITE_FD], STDOUT_FILENO) != 1)
                        err(1, "dup2() failed (somehow?!)\n");

                close(fds[0][0]);
                close(fds[0][1]);
                close(fds[1][0]);
                close(fds[1][1]);

                if (execv(BS(go_binary), argv) == (-1))
                        err(1, "exec() failed\n");
        }

        close(fds[0][READ_FD]);
        close(fds[1][WRITE_FD]);

        gd->write_fd = fds[0][WRITE_FD];
        gd->read_fd = fds[1][READ_FD];
        gd->pid   = pid;

        return pid;
}

static void
openpipe(int fds[2])
{
# ifdef HAVE_PIPE2
        if (pipe2(fds, O_CLOEXEC) == (-1))
                err(1, "pipe2()");
# else
        if (pipe(fds) == (-1))
                err(1, "pipe()");
# endif
}


/*--------------------------------------------------------------------------------------*/

static void
write_buffer(int const fd, bstring const *const buf)
{
        unsigned n;
        {
                uint64_t w = (uint64_t)buf->slen;
                n = write(fd, (void *)&w, 8);
                if (n != 8)
                        err(1, "write");
        }

        n = write(fd, buf->data, buf->slen);
        if (n != buf->slen)
                err(1, "write() -> %u != %u", n, buf->slen);
}

static bstring *
read_pipe(int const fd)
{
        uint32_t num2read;
        {
                char buf[16], *p;
                UNUSED long nread = read(fd, buf, 10);
                if (nread != 10)
                      err(1, "nread (%lu) != 10", nread);
                buf[10] = '\0';

                num2read = strtoull(buf, &p, 10);
#ifndef NDEBUG
                if (p != buf + 10)
                        errx(1, "Invalid input %d, %s (%s)", num2read, buf, p);
#endif
        }

        bstring *ret = b_create(num2read + 1);
        do {
                ret->slen += read(fd, ret->data + ret->slen, num2read);
        } while (ret->slen != num2read);
        if (ret->slen != num2read)
                err(1, "read() -> %u != %u", ret->slen, num2read);
        ret->data[ret->slen] = '\0';

        return ret;

}

#endif
