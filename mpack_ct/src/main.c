#include "util.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "contrib/bsd_funcs.h"
#include "data.h"
#include "highlight.h"
#include "mpack.h"

#define PKGNAME "tag_highlight"

#define nvim_get_var_number(FD__, VARNAME_, FATAL_)            \
        numptr_to_num(nvim_get_var((FD__), b_tmp(VARNAME_),    \
                                   MPACK_NUM, NULL, (FATAL_)))

#define _FILE    __attribute__((cleanup(closefile))) FILE
#define _bstring __attribute__((cleanup(auto_b_destroy))) bstring

extern int      sockfd;
extern FILE    *decodelog;
static FILE    *logfile = NULL;
static bstring *vpipename;
static bstring *servername, *mes_servername = NULL;


static void * wait_for_input  (void *vdata);
static void * wait_for_updates(void *vdata);
static void   exit_cleanup    (void);
static int    create_socket   (bstring **name);
static void   random_crap     (void);

static inline void closefile(FILE **fpp)
{ fclose(*fpp); }
static inline void auto_b_destroy(bstring **str)
{ b_free(*str); }
static inline void pthread_exit_wrapper(UNUSED int x)
{ pthread_exit(NULL); }


/*============================================================================*/


int
main(int argc, char *argv[], UNUSED char *envp[])
{
        _Static_assert(sizeof(long) == sizeof(size_t), "Microsoft sux");
        extern const char *program_name;
        program_name = basename(argv[0]);

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
        vpipename = (argc > 1) ? b_fromcstr(argv[1]) : NULL;
        sockfd    = create_socket(&servername);

        if (vpipename) {
                nvprintf("Opening file \"%s\"\n", BS(vpipename));
                vpipe = safe_fopen(BS(vpipename), "r+");
                _bstring *line  = B_GETS(vpipe, '\n');

                line->data[--line->slen] = '\0';
                nvprintf("Line was \"%s\"\n", BS(line));
        } else {
                echo("Didn't get a pipename...");
        }

        nvprintf("sockfd is %d and servername is %s\n", sockfd, BS(servername));
        random_crap();

        settings.comp_level       = nvim_get_var_number(sockfd, PKGNAME "#compression_level", 1);
        settings.compression_type = nvim_get_var_l     (sockfd, PKGNAME "#compression_type", MPACK_STRING, NULL, 0);
        settings.ctags_args       = blist_from_var     (sockfd, PKGNAME "#ctags_args", NULL, 1);
        settings.enabled          = nvim_get_var_number(sockfd, PKGNAME "#enabled", 1);
        settings.ignored_tags     = nvim_get_var_l     (sockfd, PKGNAME "#ignored_tags", MPACK_DICT, NULL, 1);
        settings.norecurse_dirs   = blist_from_var     (sockfd, PKGNAME "#norecurse_dirs", NULL, 1);
        settings.use_compression  = nvim_get_var_number(sockfd, PKGNAME "#use_compression", 1);
        settings.verbose          = nvim_get_var_number(sockfd, PKGNAME "#verbose", 1);

        assert(settings.enabled);
        struct mpack_array *buflist = nvim_list_bufs(sockfd);

        for (unsigned i = 0; i < buflist->qty; ++i) {
                nvprintf("Attempting to initialize buffer %u\n",
                         buflist->items[i]->data.ext->num);
                bool ret = new_buffer(sockfd, buflist->items[i]->data.ext->num);
                if (ret) {

                }
        }
        
        destroy_mpack_array(buflist);

        pthread_t event_loop, main_loop;
        pthread_create(&main_loop, NULL,  &wait_for_input, NULL);
        pthread_create(&event_loop, NULL, &wait_for_updates, &main_loop);

        pthread_join(event_loop, NULL);
        pthread_join(main_loop, NULL);

        return 0;
}


/*============================================================================*/


static void *
wait_for_input(UNUSED void *vdata)
{
        for (unsigned i = 0; i < buffers.qty; ++i) {
                nvprintf("Attaching to buffer %d\n", buffers.lst[i]->num);
                nvim_buf_attach(1, buffers.lst[i]->num);
        }

        logfile = safe_fopen_fmt("%s/.tag_highlight_log/buflog", "w+", getenv("HOME"));

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


static void *
wait_for_updates(void *vdata)
{
        pthread_t mainloop = *((int *)vdata);

        for (;;) {
                _bstring *tmp = B_GETS(vpipe, '\n');
                tmp->data[--tmp->slen] = '\0';
                nvprintf("Recieved \"%s\"; waking up!\n", BS(tmp));

                switch (tmp->data[0]) {
                case 'A': { /* New buffer was opened. */
                        struct mpack_array *buflist = nvim_list_bufs(sockfd);

                        for (unsigned i = 0; i < buflist->qty; ++i) {
                                const int bufnum = buflist->items[i]->data.ext->num;

                                if (!find_buffer(bufnum) ) {
                                        new_buffer(sockfd, bufnum);
#if 0
                                        b_list *tmp = get_archived_tags(find_buffer(bufnum));

                                        _FILE *blagh = safe_fopen_fmt("%s/tags.log", "w",
                                                                      getenv("HOME"));
                                        b_dump_list(blagh, tmp);
                                        b_list_destroy(tmp);

                                        nvprintf("Attaching to buffer %d\n", bufnum);
                                        (void)nvim_buf_attach(1, bufnum);
#endif
                                }
                        }

                        destroy_mpack_array(buflist);
                        break;
                }

                case 'B': { /* Buffer was written, or filetype/syntax was changed. */
                        const int bufnum = nvim_get_current_buf(sockfd);
                        assert(run_ctags(bufnum, NULL));
                        break;
                }

                case 'C': { /* User generated kill signal. */
                        b_destroy(tmp);
                        pthread_kill(mainloop, SIGUSR1);
                        pthread_exit(NULL);
                }

                default:
                          break;
                }

                /* b_destroy(tmp); */
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
        b_free(settings.compression_type);
        b_list_destroy(settings.ctags_args);
        b_list_destroy(settings.norecurse_dirs);
        destroy_dictionary(settings.ignored_tags);

        for (unsigned i = 0; i < buffers.mlen; ++i)
                destroy_bufdata(buffers.lst + i);
        free(buffers.lst);

        for (unsigned i = 0; i < ftdata_len; ++i)
                if (ftdata[i].initialized) {
                        b_list_destroy(ftdata[i].equiv);
                        b_free(ftdata[i].order);
                }

        free_backups(&backup_pointers);
        free(backup_pointers.lst);

        fclose(mpack_log);
        fclose(decodelog);
        if (logfile)
                fclose(logfile);
        if (vpipe) {
                fclose(vpipe);
                unlink(BS(vpipename));
                b_free(vpipename);
        }
        close(sockfd);
}


static int
create_socket(bstring **name)
{
        *name = nvim_call_function(1, B("serverstart"), MPACK_STRING, NULL, 1);

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strlcpy(addr.sun_path, (char *)(*name)->data, sizeof(addr.sun_path) - 1);

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == (-1))
                err(1, "Failed to create socket instance.");
        if (connect(fd, (struct sockaddr *)(&addr), sizeof(addr)) == (-1))
                err(2, "Failed to connect to socket.");

        return fd;
}


static void
random_crap(void)
{
        free(nvim_call_function_args(sockfd, B("line2byte"), MPACK_NUM, NULL, 0, "d", 5));
        b_free(nvim_call_function_args(sockfd, B("glob2regpat"), MPACK_STRING, NULL, 0, "s",
                                       B("/home/{bml,brendan}/.{vim,emacs.d}/*.{vim,el}")));
#if 0
        nvim_command(sockfd, B("echom 'Hello, world!'"), 0);
        nvim_command(sockfd, B("echom Hello, world!'"), 0);
        b_list_destroy(nvim_buf_get_lines(sockfd, 1, 0, -1));

        /* nvim_call_function_args(sockfd, B("string"), MPACK_STRING, NULL, 0,
                                "[d,d,s,[B,s,[[],[d,s,d,[dddddddddd]]],B],s,[]]",
                                1, 2, B("hello"), true, B("you must die"), -1,
                                B("DIE"), 99283839, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                -1, false, B("hahaha")); */
        fputc('\n', decodelog);
        fflush(mpack_log);
#endif

        /* _bstring *tmp = nvim_command_output(sockfd, B("34print"), MPACK_STRING, NULL, 0);
        nvprintf("Retval was \"%s\"\n", BS(tmp)); */
}
