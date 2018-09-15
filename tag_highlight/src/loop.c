#include "util/util.h"

#include "clang/clang.h"
#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include <signal.h>

extern pthread_t top_thread;
static void handle_nvim_response(int fd, mpack_obj *obj);
static void notify_waiting_thread(mpack_obj *obj, struct nvim_wait *cur);

/*======================================================================================*/

void *
event_loop(void *vdata)
{
        int fd;
        if (vdata)
                fd = *(int *)vdata;
        else
                fd = 1;

        for (;;) {
                mpack_obj *obj = decode_stream(fd);
                if (!obj)
                        errx(1, "Got NULL object from decoder, cannot continue.");

                const enum message_types mtype = m_expect(m_index(obj, 0), E_NUM, false).num;

                switch (mtype) {
                case MES_NOTIFICATION:
                        handle_nvim_event(obj);
                        mpack_destroy(obj);    
                        break;
                case MES_RESPONSE:
                        handle_nvim_response(fd, obj);
                        break;
                case MES_REQUEST:
                default:
                        abort();
                }
        }

        pthread_exit(NULL);
}

static void
handle_nvim_response(const int fd, mpack_obj *obj)
{
        for (;;) {
                if (!wait_list || wait_list->qty == 0) {
                        fsleep(0.01L);
                        continue;
                }
                for (unsigned i = 0; i < wait_list->qty; ++i) {
                        struct nvim_wait *cur = wait_list->lst[i];
                        if (!cur || !cur->cond)
                                errx(1, "Got an invalid wait list object in %s\n", FUNC_NAME);
                        if (cur->fd != fd)
                                continue;
                        const int count = m_expect(m_index(obj, 1), E_NUM, false).num;
                        if (cur->count != count)
                                continue;

                        notify_waiting_thread(obj, cur);
                        return;
                }
                
                fsleep(0.01L);
        }
}

static void
notify_waiting_thread(mpack_obj *obj, struct nvim_wait *cur)
{
        cur->obj      = obj;
        const int ret = pthread_cond_signal(cur->cond);
        if (ret != 0)
                errx(1, "pthread_signal_cond failed with status %d", ret);
}

/*======================================================================================*/

/* 
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided clear command.
 */
noreturn void *
interrupt_call(void *vdata)
{
        static int             bufnum    = (-1);
        /* static pthread_mutex_t int_mutex = PTHREAD_MUTEX_INITIALIZER; */
        struct int_pdata      *data      = vdata;
        timer t;

        /* pthread_mutex_lock(&int_mutex); */

        if (data->val != 'H')
                echo("Recieved \"%c\"; waking up!", data->val);

        switch (data->val) {
        /*
         * New buffer was opened or current buffer changed.
         */
        case 'A':  /* FIXME Fix these damn letters, they've gotten totally out of order. */
        case 'D': {
                const int prev = bufnum;
                TIMER_START_BAR(t);
                bufnum                = nvim_get_current_buf(0);
                struct bufdata *bdata = find_buffer(bufnum);

                if (!bdata) {
                try_attach:
                        if (new_buffer(0, bufnum)) {
                                nvim_buf_attach(BUFFER_ATTACH_FD, bufnum);
                                bdata = find_buffer(bufnum);

                                get_initial_lines(bdata);
                                get_initial_taglist(bdata);
                                update_highlight(bufnum, bdata);

                                TIMER_REPORT(t, "initialization");
                        }
                } else if (prev != bufnum) {
                        if (!bdata->calls)
                                get_initial_taglist(bdata);

                        update_highlight(bufnum, bdata);
                        TIMER_REPORT(t, "update");
                }

                break;
        }
        /*
         * Buffer was written, or filetype/syntax was changed.
         */
        case 'B':
        case 'F': {
                TIMER_START_BAR(t);
                bufnum                 = nvim_get_current_buf(0);
                struct bufdata *bdata  = find_buffer(bufnum);

                if (!bdata) {
                        echo("Failed to find buffer! %d -> p: %p\n",
                             bufnum, (void *)bdata);
                        goto try_attach;
                }

                if (update_taglist(bdata, (data->val == 'F'))) {
                        clear_highlight(nvim_get_current_buf(0), NULL);
                        update_highlight(bufnum, bdata);
                        TIMER_REPORT(t, "update");
                }

                break;
        }
        /*
         * User called the kill command.
         */
        case 'C': {
                clear_highlight(nvim_get_current_buf(0), NULL);
#ifdef DOSISH
                pthread_kill(data->parent_tid, SIGTERM);
#else
                pthread_kill(/* data->parent_tid */ top_thread, SIGUSR1);
#endif
                break;
        }
        /*
         * User called the clear highlight command.
         */
        case 'E':
                clear_highlight(nvim_get_current_buf(0), NULL);
                break;

        case 'H':
                break;

        case 'I': {
                bufnum                = nvim_get_current_buf(0);
                struct bufdata *bdata = find_buffer(bufnum);
                update_highlight(bufnum, bdata, true);
                break;
        }
        default:
                ECHO("Hmm, nothing to do...");
                break;
        }

        xfree(vdata);
        /* pthread_mutex_unlock(&int_mutex); */
        pthread_exit(NULL);
}

void
get_initial_lines(struct bufdata *bdata)
{
        b_list *tmp = nvim_buf_get_lines(0, bdata->num, 0, (-1));
        if (bdata->lines->qty == 1)
                ll_delete_node(bdata->lines, bdata->lines->head);
        ll_insert_blist_after(bdata->lines, bdata->lines->head, tmp, 0, (-1));

        xfree(tmp->lst);
        xfree(tmp);
        bdata->initialized = true;
}
