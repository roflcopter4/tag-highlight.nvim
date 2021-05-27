package main

/*
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

int Write_Sock, Read_Sock, Read_FD;

#define err(n, ...) (((warn)(__VA_ARGS__)), fflush(stderr), exit(n))

static void
initialize_sockets(char *path1, char *path2)
{
        struct sockaddr_un addr[2];
        int fds[2];
        int acc;

        memset(&addr[0], 0, sizeof(addr[0]));
        strcpy(addr[0].sun_path, path1);
        addr[0].sun_family = AF_UNIX;

        if ((fds[0] = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == (-1))
                err(1, "Failed to create socket instance");
        if ((fds[1] = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == (-1))
                err(1, "Failed to create socket instance");

        if (connect(fds[0], (struct sockaddr *)(&(addr[0])), sizeof(addr[0])) == (-1))
                err(1, "Failed to connect to socket");

        memset(&addr[1], 0, sizeof(addr[1]));
        strcpy(addr[1].sun_path, path2);
        addr[1].sun_family = AF_UNIX;
        unlink(path2);

        if (bind(fds[1], (struct sockaddr *)(&(addr[1])), sizeof(addr[1])) == (-1))
                err(2, "bind()");
        if (listen(fds[1], 1) == (-1))
                err(3, "Failed to listen to socket");

        // kill(getppid(), SIGURG);

        if ((acc = accept(fds[1], NULL, NULL)) < 0)
                err(1, "accept");

	  fprintf(stderr, "%d && %d ---> %d\n", fds[0], fds[1], acc); fflush(stderr);

        Write_Sock = fds[0];
	  Read_Sock  = fds[1];
        Read_FD    = acc;
}

#define BUFFER_SIZE (INTMAX_C(1048576))

static int
wrap_recv(int const sockfd, char **bufp)
{
	*bufp = malloc(BUFFER_SIZE);
	intmax_t nread = recv(sockfd, *bufp, BUFFER_SIZE, MSG_WAITALL);
	if (nread < 0)
		err(1, "nread");
	return (int)nread;
}

static ssize_t
wrap_send(int const sockfd, const _GoString_ gstr)
{
	ssize_t const  len  = _GoStringLen(gstr);
	char    const *buf  = _GoStringPtr(gstr);
	ssize_t const nsent = send(sockfd, buf, len, MSG_EOR);

	if (nsent == (-1))
		err(1, "send");
	if (nsent != len)
		err(1, "send");

	return nsent;
}
*/
import "C"

import (
	"unsafe"
)

type rw_pair struct {
	read  int32
	write int32
}

type sockets struct {
	fd    rw_pair
	sock  rw_pair
	paths [2]string
}

func (this *sockets) Recv() string {
	fd := this.fd.read
	var (
		c_buf  *C.char
		c_len  = C.wrap_recv(C.int(fd), &c_buf)
		go_str = C.GoStringN(c_buf, c_len)
	)
	return go_str
}

func (this *sockets) Send(buf string) {
	fd := this.fd.write
	C.wrap_send(C.int(fd), buf)
}

// func connect_socket(path string) {
//       var (
//             err error
//             c   *net.Conn
//             addr *net.UnixAddr
//       )
//
//       c, err = net.Dial("unixpacket", path)
//       if err != nil {
//             panic(err)
//       }
//
//       addr, err = ResolveUnixAddr("unixpacket", path)
//       if err != nil {
//             panic(err)
//       }
//
//       addr
// }

// func connect_socket_sys(path string) {
//       var (
//             err  error
//             fd   int
//             addr unix.SockaddrUnix
//       )
//       addr.Name = path
//       fd, err = unix.Socket(unix.AF_UNIX, unix.SOCK_SEQPACKET, 0)
//       if err != nil {
//             panic(err)
//       }
//
//       err = unix.Connect(fd, &addr)
//       if err != nil {
//             panic(err)
//       }
// }

func connect_socket_c(paths [2]string) *sockets {
	var (
		fds     [3]int32
		c_path1 = C.CString(paths[0])
		c_path2 = C.CString(paths[1])
	)
	defer C.free(unsafe.Pointer(c_path1))
	defer C.free(unsafe.Pointer(c_path2))

	C.initialize_sockets(c_path1, c_path2)
	fds[0] = int32(C.Write_Sock)
	fds[1] = int32(C.Read_Sock)
	fds[2] = int32(C.Read_FD)

	return &sockets{
		fd:    rw_pair{fds[2], fds[0]},
		sock:  rw_pair{fds[1], fds[0]},
		paths: paths,
	}
}

func wait(sock *sockets) string {
	return sock.Recv()
}

func (this *Parsed_Data) WriteOutput(sock *sockets) {
	sock.Send(this.Output)
}
