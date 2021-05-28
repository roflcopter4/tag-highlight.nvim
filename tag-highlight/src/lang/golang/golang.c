#include "Common.h"
#include "lang/lang.h"
#include "lang/golang/golang.h"

#include "contrib/p99/p99_count.h"

#ifndef WEXITSTATUS
# define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
#endif
#ifdef DOSISH
# define CMD_SUFFIX ".exe"
#else
# define CMD_SUFFIX
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wformat"

extern void try_go_crap(Buffer *bdata);

struct go_output {
        int ch;
        struct {
                unsigned line;
                unsigned column;
        } __attribute__((aligned(8))) start, end;
        struct {
                unsigned len;
                char     str[1024];
        } __attribute__((aligned(128))) ident;
} __attribute__((aligned(128)));



static mpack_arg_array *parse_go_output(Buffer *bdata, b_list *output);
static b_list     *separate_and_sort(bstring *output);
static inline bool ident_is_ignored(Buffer *bdata, bstring const *tok) __attribute__((pure));

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
        if (cnt_val > 3) {
                p99_count_dec(&bdata->lock.num_workers);
                return retval;
        }

        pthread_mutex_lock(&bdata->lock.lang_mtx);

#if 0 //ndef DOSISH
        if (!atomic_flag_test_and_set(&bdata->godata.flg)) {
                start_binary(bdata);
        } 
        else {
                errno = 0;
                kill(bdata->godata.pid, 0);
                int e = errno;
                if (e != 0) {
                        shout("Binary not available: %d : %s", e, strerror(e));
                        close(bdata->godata.rd_fd);
                        close(bdata->godata.wr_fd);
                        start_binary(bdata);
                }
        }
#endif

        pthread_mutex_lock(&bdata->lock.total);
        bstring *tmp = ll_join_bstrings(bdata->lines, '\n');
        pthread_mutex_unlock(&bdata->lock.total);

        if (!tmp || tmp->slen == 0)
                goto error;

        struct golang_data *gd = bdata->godata.sock_info;

#if 0
        write_buffer(bdata, tmp);
        b_free(tmp);
        tmp = read_pipe(bdata);
#endif
        /* eprintf("Read %'u bytes (go output)", tmp->slen); */
        /* eprintf("Writing %u bytes.\n", tmp->slen); */

        golang_send_msg(gd->write_fd, tmp);
        talloc_free(tmp);
        /* NANOSLEEP(3, NSEC2SECOND / 2); */
#if 0
        {
                struct timespec *tmp = MKTIMESPEC(3, (NSEC2SECOND / 2));
                eprintf("Sleeping for %g seconds", TIMESPEC2DOUBLE(tmp));
                nanosleep(tmp, NULL);
        }
#endif
        tmp = golang_recv_msg(gd->read_fd);

        if (!tmp || !tmp->data || tmp->slen == 0) {
                warnx("What is it empty or somethin?");
                retval = (-1);
                goto error;
        }

        b_list *data = separate_and_sort(tmp);
        mpack_arg_array *calls = parse_go_output(bdata, data);
        talloc_free(data);

        b_free(tmp);

        pthread_mutex_lock(&bdata->lock.total);
        pthread_mutex_unlock(&bdata->lock.lang_mtx);
        p99_count_dec(&bdata->lock.num_workers);

        nvim_call_atomic(calls);
        talloc_free(calls);

        pthread_mutex_unlock(&bdata->lock.total);


        return retval;

error:
        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&bdata->lock.lang_mtx);
        return retval;
}

/*--------------------------------------------------------------------------------------*/

static mpack_arg_array *
parse_go_output(Buffer *bdata, b_list *output)
{
        struct go_output data;
        bstring const   *group;
        mpack_arg_array *calls = new_arg_array();

        memset(&data, 0, sizeof(data));

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        B_LIST_FOREACH (output, tok) {
                /* The data is so regular that we can get away with using scanf.
                 * I feel a bit dirty though. */
                /* warnx("lookin at \"%*s\"", (int)tok->slen, BS(tok)); */
                sscanf(BS(tok), "%c\t%u\t%u\t%u\t%u\t%u\t%1023s", &data.ch,
                       &data.start.line, &data.start.column, &data.end.line,
                       &data.end.column, &data.ident.len, data.ident.str);

                {
                        bstring tmp = bt_fromblk(data.ident.str, data.ident.len);
                        if (ident_is_ignored(bdata, &tmp))
                                continue;
                }

                group = find_group(bdata->ft, data.ch);
                /* warnx("%s %s %c", BS(group), BTS(bdata->ft->ctags_name), data.ch); */
                if (group) {
                        line_data const ln = {data.start.line, data.start.column, data.end.column};
                        /* warnx("%u - %u - %u", data.start.line, data.start.column, data.end.column); */
                        add_hl_call(calls, bdata->num, bdata->hl_id, group, &ln);
                }
        }

        return calls;
}

/*======================================================================================*/

noreturn void *
highlight_go_pthread_wrapper(void *vdata)
{
        highlight_go((Buffer *)vdata);
        pthread_exit();
}

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

        output->data = bak;
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
#pragma GCC diagnostic pop
