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

__attribute__((__constructor__))
static void golang_sock_init(void)
{
        pthread_mutex_init(&golang_init_mtx);
}

/*======================================================================================*/

#if 0
static bstring *sock_golang_recv_msg(int fd);
static void sock_golang_send_msg(int fd, bstring const *msg);
#endif

static bstring *read_pipe(int read_fd);
static void write_buffer(int fd, bstring const *buf);

bstring *
golang_recv_msg(int const fd)
{
        return read_pipe(fd);
}

void
golang_send_msg(int const fd, bstring const *const msg)
{
        write_buffer(fd, msg);
}

/*======================================================================================*/

#if 0
static pid_t start_go_process(Buffer *bdata, struct golang_data *gd, int closeme);

void
golang_clear_data(Buffer *bdata)
{
#ifndef DOSISH
        kill(bdata->godata.pid, SIGTERM);
#endif
#if 0
                close(bdata->godata.rd_fd);
                close(bdata->godata.wr_fd);
#endif
        struct golang_data *gd = bdata->godata.sock_info;
        close(gd->read_fd);
        close(gd->read_sock);
        close(gd->write_sock);
        unlink(BS(gd->path1));
        unlink(BS(gd->path2));
        talloc_free(gd);
}

static void
usr_handler(UNUSED int sig)
{}

static void
set_signal_mask(void)
{
        struct sigaction   act;
        memset(&act, 0, sizeof(act));
        act.sa_handler = usr_handler;
        sigaction(SIGUSR1, &act, NULL);
}

void
golang_buffer_init(Buffer *bdata)
{
        pthread_mutex_lock(&golang_init_mtx);

        struct golang_data *gd;
        struct sockaddr_un addr[2];
        int   fds[2];
        int   acc;
#if 0
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGURG);
#endif
        /* set_signal_mask(); */

        atomic_flag_clear(&bdata->godata.flg);

        gd = talloc(bdata, struct golang_data);
        gd->path1 = nvim_call_function(B("tempname"), E_STRING).ptr;
        gd->path2 = nvim_call_function(B("tempname"), E_STRING).ptr;
        talloc_steal(gd, gd->path1);
        talloc_steal(gd, gd->path2);

        memset(&addr[0], 0, sizeof(addr[0]));
        memset(&addr[1], 0, sizeof(addr[1]));
        memcpy(addr[0].sun_path, gd->path1->data, gd->path1->slen + 1);
        memcpy(addr[1].sun_path, gd->path2->data, gd->path2->slen + 1);
        addr[0].sun_family = addr[1].sun_family  = AF_UNIX;
        unlink(BS(gd->path1));

        if ((fds[0] = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == (-1))
                err(1, "Failed to create socket instance");
        if (bind(fds[0], (struct sockaddr *)&addr[0], sizeof(addr[0])) == (-1))
                err(2, "Failed to bind the socket");
        if (listen(fds[0], 1) == (-1))
                err(3, "Failed to listen to socket");

        bdata->godata.pid = start_go_process(bdata, gd, fds[0]);

        if ((acc = accept(fds[0], NULL, NULL)) < 0)
                err(1, "accept");

        /* sigwaitinfo(&set, NULL); */
        /* pause(); */

        if ((fds[1] = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == (-1))
                err(1, "Failed to create socket instance");
#if 0
        if (connect(fds[1], (struct sockaddr *)(&(addr[1])), sizeof(addr[1])) == (-1))
                err(1, "Failed to connect to socket");
#endif
        while (connect(fds[1], (struct sockaddr *)(&(addr[1])), sizeof(addr[1])) == (-1))
                ;

        gd->read_sock  = fds[0];
        gd->write_sock = fds[1];
        gd->read_fd    = acc;
        gd->write_fd   = gd->write_sock;

        bdata->godata.sock_info = gd;
        pthread_mutex_unlock(&golang_init_mtx);
}


static pid_t
start_go_process(Buffer *bdata, struct golang_data *gd, int const closeme)
{
        pid_t pid;

        char *const argv[] = {
                BS(settings.go_binary),
                (char *)program_invocation_short_name,
                (char *)is_debug,
                BS(bdata->name.full),
                BS(bdata->name.path),
                BS(bdata->topdir->pathname),
                BS(gd->path1),
                BS(gd->path2),
                (char *)0
        };

        if ((pid = fork()) == 0) {
                close(closeme);
                if (execv(BS(settings.go_binary), argv) == (-1))
                        err(1, "execv failed");
        }

        return pid;
}

/*--------------------------------------------------------------------------------------*/

static bstring *
sock_golang_recv_msg(int const fd)
{
        static const size_t buffer_size = SIZE_C(1048576);

        bstring *ret = b_create(buffer_size);

        ssize_t nread;
        nread = recv(fd, ret->data, ret->mlen, MSG_WAITALL);
        if (nread < 0)
                err(1, "recv");

        if (ret->data[nread - 1] == '\0')
                ret->slen = nread - 1;
        else
                ret->data[(ret->slen = nread)] = '\0';

        return ret;
}

static void
sock_golang_send_msg(int const fd, bstring const *const msg)
{
        if (send(fd, msg->data, msg->slen, MSG_EOR) == (-1))
                err(1, "send");
}
#endif

/*======================================================================================*/

static pid_t start_binary(Buffer *bdata);

void
golang_clear_data(Buffer *bdata)
{
        pthread_mutex_lock(&golang_init_mtx);
#ifndef DOSISH
        kill(bdata->godata.pid, SIGTERM);
#endif
        struct golang_data *gd = bdata->godata.sock_info;
        if (gd) {
                close(gd->read_fd);
                close(gd->write_fd);
                talloc_free(gd);
                bdata->godata.sock_info = NULL;
        }
        pthread_mutex_unlock(&golang_init_mtx);
}

void
golang_buffer_init(Buffer *bdata)
{
        pthread_mutex_lock(&golang_init_mtx);
        bdata->godata.sock_info = talloc_zero(bdata, struct golang_data);
        start_binary(bdata);
        struct golang_data *gd = bdata->godata.sock_info;
        echo ("write: %d, read: %d", gd->write_fd, gd->read_fd);
        pthread_mutex_unlock(&golang_init_mtx);
}

/*--------------------------------------------------------------------------------------*/

#ifdef DOSISH

/* 
 * I *could* try to use _pipe(), but... I dunno. Still have to start the damned process,
 * which requires Win32 HANDLEs.
 */
static pid_t
start_binary(Buffer *bdata)
{
        PROCESS_INFORMATION pi;
        HANDLE hand[2];
        int    fds[2];
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

        win32_start_process_with_pipe(BS(commandline), hand, &pi);
        b_free(commandline);

        if ((fds[0] = _open_osfhandle((intptr_t)hand[0], 0)) == (-1))
                errx(1, "_open_osfhandle failed");
        if ((fds[1] = _open_osfhandle((intptr_t)hand[1], 0)) == (-1))
                errx(1, "_open_osfhandle failed");
        gd->write_fd = fds[WRITE_FD];
        gd->read_fd = fds[READ_FD];
        bdata->godata.pid   = 0;

        return 0;
}

/*--------------------------------------------------------------------------------------*/
#else

#include <sys/wait.h>

/* If you're lazy and you know it clap your hands CLAP CLAP */
static void openpipe(int fds[2]);
static noreturn void *await_certain_death(void *vdata);

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
                repr[0],
                repr[1],
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


#if 0
        pid_t *arg = malloc(sizeof(pid_t));
        *arg = pid;
        START_DETACHED_PTHREAD(await_certain_death, arg);
#endif

        close(fds[0][READ_FD]);
        close(fds[1][WRITE_FD]);

        gd->write_fd = fds[0][WRITE_FD];
        gd->read_fd = fds[1][READ_FD];
        bdata->godata.pid   = pid;

        return pid;
}

static noreturn void *
await_certain_death(void *vdata)
{
        pid_t const pid = *((pid_t *)vdata);
        int         st;
        free(vdata);
        if (waitpid(pid, &st, 0) == (-1))
                err(1, "wtf");
        shout("Catastrophe! Binary exited! Status %d.", st);
        pthread_exit();
}

static void
openpipe(int fds[2])
{
        if (pipe(fds) == (-1))
                err(1, "pipe2()");
#if 0
# ifdef HAVE_PIPE2
        if (pipe2(fds, O_CLOEXEC) == (-1))
                err(1, "pipe2()");
# else
        int flg;
        if (pipe(fds) == (-1))
                err(1, "pipe()");
        /* Surely the compiler will unroll this... */
        for (int i = 0; i < 2; ++i) {
                if ((flg = fcntl(fds[i], F_GETFL)) == (-1))
                        err(3+i, "fcntl(F_GETFL)");
                if (fcntl(fds[i], F_SETFL, flg | O_CLOEXEC) == (-1))
                        err(5+i, "fcntl(F_SETFL)");
        }
# endif
#if defined __linux__  /* Can't do this on the BSDs. */
        if (fcntl(fds[0], F_SETPIPE_SZ, 65535) == (-1))
                err(2, "fcntl(F_SETPIPE_SZ)");
# endif
#endif
}

#endif

/*--------------------------------------------------------------------------------------*/

static void
write_buffer(int const fd, bstring const *const buf)
{
        unsigned n;
        {
#if 0
                char len_str[16];
                unsigned slen = sprintf(len_str, "%010u", buf->slen);
                eprintf("Writing %'d as %s (num2read)\n", buf->slen, len_str);
                n = write(fd, len_str, slen);
                if (n != slen)
                        err(1, "write() -> %u != %u", n, slen);
                eprintf("Wrote %'u bytes (num2read).", n);
#endif
                uint64_t w = (uint64_t)buf->slen;
                n = write(fd, (void *)&w, 8);
                if (n != 8)
                        err(1, "write");
        }

#if 0
        {
                struct timespec *tmp = MKTIMESPEC(3, (NSEC2SECOND / 2));
                eprintf("Sleeping for %g seconds", TIMESPEC2DOUBLE(tmp));
                nanosleep(tmp, NULL);
        }
#endif

        /* eprintf("Writing %'u bytes (the buffer)\n", buf->slen); */
        n = write(fd, buf->data, buf->slen);
        /* eprintf("Wrote %'u bytes (the buffer).", n); */
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
                assert(nread == 10);
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
