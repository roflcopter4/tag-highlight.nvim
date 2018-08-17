#include "util.h"
#include <setjmp.h>
#include <signal.h>

#ifdef DOSISH
#  include <direct.h>
#  undef mkdir
#  define mkdir(PATH_, MODE_) _mkdir(PATH_)
#else
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/time.h>
#  include <sys/un.h>
#endif

/* #undef fsleep */
/* #define fsleep(...) */

#include "data.h"
#include "highlight.h"
#include "mpack.h"

#define nvim_get_var_pkg(FD__, VARNAME_, EXPECT_) \
        nvim_get_var((FD__), B(PKG "#" VARNAME_), (EXPECT_))

#define TDIFF(STV1, STV2)                                                \
        (((long double)((STV2).tv_usec - (STV1).tv_usec) / 1000000.0L) + \
         ((long double)((STV2).tv_sec - (STV1).tv_sec)))

#define BUF_WAIT_LINES(ITERS)                           \
        do {                                            \
                for (int _m_ = 0; _m_ < (ITERS); ++_m_) \
                        if (!bdata->initialized)        \
                                fsleep(0.1);            \
        } while (0)


extern jmp_buf         exit_buf;
extern int             decode_log_raw;
extern FILE           *decode_log, *cmd_log, *echo_log, *main_log;
static pthread_t       top_thread;
static struct timeval  gtv;


static void          exit_cleanup(void);
static int           create_socket(int mes_fd);
static comp_type_t   get_compression_type(int fd);
static void          get_init_lines(struct bufdata *bdata);
static void          open_main_log(void);
NORETURN static void sigusr_wrap(UNUSED int notused);

//extern b_list *get_pcre2_matches(const bstring *pattern, const bstring *subject,
//                                 const int flags);

/*======================================================================================*/


int
main(UNUSED int argc, char *argv[])
{
        atexit(exit_cleanup);
        gettimeofday(&gtv, NULL);
        extern const char *program_name;
        top_thread   = pthread_self();
        program_name = basename(argv[0]);

#ifdef DOSISH
        HOME = alloca(PATH_MAX+1);
        snprintf(HOME, PATH_MAX+1, "%s%s", getenv("HOMEDRIVE"), getenv("HOMEPATH"));
        /* Set the standard streams to binary mode in Windows. */
        if (_setmode(0, O_BINARY) == (-1))
                err(1, "Failed to change stdin to binary mode.");
        if (_setmode(1, O_BINARY) == (-1))
                err(1, "Failed to change stdout to binary mode.");
        if (_setmode(2, O_BINARY) == (-1))
                err(1, "Failed to change stderr to binary mode.");
#else
        HOME = getenv("HOME");
        {
                struct sigaction temp;
                memset(&temp, 0, sizeof(temp));
                temp.sa_handler = sigusr_wrap;
                sigaction(SIGUSR1, &temp, NULL);
                sigaction(SIGTERM, &temp, NULL);
        }
#endif
#ifdef DEBUG
        mpack_log      = safe_fopen_fmt("%s/mpack.log", "wb", HOME);
        decode_log     = safe_fopen_fmt("%s/stream_decode.log", "wb", HOME);
        cmd_log        = safe_fopen_fmt("%s/commandlog.log", "wb", HOME);
        echo_log       = safe_fopen_fmt("%s/echo.log", "wb", HOME);
        decode_log_raw = safe_open_fmt(
            "%s/decode_raw.log", O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0644, HOME);
#endif
        sockfd = create_socket(1);
        eprintf("sockfd is %d\n", sockfd);

        /* Grab user settings defined either in their .vimrc or otherwise by the
         * vimscript plugin. */
        settings.comp_type       = get_compression_type(0);
        settings.comp_level      = nvim_get_var_pkg(0, "compression_level", E_NUM        ).num;
        settings.ctags_args      = nvim_get_var_pkg(0, "ctags_args",        E_STRLIST    ).ptr;
        settings.enabled         = nvim_get_var_pkg(0, "enabled",           E_BOOL       ).num;
        settings.ignored_ftypes  = nvim_get_var_pkg(0, "ignore",            E_STRLIST    ).ptr;
        settings.ignored_tags    = nvim_get_var_pkg(0, "ignored_tags",      E_MPACK_DICT ).ptr;
        settings.norecurse_dirs  = nvim_get_var_pkg(0, "norecurse_dirs",    E_STRLIST    ).ptr;
        settings.settings_file   = nvim_get_var_pkg(0, "settings_file",     E_STRING     ).ptr;
        settings.use_compression = nvim_get_var_pkg(0, "use_compression",   E_BOOL       ).num;
        settings.verbose         = nvim_get_var_pkg(0, "verbose",           E_BOOL       ).num;

        assert(settings.enabled);

        open_main_log();
        int initial_buf;

        /* Initialize all opened buffers. */
        for (unsigned attempts = 0; buffers.mkr == 0; ++attempts) {
                if (attempts > 0) {
                        if (buffers.bad_bufs.qty)
                                buffers.bad_bufs.qty = 0;
                        fsleep(3.0L);
                        echo("Retrying (attempt number %u)\n", attempts);
                }

                initial_buf = nvim_get_current_buf(0);
                if (new_buffer(0, initial_buf)) {
                        struct timeval  tv2;
                        struct bufdata *bdata = find_buffer(initial_buf);

                        nvim_buf_attach(1, initial_buf);
                        get_init_lines(bdata);
                        get_initial_taglist(bdata);
                        update_highlight(initial_buf, bdata);

                        gettimeofday(&tv2, NULL);
                        SHOUT("Time for file initialization: %Lfs", TDIFF(gtv, tv2));
                }
        }

        /* Rewinding with longjmp is the easiest way to ensure we get back to
         * the main thread from any sub threads at exit time, ensuring a smooth
         * cleanup. */
        int retval = setjmp(exit_buf);

        if (retval == 0) {
                for (;;) {
                        mpack_obj *event = decode_stream(1, MES_NOTIFICATION);
                        if (event) {
                                handle_nvim_event(event);
                                mpack_destroy(event);
                        }
                }
        }

        return 0;
}


/*======================================================================================*/

#define WAIT_TIME (0.08L)


/* 
 * Handle an update from the small vimscript plugin. Updates are recieved upon
 * the autocmd events "BufNew, BufEnter, Syntax, and BufWrite", as well as in
 * response to the user calling the provided clear command.
 */
void *
interrupt_call(void *vdata)
{
        static int             bufnum    = (-1);
        static pthread_mutex_t int_mutex = PTHREAD_MUTEX_INITIALIZER;
        struct int_pdata      *data      = vdata;
        struct timeval         ltv1, ltv2;

        pthread_mutex_lock(&int_mutex);

        echo("Recieved \"%c\"; waking up!\n", data->val);

        switch (data->val) {
        /*
         * New buffer was opened or current buffer changed.
         */
        case 'A':
        case 'D': {
                fsleep(WAIT_TIME);
                const int prev = bufnum;
                gettimeofday(&ltv1, NULL);
                bufnum                = nvim_get_current_buf(0);
                struct bufdata *bdata = find_buffer(bufnum);

                if (!bdata) {
                try_attach:
                        if (new_buffer(0, bufnum)) {
                                nvim_buf_attach(1, bufnum);
                                bdata = find_buffer(bufnum);

                                get_init_lines(bdata);
                                get_initial_taglist(bdata);
                                update_highlight(bufnum, bdata);

                                gettimeofday(&ltv2, NULL);
                                SHOUT("Time for file initialization: %Lfs",
                                      TDIFF(ltv1, ltv2));
                        }
                } else if (prev != bufnum) {
                        if (!bdata->calls)
                                get_initial_taglist(bdata);
                        fsleep(0.05L);

                        update_highlight(bufnum, bdata);
                        gettimeofday(&ltv2, NULL);
                        SHOUT("Time for update: %Lfs", TDIFF(ltv1, ltv2));
                }

                break;
        }
        /*
         * Buffer was written, or filetype/syntax was changed.
         */
        case 'B': {
                fsleep(WAIT_TIME);
                gettimeofday(&ltv1, NULL);
                bufnum                 = nvim_get_current_buf(0);
                struct bufdata *bdata  = find_buffer(bufnum);

                if (!bdata) {
                        echo("Failed to find buffer! %d -> p: %p\n",
                             bufnum, (void *)bdata);
                        goto try_attach;
                }

                if (update_taglist(bdata)) {
                        update_highlight(bufnum, bdata);
                        gettimeofday(&ltv2, NULL);
                        SHOUT("Time for update: %Lfs", TDIFF(ltv1, ltv2));
                }

                break;
        }
        /*
         * User called the kill command.
         */
        case 'C': {
                clear_highlight(nvim_get_current_buf(0), NULL);
#ifdef DOSISH
                pthread_kill(data->parent_tid, SIGTERM);
#else
                pthread_kill(data->parent_tid, SIGUSR1);
#endif
                break;
        }
        /*
         * User called the clear highlight command.
         */
        case 'E':
                clear_highlight(nvim_get_current_buf(0), NULL);
                break;

        default:
                echo("Hmm, nothing to do...");
                break;
        }

        free(vdata);
        pthread_mutex_unlock(&int_mutex);
        pthread_exit(NULL);
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
        destroy_mpack_dict(settings.ignored_tags);

        for (unsigned i = 0; i < buffers.mlen; ++i)
                destroy_bufdata(buffers.lst + i);

        /* genlist_destroy(top_dirs); */
        free(top_dirs->lst);
        free(top_dirs);

        for (unsigned i = 0; i < ftdata_len; ++i)
                if (ftdata[i].initialized) {
                        if (ftdata[i].ignored_tags) {
                                free(ftdata[i].ignored_tags->lst);
                                free(ftdata[i].ignored_tags);
                        }
                        b_list_destroy(ftdata[i].equiv);
                        b_free(ftdata[i].order);
                        b_free(ftdata[i].restore_cmds);
                }

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
        close(sockfd);
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
        bstring *tmp = nvim_get_var_pkg(fd, "compression_type", E_STRING).ptr;
        enum comp_type_e  ret = COMP_NONE;

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
                echo("Warning: unrecognized compression type \"%s\", "
                     "defaulting to no compression.", BS(tmp));
        }

        echo("Comp type is %s", BS(tmp));

        b_destroy(tmp);
        return ret;
}


/*======================================================================================*/


NORETURN static void
sigusr_wrap(UNUSED int notused)
{
        if (pthread_equal(top_thread, pthread_self()))
                longjmp(exit_buf, 1);
        else
                pthread_exit(NULL);
}


static void
open_main_log(void)
{
#ifdef DEBUG
        {
                char buf[PATH_MAX + 1];
                snprintf(buf, PATH_MAX + 1, "%s/.tag_highlight_log", HOME);
                mkdir(buf, 0777);
                main_log = safe_fopen_fmt("%s/buf.log", "wb+", buf);
        }
#endif
}


static void
get_init_lines(struct bufdata *bdata)
{
        b_list *tmp = nvim_buf_get_lines(0, bdata->num, 0, (-1));
        if (bdata->lines->qty == 1)
                ll_delete_node(bdata->lines, bdata->lines->head);
        ll_insert_blist_after(bdata->lines, bdata->lines->head, tmp, 0, (-1));

        free(tmp->lst);
        free(tmp);
        bdata->initialized = true;
}
