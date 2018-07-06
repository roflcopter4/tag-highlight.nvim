#include "util.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "contrib/bsd_funcs.h"
#include "data.h"
#include "highlight.h"
#include "mpack.h"

#define PKG "tag_highlight"
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

#define _FILE    __attribute__((__cleanup__(closefile))) FILE
#define _bstring __attribute__((__cleanup__(auto_b_destroy))) bstring

extern FILE    *decodelog;
static FILE    *logfile = NULL;
static bstring *vpipename;
static bstring *servername, *mes_servername = NULL;


static void *           interrupt_loop(void *vdata);
static void *           buffer_event_loop(void *vdata);
static void             exit_cleanup(void);
static int              create_socket(bstring **name);
static enum comp_type_e get_compression_type(void);


static inline void closefile(FILE **fpp)
{ fclose(*fpp); }
static inline void auto_b_destroy(bstring **str)
{ b_destroy(*str); }
static inline void pthread_exit_wrapper(UNUSED int x)
{ pthread_exit(NULL); }

FILE *cmdlog;

/*============================================================================*/


int
main(int argc, char *argv[], UNUSED char *envp[])
{
        _Static_assert(sizeof(long) == sizeof(size_t), "Microsoft sux");
        extern const char *program_name;
        program_name = basename(argv[0]);

        unlink("/home/bml/final_output.log");

        assert(atexit(exit_cleanup) == 0);
        {
                /* const struct sigaction temp1 = {{exit}, {{0}}, 0, 0};
                sigaction(SIGTERM, &temp1, NULL);
                sigaction(SIGPIPE, &temp1, NULL); */

                const struct sigaction temp2 = {{pthread_exit_wrapper}, {{0}}, 0, 0};
                sigaction(SIGUSR1, &temp2, NULL);
        }

        mpack_log = safe_fopen_fmt("%s/somecrap.log", "w+", getenv("HOME"));
        decodelog = safe_fopen_fmt("%s/stream_decode.log", "w", getenv("HOME"));
        cmdlog    = safe_fopen_fmt("%s/commandlog.log", "wb", getenv("HOME"));
        vpipename = (argc > 1) ? b_fromcstr(argv[1]) : NULL;
        sockfd    = create_socket(&servername);

        if (vpipename) {
                /* nvprintf("Opening file \"%s\"\n", BS(vpipename)); */
                vpipe = safe_fopen(BS(vpipename), "r+");
#if 0
                _bstring *line  = B_GETS(vpipe, '\n');

                line->data[--line->slen] = '\0';
                nvprintf("Line was \"%s\"\n", BS(line));
#endif
        } else
                errx(1, "Didn't get a pipename...");

        /* nvprintf("sockfd is %d and servername is %s\n", sockfd, BS(servername)); */

#if 0
        mpack_obj *dumb = encode_fmt("d,d,d,[s,s]", 1, 2, 3, B("Hello"), B("die"));
        mpack_print_object(dumb, decodelog);
        mpack_obj *dumber = decode_obj(*dumb->packed, 0);
        mpack_print_object(dumber, decodelog);
#endif

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
        struct mpack_array *buflist = nvim_list_bufs(sockfd);

        for (unsigned i = 0; i < buflist->qty; ++i) {
                nvprintf("Attempting to initialize buffer %u\n",
                         buflist->items[i]->data.ext->num);
                new_buffer(sockfd, buflist->items[i]->data.ext->num);
        }

        destroy_mpack_array(buflist);

        pthread_t event_loop, main_loop;
        pthread_create(&main_loop, NULL,  &buffer_event_loop, NULL);
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
        for (unsigned i = 0; i < buffers.mkr; ++i) {
                nvprintf("Attaching to buffer %d\n", buffers.lst[i]->num);
                nvim_buf_attach(1, buffers.lst[i]->num);
        }
        logfile = safe_fopen_fmt("%s/.tag_highlight_log/buflog", "w+",
                                 getenv("HOME"));

        for (;;) {
                mpack_obj *event = decode_stream(1, MES_NOTIFICATION);
                if (event) {
                        mpack_print_object(event, logfile);
                        fflush(logfile);

                        const enum event_types type = handle_nvim_event(event);

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
        pthread_t mainloop = *((int *)vdata);
        int       bufnum   = nvim_get_current_buf(sockfd);

        {
                struct bufdata *bdata  = find_buffer(bufnum);

                while (!bdata->lines->qty)
                        usleep(100 * 1000);

                usleep(100 * 1000);
                /* nvprintf("We're going in! There are %u lines!", bdata->lines->qty); */
                update_highlight(bufnum, bdata);
        }

        for (;;) {
                bstring *tmp = B_GETS(vpipe, '\n');
                tmp->data[--tmp->slen] = '\0';
                nvprintf("Recieved \"%s\"; waking up!\n", BS(tmp));

                switch (tmp->data[0])
                {
                /* New buffer was opened. */
                case 'A': {
                        struct mpack_array *buflist = nvim_list_bufs(sockfd);

                        for (unsigned i = 0; i < buflist->qty; ++i) {
                                const int bufnum = buflist->items[i]->data.ext->num;

                                if (!find_buffer(bufnum) && new_buffer(sockfd, bufnum)) {
                                        nvprintf("Attaching to buffer %d\n", bufnum);
                                        nvim_buf_attach(1, bufnum);
                                }
                        }

                        destroy_mpack_array(buflist);
                        break;
                }

                /* Buffer was written, or filetype/syntax was changed. */
                case 'B': {
                        int bufnum, index;
                        struct bufdata *bdata;
retry:
                        bufnum = nvim_get_current_buf(sockfd);
                        bdata  = find_buffer(bufnum);
                        index  = find_buffer_ind(bufnum);

                        if (index < 0 || !bdata) {
                                warnx("Failed to find buffer! %d -> i: %d, p: %p\n",
                                      bufnum, index, (void*)bdata);
                                break;
                        }
                        for (int i = 0; i < 5 && bdata->lines->qty == 0; ++i) {
                                nvprintf("waiting on buffer number %d, internally known as %d\n",
                                         bdata->num, index);
                                usleep(100000);
                        }
                        if (bdata->lines->qty == 0) {
                                echo("retrying");
                                goto retry;
                        }

                        if (update_taglist(bdata))
                                update_highlight(bufnum, bdata);
                        break;
                }

                /* User called the kill command. */
                case 'C': {
                        b_destroy(tmp);
                        clear_highlight(nvim_get_current_buf(sockfd), NULL);
                        pthread_kill(mainloop, SIGUSR1);
                        pthread_exit(NULL);
                        break; /* NOTREACHED */
                }

                case 'D': {
                        const int prev = bufnum;
                        bufnum = nvim_get_current_buf(sockfd);
                        warnx("I see that the buffer changed from %d to %d...\n", prev, bufnum);
                        update_highlight(bufnum, NULL);
                        break;
                }

                case 'E':
                        clear_highlight(nvim_get_current_buf(sockfd), NULL);
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
 * free everything at exit for debugging purposes.
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
                        free(ftdata[i].ignored_tags->lst);
                        free(ftdata[i].ignored_tags);
                        b_list_destroy(ftdata[i].equiv);
                        b_free(ftdata[i].order);
                }

        free_backups(&backup_pointers);
        free(backup_pointers.lst);

        fclose(mpack_log);
        fclose(decodelog);
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
        strlcpy(addr.sun_path, BS(*name), sizeof(addr.sun_path) - 1);

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == (-1))
                err(1, "Failed to create socket instance.");
        if (connect(fd, (struct sockaddr *)(&addr), sizeof(addr)) == (-1))
                err(2, "Failed to connect to socket.");

        return fd;
}


static enum comp_type_e
get_compression_type(void)
{
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

        b_destroy(tmp);
        return ret;
}
