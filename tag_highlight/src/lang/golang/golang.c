// +build ignore

/* Above lines (including the blank line) are necessary to keep go from trying
 * to include this file in its build. */
#include "Common.h"
#include "lang/lang.h"

#include "contrib/p99/p99_count.h"
#include <signal.h>
#include <sys/stat.h>

#ifndef WEXITSTATUS
#  define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
#endif

extern void try_go_crap(Buffer *bdata);

struct go_output {
        int ch;
        struct {
                unsigned line;
                unsigned column;
        } start, end;
        struct {
                unsigned len;
                char     str[1024];
        } ident;
};

#ifdef DEBUG
static const char is_debug[] = "1";
#else
static const char is_debug[] = "0";
#endif
#ifdef DOSISH
#  define CMD_SUFFIX ".exe"
#else
#  define CMD_SUFFIX
#endif
#define READ_FD  (0)
#define WRITE_FD (1)

static pid_t       start_binary(Buffer *bdata);
static void        parse_go_output(Buffer *bdata, b_list *output);
static b_list     *separate_and_sort(bstring *output);
static inline bool ident_is_ignored(Buffer *bdata, bstring const *tok) __attribute__((pure));

static void write_buffer(Buffer *bdata, bstring *buf);
static bstring * read_pipe(Buffer *bdata);

/*======================================================================================*/

bstring *
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

int
highlight_go(Buffer *bdata)
{
        int      retval  = 0;
        unsigned cnt_val = p99_count_inc(&bdata->lock.num_workers);
        if (cnt_val > 5) {
                p99_count_dec(&bdata->lock.num_workers);
                return retval;
        }

        pthread_mutex_lock(&bdata->lock.total);

        if (!atomic_flag_test_and_set(&bdata->godata.flg)) {
                start_binary(bdata);
        } else {
                errno = 0;
                kill(bdata->godata.pid, 0);
                int e = errno;
                if (e != 0) {
                        char errbuf[256];
                        strerror_r(e, errbuf, 256);
                        echo("Binary not available: %d: %s", e, errbuf);
                        close(bdata->godata.rd_fd);
                        close(bdata->godata.wr_fd);
                        start_binary(bdata);
                }
        }

        bstring *tmp = ll_join_bstrings(bdata->lines, '\n');
        write_buffer(bdata, tmp);
        b_free(tmp);
        tmp = read_pipe(bdata);

        if (!tmp || !tmp->data || tmp->slen == 0) {
                retval = (-1);
                goto cleanup;
        }

        b_list *data = separate_and_sort(tmp);
        parse_go_output(bdata, data);
        b_list_destroy(data);

cleanup:
        b_free(tmp);
        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&bdata->lock.total);
        return retval;
}

/*--------------------------------------------------------------------------------------*/

static void openpipe(int fds[2]);

static pid_t
start_binary(Buffer *bdata)
{
        bstring const *go_binary = settings.go_binary;
        char *const argv[] = {
                BS(go_binary),
                (char *)program_invocation_short_name,
                (char *)is_debug,
                BS(bdata->name.full),
                BS(bdata->name.path),
                BS(bdata->topdir->pathname),
                (char *)0
        };

        int fds[2][2], pid;
        openpipe(fds[0]);
        openpipe(fds[1]);

        if ((pid = fork()) == 0) {
                if (dup2(fds[0][READ_FD], STDIN_FILENO) == (-1))
                        err(1, "dup2() failed\n");
                if (dup2(fds[1][WRITE_FD], STDOUT_FILENO) == (-1))
                        err(1, "dup2() failed\n");

                close(fds[0][0]);
                close(fds[0][1]);
                close(fds[1][0]);
                close(fds[1][1]);

                if (execvp(BS(go_binary), argv) == (-1))
                        err(1, "exec() failed\n");
        }

        close(fds[0][READ_FD]);
        close(fds[1][WRITE_FD]);
        bdata->godata.wr_fd = fds[0][WRITE_FD];
        bdata->godata.rd_fd = fds[1][READ_FD];
        bdata->godata.pid   = pid;

        return pid;
}

static void
openpipe(int fds[2])
{
        int flg;
        if (pipe(fds) == (-1))
                err(1, "pipe()");
#ifdef __linux__
        if (fcntl(fds[0], F_SETPIPE_SZ, 16384) == (-1))
                err(2, "fcntl(F_SETPIPE_SZ)");
#endif

        for (int i = 0; i < 2; ++i) {
                if ((flg = fcntl(fds[i], F_GETFL)) == (-1))
                        err(3+i, "fcntl(F_GETFL)");
                if (fcntl(fds[i], F_SETFL, flg | O_CLOEXEC) == (-1))
                        err(5+i, "fcntl(F_SETFL)");
        }
}

/*--------------------------------------------------------------------------------------*/

static void
write_buffer(Buffer *bdata, bstring *buf)
{
        unsigned n;
        {
                char len_str[16];
                unsigned slen = sprintf(len_str, "%010u", buf->slen);
                /* echo("Writing %d as %s\n", buf->slen, len_str); */
                n = write(bdata->godata.wr_fd, len_str, slen);
                if (n != slen)
                        err(1, "write() -> %u != %u", n, slen);
        }
        n = write(bdata->godata.wr_fd, buf->data, buf->slen);
        if (n != buf->slen)
                err(1, "write() -> %u != %u", n, buf->slen);
}

static bstring *
read_pipe(Buffer *bdata)
{
        uint32_t num2read;
        {
                char buf[16], *p;
                long nread = read(bdata->godata.rd_fd, buf, 10);
                assert(nread == 10);
                buf[10] = '\0';

                num2read = strtoull(buf, &p, 10);
                assert(p == buf + 10);
        }

        bstring *ret = b_create(num2read + 1);
        do {
                ret->slen += read(bdata->godata.rd_fd, ret->data + ret->slen, num2read);
        } while (ret->slen != num2read);
        if (ret->slen != num2read)
                err(1, "read() -> %u != %u", ret->slen, num2read);
        ret->data[ret->slen] = '\0';

        return ret;

}

/*--------------------------------------------------------------------------------------*/

noreturn void *
highlight_go_pthread_wrapper(void *vdata)
{
        highlight_go((Buffer *)vdata);
        pthread_exit();
}

static void
parse_go_output(Buffer *bdata, b_list *output)
{
        struct go_output data;
        bstring const   *group;
        mpack_arg_array *calls = new_arg_array();

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        B_LIST_FOREACH (output, tok) {
                /* The data is so regular that we can get away with using scanf.
                 * I feel a bit dirty though. */
                sscanf(BS(tok), "%lc\t%u\t%u\t%u\t%u\t%u\t%1023s", &data.ch,
                       &data.start.line, &data.start.column, &data.end.line,
                       &data.end.column, &data.ident.len, data.ident.str);

                {
                        bstring tmp = bt_fromblk(data.ident.str, data.ident.len);
                        if (ident_is_ignored(bdata, &tmp))
                                continue;
                }

                group = find_group(bdata->ft, data.ch);
                if (group) {
                        line_data const ln = {data.start.line, data.start.column, data.end.column};
                        add_hl_call(calls, bdata->num, bdata->hl_id, group, &ln);
                }
        }

        nvim_call_atomic(calls);
        mpack_destroy_arg_array(calls);
}

/*======================================================================================*/

static b_list *
separate_and_sort(bstring *output)
{
        uint8_t *bak = output->data;
        b_list  *ret = b_list_create();
        bstring  tok = BSTR_STATIC_INIT;
        while (b_memsep(&tok, output, '\n')) {
                bstring *tmp = b_refblk(tok.data, tok.slen + 1U, true);

                if (b_list_append(ret, tmp) != BSTR_OK)
                        errx(1, "Fatal BSTRING runtime error");
        }

        //if (b_list_remove_dups_2(&ret) != BSTR_OK)
        //        errx(1, "Fatal BSTRING runtime error");

#if 0
        ECHO("Sorted. List is now %u in size\n", ret->qty);
        for (unsigned i = 0; i < ret->qty; ++i) {
                eprintf("%s\n", BS(ret->lst[i]));
        }
#endif
        output->data = bak;
        /* free(output); */
        /* frej(bak); */


        qsort(ret->lst, ret->qty, sizeof(bstring *), &b_strcmp_fast_wrap);
        return ret;
}

static inline bool
ident_is_ignored(Buffer *bdata, bstring const *tok)
{
        if (!bdata->ft->ignored_tags)
                return false;
        return B_LIST_BSEARCH_FAST(bdata->ft->ignored_tags, tok) != NULL;
}
