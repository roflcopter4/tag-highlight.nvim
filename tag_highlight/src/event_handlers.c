#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "contrib/p99/p99_atomic.h"
#include <signal.h>

#define BT bt_init

/*======================================================================================*/

const event_id event_list[] = {
        { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES        },
        { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
        { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH       },
        { BT("vim_event_update"),           EVENT_VIM_UPDATE       },
};

FILE *                   api_buffer_log   = NULL;
p99_futex volatile       event_loop_futex = P99_FUTEX_INITIALIZER(0);
static pthread_mutex_t   handle_mutex;
static pthread_mutex_t   nvim_event_handler_mutex;
P99_FIFO(event_node_ptr) nvim_event_queue;

#define CTX event_handlers_talloc_ctx_
void *event_handlers_talloc_ctx_ = NULL;

__attribute__((__constructor__))
static void event_handlers_initializer(void)
{
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&handle_mutex, &attr);
        pthread_mutex_init(&nvim_event_handler_mutex, &attr);
        p99_futex_init((p99_futex *)&event_loop_futex, 0);
}

extern noreturn void *highlight_go_pthread_wrapper(void *vdata);
static void handle_nvim_response    (mpack_obj *obj, int fd);
static void handle_nvim_notification(mpack_obj *event);

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

static void      handle_line_event(Buffer *bdata, mpack_array *arr);
static event_idp id_event         (mpack_obj *event) __attribute__((__pure__));

noreturn void *
handle_nvim_message(void *vdata)
{
        struct event_data *data = vdata;
        mpack_obj         *obj  = data->obj;
        int const          fd   = data->fd;
        free(data);

        nvim_message_type const mtype = mpack_expect(mpack_index(obj, 0), E_NUM).num;

        switch (mtype) {
        case MES_NOTIFICATION: {
                talloc_steal(CTX, obj);
                handle_nvim_notification(obj);
                break;
        }
        case MES_RESPONSE: {
                talloc_steal(CTX, obj);
                handle_nvim_response(obj, fd);
                break;
        }
        case MES_REQUEST:
                errx(1, "Recieved request in %s somehow. "
                     "This should be \"impossible\"?\n", FUNC_NAME);
        default:
                errx(1, "Recieved invalid object type from neovim. "
                     "This should be \"impossible\"?\n");
        }

        pthread_exit();
}

static void
handle_nvim_notification(mpack_obj *event)
{
        assert(event);
        //pthread_mutex_lock(&nvim_event_handler_mutex);

        mpack_array *arr   = mpack_expect(mpack_index(event, 2), E_MPACK_ARRAY).ptr;
        event_idp    type  = id_event(event);
        /* mpack_print_object(api_buffer_log, event); */

        if (type->id == EVENT_VIM_UPDATE) {
                uint64_t *tmp = malloc(sizeof(uint64_t));
                *tmp          = mpack_expect(arr->lst[0], E_NUM).num;
                START_DETACHED_PTHREAD(event_autocmd, tmp);
        } else {
                int const bufnum = mpack_expect(arr->lst[0], E_NUM).num;
                Buffer   *bdata  = find_buffer(bufnum);

                if (!bdata)
                        errx(1, "Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_LINES:
                        handle_line_event(bdata, arr);
                        break;
                case EVENT_BUF_CHANGED_TICK: {
                        pthread_mutex_lock(&bdata->lock.ctick);
                        uint32_t const new_tick = mpack_expect(arr->lst[1], E_NUM).num;
                        if (new_tick > atomic_load_explicit(&bdata->ctick, memory_order_acquire))    
                                atomic_store_explicit(&bdata->ctick, new_tick, memory_order_release);
                        pthread_mutex_unlock(&bdata->lock.ctick);
                        break;
                }
                case EVENT_BUF_DETACH:
                        clear_highlight(bdata);
                        destroy_buffer(bdata);
                        echo("Detaching from buffer %d", bufnum);
                        break;
                default:
                        abort();
                }
        }

        /* mpack_destroy_object(event); */
        talloc_free(event);
        //pthread_mutex_unlock(&nvim_event_handler_mutex);
}

static void
handle_nvim_response(mpack_obj *obj, int fd)
{
        unsigned const   count = mpack_expect(mpack_index(obj, 1), E_NUM).num;
        _nvim_wait_node *node  = NULL;

        if (fd == 0)
                ++fd;

        for (;;) {
                node = P99_FIFO_POP(&_nvim_wait_queue);
                if (!node) {
                        eprintf("Queue is empty.");
                        TALLOC_FREE(obj);
                        pthread_exit();
                }

                if (node->fd == fd && node->count == count)
                        break;

                P99_FIFO_APPEND(&_nvim_wait_queue, node);
        }

        atomic_store_explicit(&node->obj, obj, memory_order_release);
        p99_futex_wakeup(&node->fut, 1U, P99_FUTEX_MAX_WAITERS);
}

static event_idp
id_event(mpack_obj *event)
{
        bstring const *typename = event->arr->lst[1]->str;

        for (unsigned i = 0, size = (unsigned)ARRSIZ(event_list); i < size; ++i)
                if (b_iseq(typename, &event_list[i].name))
                        return &event_list[i];

        errx(1, "Failed to identify event type \"%s\".\n", BS(typename));
}

/*======================================================================================*/

static inline void replace_line(Buffer *bdata, b_list *new_strings,
                                int lineno, int index);
static inline void line_event_multi_op(Buffer *bdata, b_list *new_strings,
                                       int first, int num_to_modify);

static noreturn void *update_highlight_wrapper(void *bdata);

static void
handle_line_event(Buffer *bdata, mpack_array *arr)
{
        //pthread_mutex_lock(&handle_mutex);

        if (arr->qty < 5)
                errx(1, "Received an array from neovim that is too small. This "
                        "shouldn't be possible.");
        else if (arr->lst[5]->boolean)
                errx(1, "Error: Continuation condition is unexpectedly true, "
                        "cannot continue.");

        pthread_mutex_lock(&bdata->lock.total);

        pthread_mutex_lock(&bdata->lock.ctick);
        unsigned const new_tick = mpack_expect(arr->lst[1], E_NUM, true).num;
        if (new_tick > atomic_load(&bdata->ctick))
                atomic_store(&bdata->ctick, new_tick);
        pthread_mutex_unlock(&bdata->lock.ctick);

        int const first       = mpack_expect(arr->lst[2], E_NUM, true).num;
        int const last        = mpack_expect(arr->lst[3], E_NUM, true).num;
        b_list   *new_strings = mpack_expect(arr->lst[4], E_STRLIST, true).ptr;
        int const diff        = last - first;
        bool      empty       = false;

        //pthread_mutex_lock(&bdata->lines->lock);

        /*
         * NOTE: For some reason neovim sometimes sends updates with an empty
         *       list in which both the first and last line are the same. God
         *       knows what this is supposed to indicate. I'll just ignore them.
         */

        if (new_strings->qty) {
                /* An "initial" update, recieved only if asked for when attaching
                 * to a buffer. We never ask for this, so this shouldn't occur. */
                if (last == (-1)) {
                        errx(1, "Got initial update somehow...");
                } 
                /* Useless update, one empty string in an empty buffer. */
                else if (bdata->lines->qty         <= 1 &&
                         first                     == 0 && /* Empty buffer... */
                         new_strings->qty          == 1 && /* with one string... */
                         new_strings->lst[0]->slen == 0    /* which is emtpy. */)
                {
                        empty = true;
                } 
                /* Inserting above the first line in the file. */
                else if (first == 0 && last == 0) {
                        ll_insert_blist_before_at(bdata->lines, first, new_strings,
                                                  0, (-1));
                } 
                /* The most common scenario: we recieved at least one string which
                 * may be empty only if the buffer is not empty. Moved to a helper
                 * function for clarity. */
                else {
                        line_event_multi_op(bdata, new_strings, first, diff);
                }
        }
        else if (first != last) {
                /* If the replacement list is empty then we're just deleting lines. */
                ll_delete_range_at(bdata->lines, first, diff);
        }

        /* Neovim always considers there to be at least one line in any buffer.
         * An empty buffer therefore must have one empty line. */
        if (bdata->lines->qty == 0)
                ll_append(bdata->lines, b_empty_string());

        if (!bdata->initialized && !empty)
                bdata->initialized = true;

        talloc_free(new_strings);
        //pthread_mutex_unlock(&bdata->lines->lock);
        pthread_mutex_unlock(&bdata->lock.total);

#if 0
        if (!empty && bdata->ft->has_parser) {
                if (bdata->ft->is_c)
                        START_DETACHED_PTHREAD(highlight_c_pthread_wrapper, bdata);
                else if (bdata->ft->id == FT_GO)
                        START_DETACHED_PTHREAD(highlight_go_pthread_wrapper, bdata);
        }
#endif
        if (!empty && bdata->ft->has_parser)
                START_DETACHED_PTHREAD(update_highlight_wrapper, bdata);
        //pthread_mutex_unlock(&handle_mutex);
}

static inline void
replace_line(Buffer *bdata, b_list *new_strings,
             int const lineno, int const index)
{
        pthread_mutex_lock(&bdata->lines->lock);
        ll_node *node = ll_at(bdata->lines, lineno);
        talloc_free(node->data);
        node->data = talloc_move(node, &new_strings->lst[index]);
        pthread_mutex_unlock(&bdata->lines->lock);
}

/*
 * first:         Index of the first string to replace in the buffer (if any)
 * num_to_modify: Number of existing buffer lines to replace and/or delete
 *
 * Handles a neovim line update event in which we received at least one string in a
 * buffer that is not empty. If diff is non-zero, we first delete the lines in the range
 * `first + diff`, and then insert the new line(s) after `first` if it is now the last
 * line in the file, and before it otherwise.
 */
static inline void
line_event_multi_op(Buffer *bdata, b_list *new_strings, int const first, int num_to_modify)
{
        int const num_new   = (int)new_strings->qty;
        int const num_lines = MAX(num_to_modify, num_new);
        int const olen      = bdata->lines->qty;

        /* This loop is only meaningful when replacing lines.
         * All other paths break after the first iteration. */
        for (int i = 0; i < num_lines; ++i) {
                if (num_to_modify-- > 0 && i < olen) {
                        /* There are still strings to be modified. If we still have a
                         * replacement available then we use it. Otherwise we are instead
                         * deleting a range of lines. */
                        if (i < num_new) {
                                replace_line(bdata, new_strings, first + i, i);
                        } else {
                                ll_delete_range_at(bdata->lines, first + i,
                                                   num_to_modify + 1);
                                break;
                        }
                } else {
                        /* There are no more strings to be modified and there is one or
                         * more strings remaining in the list. These are to be inserted
                         * into the buffer.
                         * If the first line to insert (first + i) is at the end of the
                         * file then we append it, otherwise we prepend. */
                        if ((first + i) >= bdata->lines->qty) {
                                ll_insert_blist_after_at(bdata->lines, first + i,
                                                         new_strings, i, (-1));
                        } else {
                                ll_insert_blist_before_at(bdata->lines, first + i,
                                                          new_strings, i, (-1));
                        }
                        break;
                }
        }
}

static noreturn void *
update_highlight_wrapper(void *bdata)
{
        update_highlight(bdata);
        pthread_exit();
}
