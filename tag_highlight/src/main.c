#include "Common.h"
#include "highlight.h"

#include "contrib/p99/p99_futex.h"

#ifdef DOSISH
#  define WIN_BIN_FAIL(STREAM) \
        err(1, "Failed to change stream \"" STREAM "\" to binary mode.")
const char *program_invocation_short_name;
#endif
#define WAIT_TIME  (3.0)

extern FILE        *cmd_log, *echo_log, *main_log, *mpack_raw;
static struct timer main_timer               = TIMER_STATIC_INITIALIZER;
p99_futex           first_buffer_initialized = P99_FUTEX_INITIALIZER(0);
char                LOGDIR[SAFE_PATH_MAX];
pthread_t           top_thread;

static void           init                (char **argv);
static void           platform_init       (char **argv);
static void           get_settings        (void);
static void           open_logs           (void);
static void           exit_cleanup        (void);
static void           quick_cleanup       (void);
static comp_type_t    get_compression_type(void);
static noreturn void *main_initialization (void *arg);
extern void           run_event_loop      (int fd);

/*======================================================================================*/

int
main(UNUSED int argc, char *argv[])
{
        TIMER_START(&main_timer);

        /* This function will ultimately spawn an asynchronous thread that will try to
         * attach to the current buffer, if possible. */
        init(argv);

        /* This normally does not return. */
        run_event_loop(STDIN_FILENO);

        /* If the user explicitly gives the Vim command to stop the plugin, the loop
         * returns and we clean everything up. We don't do this when Neovim exits because
         * it freezes until all child processes have stopped. This delay is noticeable
         * and annoying, so normally we just call quick_exit or _Exit instead. */
        eprintf("Right, cleaning up!");
        return EXIT_SUCCESS;
}

/*======================================================================================*/
/* General Setup */

static void
init(char **argv)
{
        top_thread = pthread_self();
        platform_init(argv);
        open_logs();
        p99_futex_init(&first_buffer_initialized, 0);
        atexit(exit_cleanup);
        at_quick_exit(quick_cleanup);
        START_DETACHED_PTHREAD(main_initialization);
}

static void
platform_init(char **argv)
{
        if (!program_invocation_short_name)
                program_invocation_short_name = basename(argv[0]);
#ifdef DOSISH
        HOME = getenv("USERPROFILE");

        /* Set the standard streams to binary mode on Windows */
        if (_setmode(STDIN_FILENO, O_BINARY) == (-1))
                WIN_BIN_FAIL("stdin");
        if (_setmode(STDOUT_FILENO, O_BINARY) == (-1))
                WIN_BIN_FAIL("stdout");
        if (_setmode(STDERR_FILENO, O_BINARY) == (-1))
                WIN_BIN_FAIL("stderr");
#else
        HOME = getenv("HOME");
#endif
}

/*
 * Open debug logs
 */
static void
open_logs(void)
{
#if defined DEBUG && defined DEBUG_LOGS
        extern char LOGDIR[];
        snprintf(LOGDIR, SAFE_PATH_MAX, "%s/.tag_highlight_log", HOME);
        mkdir(LOGDIR, 0777);
        mpack_log      = safe_fopen_fmt("%s/mpack.log", "wb", LOGDIR);
        mpack_raw      = safe_fopen_fmt("%s/mpack_raw", "wb", LOGDIR);
        setvbuf(mpack_raw, NULL, 0, _IONBF);
        cmd_log        = safe_fopen_fmt("%s/commandlog.log", "wb", LOGDIR);
        echo_log       = safe_fopen_fmt("%s/echo.log", "wb", LOGDIR);
        main_log       = safe_fopen_fmt("%s/buf.log", "wb+", LOGDIR);

        /* clang_log_file = safe_fopen_fmt("%s/clang.log", "wb", BS(settings.cache_dir)); */
#endif
}

static noreturn void *
main_initialization(UNUSED void *arg)
{
        get_settings();
        nvim_set_client_info(B(PKG), 0, 4, B("alpha"));

        int     initial_buf = nvim_get_current_buf();
        Buffer *bdata       = new_buffer(initial_buf);

        if (bdata) {
                nvim_buf_attach(bdata->num);
                get_initial_lines(bdata);
                get_initial_taglist(bdata);
                update_highlight(bdata);

                TIMER_REPORT(&main_timer, "main initialization");
                P99_FUTEX_COMPARE_EXCHANGE(&first_buffer_initialized, value,
                    true, 1U, 0U, P99_FUTEX_MAX_WAITERS);
        }

        pthread_exit();
}

/*
 * Grab user settings defined their .vimrc and/or the defaults from the vimscript plugin.
 */
static void
get_settings(void)
{
        settings.enabled        = nvim_get_var(B(PKG "enabled"),   E_BOOL  ).num;
        settings.ctags_bin      = nvim_get_var(B(PKG "ctags_bin"), E_STRING).ptr;
        if (!settings.enabled || !settings.ctags_bin)
                exit(0);
        settings.cache_dir      = nvim_call_function(B(PKG "install_info#GetCachePath"), E_STRING).ptr;
        settings.comp_type      = get_compression_type();
        settings.comp_level     = nvim_get_var(B(PKG "compression_level"), E_NUM       ).num;
        settings.ctags_args     = nvim_get_var(B(PKG "ctags_args"),        E_STRLIST   ).ptr;
        settings.ignored_ftypes = nvim_get_var(B(PKG "ignore"),            E_STRLIST   ).ptr;
        settings.ignored_tags   = nvim_get_var(B(PKG "ignored_tags"),      E_MPACK_DICT).ptr;
        settings.norecurse_dirs = nvim_get_var(B(PKG "norecurse_dirs"),    E_STRLIST   ).ptr;
        settings.settings_file  = nvim_get_var(B(PKG "settings_file"),     E_STRING    ).ptr;
        settings.verbose        = nvim_get_var(B(PKG "verbose"),           E_BOOL      ).num;
#ifdef DEBUG /* Verbose output should be forcibly enabled in debug mode. */
        settings.verbose = true;
#endif
}

static comp_type_t
get_compression_type(void)
{
        bstring    *tmp = nvim_get_var(B(PKG "compression_type"), E_STRING).ptr;
        comp_type_t ret = COMP_NONE;

        if (b_iseq_lit_any(tmp, "gzip", "gz"))
                ret = COMP_GZIP;
        else if (b_iseq_lit_any(tmp, "lzma", "xz"))
        {
#ifdef LZMA_SUPPORT
                ret = COMP_LZMA;
#else
                ret = COMP_GZIP;
                shout("Compression type is set to '%s', but only gzip is "
                      "supported in this build. Defaulting to 'gzip'.", BS(tmp));
#endif
        }
        else if (b_iseq_lit(tmp, "none"))
                NOP;
        else
                shout("Warning: unrecognized compression type \"%s\", "
                      "defaulting to no compression.", BS(tmp));

        b_destroy(tmp);
        return ret;
}

/*======================================================================================*/

extern void destroy_bnode(void *vdata);

/*
 * Free everything at exit for debugging purposes.
 */
static void
exit_cleanup(void)
{
        extern linked_list *buffer_list;
        /* extern genlist *top_dirs; */
        extern b_list  *seen_files;
        extern bool    process_exiting;
        process_exiting = true;

        b_destroy(settings.cache_dir);
        b_destroy(settings.ctags_bin);
        b_destroy(settings.settings_file);
        b_list_destroy(seen_files);
        b_list_destroy(settings.ctags_args);
        b_list_destroy(settings.norecurse_dirs);
        b_list_destroy(settings.ignored_ftypes);

#if 0
        for (unsigned i = 0; i < buffers.mlen; ++i)
                destroy_bufdata(buffers.lst + i);  
#endif
        eprintf("have %d in lst\n", buffer_list->qty);
        /* destroy_buffer(find_buffer(1)); */
#if 0
        LL_FOREACH_F (buffer_list, node) {
                destroy_bnode(node);
                /* node->data = NULL; */
        }
#endif
#if 0
        for (int i = 0, n = buffer_list->qty; i < n; ++i) {
                ll_delete_at(buffer_list, 0);
        }
#endif
        /* free(buffer_list->intern); */
        /* free(buffer_list);         */

        ll_destroy(buffer_list);

        if (top_dirs) {
                free(top_dirs->lst);
                free(top_dirs);
        }

        for (unsigned i = 0; i < ftdata_len; ++i) {
                struct filetype *ft = &ftdata[i];
                if (ft->initialized) {
                        if (ft->ignored_tags) {
                                b_list *igt = ft->ignored_tags;
                                for (unsigned x = 0; x < igt->qty; ++x)
                                        if (igt->lst[x]->flags & BSTR_MASK_USR1)
                                                b_destroy(igt->lst[x]);
                                free(igt->lst);
                                free(igt);
                        }
                        b_list_destroy(ft->equiv);
                        b_destroy(ft->order);
                        b_destroy(ft->restore_cmds);
                }
        }

        mpack_dict_destroy(settings.order);
        mpack_dict_destroy(settings.ignored_tags);
        quick_cleanup();
}

/* 
 * Basically just close any logs.
 */
static void
quick_cleanup(void)
{
        if (mpack_log)
                fclose(mpack_log);
        if (cmd_log)
                fclose(cmd_log);
        if (main_log)
                fclose(main_log);
        if (echo_log)
                fclose(echo_log);
}
