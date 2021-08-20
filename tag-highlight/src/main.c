#include "Common.h"
#include "highlight.h"

#include "contrib/p99/p99_futex.h"

#if defined SANITIZE && defined DEBUG
#  include "sanitizer/common_interface_defs.h"
__attribute__((const))
const char *__asan_default_options(void)
{
        return "fast_unwind_on_malloc=0";
}
#endif

#ifdef DOSISH
#  define WIN_BIN_FAIL(STREAM) \
        err(1, "Failed to change stream \"" STREAM "\" to binary mode.")
#  define CMD_SUFFIX ".exe"
#else
#  define CMD_SUFFIX
#endif
#ifndef STDIN_FILENO
#  define STDIN_FILENO  (0)
#  define STDOUT_FILENO (1)
#  define STDERR_FILENO (2)
#endif

extern FILE *main_log, *mpack_raw;
FILE        *talloc_log_file;
pthread_t    top_thread;
char         LOGDIR[SAFE_PATH_MAX];
p99_futex    first_buffer_initialized = P99_FUTEX_INITIALIZER(0);
static struct timer main_timer        = STRUCT_TIMER_INITIALIZER;

extern void run_event_loop(int fd);
extern void exit_cleanup(void);
static void general_init(void);
static void platform_init(char **argv);
static void get_settings(void);
static void open_logs(void);
static void quick_cleanup(void);
static void initialize_talloc_contexts(void);
static void clean_talloc_contexts(void);
static comp_type_t    get_compression_type(void);
static bstring       *get_go_binary(void);
static noreturn void *neovim_init(void *arg);

extern void *mpack_decode_talloc_ctx_;
extern void *mpack_encode_talloc_ctx_;
extern void *event_loop_talloc_ctx_;
extern void *event_handlers_talloc_ctx_;
extern void *nvim_common_talloc_ctx_;
extern void *clang_talloc_ctx_;
extern void *buffer_talloc_ctx_;
extern void *tok_scan_talloc_ctx_;
extern void *update_top_talloc_ctx_;
extern void *BSTR_talloc_top_ctx;
UNUSED static void *main_top_talloc_ctx_ = NULL;
#define CTX main_top_talloc_ctx

/*======================================================================================*/

int
main(UNUSED int argc, char *argv[])
{
        talloc_disable_null_tracking();
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

        return EXIT_SUCCESS;
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
        talloc_log_file = safe_fopen_fmt("wb", "%s/talloc_report.log", HOME);
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
        START_DETACHED_PTHREAD(neovim_init);
}

static void
initialize_talloc_contexts(void)
{
#if 0
#define _CTX main_top_talloc_ctx_
        main_top_talloc_ctx_       = talloc_named_const(NULL, 0, "Main Top");
        mpack_encode_talloc_ctx_   = talloc_named_const(_CTX, 0, "Mpack Encode Top");
        mpack_decode_talloc_ctx_   = talloc_named_const(_CTX, 0, "Mpack Decode Top");
        nvim_common_talloc_ctx_    = talloc_named_const(_CTX, 0, "Nvim Common Top");
        clang_talloc_ctx_          = talloc_named_const(_CTX, 0, "Clang Top");
        event_loop_talloc_ctx_     = talloc_named_const(_CTX, 0, "Event Loop Top");
        event_handlers_talloc_ctx_ = talloc_named_const(_CTX, 0, "Event Handlers Top");
        buffer_talloc_ctx_         = talloc_named_const(_CTX, 0, "Buffer Top Context");
        tok_scan_talloc_ctx_       = talloc_named_const(_CTX, 0, "Token Scanner Top Context");
        update_top_talloc_ctx_     = talloc_named_const(_CTX, 0, "Update Top Context");
        BSTR_talloc_top_ctx        = talloc_named_const(_CTX, 0, "Bstring Top Context");
#undef _CTX
#endif
}

static void
clean_talloc_contexts(void)
{
#if 0
        talloc_report_full(main_top_talloc_ctx_, talloc_log_file);
        fclose(talloc_log_file);
        talloc_free(main_top_talloc_ctx_);
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
#ifdef DEBUG
        bstring *tmp = b_fromcstr(program_invocation_name);
        bstring *dir = b_dirname(tmp);
        
        mpack_log = safe_fopen_fmt("web", "%s" PATHSEP_STR "mpack.log", BS(dir));
        mpack_raw = safe_fopen_fmt("web", "%s" PATHSEP_STR "mpack_raw", BS(dir));
        
        talloc_free(tmp);
        talloc_free(dir);

        at_quick_exit(quick_cleanup);
#endif
}

static noreturn void *
neovim_init(UNUSED void *arg)
{
        get_settings();
        nvim_set_client_info(B(PKG), 0, 5, B("alpha"));

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
        settings.go_binary      = get_go_binary();
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
                exit(EXIT_SUCCESS);

        /* If I were smart, I might find a way to avoid needing all these calls.
         * Unfortunately, I am not smart. */
        settings.talloc_ctx = talloc_named_const(NULL, 0, "Settings talloc context.");
        talloc_steal(settings.talloc_ctx, settings.go_binary);
        talloc_steal(settings.talloc_ctx, settings.cache_dir);
        talloc_steal(settings.talloc_ctx, settings.ctags_bin);
        talloc_steal(settings.talloc_ctx, settings.settings_file);
        talloc_steal(settings.talloc_ctx, settings.ctags_args);
        talloc_steal(settings.talloc_ctx, settings.ignored_ftypes);
        talloc_steal(settings.talloc_ctx, settings.norecurse_dirs);
        talloc_steal(settings.talloc_ctx, settings.ignored_tags);
        talloc_steal(settings.talloc_ctx, settings.order);
}

/*--------------------------------------------------------------------------------------*/

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
                echo("Compression type is set to '%s', but only gzip is "
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

static bstring *
get_go_binary(void)
{
        struct stat st;

        bstring *go_binary = nvim_call_function(B(PKG "install_info#GetBinaryPath"), E_STRING).ptr;
        b_catlit(go_binary, "/golang" CMD_SUFFIX);

        if (stat(BS(go_binary), &st) != 0) {
                b_free(go_binary);
                go_binary = NULL;
        }
        
        return go_binary;
}

/*======================================================================================*/

extern void clear_bnode(void *vdata, bool blocking);

/*
 * Free everything at exit for debugging purposes.
 */
void
exit_cleanup(void)
{
        extern bool         process_exiting;
        extern linked_list *buffer_list;
        static atomic_flag  flg = ATOMIC_FLAG_INIT;

        if (atomic_flag_test_and_set(&flg))
                return;

        if (!process_exiting)
                LL_FOREACH_F (buffer_list, node)
                        clear_bnode(node->data, true);

        talloc_free(buffer_list);

        for (unsigned i = 0; i < ftdata_len; ++i) {
                talloc_free(ftdata[i]->ignored_tags);
                talloc_free(ftdata[i]->restore_cmds);
                talloc_free(ftdata[i]);
                ftdata[i] = NULL;
        }

        talloc_free(ftdata);
        talloc_free(top_dirs);
        talloc_free(settings.talloc_ctx);

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
        if (mpack_raw)
                fclose(mpack_raw);
}
