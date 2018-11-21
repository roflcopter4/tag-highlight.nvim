#ifndef SRC_NVIM_API_READ_H_
#define SRC_NVIM_API_READ_H_

#include "Common.h"

#include "my_p99_common.h"
#include "p99/p99_count.h"
#include "p99/p99_fifo.h"
#include "p99/p99_futex.h"
#include "p99/p99_lifo.h"

typedef volatile p99_futex vfutex_t;

P44_DECLARE_FIFO(mpack_obj_node);
P44_DECLARE_LIFO(mpack_obj_node);
struct mpack_obj_node {
        mpack_obj      *obj;
        mpack_obj_node *p99_fifo;
        mpack_obj_node *p99_lifo;
        unsigned        count;
};


#if 0
P44_DECLARE_FIFO(nvim_wait_queue);
P44_DECLARE_LIFO(nvim_wait_queue);
struct nvim_wait_queue {
        vfutex_t *volatile  fut;
        mpack_obj          *obj;
        nvim_wait_queue    *p99_lifo;
        nvim_wait_queue    *p99_fifo;
        unsigned            count;
};
#endif

/* extern P99_FIFO(nvim_wait_queue_ptr) nvim_wait_queue_head; */

extern P99_FIFO(mpack_obj_node_ptr) handle_fifo_head;
extern P99_FIFO(mpack_obj_node_ptr) mpack_obj_queue;
extern P99_LIFO(mpack_obj_node_ptr) mpack_obj_stack;

extern volatile p99_count _nvim_count;

#endif /* read.h */
