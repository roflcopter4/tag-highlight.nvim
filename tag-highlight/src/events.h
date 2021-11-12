#ifndef THL_EVENTS_H_
#define THL_EVENTS_H_
#include "Common.h"
#include "highlight.h"
#include "my_p99_common.h"
__BEGIN_DECLS
/*======================================================================================*/

#define EVENT_LIB_EV    1
#define EVENT_LIB_NONE  2
#define EVENT_LIB_LIBUV 3
#ifdef DOSISH
#  define USE_EVENT_LIB  EVENT_LIB_NONE
#  define KILL_SIG       SIGTERM
#else
#  if 1
#    define USE_EVENT_LIB EVENT_LIB_LIBUV
#  elif defined HAVE_LIBEV
#    define USE_EVENT_LIB  EVENT_LIB_EV
#  else
#    define USE_EVENT_LIB  EVENT_LIB_NONE
#  endif
#  define KILL_SIG       SIGUSR1
extern pthread_t event_loop_thread;
#endif

P99_DECLARE_STRUCT(event_id);
P99_DECLARE_FIFO(event_node);
typedef const event_id *event_idp;

enum event_types {
        EVENT_BUF_LINES,
        EVENT_BUF_CHANGED_TICK,
        EVENT_BUF_DETACH,
        EVENT_VIM_UPDATE,
};

struct event_id {
        bstring          const name;
        enum event_types const id;
};

struct event_node {
        _Atomic(mpack_obj *) obj;
        event_node *p99_fifo;
};

struct event_data {
        int        fd;
        mpack_obj *obj;
};

extern const struct event_id event_list[];
extern p99_futex volatile _nvim_wait_futex;

extern noreturn void *event_autocmd(void *vdata);

/*===========================================================================*/
/* Event handlers */

extern void handle_nvim_message(struct event_data *data);


/*======================================================================================*/
#endif /* events.h */
// vim: ft=c
