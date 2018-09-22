#include "util/util.h"

#include "clang.h"
#include "data.h"
#include "intern.h"
#include "mpack/mpack.h"

static pthread_once_t libclang_waiter_once = PTHREAD_ONCE_INIT;

struct lc_thread {
        struct bufdata *bdata;
        const int       first;
        const int       last;
        const unsigned  ctick;
};

static noreturn void  libclang_waiter(void);
static noreturn void *do_launch_libclang_waiter(UNUSED void *notused);

void *
libclang_threaded_highlight(void *vdata)
{
        static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
        if (pthread_mutex_trylock(&mut) == EBUSY)
                pthread_exit(NULL);
        libclang_highlight((struct bufdata *)vdata, 0, (-1));
        pthread_mutex_unlock(&mut);
        pthread_exit(NULL);
}

void
launch_libclang_waiter(void)
{
        START_DETACHED_PTHREAD(do_launch_libclang_waiter, NULL);
}

static noreturn void *
do_launch_libclang_waiter(UNUSED void *notused)
{
        pthread_once(&libclang_waiter_once, libclang_waiter);
        pthread_exit(NULL);
}

static noreturn void
libclang_waiter(void)
{
        fsleep(2.0L);
        int      last_bufnum = nvim_get_current_buf(0);
        unsigned last_ctick  = nvim_buf_get_changedtick(0, last_bufnum);

        for (;;fsleep(0.8L)) {
                const int       bufnum = nvim_get_current_buf(0);
                struct bufdata *bdata  = find_buffer(bufnum);

                if (!bdata || !bdata->ft->is_c) {
                        fsleep(5.0L);
                        continue;
                }
                if (last_bufnum != bufnum) {
                        last_bufnum = bufnum;
                        last_ctick  = 0;
                        continue;
                }

                unsigned ctick = nvim_buf_get_changedtick(0, bufnum);
                if (ctick != last_ctick) {
                        last_ctick = ctick;
                        libclang_highlight(bdata, 0, (-1));
                }
        }

        pthread_exit(NULL);
}
