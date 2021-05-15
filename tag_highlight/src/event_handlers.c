#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "contrib/p99/p99_atomic.h"
#include <signal.h>
#include <stdatomic.h>

#define BT bt_init

/*======================================================================================*/

struct obj_wrapper {
        mpack_obj *obj;
        int fd;
};

const event_id event_list[] = {
    {BT("nvim_buf_lines_event"), EVENT_BUF_LINES},
    {BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK},
    {BT("nvim_buf_detach_event"), EVENT_BUF_DETACH},
    {BT("vim_event_update"), EVENT_VIM_UPDATE},
};

FILE *api_buffer_log                = NULL;
p99_futex volatile event_loop_futex = P99_FUTEX_INITIALIZER(0);
static pthread_mutex_t handle_mutex;
static pthread_mutex_t nvim_event_handler_mutex;
P99_FIFO(event_node_ptr) nvim_event_queue;

#define CTX event_handlers_talloc_ctx_
void *event_handlers_talloc_ctx_ = NULL;

__attribute__((__constructor__)) static void
event_handlers_initializer(void)
{
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&handle_mutex, &attr);
        pthread_mutex_init(&nvim_event_handler_mutex, &attr);
        p99_futex_init((p99_futex *)&event_loop_futex, 0);
}


extern noreturn void *highlight_go_pthread_wrapper(void *vdata);
static noreturn void *wrap_update_highlight(void *vdata);
static noreturn void *wrap_handle_nvim_response(void *wrapper);
static void           handle_nvim_response(mpack_obj *obj, int fd);
static void           handle_nvim_notification(mpack_obj *event);

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

static void handle_buffer_update(Buffer *bdata, mpack_array *arr, event_idp type);
static inline bool check_mutex_consistency(pthread_mutex_t *mtx, int val, const char *msg);

static bool      handle_line_event(Buffer *bdata, mpack_array *arr);
static event_idp id_event(mpack_obj *event) __attribute__((__pure__));

void
handle_nvim_message(struct event_data *data)
{
        mpack_obj *obj = data->obj;
        int const  fd  = data->fd;

        nvim_message_type const mtype = mpack_expect(mpack_index(obj, 0), E_NUM).num;

        switch (mtype) {
        case MES_NOTIFICATION: {
                talloc_steal(CTX, obj);
                handle_nvim_notification(obj);
                break;
        }
        case MES_RESPONSE: {
                talloc_steal(CTX, obj);
                struct obj_wrapper *tmp = malloc(sizeof *tmp);
                tmp->obj = obj;
                tmp->fd  = fd;
                START_DETACHED_PTHREAD(wrap_handle_nvim_response, tmp);
                break;
        }
        case MES_REQUEST:
                errx(1, "Recieved request in %s somehow. This should be \"impossible\"?\n", FUNC_NAME);
        default:
                errx(1, "Recieved invalid object type from neovim. This should be \"impossible\"?\n");
        }
}

static void
handle_nvim_notification(mpack_obj *event)
{
        assert(event);
        mpack_array *arr  = mpack_expect(mpack_index(event, 2), E_MPACK_ARRAY).ptr;
        event_idp    type = id_event(event);

        if (type->id == EVENT_VIM_UPDATE) {
                uint64_t *tmp = malloc(sizeof(uint64_t));
                *tmp          = mpack_expect(arr->lst[0], E_NUM).num;
                START_DETACHED_PTHREAD(event_autocmd, tmp);
        } else {
                int const bufnum = mpack_expect(arr->lst[0], E_NUM).num;
                Buffer *  bdata  = find_buffer(bufnum);

                if (!bdata)
                        errx(1, "Update called on uninitialized buffer.");

                switch (type->id) {
                case EVENT_BUF_CHANGED_TICK:
                case EVENT_BUF_LINES:
                        /* Moved to a helper function for brevity/sanity. */
                        handle_buffer_update(bdata, arr, type);
                        break;
                case EVENT_BUF_DETACH:
                        clear_highlight(bdata);
                        destroy_buffer(bdata);
                        echo("Detaching from buffer %d", bufnum);
                        break;
                default:
                        abort();
                }
        }

        talloc_free(event);
}

static void
handle_buffer_update(Buffer *bdata, mpack_array *arr, event_idp type)
{
        uint32_t const new_tick = mpack_expect(arr->lst[1], E_NUM).num;
        bool empty = false;

        p99_futex_exchange(&bdata->ctick, new_tick, 0, 0, 0, 0);

        if (type->id == EVENT_BUF_LINES)
                empty = handle_line_event(bdata, arr);
        if (bdata->ft->has_parser && !empty )
                START_DETACHED_PTHREAD(wrap_update_highlight, bdata);

                //update_highlight(bdata);
}

#if 0
static void
handle_buffer_update(Buffer *bdata, mpack_array *arr, event_idp type)
{
        uint32_t const new_tick = mpack_expect(arr->lst[1], E_NUM).num;
        unsigned tmp;
        bool     empty = 0, killme = true;

        int mret;

        mret = pthread_mutex_lock(&bdata->lock.cond_mtx);
        if (check_mutex_consistency(&bdata->lock.cond_mtx, mret, "1st")) {
                if ((killme = atomic_flag_test_and_set(&bdata->ctick_seen_3))) {
                        //atomic_fetch_add(&bdata->ctick, 1);
                        pthread_cond_broadcast(&bdata->lock.cond);
                } else {
                        p99_futex_add(&bdata->ctick, 1U);
                }
        }

        p99_count_inc(&bdata->lock.cond_waiters);
        struct timespec ts;
        mret = 0;
        while (!((tmp = p99_futex_load(&bdata->ctick)) == (new_tick - 1))) {
                if (tmp > new_tick) {
                        //echo("Sigh. %u > %u -- also %s", tmp, new_tick, BTS(type->name));
                        if (type->id == EVENT_BUF_CHANGED_TICK) {
                                /* XXX This just has to be wrong XXX */
                                if (!killme || atomic_flag_test_and_set(&bdata->ctick_seen_2)) {
                                        //atomic_fetch_add(&bdata->ctick, -1);
                                        pthread_cond_broadcast(&bdata->lock.cond);
                                }
                                p99_count_dec(&bdata->lock.cond_waiters);
                                return;
                        } else {
                                errx(1, "Fatal: A buffer update was skipped somehow. "
                                        "State of internal buffer is suspect.");
                        }
                }

                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 1;
                ts.tv_nsec += ((uintmax_t)NSEC2SECOND / UINTMAX_C(200));
                mret = pthread_cond_timedwait(&bdata->lock.cond, &bdata->lock.cond_mtx, &ts);

                /*
                 * I don't actually like the timed wait. But it might
                 * simply be a better idea.
                 */
                /* mret = pthread_cond_wait(&bdata->lock.cond, &bdata->lock.cond_mtx); */
                check_mutex_consistency(&bdata->lock.cond_mtx, mret, "loop");
        }

        check_mutex_consistency(&bdata->lock.cond_mtx, mret, "2nd");
#if 0
        killme = atomic_flag_test_and_set(&bdata->ctick_seen_3);
        
        P99_FUTEX_COMPARE_EXCHANGE(&bdata->ctick, value,
                value == (new_tick - 1) || !killme,
                value, 0U, 0U
        );

        if (type->id == EVENT_BUF_LINES)
                empty = handle_line_event(bdata, arr);

        p99_count_dec(&bdata->lock.cond_waiters);
        P99_FUTEX_COMPARE_EXCHANGE(&bdata->ctick, value,
                true, value + 1, 0U, P99_FUTEX_MAX_WAITERS
        );

                if (type->id == EVENT_BUF_LINES)
                        empty = handle_line_event(bdata, arr);

                //if (!seen)
                p99_futex_exchange(&bdata->ctick, new_tick,
                                0U, UINT_MAX, 0U, P99_FUTEX_MAX_WAITERS);
#endif
        if (type->id == EVENT_BUF_LINES)
                empty = handle_line_event(bdata, arr);
        p99_count_dec(&bdata->lock.cond_waiters);
        pthread_mutex_unlock(&bdata->lock.cond_mtx);
        p99_futex_exchange(&bdata->ctick, new_tick, 0, 0, 0, 0);
        /* atomic_fetch_add(&bdata->ctick, 1); */
        pthread_cond_broadcast(&bdata->lock.cond);

        if (bdata->ft->has_parser && !empty && p99_futex_load(&bdata->lock.cond_waiters) <= 1) {
                update_highlight(bdata);
        }
}
#endif

/*======================================================================================*/

static noreturn void *
wrap_handle_nvim_response(void *wrapper)
{
        mpack_obj *obj = ((struct obj_wrapper *)wrapper)->obj;
        int        fd  = ((struct obj_wrapper *)wrapper)->fd;
        free(wrapper);
        handle_nvim_response(obj, fd);
        pthread_exit(NULL);
}

static void
handle_nvim_response(mpack_obj *obj, int fd)
{
        unsigned const  count = mpack_expect(mpack_index(obj, 1), E_NUM).num;
        nvim_wait_node *node  = NULL;

        if (fd == 0)
                ++fd;

        for (;;) {
                node = P99_FIFO_POP(&nvim_wait_queue);
                if (!node) {
                        fprintf(stderr, "Queue is empty.\n");
                        fflush(stderr);
                        TALLOC_FREE(obj);
                        return;
                }

                if (node->fd == fd && node->count == count)
                        break;

                P99_FIFO_APPEND(&nvim_wait_queue, node);
        }

        atomic_store(&node->obj, obj);
        p99_futex_wakeup(&node->fut, 1U, P99_FUTEX_MAX_WAITERS);
        /* pthread_cond_signal(&node->cond); */
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

static inline bool
check_mutex_consistency(pthread_mutex_t *mtx, int val, const char *const msg)
{
        bool ret;
        switch (val) {
        case 0:
        case ETIMEDOUT:
                ret = false;
                break;
        case EOWNERDEAD:
                val = pthread_mutex_consistent(mtx);
                if (val != 0)
                        err(1, "%s: Failed to make mutex consistent (%d)", msg, val);
                ret = true;
                break;
        case ENOTRECOVERABLE:
                errx(1, "%s: Mutex has been rendered unrecoverable.", msg);
        case EINVAL:
                ret = false;
                break;
                /* errx(1, "%s: Mutex is (somehow?) invalid.", msg); */
        default:
                errx(1, "%s: Impossible?! %d -> %s", msg, val, strerror(val));
        }
        return ret;
}

/*======================================================================================*/

static inline void replace_line(Buffer *bdata, b_list *new_strings, int lineno, int index);
static inline void
line_event_multi_op(Buffer *bdata, b_list *new_strings, int first, int num_to_modify);
static noreturn void *update_highlight_wrapper(void *bdata);

static bool
handle_line_event(Buffer *bdata, mpack_array *arr)
{
        // pthread_mutex_lock(&handle_mutex);

        if (arr->qty < 5)
                errx(1, "Received an array from neovim that is too small. This "
                        "shouldn't be possible.");
        else if (arr->lst[5]->boolean)
                errx(1, "Error: Continuation condition is unexpectedly true, "
                        "cannot continue.");

        pthread_mutex_lock(&bdata->lock.total);

        int const first = mpack_expect(arr->lst[2], E_NUM, true).num;
        int const last  = mpack_expect(arr->lst[3], E_NUM, true).num;
        int const diff  = last - first;
        bool      empty = false;

        b_list *new_strings = mpack_expect(arr->lst[4], E_STRLIST, true).ptr;

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
                else if (bdata->lines->qty <= 1 && first == 0 && /* Empty buffer... */
                         new_strings->qty == 1 &&                /* with one string... */
                         new_strings->lst[0]->slen == 0 /* which is emtpy. */) {
                        empty = true;
                }
                /* Inserting above the first line in the file. */
                else if (first == 0 && last == 0) {
                        ll_insert_blist_before_at(bdata->lines, first, new_strings, 0, (-1));
                }
                /* The most common scenario: we recieved at least one string which
                 * may be empty only if the buffer is not empty. Moved to a helper
                 * function for clarity. */
                else {
                        line_event_multi_op(bdata, new_strings, first, diff);
                }
        } else if (first != last) {
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
        pthread_mutex_unlock(&bdata->lock.total);
        return empty;
}

static inline void
replace_line(Buffer *bdata, b_list *new_strings, int const lineno, int const index)
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
                                ll_delete_range_at(bdata->lines, first + i, num_to_modify + 1);
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
wrap_update_highlight(void *vdata)
{
        update_highlight(vdata);
        pthread_exit();
}
