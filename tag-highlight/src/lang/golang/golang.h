#ifndef SRC_LANG_GOLANG_GOLANG_H_
#define SRC_LANG_GOLANG_GOLANG_H_

#include "Common.h"
#include "highlight.h"

__BEGIN_DECLS
/*======================================================================================*/

struct golang_data {
        bstring *path1;
        bstring *path2;
        int      write_sock;
        int      read_sock;
        int      read_fd;
        pthread_mutex_t mut;
};

extern int highlight_go(Buffer *bdata);
extern bstring *get_go_binary(void);
extern void golang_clear_data(Buffer *bdata);
extern void golang_buffer_init(Buffer *bdata);
extern bstring *golang_recv_msg(int fd);
extern void golang_send_msg(int fd, bstring const *msg);

/*======================================================================================*/
__END_DECLS
#endif /* golang.h */
