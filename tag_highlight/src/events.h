#ifndef THL_EVENTS_H_
#define THL_EVENTS_H_
#include "Common.h"
__BEGIN_DECLS
/*======================================================================================*/


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

extern const struct event_id event_list[];

extern noreturn void *event_autocmd(void *vdata);

/*======================================================================================*/
#endif /* events.h */
