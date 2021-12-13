#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "contrib/p99/p99_atomic.h"

#define BT bt_init

/*--------------------------------------------------------------------------------------*/

struct message_args {
    alignas(16)
      mpack_obj *obj;
    alignas(8)
      int fd;
};

const event_id event_list[] = {
      { BT("nvim_buf_lines_event"),       EVENT_BUF_LINES },
      { BT("nvim_buf_changedtick_event"), EVENT_BUF_CHANGED_TICK },
      { BT("nvim_buf_detach_event"),      EVENT_BUF_DETACH },
      { BT("vim_event_update"),           EVENT_VIM_UPDATE },
};

FILE *api_buffer_log = NULL;
p99_futex volatile event_loop_futex = P99_FUTEX_INITIALIZER(0);
static pthread_mutex_t   handle_mutex;
static pthread_mutex_t   nvim_event_handler_mutex;
P99_FIFO(event_node_ptr) nvim_event_queue;

#define CTX event_handlers_talloc_ctx_
void *event_handlers_talloc_ctx_ = NULL;

__attribute__((__constructor__(200)))
static void event_handlers_initializer(void)
{
      pthread_mutexattr_t attr;
      pthread_mutexattr_init(&attr);
      pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      pthread_mutex_init(&handle_mutex, &attr);
      pthread_mutex_init(&nvim_event_handler_mutex, &attr);
      p99_futex_init((p99_futex *)&event_loop_futex, 0);
}

extern void exit_cleanup(void);
extern noreturn void *highlight_go_pthread_wrapper(void *vdata);
static noreturn void *wrap_update_highlight(void *vdata);
static noreturn void *wrap_handle_nvim_response(void *wrapper);
static void           handle_nvim_response(mpack_obj *obj, int fd);
static void           handle_nvim_notification(mpack_obj *event);

/*======================================================================================*/
/* Event Handlers */
/*======================================================================================*/

static void handle_buffer_update(Buffer *bdata, mpack_array *arr, event_idp type);
static bool check_mutex_consistency(pthread_mutex_t *mtx, int val, const char *msg);

static bool      handle_line_event(Buffer *bdata, mpack_array *arr);
static event_idp id_event(mpack_obj *event) __attribute__((__pure__));

void
handle_nvim_message(struct event_data *data)
{
      mpack_obj *obj = data->obj;
      int const  fd  = data->fd;
      talloc_steal(CTX, obj);

      nvim_message_type const mtype = mpack_expect(mpack_index(obj, 0), E_NUM).num;

      switch (mtype) {
      case MES_NOTIFICATION: {
            handle_nvim_notification(obj);
            break;
      }
      case MES_RESPONSE: {
            struct message_args *tmp = aligned_alloc_for(struct message_args);
            tmp->obj = obj;
            tmp->fd  = fd;
            START_DETACHED_PTHREAD(wrap_handle_nvim_response, tmp);
            break;
      }
      case MES_REQUEST:
            errx(1, "Recieved request in %s somehow. This should be "
                    "\"impossible\"?\n", FUNC_NAME);
      default:
            errx(1, "Recieved invalid object type from neovim. "
                    "This should be \"impossible\"?\n");
      }
}

static void *
destroy_buffer_thread(void *vdata)
{
      destroy_buffer(vdata, DES_BUF_SHOULD_CLEAR | DES_BUF_DESTROY_NODE | DES_BUF_TALLOC_FREE);
      pthread_exit();
}

static void
handle_nvim_notification(mpack_obj *event)
{
      assert(event);
      mpack_array *arr  = mpack_expect(mpack_index(event, 2), E_MPACK_ARRAY).ptr;
      event_idp    type = id_event(event);

      if (type->id == EVENT_VIM_UPDATE) {
            /* It's hard to think of a more pointless use of the `sizeof' operator. */
            uint64_t *tmp = malloc(sizeof(uint64_t));
            *tmp          = mpack_expect(arr->lst[0], E_NUM).num;
            START_DETACHED_PTHREAD(event_autocmd, tmp);
      } else {
            int const bufnum = mpack_expect(arr->lst[0], E_NUM).num;
            Buffer   *bdata  = find_buffer(bufnum);

            if (!bdata)
                  errx(1, "Update called on uninitialized buffer.");

            switch (type->id) {
            case EVENT_BUF_CHANGED_TICK:
            case EVENT_BUF_LINES:
                  /* Moved to a helper function for brevity/sanity. */
                  handle_buffer_update(bdata, arr, type);
                  break;
            case EVENT_BUF_DETACH:
                  echo("Detaching from buffer %d", bufnum);
                  START_DETACHED_PTHREAD(destroy_buffer_thread, bdata);
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
      uint64_t const new_tick = mpack_expect(arr->lst[1], E_NUM, false, E_MPACK_NIL).num;
      if (new_tick != 0)
            p99_futex_exchange(&bdata->ctick, (uint32_t)new_tick);

      if (type->id == EVENT_BUF_LINES) {
            handle_line_event(bdata, arr);
            if (bdata->ft->has_parser)
                  START_DETACHED_PTHREAD(wrap_update_highlight, bdata);
      }
}

/*--------------------------------------------------------------------------------------*/

static noreturn void *
wrap_handle_nvim_response(void *wrapper)
{
      mpack_obj *obj = ((struct message_args *)wrapper)->obj;
      int        fd  = ((struct message_args *)wrapper)->fd;
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
            /* There's a potential race between the waiting thread pushing its node
             * to the stack and our arrival, so we cycle through the queue and even
             * wait if necessary. */
            node = P99_FIFO_POP(&nvim_wait_queue);
            if (node) {
                  if (node->fd == fd && node->count == count)
                        break;
                  P99_FIFO_APPEND(&nvim_wait_queue, node);
            } else {
                  /* Nothing is in the queue at all. 50ns smoke break. */
                  clock_nanosleep_for(0, UINTMAX_C(50));
            }
      }

      atomic_store_explicit(&node->obj, obj, memory_order_seq_cst);
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

UNUSED
static inline bool
check_mutex_consistency(UNUSED pthread_mutex_t *mtx, int val, const char *const msg)
{
      bool ret;
      switch (val) {
      case 0:
      case ETIMEDOUT:
            ret = false;
            break;
      case EOWNERDEAD:
#ifndef __MINGW__
            val = pthread_mutex_consistent(mtx);
#endif
            if (val != 0)
                  err(1, "%s: Failed to make mutex consistent (%d)", msg, val);
            ret = true;
            break;
      case ENOTRECOVERABLE:
            errx(1, "%s: Mutex has been rendered unrecoverable.", msg);
      case EINVAL:
            ret = false;
            break;
      default:
            errx(1, "%s: Impossible?! %d -> %s", msg, val, strerror(val));
      }
      return ret;
}

/*--------------------------------------------------------------------------------------*/

static inline void replace_line(Buffer *bdata, b_list *new_strings, int lineno, int index);
static inline void
line_event_multi_op(Buffer *bdata, b_list *new_strings, int first, int num_to_modify);

static bool
handle_line_event(Buffer *bdata, mpack_array *arr)
{
      if (arr->qty < 5)
            errx(1, "Received an array from neovim that is too small. This "
                    "shouldn't be possible.");
      else if (arr->lst[5]->boolean)
            /* FIXME: This is a documented condition that should be handled. */
            errx(1, "Error: Continuation condition is unexpectedly true, "
                    "cannot continue.");

      pthread_mutex_lock(&bdata->lock.total);

      int const first     = mpack_expect(arr->lst[2], E_NUM, true).num;
      int const last      = mpack_expect(arr->lst[3], E_NUM, true).num;
      int const diff      = last - first;
      bool      empty     = false;
      b_list *new_strings = mpack_expect(arr->lst[4], E_STRLIST, true).ptr;

      /*
       * NOTE: For some reason neovim sometimes sends updates with an empty
       *       list in which both the first and last line are the same. God
       *       knows what this is supposed to indicate. I'll just ignore them.
       */

      if (new_strings->qty) {
            if (last == (-1)) {
                  /* An "initial" update, recieved only if asked for when attaching
                   * to a buffer. We never ask for this, so this shouldn't occur. */
                  errx(1, "Got initial update somehow...");
            }
            else if (bdata->lines->qty <= 1 && first == 0 && /* Empty buffer... */
                     new_strings->qty == 1 &&                /* with one string... */
                     new_strings->lst[0]->slen == 0          /* which is emtpy. */)
            {
                  /* Useless update, one empty string in an empty buffer. */
                  empty = true;
            }
            else if (first == 0 && last == 0) {
                  /* Inserting above the first line in the file. */
                  ll_insert_blist_before_at(bdata->lines, first, new_strings, 0, (-1));
            }
            else {
                  /* The most common scenario: we recieved at least one string which
                   * may be empty only if the buffer is not empty. Moved to a helper
                   * function for clarity. */
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
      int const num_lines = MAXOF(num_to_modify, num_new);
      int const olen      = bdata->lines->qty;

      /* This loop is only meaningful when replacing lines.
       * All other paths break after the first iteration. */
      for (int i = 0; i < num_lines; ++i) {
            int const index = first + i;
            if (num_to_modify-- > 0 && i < olen) {
                  /* There are still strings to be modified. If we still have a
                   * replacement available then we use it. Otherwise we are instead
                   * deleting a range of lines. */
                  if (i < num_new) {
                        replace_line(bdata, new_strings, index, i);
                  } else {
                        ll_delete_range_at(bdata->lines, index, num_to_modify + 1);
                        break;
                  }
            } else {
                  /* There are no more strings to be modified and there is one or
                   * more strings remaining in the list. These are to be inserted
                   * into the buffer.
                   * If the first line to insert (first + i) is at the end of the
                   * file then we append it, otherwise we prepend. */
                  if ((first + i) >= bdata->lines->qty)
                        ll_insert_blist_after_at(bdata->lines, index,
                                                 new_strings, i, (-1));
                  else
                        ll_insert_blist_before_at(bdata->lines, index,
                                                  new_strings, i, (-1));
                  break;
            }
      }
}


/*
 * This just tries to address the fact that under the current scheme in which only a
 * limited number of threads will be allowed to wait for a chance to update, it's
 * possible to fail to update for the last handful of changes. This waiting thread should
 * hopefully belatedly fix the situation.
 */
static noreturn void *
delayed_update_highlight(void *vdata)
{
      /* Sleep for 1 & 1/2 seconds. */
      NANOSLEEP_FOR_SECOND_FRACTION(1, 1, 2);
      update_highlight(vdata);
      pthread_exit();
}

static noreturn void *
wrap_update_highlight(void *vdata)
{
      Buffer *bdata = vdata;
      if (p99_futex_add(&bdata->lock.hl_waiters, 1U) == 0)
            START_DETACHED_PTHREAD(delayed_update_highlight, bdata);
      update_highlight(vdata);
      p99_futex_add(&bdata->lock.hl_waiters, -(1U));
      pthread_exit();
}

/*======================================================================================*/
/*
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided 'clear', 'stop', or 'update' commands.
 */

extern atomic_int global_previous_buffer;
atomic_int global_previous_buffer = ATOMIC_VAR_INIT(-1);

static pthread_mutex_t autocmd_mutex;

P99_DECLARE_ENUM(vimscript_message_type, int,
                 VIML_BUF_NEW,
                 VIML_BUF_CHANGED,
                 VIML_BUF_SYNTAX_CHANGED,
                 VIML_UPDATE_TAGS,
                 VIML_UPDATE_TAGS_FORCE,
                 VIML_CLEAR_BUFFER,
                 VIML_STOP,
                 VIML_EXIT
                 );
P99_DEFINE_ENUM(vimscript_message_type);

static void event_buffer_changed(void);
static void event_syntax_changed(void);
static void event_want_update(vimscript_message_type val);
static void event_force_update(void);
static noreturn void event_stop(void);
static noreturn void event_exit(void);
static void attach_new_buffer(int num);

extern __always_inline void global_previous_buffer_set(int num);
extern __always_inline int  global_previous_buffer_get(void);
extern __always_inline int  global_previous_buffer_exchange(int num);


__attribute__((__constructor__(400)))
void autocmd_constructor(void)
{
      pthread_mutex_init(&autocmd_mutex);
}

/*--------------------------------------------------------------------------------------*/

noreturn void *
event_autocmd(void *vdata)
{
      pthread_mutex_lock(&autocmd_mutex);

      vimscript_message_type val;
      {
            uint64_t const tmp = *((uint64_t *)vdata);
            assert(tmp < INT_MAX);
            val = (vimscript_message_type)tmp;
            free(vdata);
      }

      switch (val) {
      case VIML_BUF_NEW:
      case VIML_BUF_CHANGED:
            event_buffer_changed();
            break;

      case VIML_BUF_SYNTAX_CHANGED:
            /* Have to completely reconsider a buffer if the active syntax
             * (ie language) is changed. */
            event_syntax_changed();
            break;

      case VIML_UPDATE_TAGS:
            /* Usually indicates that the buffer was written. */
            event_want_update(val);
            break;

      case VIML_UPDATE_TAGS_FORCE:
            /* User forced an update. */
            event_force_update();
            break;

      case VIML_STOP:
            /* User called the kill command. */
            event_stop();
            break;

      case VIML_EXIT:
            /* Neovim is exiting */
            event_exit();
            break;

      case VIML_CLEAR_BUFFER:
            /* User called the clear highlight command. */
            clear_highlight();
            break;

      default:
            break;
      }

      pthread_mutex_unlock(&autocmd_mutex);
      pthread_exit();
}

inline void
global_previous_buffer_set(int const num)
{
      atomic_store_explicit(&global_previous_buffer, num, memory_order_release);
}

inline int
global_previous_buffer_get(void)
{
      return atomic_load_explicit(&global_previous_buffer, memory_order_acquire);
}

inline int
global_previous_buffer_exchange(int const num)
{
      return atomic_exchange_explicit(&global_previous_buffer, num, memory_order_acq_rel);
}

/*--------------------------------------------------------------------------------------*/

static void
event_buffer_changed(void)
{
      int const num   = nvim_get_current_buf();
      int const prev  = global_previous_buffer_exchange(num);
      Buffer   *bdata = find_buffer(num);

      if (prev == num && bdata)
            return;

      if (1) {
            Buffer *prev_bdata = find_buffer(prev);
            if (prev_bdata && prev_bdata->initialized && prev_bdata->ft->is_c)
                  libclang_suspend_translationunit(prev_bdata);
      }

      if (bdata && bdata->initialized)
            update_highlight(bdata, HIGHLIGHT_NORMAL);
      else
            attach_new_buffer(num);
}

static void
event_syntax_changed(void)
{
      int const num   = nvim_get_current_buf();
      Buffer   *bdata = find_buffer(num);
      global_previous_buffer_set(num);

      if (!bdata)
            return;
      bstring *ft = nvim_buf_get_option(num, B("ft"), E_STRING).ptr;

      if (!b_iseq(ft, &bdata->ft->vim_name)) {
            echo("Filetype changed. Updating.");
            destroy_buffer(bdata, DES_BUF_SHOULD_CLEAR | DES_BUF_TALLOC_FREE | DES_BUF_DESTROY_NODE);
            attach_new_buffer(num);
      }

      b_free(ft);
}

static void
event_want_update(UNUSED vimscript_message_type val)
{
      int const num = nvim_get_current_buf();
      global_previous_buffer_set(num);
      Buffer *bdata = find_buffer(num);

      if (bdata) {
#if 0
            if (update_taglist(bdata, UPDATE_TAGLIST_NORMAL)) {
                  clear_highlight(bdata);
                  update_highlight(bdata, HIGHLIGHT_UPDATE);
            }
#endif
      } else if (have_seen_bufnum(num)) {
            attach_new_buffer(num);
      } else {
            echo("Failed to find buffer! %d -> p: %p", num, (void *)bdata);
      }
}

static void
event_force_update(void)
{
      UNUSED struct timer t = STRUCT_TIMER_INITIALIZER;
      TIMER_START(&t);

      int const num = nvim_get_current_buf();
      global_previous_buffer_set(num);
      Buffer *bdata = find_buffer(num);

      if (bdata)
            update_highlight(bdata, HIGHLIGHT_UPDATE_FORCE);
      else
            attach_new_buffer(num);

      TIMER_REPORT(&t, "Forced update");
}

static noreturn void
event_halt(bool const nvim_exiting)
{
      extern void stop_event_loop(int status);

      /* If neovim is shutting down, then we should also shut down ASAP. Taking too
       * long will hang the editor for a few seconds, which is intolerable. */
      if (nvim_exiting)
            quick_exit(0);

      clear_highlight(, true);
      exit_cleanup();

#ifdef DOSISH
      exit(0);
#else
      stop_event_loop(0);
      /* pthread_kill(event_loop_thread, KILL_SIG); */
      /* raise(SIGUSR1); */
      pthread_exit();
#endif
}

static noreturn void
event_stop(void)
{
      event_halt(false);
}

static noreturn void
event_exit(void)
{
      extern bool process_exiting;
      process_exiting = true;
      event_halt(true);
}

static void
attach_new_buffer(int const num)
{
      UNUSED struct timer t = STRUCT_TIMER_INITIALIZER;
      Buffer *bdata = new_buffer(num);

      if (bdata) {
            TIMER_START(&t);
            nvim_buf_attach(num);

            get_initial_lines(bdata);
            get_initial_taglist(bdata);
            update_highlight(bdata, HIGHLIGHT_UPDATE);
            settings.buffer_initialized = true;

            TIMER_REPORT(&t, "initialization");
      } else {
            ECHO("Failed to attach to buffer number %d.", num);
      }
}
