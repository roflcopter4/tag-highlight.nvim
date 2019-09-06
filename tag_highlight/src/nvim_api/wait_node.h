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


P44_DECLARE_FIFO(_nvim_wait_node);
struct _nvim_wait_node {
        int        fd;
        unsigned   count;
        p99_futex  fut;
        mpack_obj *obj;

        _nvim_wait_node_ptr p99_fifo;
};
P99_FIFO(_nvim_wait_node_ptr) _nvim_wait_queue;


#ifdef __cplusplus
}
#endif
#endif /* wait_node.h */
