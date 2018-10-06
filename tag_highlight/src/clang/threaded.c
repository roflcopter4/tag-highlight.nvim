#include "tag_highlight.h"

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
        libclang_highlight((struct bufdata *)vdata);
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
        fsleep(2.0);
        int      last_bufnum = nvim_get_current_buf();
        unsigned last_ctick  = nvim_buf_get_changedtick(,last_bufnum);

        for (;;fsleep(0.8)) {
                const int       bufnum = nvim_get_current_buf();
                struct bufdata *bdata  = find_buffer(bufnum);

                if (!bdata || !bdata->ft->is_c) {
                        fsleep(1.0);
                        continue;
                }
                if (last_bufnum != bufnum) {
                        last_bufnum = bufnum;
                        last_ctick  = 0;
                        continue;
                }

                unsigned ctick = nvim_buf_get_changedtick(,bufnum);
                if (ctick != last_ctick) {
                        last_ctick = ctick;
                        libclang_highlight(bdata);
                }
        }

        pthread_exit();
}
