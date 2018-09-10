#include "util/util.h"

#include "clang.h"
#include "clang_intern.h"
#include "data.h"
#include "mpack/mpack.h"

#define CLD(s)    ((struct clangdata *)((s)->clangdata))
#define TMPSIZ    (512)
#define CS(CXSTR) (clang_getCString(CXSTR))
#define TUFLAGS                                          \
        (  CXTranslationUnit_DetailedPreprocessingRecord \
         | CXTranslationUnit_KeepGoing                   \
         | CXTranslationUnit_PrecompiledPreamble         \
         | CXTranslationUnit_Incomplete)

#define DUMPDATA()                                                 \
        do {                                                       \
                char dump[TMPSIZ];                                 \
                snprintf(dump, TMPSIZ, "%s/XXXXXX.log", tmp_path); \
                const int dumpfd = mkstemps(dump, 4);              \
                argv_dump_fd(dumpfd, comp_cmds);                   \
                b_list_dump_fd(dumpfd, enumlist.enumerators);      \
                close(dumpfd);                                     \
        } while (0)

static pthread_mutex_t lc_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static genlist *update_list;

struct lc_thread {
        struct bufdata *bdata;
        const int       first;
        const int       last;
        const int       ctick;
};

extern mpack_call_array *
libclang_highlight___(struct bufdata *bdata, const int first, const int last);

void *
libclang_threaded_highlight(void *vdata)
{
        /* pthread_exit(NULL); */
        /* abort(); */
#if 0
        struct lc_thread *data = vdata;

        /* mpack_call_array *calls = libclang_highlight___(data->bdata, 0, (-1)); */
        /* mpack_call_array *calls = libclang_highlight___(data->bdata, 0, (-1)); */

        /* pthread_mutex_lock(&lc_thread_mutex); */
        /* pthread_cond_wait(&lclang_cond, &lc_thread_mutex); */

        /* if (data->bdata->ctick != data->ctick) {
                eprintf("Bailing!\n");
                goto bail;
        } */

        /* echo("Updating!!!!\n"); */

        auto_type calls = libclang_highlight___(data->bdata, data->first, data->last);
        pthread_mutex_lock(&lc_thread_mutex);
        genlist_append(update_list, calls);
        pthread_mutex_unlock(&lc_thread_mutex);

        /* libclang_highlight(data->bdata, 0, (-1)); */

        /* nvim_call_atomic(0, calls); */

bail:
        /* pthread_mutex_unlock(&lc_thread_mutex); */
        /* destroy_call_array(calls); */
        free(vdata);
        pthread_exit(NULL);
#endif
        static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
        
        if (pthread_mutex_trylock(&mut) == EBUSY)
                pthread_exit(NULL);

#if 0
        if (first) {
                fsleep(2.0L);
                first = false;
        }
#endif

        libclang_highlight((struct bufdata *)vdata, 0, (-1));
        pthread_mutex_unlock(&mut);
        pthread_exit(NULL);
}

void *
libclang_waiter(UNUSED void *vdata)
{
        pthread_exit(NULL);
#if 0
        static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
        static bool first = true;
        
        if (pthread_mutex_trylock(&mut) == EBUSY)
                pthread_exit(NULL);

#if 0
        if (first) {
                fsleep(2.0L);
                first = false;
        }
#endif

        libclang_highlight((struct bufdata *)vdata, 0, (-1));

        pthread_mutex_unlock(&mut);
#endif

        fsleep(1.2L);
        int      last_bufnum = nvim_get_current_buf(0);
        unsigned last_ctick  = nvim_buf_get_changedtick(0, last_bufnum);

        for (;;) {
                int             bufnum = nvim_get_current_buf(0);
                struct bufdata *bdata  = find_buffer(bufnum);

                if (!bdata)
                        continue;
                if (!bdata->ft->is_c) {
                        fsleep(2.0L);
                        continue;
                }
                if (last_bufnum != bufnum) {
                        last_bufnum = bufnum;
                        last_ctick  = 0;
                        continue;
                }

                unsigned ctick = nvim_buf_get_changedtick(0, bufnum);
                if (ctick != last_ctick) {
                        last_ctick            = ctick;
                        struct bufdata *bdata = find_buffer(bufnum);
                        libclang_highlight(bdata, 0, (-1));
                }

                fsleep(0.2L);
        }

#if 0
        static int ncall = 0;
        if (ncall++ == 0) {
                update_list = genlist_create();
                return NULL;
        }

        for (;;) {
                fsleep(0.50L);
                eprintf("Woke up.");
                if (update_list->qty == 0)
                        continue;
                pthread_mutex_lock(&lc_thread_mutex);
                for (unsigned i = update_list->qty; i > 0; --i) {
                        nvim_call_atomic(0, update_list->lst[i]);
                        destroy_call_array(update_list->lst[i]);
                        update_list->lst = NULL;
                }
                update_list->qty = 0;
                pthread_mutex_unlock(&lc_thread_mutex);
        }

#endif
        pthread_exit(NULL);
}
