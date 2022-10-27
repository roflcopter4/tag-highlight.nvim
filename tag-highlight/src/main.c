#include "Common.h"
#include "highlight.h"

#include "contrib/p99/p99_futex.h"

#if 0
#  define JEMALLOC_MANGLE
#  include <jemalloc/jemalloc.h>
#endif

#if defined SANITIZE && defined DEBUG
#  include "sanitizer/common_interface_defs.h"
__attribute__((__const__)) const char *
__asan_default_options(void)
{
      return "fast_unwind_on_malloc=0";
}
#endif

#ifdef _WIN32
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
#ifdef __clang__
#  define CLANG_ENUM_SIZE(size) : size
#else
#  define CLANG_ENUM_SIZE(size)
#endif
#ifdef noreturn
#  error "noreturn is defined somehow??"
#endif

#ifdef __attribute__
#  error "??"
#endif

/*--------------------------------------------------------------------------------------*/

enum log_filename_id CLANG_ENUM_SIZE(uint32_t){
    LOG_ID_MPACK_MSG = 0,
    LOG_ID_MPACK_RAW_READ,
    LOG_ID_MPACK_RAW_WRITE,

    LOG_ID_LAST_ELEMENT = LOG_ID_MPACK_RAW_WRITE
};

alignas(512)
char log_filenames[LOG_ID_LAST_ELEMENT + 1][4096];

extern FILE *main_log;
extern FILE *mpack_raw_read;
FILE        *talloc_log_file;
pthread_t    top_thread;
char         LOGDIR[SAFE_PATH_MAX];
p99_futex    first_buffer_initialized = P99_FUTEX_INITIALIZER(0);

static struct timer main_timer = STRUCT_TIMER_INITIALIZER;

/*--------------------------------------------------------------------------------------*/

extern void talloc_emergency_library_init(void);

extern void           run_event_loop(int fd, char *servername);
extern void           exit_cleanup(void);
static void           general_init(char const *const *argv);
static void           platform_init(char const *const *argv);
static void           open_logs(char const *cache_dir);
static void           get_settings(void);
static void           quick_cleanup(void);
static void           clean_talloc_contexts(void);
static comp_type_t    get_compression_type(void);
static bstring       *get_go_binary(void);
NORETURN static void *neovim_init(void *arg);

extern void        *mpack_decode_talloc_ctx_;
extern void        *mpack_encode_talloc_ctx_;
extern void        *event_loop_talloc_ctx_;
extern void        *event_handlers_talloc_ctx_;
extern void        *nvim_common_talloc_ctx_;
extern void        *clang_talloc_ctx_;
extern void        *buffer_talloc_ctx_;
extern void        *tok_scan_talloc_ctx_;
extern void        *update_top_talloc_ctx_;
extern void        *BSTR_talloc_top_ctx;
UNUSED static void *main_top_talloc_ctx_ = NULL;
#define CTX main_top_talloc_ctx_

/*======================================================================================*/

int
main(UNUSED int argc, char *argv[])
{
      talloc_emergency_library_init();
      talloc_disable_null_tracking();

      TIMER_START(&main_timer);

      /* Accomodate for Win32 */
      platform_init((char const *const *)argv);

      /* This function will ultimately spawn an asynchronous thread that will try to
       * attach to the current buffer, if possible. */
      general_init((char const *const *)argv);

      /* This normally does not return. */
      run_event_loop(STDIN_FILENO, argv[2]);

      /* ... except for when it does. If the user has deliberately stopped the
       * program, we clean up. */

      exit_cleanup();
      clean_talloc_contexts();

      //malloc_stats_print(NULL, NULL, "");
      eprintf("All done!\n");
      return EXIT_SUCCESS;
}

/*======================================================================================*/

static void
platform_init(char const *const *argv)
{
      if (!program_invocation_name)
            program_invocation_name = (char *)argv[0];
      if (!program_invocation_short_name)
            program_invocation_short_name = basename(argv[0]);
#ifdef _WIN32
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
general_init(char const *const *argv)
{
#if defined SANITIZE && !defined SANITIZER_LOG_PLACE
      {
            char tmp[8192];
            snprintf(tmp, 8192, "%s/sanitizer.log", argv[1]);
            __sanitizer_set_report_path(tmp);
      }
#endif
      if (!argv[1] || !argv[2])
            errx(1, "Bad arguments.");
      top_thread = pthread_self();
      open_logs(argv[1]);
      p99_futex_init(&first_buffer_initialized, 0);
      START_DETACHED_PTHREAD(neovim_init);
}

INITIALIZER_HACK_N(initialize_talloc_contexts, 200)
{
#if defined DEBUG && 0
# define _CTX main_top_talloc_ctx_
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
# undef _CTX
#endif
}

static void
clean_talloc_contexts(void)
{
      static atomic_flag onceflg = ATOMIC_FLAG_INIT;
      if (atomic_flag_test_and_set(&onceflg))
            return;
      if (talloc_log_file)
            fclose(talloc_log_file);
      talloc_free(main_top_talloc_ctx_);
}

/*======================================================================================*/
/* General Setup */

/*
 * Open debug logs
 */
static void
open_logs(UNUSED char const *cache_dir)
{
#ifdef DEBUG
#if 0
      bstring *tmp = b_fromcstr(program_invocation_name);
      bstring *dir = b_dirname(tmp);
      size_t len = settings.cache_dir->slen;
      memcpy(tmp, settings.cache_dir->data, len);
      tmp[len++] = '/';
      memcpy(tmp + len, ("mpack_log_XXXXXX"), 17);
      (void)tmpnam(tmp);
#endif

      //talloc_log_file = safe_fopen_fmt("wb", "%s/talloc_report.log", cache_dir);

#ifdef DEBUG_LOGS
      char tmp[PATH_MAX + 1];
      braindead_tempname(tmp, cache_dir, "mpack_", NULL);
      warnd("Opening log at base \"%s\"", tmp);

      /* Sprintf is fine because PATH_MAX. */
      sprintf(log_filenames[LOG_ID_MPACK_MSG],       "%s_msg.log", tmp);
      sprintf(log_filenames[LOG_ID_MPACK_RAW_READ],  "%s_raw_read.log", tmp);
      sprintf(log_filenames[LOG_ID_MPACK_RAW_WRITE], "%s_raw_write.log", tmp);
      mpack_log       = safe_fopen(log_filenames[LOG_ID_MPACK_MSG], "wbex");
      mpack_raw_read  = safe_fopen(log_filenames[LOG_ID_MPACK_RAW_READ], "wbex");
      mpack_raw_write = safe_fopen(log_filenames[LOG_ID_MPACK_RAW_WRITE], "wbex");
#endif

#if 0
      talloc_free(tmp);
      talloc_free(dir);
#endif

      at_quick_exit(quick_cleanup);
#endif
}

static NORETURN void *
neovim_init(void *varg) //NOLINT(readability-function-cognitive-complexity)
{
      extern void global_previous_buffer_set(int num);
      char const *cache_dir = varg;

      get_settings();
      ALWAYS_ASSERT(b_iseq_cstr(settings.cache_dir, cache_dir));

      nvim_set_client_info(B(PKG), 0, 5, B("alpha"));

      int     initial_buf = nvim_get_current_buf();
      Buffer *bdata       = new_buffer(initial_buf);

      if (bdata) {
            global_previous_buffer_set((int)bdata->num);
            nvim_buf_attach(bdata->num);
            get_initial_lines(bdata);
            get_initial_taglist(bdata);
            update_highlight(bdata);

            TIMER_REPORT(&main_timer, "main initialization");
            P99_FUTEX_COMPARE_EXCHANGE(&first_buffer_initialized,
                                       value, true, 1U, 0U,
                                       P99_FUTEX_MAX_WAITERS);
      }

      pthread_exit();
}

/*
 * Grab user settings defined their .vimrc and/or the defaults from the vimscript plugin.
 */
static void
get_settings(void)
{
      settings.cache_dir = nvim_call_function(B(PKG "install_info#GetCachePath"), E_STRING).ptr;
      settings.go_binary = get_go_binary();
      settings.comp_type = get_compression_type();
      settings.comp_level     = nvim_get_var(B(PKG "compression_level"), E_NUM       ).num;
      settings.ctags_args     = nvim_get_var(B(PKG "ctags_args"),        E_STRLIST   ).ptr;
      settings.ctags_bin      = nvim_get_var(B(PKG "ctags_bin"),         E_STRING    ).ptr;
      settings.enabled        = nvim_get_var(B(PKG "enabled"),           E_BOOL      ).num;
      settings.ignored_ftypes = nvim_get_var(B(PKG "ignore"),            E_STRLIST   ).ptr;
      settings.ignored_tags   = nvim_get_var(B(PKG "ignored_tags"),      E_MPACK_DICT).ptr;
      settings.norecurse_dirs = nvim_get_var(B(PKG "norecurse_dirs"),    E_STRLIST   ).ptr;
      settings.settings_file  = nvim_get_var(B(PKG "settings_file"),     E_STRING    ).ptr;
      settings.verbose        = nvim_get_var(B(PKG "verbose"),           E_BOOL      ).num;
      settings.run_ctags      = nvim_get_var(B(PKG "run_ctags"),         E_BOOL      ).num;

#ifdef DEBUG /* Verbose output should be forcibly enabled in debug mode. */
      settings.verbose = true;
#endif

      if (!settings.enabled)
            exit(EXIT_SUCCESS);

      /* If I were smart, I might find a way to avoid needing all these calls.
       * Unfortunately, I am not smart. */
      settings.talloc_ctx = talloc_named_const(CTX, 0, "Settings talloc context.");
      talloc_steal(settings.talloc_ctx, settings.cache_dir);
      talloc_steal(settings.talloc_ctx, settings.go_binary);
      talloc_steal(settings.talloc_ctx, settings.ctags_args);
      talloc_steal(settings.talloc_ctx, settings.ctags_bin);
      talloc_steal(settings.talloc_ctx, settings.ignored_ftypes);
      talloc_steal(settings.talloc_ctx, settings.ignored_tags);
      talloc_steal(settings.talloc_ctx, settings.norecurse_dirs);
      talloc_steal(settings.talloc_ctx, settings.settings_file);
}

/*--------------------------------------------------------------------------------------*/

static comp_type_t
get_compression_type(void)
{
      bstring    *tmp = nvim_get_var(B(PKG "compression_type"), E_STRING).ptr;
      comp_type_t ret = COMP_NONE;

      if (b_iseq_lit_any(tmp, "gzip", "gz"))
            ret = COMP_GZIP;
      else if (b_iseq_lit_any(tmp, "lzma", "xz")) {
#ifdef LZMA_SUPPORT
            ret = COMP_LZMA;
#else
            ret = COMP_GZIP;
            warnd("Compression type is set to '%s', but only gzip is "
                  "supported in this build. Defaulting to 'gzip'.",
                  BS(tmp));
#endif
      } else if (b_iseq_lit(tmp, "none"))
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

      bstring *go_binary =
          nvim_call_function(B(PKG "install_info#GetBinaryPath"), E_STRING).ptr;
      b_catlit(go_binary, PATHSEP_STR "golang" CMD_SUFFIX);

      if (stat(BS(go_binary), &st) != 0) {
            b_free(go_binary);
            go_binary = NULL;
      }

      return go_binary;
}

/*======================================================================================*/

/*
 * Free everything at exit for debugging purposes.
 */
void
exit_cleanup(void)
{
      extern linked_list *buffer_list;
      static atomic_flag  flg = ATOMIC_FLAG_INIT;

      if (atomic_flag_test_and_set(&flg))
            return;

      if (talloc_log_file)
            talloc_report_full(main_top_talloc_ctx_, talloc_log_file);
      TALLOC_FREE(buffer_list);
      TALLOC_FREE(top_dirs);
      TALLOC_FREE(ftdata);
      TALLOC_FREE(settings.talloc_ctx);

      quick_cleanup();
}


static NORETURN void *
dumb_thread_wrapper(UNUSED void *vdata)
{
      pthread_exit();
}

/*
 * Basically just close any logs.
 */
static void
quick_cleanup(void)
{
      if (talloc_log_file)
            talloc_report_full(main_top_talloc_ctx_, talloc_log_file);
      pthread_t pids[3];
      memset(&pids, 0, sizeof(pids));

      if (mpack_log) {
            fclose(mpack_log);
            mpack_log = NULL;
            pthread_create(&pids[0], NULL, dumb_thread_wrapper, log_filenames[LOG_ID_MPACK_MSG]);
      }
      if (mpack_raw_write) {
            fclose(mpack_raw_write);
            mpack_raw_write = NULL;
            pthread_create(&pids[1], NULL, dumb_thread_wrapper, log_filenames[LOG_ID_MPACK_RAW_READ]);
      }
      if (mpack_raw_read) {
            fclose(mpack_raw_read);
            mpack_raw_read = NULL;
            pthread_create(&pids[2], NULL, dumb_thread_wrapper, log_filenames[LOG_ID_MPACK_RAW_WRITE]);
      }

      //if (pids[0])
            pthread_join(pids[0], NULL);
      //if (pids[1])
            pthread_join(pids[1], NULL);
      //if (pids[2])
            pthread_join(pids[2], NULL);
}
