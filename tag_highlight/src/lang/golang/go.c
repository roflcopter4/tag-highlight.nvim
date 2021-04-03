// +build ignore

/* Above lines (including the blank line) are necessary to keep go from trying
 * to include this file in its build. */
#include "Common.h"
#include "lang/lang.h"

#include "contrib/p99/p99_count.h"
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

static void        parse_go_output(Buffer *bdata, b_list *output);
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
        bstring const *go_binary = settings.go_binary;

        int      retval  = 0;
        unsigned cnt_val = p99_count_inc(&bdata->lock.num_workers);
        if (cnt_val >= 2) {
                p99_count_dec(&bdata->lock.num_workers);
                return retval;
        }

        pthread_mutex_lock(&bdata->lock.total);
        bstring *tmp = ll_join_bstrings(bdata->lines, '\n');

        char *const argv[] = {
                BS(go_binary),
                (char *)program_invocation_short_name,
                (char *)is_debug,
                BS(bdata->name.full),
                BS(bdata->name.path),
                BS(bdata->topdir->pathname),
                (char *)0
        };

        bstring         *rd   = get_command_output(BS(go_binary), argv, tmp, &retval);
        if (retval != 0)
                errx(1, "Fuck! (%d)", retval);
        if (!rd || !rd->data || rd->slen == 0) {
                b_free(rd);
                retval = (-1);
                goto cleanup;
        }

        b_list *data = separate_and_sort(rd);

        parse_go_output(bdata, data);
        b_destroy_all(tmp, rd);
        b_list_destroy(data);

cleanup:
        b_destroy(tmp);
        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&bdata->lock.total);
        return retval;
}

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
