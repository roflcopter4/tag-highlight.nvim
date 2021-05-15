#ifndef NVIM_API_WAIT_NODE_H_
#define NVIM_API_WAIT_NODE_H_

#include "Common.h"
#include "mpack/mpack.h"
#include "my_p99_common.h"

#include "contrib/p99/p99_fifo.h"
#include "contrib/p99/p99_futex.h"

#ifdef __cplusplus
extern "C" {
#endif


P99_DECLARE_FIFO(nvim_wait_node);
struct nvim_wait_node {
        int        fd;
        unsigned   count;
        p99_futex  fut;
        _Atomic(mpack_obj *) obj;
        pthread_mutex_t mtx;
        pthread_cond_t cond;

        nvim_wait_node_ptr p99_fifo;
};
extern P99_FIFO(nvim_wait_node_ptr) nvim_wait_queue;


#ifdef __cplusplus
}
#endif
#endif /* wait_node.h */
