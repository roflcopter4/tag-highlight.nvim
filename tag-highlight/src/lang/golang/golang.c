#include "Common.h"
#include "lang/lang.h"
#include "lang/golang/golang.h"

#include "contrib/p99/p99_count.h"

#ifndef WEXITSTATUS
# define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
#endif
#ifdef _WIN32
# define CMD_SUFFIX ".exe"
#else
# define CMD_SUFFIX
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wformat"

#define ALIGN(n) __attribute__((__aligned__(n)))

extern void try_go_crap(Buffer *bdata);


/* Clang doesn't shut up unless you use __attribute__((aligned)). Someone should tell
 * the LLVM devs that alignas exists. */
struct go_output {
        int ch;
        struct {
                unsigned line;
                unsigned column;
        } ALIGN(8) start, end;
        struct {
                char     str[1024];
                unsigned len;
        } ALIGN(128) ident;
} ALIGN(128);

#undef ALIGN

static mpack_arg_array *parse_go_output(Buffer *bdata, b_list *output);
static b_list          *separate_and_sort(bstring *output);
static inline bool ident_is_ignored(Buffer *bdata, bstring const *tok) __attribute__((pure));

/*======================================================================================*/

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

#if 0 //ndef _WIN32
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

        struct golang_data *gd = bdata->godata.sock_info;
        mpack_arg_array    *calls;
        b_list *data;

        if (!tmp || tmp->slen == 0)
                goto error;

        golang_send_msg(gd, tmp);
        talloc_free(tmp);
        tmp = golang_recv_msg(gd);

        if (!tmp || !tmp->data || tmp->slen == 0)
                goto error;

        data  = separate_and_sort(tmp);
        calls = parse_go_output(bdata, data);
        talloc_free(data);
        b_free(tmp);

        pthread_mutex_lock(&bdata->lock.total);
        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&bdata->lock.lang_mtx);

        nvim_call_atomic(calls);
        talloc_free(calls);
        pthread_mutex_unlock(&bdata->lock.total);
        return retval;

error:
        retval = retval == 0 ? (-1) : retval;
        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&bdata->lock.lang_mtx);
        return retval;
}

/*--------------------------------------------------------------------------------------*/

static mpack_arg_array *
parse_go_output(Buffer *bdata, b_list *output)
{
        struct go_output out;
        bstring const   *group;
        mpack_arg_array *calls = new_arg_array();
        memset(&out, 0, sizeof(out));

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(bdata->num);
        else
                add_clr_call(calls, (int)bdata->num, (int)bdata->hl_id, 0, -1);

        B_LIST_FOREACH (output, tok) {
                /* The out is so regular that we can get away with using scanf.
                 * I feel a bit dirty though. */
                /*NOLINTNEXTLINE(cert-err34-c)*/
                sscanf(BS(tok), "%c\t%u\t%u\t%u\t%u\t%u\t%1023s", &out.ch,
                       &out.start.line, &out.start.column, &out.end.line,
                       &out.end.column, &out.ident.len, out.ident.str);

                if (ident_is_ignored(bdata, btp_fromblk(out.ident.str, out.ident.len)))
                        continue;

                group = find_group(bdata->ft, out.ch);
                if (group) {
                        line_data const ln = {out.start.line, out.start.column, out.end.column};
                        add_hl_call(calls, (int)bdata->num, (int)bdata->hl_id, group, &ln);
                }
        }

        return calls;
}

/*======================================================================================*/

NORETURN void *
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
                if (tok.data[tok.slen-1] == '\r')
                        tok.data[--tok.slen] = '\0';
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
