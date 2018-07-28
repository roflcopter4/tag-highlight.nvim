#include "util.h"
#include <setjmp.h>
#include <signal.h>

#ifdef DOSISH
#  pragma comment(lib, "Ws2_32.lib")
#else
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <sys/un.h>
#endif

#include "data.h"
#include "highlight.h"
#include "mpack.h"

#define nvim_get_var_pkg(FD__, VARNAME_, EXPECT_, KEY_, FATAL_) \
        nvim_get_var((FD__), B(PKG "#" VARNAME_), (EXPECT_), (KEY_), (FATAL_))

#define TDIFF(STV1, STV2)                                          \
        (((long double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0L) + \
         ((long double)(tv2.tv_sec - tv1.tv_sec)))

extern jmp_buf  exit_buf;
extern int      decode_log_raw;
extern FILE    *decode_log, *cmd_log;
static FILE    *main_log;
static bstring *vpipename, *servername, *mes_servername;
static struct timeval tv1, tv2;
static pthread_t top_thread;


static void *      async_init_buffers(void *vdata);
static void *      buffer_event_loop(void *vdata);
static void        exit_cleanup(void);
static int         create_socket(bstring **name);
static comp_type_t get_compression_type(int fd);

NORETURN static void sigusr_wrap(UNUSED int _);

//extern b_list * get_pcre2_matches(const bstring *pattern, const bstring *subject, const int flags);

/*============================================================================*/


void tst(const bstring *s, ...)
{
        va_list ap;
        va_start(ap, s);
        bstring *tmp = b_vsprintf(s, ap);
        va_end(ap);
        b_fputs(stderr, tmp, B("\n"));
        b_destroy(tmp);
}


int
main(int argc, char *argv[])
{
        atexit(exit_cleanup);
        gettimeofday(&tv1, NULL);
        extern const char *program_name;
        top_thread   = pthread_self();
        program_name = basename(argv[0]);
        HOME         = getenv("HOME");

#ifdef DOSISH
        if (_setmode(0, O_BINARY) == (-1))
                err(1, "Failed to change stdin to binary mode.");
        if (_setmode(1, O_BINARY) == (-1))
                err(1, "Failed to change stdout to binary mode.");
        if (_setmode(2, O_BINARY) == (-1))
                err(1, "Failed to change stderr to binary mode.");
#else
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
        decode_log_raw = safe_open_fmt(
            "%s/decode_raw.log", O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0644, HOME);
#endif

        vpipename = (argc > 1) ? b_fromcstr(argv[1]) : NULL;
        sockfd    = create_socket(&servername);
        if (vpipename == NULL)
               errx(1, "Didn't get a pipename...");

        settings.comp_type       = get_compression_type(0);
        settings.comp_level      = nvim_get_var_num_pkg(0, "compression_level", 1);
        settings.ctags_args      = blist_from_var_pkg  (0, "ctags_args", NULL, 1);
        settings.enabled         = nvim_get_var_num_pkg(0, "enabled", 1);
        settings.ignored_tags    = nvim_get_var_pkg    (0, "ignored_tags", MPACK_DICT, NULL, 1);
        settings.norecurse_dirs  = blist_from_var_pkg  (0, "norecurse_dirs", NULL, 1);
        settings.ignored_ftypes  = blist_from_var_pkg  (0, "ignore", NULL, 1);
        settings.use_compression = nvim_get_var_num_pkg(0, "use_compression", 1);
        settings.verbose         = nvim_get_var_num_pkg(0, "verbose", 1);

        assert(settings.enabled);

        bstring *test = b_sprintf(B("you %s are a %s, you %d %ld %d!"), B("sir"),
                                  B("silly, silly man"), 893, 9838203l, (-293));
        echo("Test is '%s'", BS(test));
        b_destroy(test);

        tst(B("%s --- %llu --- %s"), B("hi"), 0xFFFFFFFFFFllu, B("bye"));

        /* Initialize all opened buffers. */
        for (unsigned attempts = 0; buffers.mkr == 0; ++attempts) {
                if (attempts > 0) {
                        if (buffers.bad_bufs.qty)
                                buffers.bad_bufs.qty = 0;
                        fsleep(3.0L);
                        echo("Retrying (attempt number %u)\n", attempts);
                }

                mpack_array_t *buflist = nvim_list_bufs(0);
                for (unsigned i = 0; i < buflist->qty; ++i)
                        new_buffer(0, buflist->items[i]->data.ext->num);

                destroy_mpack_array(buflist);
        }

        int retval = setjmp(exit_buf);

        if (retval == 0) {
                pthread_t event_loop;
                pthread_attr_t attr;
                MAKE_PTHREAD_ATTR_DETATCHED(&attr);
                pthread_create(&event_loop, &attr, &async_init_buffers, NULL);
                (void)buffer_event_loop(NULL);
        }

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

        main_log = safe_fopen_fmt("%s/.tag_highlight_log/buf.log", "wb+", HOME);

        for (;;) {
                mpack_obj *event = decode_stream(1, MES_NOTIFICATION);
                if (event) {
                        mpack_print_object(event, main_log);
                        fflush(main_log);

                        handle_nvim_event(event);

                        if (event)
                                mpack_destroy(event);
                }
        }

        return NULL;
}


/*
 * Wait for the main thread to receive and process the file data from neovim,
 * then apply the initial highlights.
 */
static void *
async_init_buffers(UNUSED void *vdata)
{
        const int       bufnum = nvim_get_current_buf(0);
        struct bufdata *bdata  = find_buffer(bufnum);
        assert(bdata);

        while (bdata->lines->qty <= 1)
                fsleep(WAIT_TIME);

        /* extern int main_hl_id;
        my_parser(bufnum, bdata);
        sleep(5000);
        nvim_buf_clear_highlight(0, bufnum, main_hl_id, 0, -1);
        sleep(2); */

        get_initial_taglist(bdata, bdata->topdir);
        update_highlight(bufnum, bdata);
        gettimeofday(&tv2, NULL);
        SHOUT("Time for initialization: %Lfs", TDIFF(tv1, tv2));

        pthread_exit(NULL);
}


void *
interrupt_call(void *vdata)
{
        struct int_pdata      *data      = vdata;
        static pthread_mutex_t int_mutex = PTHREAD_MUTEX_INITIALIZER;
        static int             bufnum    = 1;
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
                gettimeofday(&tv1, NULL);
                bufnum                = nvim_get_current_buf(0);
                struct bufdata *bdata = find_buffer(bufnum);

                if (!bdata) {
                        echo("is new buffer...");
                        if (!is_bad_buffer(bufnum) && new_buffer(0, bufnum)) {
                                nvim_buf_attach(1, bufnum);
                                bdata = find_buffer(bufnum);

                                while (bdata->lines->qty <= 1) {
                                        echo("sleeping");
                                        fsleep(WAIT_TIME);
                                }
                                get_initial_taglist(bdata, bdata->topdir);
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
        if (decode_log)
                fclose(decode_log);
        if (cmd_log)
                fclose(cmd_log);
        if (main_log)
                fclose(main_log);
        if (vpipe) {
                fclose(vpipe);
                unlink(BS(vpipename));
                b_free(vpipename);
        }
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
create_socket(bstring **name)
{
        *name = nvim_call_function(1, B("serverstart"), MPACK_STRING, NULL, 1);

#if defined(DOSISH)
        const int fd = safe_open(BS(*name), O_RDWR|O_BINARY, S_IWRITE|S_IREAD);
#else
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, (*name)->data, (*name)->slen + 1);

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == (-1))
                err(1, "Failed to create socket instance.");
        if (connect(fd, (struct sockaddr *)(&addr), sizeof(addr)) == (-1))
                err(2, "Failed to connect to socket.");
#endif

        return fd;
}


static comp_type_t
get_compression_type(const int fd) {
        bstring *tmp = nvim_get_var_pkg(fd, "compression_type", MPACK_STRING, NULL, 0);
        enum comp_type_e  ret = COMP_NONE;

        if (b_iseq(tmp, B("gzip")))
                ret = COMP_GZIP;
        else if (b_iseq(tmp, B("lzma"))) {
#ifdef LZMA_SUPPORT
                ret = COMP_LZMA;
#else
                warnx("Compression type is set to 'lzma', but only gzip is "
                      "supported in this build.");
                ret = COMP_GZIP;
#endif
        } else if (b_iseq(tmp, B("none")))
                ret = COMP_NONE;
        else
                echo("Warning: unrecognized compression type \"%s\", "
                     "defaulting to no compression.", BS(tmp));

        echo("Comp type is %s", BS(tmp));

        b_destroy(tmp);
        return ret;
}


/*============================================================================*/


NORETURN static void
sigusr_wrap(UNUSED int _)
{
        if (pthread_equal(top_thread, pthread_self()))
                longjmp(exit_buf, 1);
        else
                pthread_exit(NULL);
}
