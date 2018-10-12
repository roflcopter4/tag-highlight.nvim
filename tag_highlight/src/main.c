#include "tag_highlight.h"

#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "p99/p99_defarg.h"
#include "clang/clang.h"

#ifdef DOSISH
#  define WIN_BIN_FAIL(STREAM) \
        err(1, "Failed to change " STREAM "to binary mode.")
#endif
#ifdef HAVE_PAUSE
#  define PAUSE() pause()
#else
#  define PAUSE() do fsleep(1000000.0); while (1)
#endif

#define SIGHANDLER_QUICK  (1)
#define SIGHANDLER_NORMAL (2)
#define WAIT_TIME         (3.0)

extern FILE      *cmd_log, *echo_log, *main_log;
extern pthread_t  top_thread;
pthread_t         top_thread;
char LOGDIR[PATH_MAX+1];

static void          platform_init(char **argv);
static void          get_settings(void);
static void          open_logs(void);
static void          exit_cleanup(void);
static void          quick_cleanup(void);
static comp_type_t   get_compression_type(int fd);
static void          controlled_exit(UNUSED int notused);
static noreturn void sig_handler(UNUSED int notused);

#define get_compression_type(...) P99_CALL_DEFARG(get_compression_type, 1, __VA_ARGS__)
#define get_compression_type_defarg_0() (0)

#include "my_p99_common.h"

/*======================================================================================*/

#if 0
#include "lang/golang/src/tag_highlight/tag_highlight.h"
#endif

int
main(UNUSED int argc, char *argv[])
{
        struct timer main_timer;
        TIMER_START(main_timer);
        top_thread = pthread_self();
        platform_init(argv);
        launch_event_loop();
        get_settings();
        open_logs();

        int initial_buf = 0;

        /* Try to initialize a buffer. If the current one isn't recognized, keep
         * trying periodically until we see one that is. In case we never
         * recognize a buffer, the wait time between tries is modestly long. */
        for (unsigned attempts = 0; buffers.mkr == 0; ++attempts) {
                if (attempts > 0) {
                        if (buffers.bad_bufs.qty)
                                buffers.bad_bufs.qty = 0;
                        fsleep(WAIT_TIME);
                }

                initial_buf = nvim_get_current_buf();
                if (new_buffer(,initial_buf))
                        break;
        }

        struct bufdata *bdata = find_buffer(initial_buf);
        nvim_buf_attach(BUFFER_ATTACH_FD, bdata->num);
        get_initial_lines(bdata);
        get_initial_taglist(bdata);
        update_highlight(bdata);

        TIMER_REPORT(main_timer, "main initialization");
        nvim_set_client_info(,B("tag_highlight"), 0, 1, B("alpha"));
        at_quick_exit(quick_cleanup);
        atexit(exit_cleanup);
        /* launch_libclang_waiter(); */

        PAUSE();
        /* exit_cleanup(); */
        exit(0);
}

static void
platform_init(char **argv)
{
#ifdef DOSISH
        HOME = getenv("USERPROFILE");
        extern const char *program_invocation_short_name;
        program_invocation_short_name = basename(argv[0]);

        /* Set the standard streams to binary mode in Windows. Don't bother with
         * signals, it's not worth the effort. */
        if (_setmode(0, O_BINARY) == (-1))
                WIN_BIN_FAIL("stdin");
        if (_setmode(1, O_BINARY) == (-1))
                WIN_BIN_FAIL("stdout");
        if (_setmode(2, O_BINARY) == (-1))
                WIN_BIN_FAIL("stderr");
#else
        (void)argv;
        HOME = getenv("HOME");
        struct sigaction temp;
        memset(&temp, 0, sizeof(temp));
        temp.sa_handler = controlled_exit;
        sigaction(SIGUSR1, &temp, NULL);
        temp.sa_handler = sig_handler;
        sigaction(SIGINT,  &temp, NULL); // Sigint, sigterm and sigpipe
        sigaction(SIGPIPE, &temp, NULL); // are all possible signals when
        sigaction(SIGTERM, &temp, NULL); // nvim exits.
#endif
}

/**
 * Grab user settings defined their .vimrc or the vimscript plugin.
 */
static void
get_settings(void)
{
        settings.enabled        = nvim_get_var(,B(PKG "enabled"),   E_BOOL  ).num;
        settings.ctags_bin      = nvim_get_var(,B(PKG "ctags_bin"), E_STRING).ptr;
        if (!settings.enabled || !settings.ctags_bin)
                exit(0);
        settings.comp_type      = get_compression_type();
        settings.comp_level     = nvim_get_var(,B(PKG "compression_level"), E_NUM       ).num;
        settings.ctags_args     = nvim_get_var(,B(PKG "ctags_args"),        E_STRLIST   ).ptr;
        settings.ignored_ftypes = nvim_get_var(,B(PKG "ignore"),            E_STRLIST   ).ptr;
        settings.ignored_tags   = nvim_get_var(,B(PKG "ignored_tags"),      E_MPACK_DICT).ptr;
        settings.norecurse_dirs = nvim_get_var(,B(PKG "norecurse_dirs"),    E_STRLIST   ).ptr;
        settings.settings_file  = nvim_get_var(,B(PKG "settings_file"),     E_STRING    ).ptr;
        settings.verbose        = nvim_get_var(,B(PKG "verbose"),           E_BOOL      ).num;
#ifdef DEBUG /* Verbose output should be forcably enabled in debug mode. */
        settings.verbose = true;
#endif
}

static void
open_logs(void)
{
#ifdef DEBUG
        extern char LOGDIR[];
        snprintf(LOGDIR, PATH_MAX+1, "%s/.tag_highlight_log", HOME);
        mkdir(LOGDIR, 0777);
        mpack_log      = safe_fopen_fmt("%s/mpack.log", "wb", LOGDIR);
        cmd_log        = safe_fopen_fmt("%s/commandlog.log", "wb", LOGDIR);
        echo_log       = safe_fopen_fmt("%s/echo.log", "wb", LOGDIR);
        main_log       = safe_fopen_fmt("%s/buf.log", "wb+", LOGDIR);
#endif
}

/*======================================================================================*/

/**
 * Does what it says on the tin.
 */
void
get_initial_lines(struct bufdata *bdata)
{
        pthread_mutex_lock(&bdata->lock.total);
        b_list *tmp = nvim_buf_get_lines(,bdata->num);
        if (bdata->lines->qty == 1)
                ll_delete_node(bdata->lines, bdata->lines->head);
        ll_insert_blist_after(bdata->lines, bdata->lines->head, tmp, 0, (-1));

        xfree(tmp->lst);
        xfree(tmp);
        bdata->initialized = true;
        pthread_mutex_unlock(&bdata->lock.total);
}

/**
 * Free everything at exit for debugging purposes.
 */
static void
exit_cleanup(void)
{
        extern bool           process_exiting;
        extern struct backups backup_pointers;
        extern b_list        *seen_files;

        process_exiting = true;

        b_destroy(settings.settings_file);
        b_list_destroy(seen_files);
        b_list_destroy(settings.ctags_args);
        b_list_destroy(settings.norecurse_dirs);
        b_list_destroy(settings.ignored_ftypes);

        for (unsigned i = 0; i < buffers.mlen; ++i)
                destroy_bufdata(buffers.lst + i);

        if (top_dirs) {
                xfree(top_dirs->lst);
                xfree(top_dirs);
        }

        for (unsigned i = 0; i < ftdata_len; ++i) {
                struct filetype *ft = &ftdata[i];
                if (ft->initialized) {
                        if (ft->ignored_tags) {
                                b_list *igt = ft->ignored_tags;
                                for (unsigned x = 0; x < igt->qty; ++x)
                                        if (igt->lst[x]->flags & BSTR_MASK_USR1)
                                                b_destroy(igt->lst[x]);
                                xfree(igt->lst);
                                xfree(igt);
                        }
                        b_list_destroy(ft->equiv);
                        b_destroy(ft->order);
                        b_destroy(ft->restore_cmds);
                }
        }

        destroy_mpack_dict(settings.ignored_tags);
        free_backups(&backup_pointers);
        xfree(backup_pointers.lst);

        if (mpack_log)
                fclose(mpack_log);
        if (cmd_log)
                fclose(cmd_log);
        if (main_log)
                fclose(main_log);
        if (echo_log)
                fclose(echo_log);
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

static comp_type_t
(get_compression_type)(const int fd)
{
        bstring    *tmp = nvim_get_var(fd, B(PKG "compression_type"), E_STRING).ptr;
        comp_type_t ret = COMP_NONE;

        if (b_iseq_lit(tmp, "gzip")) {
                ret = COMP_GZIP;
        } else if (b_iseq_lit(tmp, "lzma")) {
#ifdef LZMA_SUPPORT
                ret = COMP_LZMA;
#else
                warnx("Compression type is set to 'lzma', but only gzip is "
                      "supported in this build.");
                ret = COMP_GZIP;
#endif
        } else if (b_iseq_lit(tmp, "none")) {
                ret = COMP_NONE;
        } else {
                eprintf("Warning: unrecognized compression type \"%s\", "
                        "defaulting to no compression.", BS(tmp));
        }

        eprintf("Comp type is %s", BS(tmp));
        b_destroy(tmp);
        return ret;
}

/*======================================================================================*/

static void
controlled_exit(UNUSED int notused)
{
        if (pthread_equal(top_thread, pthread_self()))
                return;
        pthread_exit();
}

static noreturn void
sig_handler(UNUSED int notused)
{
        if (pthread_equal(top_thread, pthread_self()))
                quick_exit(0);
        pthread_exit();
}
