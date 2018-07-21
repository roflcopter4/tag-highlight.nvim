#include "util.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include "contrib/bsd_funcs.h"
#include "data.h"
#include "highlight.h"
#include "mpack.h"

#define nvim_get_var_pkg(FD__, VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var((FD__), B(PKG "#" VARNAME_), (EXPECT_), (KEY_), (FATAL_))

#define TDIFF(STV1, STV2)                                          \
        (((long double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0L) + \
         ((long double)(tv2.tv_sec - tv1.tv_sec)))

extern FILE    *decodelog, *cmdlog;
static FILE    *logfile;
static bstring *vpipename, *servername, *mes_servername;
static struct timeval tv1, tv2;


static void *      interrupt_loop(void *vdata);
static void *      buffer_event_loop(void *vdata);
static void        exit_cleanup(void);
static int         create_socket(bstring **name);
static comp_type_t get_compression_type(void);

static void piss_around(void);
noreturn static void pthread_exit_wrapper(UNUSED int x);

extern b_list * get_pcre2_matches(const bstring *pattern, const bstring *subject, const int flags);


/*============================================================================*/


int
main(int argc, char *argv[])
{
        gettimeofday(&tv1, NULL);
        extern const char *program_name;
        program_name = basename(argv[0]);
        HOME         = getenv("HOME");
        assert(atexit(exit_cleanup) == 0);

        {
                /* const struct sigaction temp1 = {{exit}, {{0}}, 0, 0};
                sigaction(SIGTERM, &temp1, NULL);
                sigaction(SIGPIPE, &temp1, NULL); */

                const struct sigaction temp2 = {{pthread_exit_wrapper}, {{0}}, 0, 0};
                sigaction(SIGUSR1, &temp2, NULL);
        }

#ifdef DEBUG
        mpack_log = safe_fopen_fmt("%s/mpack.log", "w+", HOME);
        decodelog = safe_fopen_fmt("%s/stream_decode.log", "w", HOME);
        cmdlog    = safe_fopen_fmt("%s/commandlog.log", "wb", HOME);
#endif
        vpipename = (argc > 1) ? b_fromcstr(argv[1]) : NULL;
        sockfd    = create_socket(&servername);

        if (!vpipename)
                errx(1, "Didn't get a pipename...");
        vpipe = safe_fopen(BS(vpipename), "r+");

        settings.comp_type       = get_compression_type();
        settings.comp_level      = nvim_get_var_num_pkg(0, "compression_level", 1);
        settings.ctags_args      = blist_from_var_pkg  (0, "ctags_args", NULL, 1);
        settings.enabled         = nvim_get_var_num_pkg(0, "enabled", 1);
        settings.ignored_tags    = nvim_get_var_pkg    (0, "ignored_tags", MPACK_DICT, NULL, 1);
        settings.norecurse_dirs  = blist_from_var_pkg  (0, "norecurse_dirs", NULL, 1);
        settings.ignored_ftypes  = blist_from_var_pkg  (0, "ignore", NULL, 1);
        settings.use_compression = nvim_get_var_num_pkg(0, "use_compression", 1);
        settings.verbose         = nvim_get_var_num_pkg(0, "verbose", 1);

        assert(settings.enabled);

        /* piss_around(); */

        get_pcre2_matches(B("\\d+ \\w+"), B("donkey 2834 asdf 32 iuu"), 0);

        /* Initialize all opened buffers. */
        for (unsigned attempts = 0; buffers.mkr == 0; ++attempts) {
                if (attempts > 0) {
                        if (buffers.bad_bufs.qty)
                                buffers.bad_bufs.qty = 0;
                        sleep(3);
                        echo("Retrying (attempt number %u)\n", attempts);
                }

                mpack_array_t *buflist = nvim_list_bufs(0);
                for (unsigned i = 0; i < buflist->qty; ++i)
                        new_buffer(0, buflist->items[i]->data.ext->num);
                destroy_mpack_array(buflist);
        }

        pthread_t event_loop, main_loop;
        pthread_create(&main_loop,  NULL, &buffer_event_loop, NULL);
        pthread_create(&event_loop, NULL, &interrupt_loop, &main_loop);

        pthread_join(event_loop, NULL);
        pthread_join(main_loop, NULL);

        return 0;
}


/*============================================================================*/

#define WAIT_TIME (0.2L)


/*
 * Main nvim event loop. Waits for nvim buffer updates.
 */
static void *
buffer_event_loop(UNUSED void *vdata)
{
        for (unsigned i = 0; i < buffers.mkr; ++i)
                nvim_buf_attach(1, buffers.lst[i]->num);

        logfile = safe_fopen_fmt("%s/.tag_highlight_log/buflog", "w+", HOME);

        for (;;) {
                mpack_obj *event = decode_stream(1, MES_NOTIFICATION);
                if (event) {
                        mpack_print_object(event, logfile);
                        fflush(logfile);

                        handle_nvim_event(event);

                        if (event)
                                mpack_destroy(event);
                }
        }

        pthread_exit(NULL);
}


/*
 * Waits for updates from the small vimscript plugin via a named pipe. Main
 * updates are: BufNew, BufLoad, BufWrite, Syntax, Filetype, and when the
 * user calls the stop command for the plugin.
 */
static void *
interrupt_loop(void *vdata)
{
        pthread_t mainloop = *((pthread_t *)vdata);
        int       bufnum   = nvim_get_current_buf(0);

        {
                struct bufdata *bdata = find_buffer(bufnum);
                assert(bdata);

                while (bdata->lines->qty <= 1)
                        fsleep(WAIT_TIME);

                /* extern int main_hl_id;
                my_parser(bufnum, bdata);
                sleep(5000);
                nvim_buf_clear_highlight(0, bufnum, main_hl_id, 0, -1);
                sleep(2); */
                update_highlight(bufnum, bdata);
                gettimeofday(&tv2, NULL);

                SHOUT("Time for initialization: %Lfs", TDIFF(tv1, tv2));
        }

        for (;;) {
                bstring *tmp = B_GETS(vpipe, '\n', false);
                echo("Recieved \"%s\"; waking up!\n", BS(tmp));

                switch (tmp->data[0]) {
                /*
                 * New buffer was opened or current buffer changed.
                 */
                case 'A':
                case 'D': {
                        fsleep(WAIT_TIME);
                        const int prev = bufnum;
                        gettimeofday(&tv1, NULL);
                        bufnum                = nvim_get_current_buf(0);
                        struct bufdata *bdata = find_buffer(bufnum);

                        if (!bdata) {
                                if (!is_bad_buffer(bufnum) && new_buffer(0, bufnum)) {
                                        nvim_buf_attach(1, bufnum);
                                        bdata = find_buffer(bufnum);

                                        while (bdata->lines->qty <= 1) {
                                                echo("sleeping");
                                                fsleep(WAIT_TIME);
                                        }
                                        update_highlight(bufnum, bdata);

                                        gettimeofday(&tv2, NULL);
                                        SHOUT("Time for initialization: %Lfs",
                                               TDIFF(tv1, tv2));
                                }
                        } else if (prev != bufnum) {
                                update_highlight(bufnum, bdata);
                                gettimeofday(&tv2, NULL);
                                SHOUT("Time for update: %Lfs", TDIFF(tv1, tv2));
                        }

                        break;
                }
                /*
                 * Buffer was written, or filetype/syntax was changed.
                 */
                case 'B': {
                        gettimeofday(&tv1, NULL);
                        const int       curbuf = nvim_get_current_buf(0);
                        struct bufdata *bdata  = find_buffer(curbuf);

                        if (!bdata) {
                                warnx("Failed to find buffer! %d -> p: %p\n",
                                      curbuf, (void *)bdata);
                                break;
                        }

                        fsleep(WAIT_TIME);

                        if (update_taglist(bdata)) {
                                update_highlight(curbuf, bdata);
                                gettimeofday(&tv2, NULL);
                                SHOUT("Time for update: %Lfs", TDIFF(tv1, tv2));
                        }

                        break;
                }
                /*
                 * User called the kill command.
                 */
                case 'C': {
                        b_destroy(tmp);
                        clear_highlight(nvim_get_current_buf(0), NULL);
                        pthread_kill(mainloop, SIGUSR1);
                        pthread_exit(NULL);
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

                b_destroy(tmp);
        }

        pthread_exit(NULL);
}


/*============================================================================*/


/*
 * Free everything at exit for debugging purposes.
 */
static void
exit_cleanup(void)
{
        extern struct backups backup_pointers;

        b_free(servername);
        b_free(mes_servername);
        b_list_destroy(settings.ctags_args);
        b_list_destroy(settings.norecurse_dirs);
        b_list_destroy(settings.ignored_ftypes);
        destroy_mpack_dict(settings.ignored_tags);

        for (unsigned i = 0; i < buffers.mlen; ++i)
                destroy_bufdata(buffers.lst + i);

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
        if (decodelog)
                fclose(decodelog);
        if (cmdlog)
                fclose(cmdlog);
        if (logfile)
                fclose(logfile);
        if (vpipe) {
                fclose(vpipe);
                unlink(BS(vpipename));
                b_free(vpipename);
        }
        close(sockfd);
}


/*
 * Request for Neovim to create an additional server socket, then connect to it.
 * This allows us to keep requests and responses from different threads
 * separate, so they won't get mixed up.
 */
static int
create_socket(bstring **name)
{
        *name = nvim_call_function(1, B("serverstart"), MPACK_STRING, NULL, 1);

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, (*name)->data, (*name)->slen + 1);

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == (-1))
                err(1, "Failed to create socket instance.");
        if (connect(fd, (struct sockaddr *)(&addr), sizeof(addr)) == (-1))
                err(2, "Failed to connect to socket.");

        return fd;
}


static comp_type_t
get_compression_type(void) {
        bstring *tmp = nvim_get_var_pkg(0, "compression_type", MPACK_STRING, NULL, 0);
        enum comp_type_e  ret = COMP_NONE;

        if (b_iseq(tmp, B("gzip")))
                ret = COMP_GZIP;
        else if (b_iseq(tmp, B("lzma")))
                ret = COMP_LZMA;
        else if (b_iseq(tmp, B("none")))
                ret = COMP_NONE;
        else
                echo("Warning: unrecognized compression type \"%s\", "
                     "defaulting to no compression.\n", BS(tmp));

        echo("Comp type is %s", BS(tmp));

        b_destroy(tmp);
        return ret;
}


/*============================================================================*/


static void
piss_around(void)
{
        nvim_get_api_info(0);
}


noreturn static void
pthread_exit_wrapper(UNUSED int x)
{
        pthread_exit(NULL);
}
