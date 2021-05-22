#include "Common.h"
#include "events.h"
#include "highlight.h"
#include "lang/clang/clang.h"
#include "nvim_api/wait_node.h"

#include "contrib/p99/p99_atomic.h"
#include <signal.h>

P99_DECLARE_ENUM(vimscript_message_type,
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

static pthread_mutex_t autocmd_mutex;


static void event_buffer_changed(atomic_int *prev_num);
static void event_syntax_changed(atomic_int *prev_num);
static void event_want_update(atomic_int *prev_num, vimscript_message_type val);
static void event_force_update(atomic_int *prev_num);
static noreturn void event_stop(void);
static noreturn void event_exit(void);
static void attach_new_buffer(int num);

__attribute__((__constructor__)) void
mutex_constructor(void)
{
        pthread_mutex_init(&autocmd_mutex);
}

/*======================================================================================*/

/*
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided 'clear', 'stop', or 'update' commands.
 */
noreturn void *
event_autocmd(void *vdata)
{
        static atomic_int bufnum = ATOMIC_VAR_INIT(-1);
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
                event_buffer_changed(&bufnum);
                break;

        case VIML_BUF_SYNTAX_CHANGED:
                /* Have to completely reconsider a buffer if the active syntax
                 * (ie language) is changed. */
                event_syntax_changed(&bufnum);
                break;

        case VIML_UPDATE_TAGS:
                /* Usually indicates that the buffer was written. */
                event_want_update(&bufnum, val);
                break;

        case VIML_UPDATE_TAGS_FORCE:
                /* User forced an update. */
                event_force_update(&bufnum);
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

/*======================================================================================*/

static void
event_buffer_changed(atomic_int *prev_num)
{
        int const  num   = nvim_get_current_buf();
        int const  prev  = atomic_exchange(prev_num, num);
        Buffer    *bdata = find_buffer(num);

        if (prev == num && bdata)
                return;

        if (bdata) {
                if (!bdata->calls)
                        get_initial_taglist(bdata);

                update_highlight(bdata, HIGHLIGHT_NORMAL);
        } else {
                attach_new_buffer(num);
        }
}

static void
event_syntax_changed(atomic_int *prev_num)
{
        int const num   = nvim_get_current_buf();
        Buffer   *bdata = find_buffer(num);
        atomic_store_explicit(prev_num, num, memory_order_release);
        if (!bdata)
                return;
        bstring *ft = nvim_buf_get_option(num, B("ft"), E_STRING).ptr;

        if (!b_iseq(ft, &bdata->ft->vim_name)) {
                SHOUT("Filetype changed. Updating.");
                clear_highlight(bdata, true);
                destroy_buffer(bdata);
                attach_new_buffer(num);
        }

        b_free(ft);
}

static void
event_want_update(atomic_int *prev_num, UNUSED vimscript_message_type val)
{
        int const num = nvim_get_current_buf();
        atomic_store(prev_num, num);
        Buffer *bdata = find_buffer(num);

        if (bdata) {
#if 0
                if (update_taglist(bdata, UPDATE_TAGLIST_NORMAL)) {
                        clear_highlight(bdata);
                        update_highlight(bdata, HIGHLIGHT_UPDATE);
                }
#endif
        } else {
                if (have_seen_bufnum(num))
                        attach_new_buffer(num);
                else
                        echo("Failed to find buffer! %d -> p: %p", num, (void *)bdata);
        }
}

static void
event_force_update(atomic_int *prev_num)
{
        UNUSED struct timer t = STRUCT_TIMER_INITIALIZER;
        TIMER_START_BAR(&t);
        int const num = nvim_get_current_buf();
        atomic_store_explicit(prev_num, num, memory_order_release);
        Buffer *bdata = find_buffer(num);

        if (bdata) {
                update_taglist(bdata, UPDATE_TAGLIST_FORCE);
                update_highlight(bdata, HIGHLIGHT_UPDATE_FORCE);
        } else {
                attach_new_buffer(num);
        }
        TIMER_REPORT(&t, "Forced update");
}

static noreturn void
event_halt(bool const nvim_exiting)
{
        if (!nvim_exiting) {
                clear_highlight(, true);
        }
#ifdef DOSISH
        exit(0);
#else
        extern void exit_cleanup(void);

        /* If the user explicitly gives the Vim command to stop the plugin, the loop
         * returns and we clean everything up. We don't do this when Neovim exits
         * because it freezes until all child processes have stopped. */
        if (!nvim_exiting) {
                eprintf("Right, cleaning up!");
                exit_cleanup();
                eprintf("All clean!");
        }

        pthread_kill(event_loop_thread, KILL_SIG);
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

/*======================================================================================*/

static void
attach_new_buffer(int num)
{
        UNUSED struct timer t = STRUCT_TIMER_INITIALIZER;
        Buffer *bdata = new_buffer(num);

        if (bdata) {
                TIMER_START(&t);
                nvim_buf_attach(num);

                get_initial_lines(bdata);
                get_initial_taglist(bdata);
                update_highlight(bdata, HIGHLIGHT_UPDATE);

                TIMER_REPORT(&t, "initialization");
        } else {
                ECHO("Failed to attach to buffer number %d.", num);
        }
}
