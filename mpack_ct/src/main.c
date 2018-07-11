#include "util.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "contrib/bsd_funcs.h"
#include "data.h"
#include "highlight.h"
#include "mpack.h"

#undef nvim_get_var_l
#undef blist_from_var

#define nvim_get_var_num(VARNAME_, FATAL_)                                 \
        numptr_to_num(nvim_get_var(sockfd, B(PKG "#" VARNAME_), MPACK_NUM, \
                                   NULL, (FATAL_)))

#define nvim_get_var_l(VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var(sockfd, B(PKG "#" VARNAME_), (EXPECT_), (KEY_), (FATAL_))

#define blist_from_var(VARNAME_, KEY_, FATAL_)                         \
        mpack_array_to_blist(nvim_get_var(sockfd, B(PKG "#" VARNAME_), \
                                          MPACK_ARRAY, (KEY_), (FATAL_)), true)

extern FILE    *decodelog;
static FILE    *logfile;
static bstring *vpipename, *servername, *mes_servername;


static void *           interrupt_loop(void *vdata);
static void *           buffer_event_loop(void *vdata);
static void             exit_cleanup(void);
static int              create_socket(bstring **name);
static enum comp_type_e get_compression_type(void);
static void             buffer_update(int bufnum, struct bufdata *bdata);


static inline void closefile(FILE **fpp)
{ fclose(*fpp); }
static inline void auto_b_destroy(bstring **str)
{ b_destroy(*str); }
_Noreturn static inline void pthread_exit_wrapper(UNUSED int x)
{ pthread_exit(NULL); }

FILE *cmdlog;

/*============================================================================*/


int
main(int argc, char *argv[])
{
        extern const char *program_name;
        program_name = basename(argv[0]);
        HOME         = getenv("HOME");
        unlink("/home/bml/final_output.log");
        assert(atexit(exit_cleanup) == 0);

        {
                /* const struct sigaction temp1 = {{exit}, {{0}}, 0, 0};
                sigaction(SIGTERM, &temp1, NULL);
                sigaction(SIGPIPE, &temp1, NULL); */

                const struct sigaction temp2 = {{pthread_exit_wrapper}, {{0}}, 0, 0};
                sigaction(SIGUSR1, &temp2, NULL);
        }

        mpack_log = safe_fopen_fmt("%s/mpack.log", "w+", HOME);
        decodelog = safe_fopen_fmt("%s/stream_decode.log", "w", HOME);
        cmdlog    = safe_fopen_fmt("%s/commandlog.log", "wb", HOME);
        vpipename = (argc > 1) ? b_fromcstr(argv[1]) : NULL;
        sockfd    = create_socket(&servername);

        if (!vpipename)
                errx(1, "Didn't get a pipename...");
        vpipe = safe_fopen(BS(vpipename), "r+");

        settings.comp_type       = get_compression_type();
        settings.comp_level      = nvim_get_var_num("compression_level", 1);
        settings.ctags_args      = blist_from_var  ("ctags_args", NULL, 1);
        settings.enabled         = nvim_get_var_num("enabled", 1);
        settings.ignored_tags    = nvim_get_var_l  ("ignored_tags", MPACK_DICT, NULL, 1);
        settings.norecurse_dirs  = blist_from_var  ("norecurse_dirs", NULL, 1);
        settings.ignored_ftypes  = blist_from_var  ("ignore", NULL, 1);
        settings.use_compression = nvim_get_var_num("use_compression", 1);
        settings.verbose         = nvim_get_var_num("verbose", 1);

        assert(settings.enabled);

        /* Initialize all opened buffers. */
        for (unsigned attempts = 0; buffers.mkr == 0; ++attempts) {
                if (attempts > 0) {
                        if (buffers.bad_bufs.qty)
                                buffers.bad_bufs.qty = 0;
                        sleep(3);
                        nvprintf("Retrying (attempt number %u)\n", attempts);
                }
                struct mpack_array *buflist = nvim_list_bufs(0);

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
 * updates are: BufNew, BufLoad, BufWrite, Syntax, Filetype, and when the user
 * calls the stop command for the plugin.
 */
static void *
interrupt_loop(void *vdata)
{
        pthread_t mainloop = *((pthread_t *)vdata);
        int       bufnum   = nvim_get_current_buf(0);

        {
                struct bufdata *bdata = find_buffer(bufnum);
                assert(bdata);

                while (!bdata->lines->qty)
                        usleep(100 * 1000);
                update_highlight(bufnum, bdata);
        }

        for (;;) {
                bstring *tmp = B_GETS(vpipe, '\n');
                tmp->data[--tmp->slen] = '\0';
                nvprintf("Recieved \"%s\"; waking up!\n", BS(tmp));

                switch (tmp->data[0]) {
                /*
                 * New buffer was opened.
                 */
                case 'A':
                case 'D': {
#if 0
                        struct mpack_array *buflist = nvim_list_bufs(0);
                        unsigned found = 0;

                        for (unsigned i = 0; i < buflist->qty; ++i) {
                                const int curbuf = buflist->items[i]->data.ext->num;
                                if (!find_buffer(curbuf)) {
                                        if (new_buffer(0, curbuf)) {
                                                nvim_buf_attach(1, curbuf);
                                                ++found;
                                        }
                                }
                        }

                        destroy_mpack_array(buflist);
                        if (found == 0) {
                                echo("Found no buffers, bailing!");
                                break;
                        }
#endif
                        const int prev = bufnum;
                        bufnum = nvim_get_current_buf(0);
                        struct bufdata *bdata = find_buffer(bufnum);

                        if (!bdata) {
                                if (!is_bad_buffer(bufnum) && new_buffer(0, bufnum)) {
                                        nvim_buf_attach(1, bufnum);
                                        bdata = find_buffer(bufnum);
                                        while (bdata->lines->qty == 0) {
                                                echo("sleeping");
                                                usleep(100000);
                                        }
                                        update_highlight(bufnum, bdata);
                                }
                        } else if (prev != bufnum) {
                                /* if (update_taglist(bdata)) */
                                /* buffer_update(bufnum, bdata); */
                                update_highlight(bufnum, bdata);
                        }

                        break;
                }
                /* FALLTHROUGH */
                /*
                 * Buffer was written, or filetype/syntax was changed.
                 */
                case 'B': {
                        const int       curbuf = nvim_get_current_buf(0);
                        struct bufdata *bdata  = find_buffer(curbuf);

                        if (!bdata) {
                                warnx("Failed to find buffer! %d -> p: %p\n",
                                      curbuf, (void*)bdata);
                                break;
                        }

                        /* unsigned cticks = nvim_buf_get_var */
                        usleep(200000);
#if 0
                        for (int i = 0; i < 5 && bdata->lines->qty == 0; ++i)
                                usleep(100000);
                        if (bdata->lines->qty == 0) {
                                echo("retrying");
                                goto retry;
                        }
#endif

                        if (update_taglist(bdata))
                                update_highlight(curbuf, bdata);

                        /* buffer_update(0, NULL); */
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
                        break; /* NOTREACHED */
                }
                /*
                 * Current buffer changed.
                 */
#if 0
                case 'D': {
                        const int prev = bufnum;
                        bufnum = nvim_get_current_buf(0);
                        if (is_bad_buffer(bufnum)) {
                                echo("Changed to bad buffer, skipping.");
                                break;
                        }
                        warnx("I see that the buffer changed from %d to %d...\n",
                              prev, bufnum);
                        update_highlight(bufnum, NULL);
                        break;
                }
#endif
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
        destroy_dictionary(settings.ignored_tags);

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
        strcpy(addr.sun_path, BS(*name));

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == (-1))
                err(1, "Failed to create socket instance.");
        if (connect(fd, (struct sockaddr *)(&addr), sizeof(addr)) == (-1))
                err(2, "Failed to connect to socket.");

        return fd;
}


static enum comp_type_e
get_compression_type(void) {
        bstring *tmp = nvim_get_var_l("compression_type", MPACK_STRING, NULL, 0);
        enum comp_type_e  ret = COMP_NONE;

        if (b_iseq(tmp, B("gzip")))
                ret = COMP_GZIP;
        else if (b_iseq(tmp, B("lzma")))
                ret = COMP_LZMA;
        else if (b_iseq(tmp, B("none")))
                ret = COMP_NONE;
        else
                nvprintf("Warning: unrecognized compression type \"%s\", "
                         "defaulting to no compression.\n", BS(tmp));

        b_fputs(stderr, B("Comp type is "), tmp, B("\n"));

        b_destroy(tmp);
        return ret;
}


/*============================================================================*/


static void
buffer_update(int bufnum, struct bufdata *bdata)
{
        if (!bufnum)
                bufnum = nvim_get_current_buf(0);
        if (!bdata)
                bdata = find_buffer(bufnum);
        if (!bdata) {
                warnx("Failed to find buffer! %d -> p: %p\n",
                      bufnum, (void*)bdata);
                return;
        }

        /* for (int i = 0; i < 5 && bdata->lines->qty == 0; ++i) */
        while (bdata->lines->qty == 0)
                echo("sleeping"), usleep(100000);
        /* if (bdata->lines->qty == 0) {
                echo("retrying");
                goto retry;
        } */

        if (update_taglist(bdata))
                update_highlight(bufnum, bdata);
        else
                echo("Failed to process tag data!");
}
