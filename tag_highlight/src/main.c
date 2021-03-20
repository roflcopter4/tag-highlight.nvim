#include "Common.h"
#include "highlight.h"

#include "contrib/p99/p99_futex.h"

#if defined SANITIZE && defined DEBUG
#  include "sanitizer/common_interface_defs.h"
__attribute__((const))
const char *__asan_default_options(void)
{
        /* return "verbosity=1:fast_unwind_on_malloc=0" */
        return "fast_unwind_on_malloc=0"
#if 0
#ifdef SANITIZER_LOG_PLACE
        ":log_path=" SANITIZER_LOG_PLACE
#endif
#endif
        ;
}
#endif

#ifdef DOSISH
#  define WIN_BIN_FAIL(STREAM) \
        err(1, "Failed to change stream \"" STREAM "\" to binary mode.")
const char *program_invocation_name;
const char *program_invocation_short_name;
#endif
#define WAIT_TIME  (3.0)

extern bool         process_exiting;
extern FILE        *cmd_log, *echo_log, *main_log, *mpack_raw;
static struct timer main_timer               = STRUCT_TIMER_INITIALIZER;
p99_futex           first_buffer_initialized = P99_FUTEX_INITIALIZER(0);
char                LOGDIR[SAFE_PATH_MAX];
pthread_t           top_thread;
FILE *talloc_log_file;

extern void           exit_cleanup        (void);
static void           general_init        (void);
static void           platform_init       (char **argv);
static void           get_settings        (void);
static void           open_logs           (void);
static void           quick_cleanup       (void);
static comp_type_t    get_compression_type(void);
static noreturn void *neovim_init         (void *arg);
extern void           run_event_loop      (int fd);
static void initialize_talloc_contexts(void);
static void clean_talloc_contexts(void);

extern void *_mpack_decode_talloc_ctx;
extern void *_mpack_encode_talloc_ctx;
extern void *_event_loop_talloc_ctx;
extern void *_event_handlers_talloc_ctx;
extern void *_nvim_common_talloc_ctx;
extern void *_clang_talloc_ctx;
extern void *_buffer_talloc_ctx;
extern void *_tok_scan_talloc_ctx;
extern void *_update_top_talloc_ctx;
extern void *__bstring_talloc_top_ctx;
static void *main_top_talloc_ctx = NULL;
#define CTX main_top_talloc_ctx

/*======================================================================================*/

int
main(UNUSED int argc, char *argv[])
{
        talloc_disable_null_tracking();
        process_exiting = false;
        TIMER_START(&main_timer);

        /* Accomodate for Win32 */
        platform_init(argv);

        /* This function will ultimately spawn an asynchronous thread that will try to
         * attach to the current buffer, if possible. */
        general_init();

        /* This normally does not return. */
        run_event_loop(STDIN_FILENO);

        /* ... except for when it does. If the user has deliberately stopped the
         * program, we clean up. */
        clean_talloc_contexts();

        return 0;
}

/*======================================================================================*/

static void
platform_init(char **argv)
{
        if (!program_invocation_name)
                program_invocation_name = argv[0];
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

static void
general_init(void)
{
#ifdef DEBUG
        talloc_log_file = safe_fopen_fmt("%s/talloc_report.log", "wb", HOME);
#endif
#if defined SANITIZE && !defined SANITIZER_LOG_PLACE
        {
                char tmp[8192];
                snprintf(tmp, 8192, "%s/sanitizer.log", HOME);
                __sanitizer_set_report_path(tmp);
        }
#endif
        initialize_talloc_contexts();
        open_logs();
        top_thread = pthread_self();
        p99_futex_init(&first_buffer_initialized, 0);
        at_quick_exit(quick_cleanup);
        START_DETACHED_PTHREAD(neovim_init);
}

static void
initialize_talloc_contexts(void)
{
#if 0
        main_top_talloc_ctx        = talloc_named_const(CTX, 0, "Main Top");
        _mpack_encode_talloc_ctx   = talloc_named_const(CTX, 0, "Mpack Encode Top");
        _mpack_decode_talloc_ctx   = talloc_named_const(CTX, 0, "Mpack Decode Top");
        _nvim_common_talloc_ctx    = talloc_named_const(CTX, 0, "Nvim Common Top");
        _clang_talloc_ctx          = talloc_named_const(CTX, 0, "Clang Top");
        _event_loop_talloc_ctx     = talloc_named_const(CTX, 0, "Event Loop Top");
        _event_handlers_talloc_ctx = talloc_named_const(CTX, 0, "Event Handlers Top");
        _buffer_talloc_ctx         = talloc_named_const(CTX, 0, "Buffer Top Context");
        _tok_scan_talloc_ctx       = talloc_named_const(CTX, 0, "Token Scanner Top Context");
        _update_top_talloc_ctx     = talloc_named_const(CTX, 0, "Update Top Context");
        __bstring_talloc_top_ctx   = talloc_named_const(CTX, 0, "Bstring Top Context");
#endif
}

static void
clean_talloc_contexts(void)
{
#if 0
#ifdef DEBUG
        talloc_report_full(main_top_talloc_ctx, talloc_log_file);
        fclose(talloc_log_file);
#endif
        talloc_free(main_top_talloc_ctx);
#endif
}

/*======================================================================================*/
/* General Setup */

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
        mpack_log = safe_fopen_fmt("%s/mpack.log", "wb", LOGDIR);
        mpack_raw = safe_fopen_fmt("%s/mpack_raw", "wb", LOGDIR);
        setvbuf(mpack_raw, NULL, 0, _IONBF);
        cmd_log   = safe_fopen_fmt("%s/commandlog.log", "wb", LOGDIR);
        echo_log  = safe_fopen_fmt("%s/echo.log", "wb", LOGDIR);
        main_log  = safe_fopen_fmt("%s/buf.log", "wb+", LOGDIR);

        /* clang_log_file = safe_fopen_fmt("%s/clang.log", "wb", BS(settings.cache_dir)); */
#endif
}

static noreturn void *
neovim_init(UNUSED void *arg)
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
        settings.comp_type      = get_compression_type();
        settings.cache_dir      = nvim_call_function(B(PKG "install_info#GetCachePath"), E_STRING).ptr;
        settings.comp_level     = nvim_get_var(B(PKG "compression_level"), E_NUM       ).num;
        settings.ctags_args     = nvim_get_var(B(PKG "ctags_args"),        E_STRLIST   ).ptr;
        settings.ctags_bin      = nvim_get_var(B(PKG "ctags_bin"),         E_STRING    ).ptr;
        settings.enabled        = nvim_get_var(B(PKG "enabled"),           E_BOOL      ).num;
        settings.ignored_ftypes = nvim_get_var(B(PKG "ignore"),            E_STRLIST   ).ptr;
        settings.ignored_tags   = nvim_get_var(B(PKG "ignored_tags"),      E_MPACK_DICT).ptr;
        settings.norecurse_dirs = nvim_get_var(B(PKG "norecurse_dirs"),    E_STRLIST   ).ptr;
        settings.settings_file  = nvim_get_var(B(PKG "settings_file"),     E_STRING    ).ptr;
        settings.verbose        = nvim_get_var(B(PKG "verbose"),           E_BOOL      ).num;
#ifdef DEBUG /* Verbose output should be forcibly enabled in debug mode. */
        settings.verbose = true;
#endif

        if (!settings.enabled || !settings.ctags_bin)
                exit(0);

#if 0 /* Don't rock the boat I guess. */
        talloc_steal(settings, settings.cache_dir);
        talloc_steal(settings, settings.ctags_bin);
        talloc_steal(settings, settings.settings_file);
        talloc_steal(settings, settings.ctags_args);
        talloc_steal(settings, settings.ignored_ftypes);
        talloc_steal(settings, settings.norecurse_dirs);
        talloc_steal(settings, settings.ignored_tags);
        talloc_steal(settings, settings.order);
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

        talloc_free(tmp);
        return ret;
}

/*======================================================================================*/

extern void clear_bnode(void *vdata, bool blocking);

/*
 * Free everything at exit for debugging purposes.
 */
void
exit_cleanup(void)
{
        extern linked_list *buffer_list;
        static atomic_flag flg = ATOMIC_FLAG_INIT;
        if (atomic_flag_test_and_set(&flg))
                return;

        if (!process_exiting)
                LL_FOREACH_F (buffer_list, node)
                        clear_bnode(node->data, true);

        talloc_free(buffer_list);
        talloc_free(top_dirs);
        talloc_free(ftdata);
        talloc_free(settings.cache_dir);
        talloc_free(settings.ctags_args);
        talloc_free(settings.ctags_bin);
        talloc_free(settings.ignored_ftypes);
        talloc_free(settings.ignored_tags);
        talloc_free(settings.norecurse_dirs);
        talloc_free(settings.order);
        talloc_free(settings.settings_file);

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
