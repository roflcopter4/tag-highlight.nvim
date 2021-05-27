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

static pthread_mutex_t golang_init_mtx;

__attribute__((__constructor__))
static void golang_sock_init(void)
{
        pthread_mutex_init(&golang_init_mtx);
}

static pid_t start_go_process(Buffer *bdata, struct golang_data *gd, int closeme);

/*--------------------------------------------------------------------------------------*/

void
golang_clear_data(Buffer *bdata)
{
#ifndef DOSISH
        kill(bdata->godata.pid, SIGTERM);
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

        gd->read_fd    = acc;
        gd->read_sock  = fds[0];
        gd->write_sock = fds[1];

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

bstring *
golang_recv_msg(int const fd)
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

void
golang_send_msg(int const fd, bstring const *const msg)
{
        if (send(fd, msg->data, msg->slen, MSG_EOR) == (-1))
                err(1, "send");
}

/*--------------------------------------------------------------------------------------*/

#if 0
int
open_msg_sock(void)
{
        struct sockaddr_un addr;
        char name[64];
        strcpy(name, "/tmp/ThlGolangSockXXXXXXXXX");
        mktemp(name);

        memset(&addr, 0, sizeof(addr));
        strcpy(addr.sun_path, name);
        addr.sun_family = AF_UNIX;
        const int con   = socket(AF_UNIX, SOCK_SEQPACKET, 0);

        if (con == (-1))
                err(1, "Failed to create socket instance.");
        if (bind(con, (struct sockaddr *)(&addr), sizeof(addr)) == (-1))
                err(2, "bind()");
        if (listen(con, 1) == (-1))
                err(3, "Failed to listen to socket.");

        pid_t pid;
        if ((pid = fork()) == 0) {
                execl("./two", "two", name, (char *)0);
        }

        int data = accept(con, NULL, NULL);
        if (data == (-1))
                err(4, "accept()");
        return 0;
}
#endif

#if 0
#define SLSN(str) ("" str ""), (sizeof(str))

static void set_signal_mask(void);
static void do_read(int fd);
static void do_write(int fd);
static void usr_handler(int sig);

static int read_sock, write_sock;

int
open_msg_sock(void)
{
        struct sockaddr_un addr[2];
        char  name[2][64];
        char  base[32];
        int   fds[2];
        int   acc;
        pid_t pid;

        set_signal_mask();
        get_temp_files(base, name);

        memset(&addr[0], 0, sizeof(addr[0]));
        strcpy(addr[0].sun_path, name[0]);
        addr[0].sun_family  = AF_UNIX;
        unlink(name[0]);

        if ((fds[0] = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == (-1))
                err(1, "Failed to create socket instance");

        if (bind(fds[0], (struct sockaddr *)(&addr[0]), sizeof(addr[0])) == (-1))
                err(2, "bind()");
        if (listen(fds[0], 1) == (-1))
                err(3, "Failed to listen to socket");

        if ((pid = fork()) == 0) {
                close(fds[0]);
                execl("./client", "client", name[0], name[1], (char *)0);
        }

        if ((acc = accept(fds[0], NULL, NULL)) < 0)
                err(1, "accept");

        pause();

        memset(&addr[1], 0, sizeof(addr[1]));
        strcpy(addr[1].sun_path, name[1]);
        addr[1].sun_family  = AF_UNIX;

        if ((fds[1] = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == (-1))
                err(1, "Failed to create socket instance");
        if (connect(fds[1], (struct sockaddr *)(&(addr[1])), sizeof(addr[1])) == (-1))
                err(1, "Failed to connect to socket");

        write_sock = fds[1];
        read_sock  = acc;

        do_read(read_sock);
        do_write(write_sock);

        close(acc);
        close(fds[0]);
        close(fds[1]);
        waitpid(pid, NULL, 0);
        unlink(name[0]);
        unlink(name[1]);
        rmdir(base);
        return 0;
}

static void
do_read(int const fd)
{
        static const size_t buffer_size = SIZE_C(1048576);
        char *buf = malloc(buffer_size);
        ssize_t nread;
        nread = recv(fd, buf, buffer_size, MSG_WAITALL);
        if (nread < 0)
                err(1, "recv");
        free(buf);
}

static void
do_write(int const fd)
{
        if (send(fd, SLSN("HELLO FAGGOT NIGGER TURD"), MSG_EOR) == (-1))
                err(1, "send");
        if (send(fd, SLSN("BCDEFGHIJ") , MSG_EOR) == (-1))
                err(1, "send");
}

#endif
