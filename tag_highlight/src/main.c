#include "util/util.h"
#include <signal.h>

#ifdef DOSISH
#  include <direct.h>
#  undef mkdir
#  define mkdir(PATH, MODE) _mkdir(PATH)
#else
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/un.h>
#endif

#include "api.h"
#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"

#define WIN_BIN_FAIL(STREAM) \
        err(1, "Failed to change " STREAM "to binary mode.")

#define LOG_MASK_PERM (O_CREAT|O_TRUNC|O_WRONLY|O_BINARY), (0644)

static const long double WAIT_TIME = 3.0L;
extern FILE *decode_log, *cmd_log, *echo_log, *main_log;
extern int   decode_log_raw;
pthread_t    top_thread;

static void        exit_cleanup(void);
static int         create_socket(int mes_fd);
static comp_type_t get_compression_type(int fd);
static void        open_main_log(void);
static void        sigusr_wrap(UNUSED int notused);

//extern b_list *get_pcre2_matches(const bstring *pattern, const bstring *subject, const int flags);

/*======================================================================================*/


int
main(UNUSED int argc, char *argv[])
{
        atexit(exit_cleanup);
        extern const char *program_name;
        top_thread   = pthread_self();
        program_name = basename(argv[0]);

        timer main_timer;
        TIMER_START(main_timer);

#ifdef DOSISH
        HOME = alloca(PATH_MAX+1);
        snprintf(HOME, PATH_MAX+1, "%s%s", getenv("HOMEDRIVE"), getenv("HOMEPATH"));

        /* Set the standard streams to binary mode in Windows. Don't bother with
         * signals, it's not worth the effort. */
        if (_setmode(0, O_BINARY) == (-1))
                WIN_BIN_FAIL("stdin");
        if (_setmode(1, O_BINARY) == (-1))
                WIN_BIN_FAIL("stdout");
        if (_setmode(2, O_BINARY) == (-1))
                WIN_BIN_FAIL("stderr");
#else
        HOME = getenv("HOME");
        {
                struct sigaction temp;
                memset(&temp, 0, sizeof(temp));
                temp.sa_handler = sigusr_wrap;
                sigaction(SIGUSR1, &temp, NULL);
                sigaction(SIGINT,  &temp, NULL); // Sigint, sigterm and sigpipe
                sigaction(SIGPIPE, &temp, NULL); // are all possible signals when
                sigaction(SIGTERM, &temp, NULL); // nvim exits.
        }
#endif
#ifdef DEBUG
        char LOGDIR[PATH_MAX+1];
        snprintf(LOGDIR, PATH_MAX+1, "%s/.tag_highlight_log", HOME);
        mkdir(LOGDIR, 0777);
        mpack_log      = safe_fopen_fmt("%s/mpack.log", "wb", LOGDIR);
        decode_log     = safe_fopen_fmt("%s/stream_decode.log", "wb", LOGDIR);
        cmd_log        = safe_fopen_fmt("%s/commandlog.log", "wb", LOGDIR);
        echo_log       = safe_fopen_fmt("%s/echo.log", "wb", LOGDIR);
        decode_log_raw = safe_open_fmt("%s/decode_raw.log", LOG_MASK_PERM, LOGDIR);
#endif

        pthread_t      thr[3];
        pthread_attr_t attr[3];
        MAKE_PTHREAD_ATTR_DETATCHED(&attr[0]);
        MAKE_PTHREAD_ATTR_DETATCHED(&attr[1]);
        MAKE_PTHREAD_ATTR_DETATCHED(&attr[2]);

        pthread_create(&thr[0], &attr[0], event_loop, NULL);
        mainchan = create_socket(1);
        pthread_create(&thr[1], &attr[1], event_loop, (void *)(&mainchan));
        bufchan = create_socket(1);
        pthread_create(&thr[2], &attr[2], event_loop, (void *)(&bufchan));

        /* Grab user settings defined either in their .vimrc or otherwise by the
         * vimscript plugin. */
        settings.enabled         = nvim_get_var_pkg(0, "enabled",   E_BOOL  ).num;
        settings.ctags_bin       = nvim_get_var_pkg(0, "ctags_bin", E_STRING).ptr;
        if (!settings.enabled || !settings.ctags_bin)
                exit(0);
        settings.comp_type       = get_compression_type(0);
        settings.comp_level      = nvim_get_var_pkg(0, "compression_level", E_NUM       ).num;
        settings.ctags_args      = nvim_get_var_pkg(0, "ctags_args",        E_STRLIST   ).ptr;
        settings.ignored_ftypes  = nvim_get_var_pkg(0, "ignore",            E_STRLIST   ).ptr;
        settings.ignored_tags    = nvim_get_var_pkg(0, "ignored_tags",      E_MPACK_DICT).ptr;
        settings.norecurse_dirs  = nvim_get_var_pkg(0, "norecurse_dirs",    E_STRLIST   ).ptr;
        settings.settings_file   = nvim_get_var_pkg(0, "settings_file",     E_STRING    ).ptr;
        settings.verbose         = nvim_get_var_pkg(0, "verbose",           E_BOOL      ).num;

#ifdef DEBUG
        settings.verbose = true;
        open_main_log();
#endif

        /* Try to initialize a buffer. If the current one isn't recognized, keep
         * trying periodically until we see one that is. In case we never
         * recognize a buffer, the wait time between tries is modestly long. */
        for (unsigned attempts = 0; buffers.mkr == 0; ++attempts) {
                if (attempts > 0) {
                        if (buffers.bad_bufs.qty)
                                buffers.bad_bufs.qty = 0;
                        fsleep(WAIT_TIME);
                        echo("Retrying (attempt number %u)\n", attempts);
                }

                const int initial_buf = nvim_get_current_buf(0);
                if (new_buffer(0, initial_buf)) {
                        struct bufdata *bdata = find_buffer(initial_buf);

                        nvim_buf_attach(BUFFER_ATTACH_FD, initial_buf);
                        get_init_lines(bdata);
                        get_initial_taglist(bdata);
                        update_highlight(initial_buf, bdata);

                        TIMER_REPORT(main_timer, "main initialization");
                }
        }

        /* const int nvimpid = nvim_get_var_pkg(0, "pid", E_NUM).num; */
        /* nvim_command(0, B("autocmd TextChanged,TextChangedI * call rpcnotify(g:tag_highlight#pid, 'text_changed', 'D')")) */

        /* Wait for something to kill us. */
        pause();

        /* The signal handler should return unless the signal was unexpected.
         * Clean up the main loop threads before exiting. */
        pthread_cancel(thr[0]);
        pthread_cancel(thr[1]);
        pthread_cancel(thr[2]);

        return 0;
}


/*======================================================================================*/


/*
 * Free everything at exit for debugging purposes.
 */
static void
exit_cleanup(void)
{
        extern bool           process_exiting;
        extern struct backups backup_pointers;
        extern b_list *       seen_files;

        process_exiting = true;

        b_free(settings.settings_file);
        b_list_destroy(seen_files);
        b_list_destroy(settings.ctags_args);
        b_list_destroy(settings.norecurse_dirs);
        b_list_destroy(settings.ignored_ftypes);

        for (unsigned i = 0; i < buffers.mlen; ++i)
                destroy_bufdata(buffers.lst + i);

        free(top_dirs->lst);
        free(top_dirs);

        for (unsigned i = 0; i < ftdata_len; ++i)
                if (ftdata[i].initialized) {
                        if (ftdata[i].ignored_tags) {
                                for (unsigned x = 0; x < ftdata[i].ignored_tags->qty; ++x)
                                        if (ftdata[i].ignored_tags->lst[x]->flags & BSTR_MASK_USR1)
                                                b_free(ftdata[i].ignored_tags->lst[x]);
                                free(ftdata[i].ignored_tags->lst);
                                free(ftdata[i].ignored_tags);
                        }
                        b_list_destroy(ftdata[i].equiv);
                        b_free(ftdata[i].order);
                        b_free(ftdata[i].restore_cmds);
                }

        destroy_mpack_dict(settings.ignored_tags);
        free_backups(&backup_pointers);
        free(backup_pointers.lst);

        if (mpack_log)
                fclose(mpack_log);
        if (decode_log)
                fclose(decode_log);
        if (cmd_log)
                fclose(cmd_log);
        if (main_log)
                fclose(main_log);
        if (echo_log)
                fclose(echo_log);
        if (decode_log_raw > 0)
                close(decode_log_raw);
        close(mainchan);
        close(bufchan);
}


/*
 * Request for Neovim to create an additional server socket, then connect to it.
 * This allows us to keep requests and responses from different threads
 * separate, so they won't get mixed up. In Windows we must instead use a named
 * pipe, which is opened like any other file.
 */
static int
create_socket(const int mes_fd)
{
        bstring *name = nvim_call_function(mes_fd, B("serverstart"), E_STRING).ptr;

#ifdef DOSISH
        const int fd = safe_open(BS(name), O_RDWR|O_BINARY, 0);
#else
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, name->data, name->slen + 1);

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == (-1))
                err(1, "Failed to create socket instance.");
        if (connect(fd, (struct sockaddr *)(&addr), sizeof(addr)) == (-1))
                err(2, "Failed to connect to socket.");
#endif

        b_free(name);
        return fd;
}


static comp_type_t
get_compression_type(const int fd)
{
        bstring    *tmp = nvim_get_var_pkg(fd, "compression_type", E_STRING).ptr;
        comp_type_t ret = COMP_NONE;

        if (b_iseq(tmp, B("gzip"))) {
                ret = COMP_GZIP;
        } else if (b_iseq(tmp, B("lzma"))) {
#ifdef LZMA_SUPPORT
                ret = COMP_LZMA;
#else
                warnx("Compression type is set to 'lzma', but only gzip is "
                      "supported in this build.");
                ret = COMP_GZIP;
#endif
        } else if (b_iseq(tmp, B("none"))) {
                ret = COMP_NONE;
        } else {
                ECHO("Warning: unrecognized compression type \"%s\", "
                     "defaulting to no compression.", tmp);
        }

        ECHO("Comp type is %s", tmp);

        b_destroy(tmp);
        return ret;
}


/*======================================================================================*/


static void
sigusr_wrap(UNUSED int notused)
{
        if (pthread_equal(top_thread, pthread_self()))
                return;
        pthread_exit(NULL);
}


static void
open_main_log(void)
{
#ifndef DEBUG
        return;
#endif
        char buf[PATH_MAX + 1];
        snprintf(buf, PATH_MAX + 1, "%s/.tag_highlight_log", HOME);
        mkdir(buf, 0777);
        main_log = safe_fopen_fmt("%s/buf.log", "wb+", buf);
}
