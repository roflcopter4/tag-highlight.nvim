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
        char ch;
        struct {
                unsigned line, column;
        } start, end;
        struct {
                unsigned len;
                char     str[32768];
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
#define ADD_CALL(CH)                                                                 \
        do {                                                                         \
                if ((group = find_group(bdata->ft, info, info->num, (CH))))          \
                        add_hl_call(calls, bdata->num, bdata->hl_id, group,          \
                                    &(line_data){data.start.line, data.start.column, \
                                                 data.end.column});                  \
        } while (0)

static void        parse_go_output(Buffer *bdata, struct cmd_info *info, b_list *output);
static b_list     *separate_and_sort(bstring *output);
static inline bool ident_is_ignored(Buffer *bdata, bstring const *tok) __attribute__((pure));
static int         b_list_remove_dups_2(b_list **listp);

/*======================================================================================*/

int
highlight_go(Buffer *bdata)
{
        int      retval  = 0;
        unsigned cnt_val = p99_count_inc(&bdata->lock.num_workers);
        if (cnt_val >= 2) {
                p99_count_dec(&bdata->lock.num_workers);
                return retval;
        }

        pthread_mutex_lock(&bdata->lock.total);
        bstring *tmp = ll_join_bstrings(bdata->lines, '\n');

        bstring *go_binary = nvim_call_function(B(PKG "install_info#GetBinaryPath"), E_STRING).ptr;
        b_catlit(go_binary, "/golang" CMD_SUFFIX);
        struct stat st;
        if (stat(BS(go_binary), &st) != 0) {
                retval = errno;
                goto cleanup;
        }
        
        ECHO("Using binary %s\n", go_binary);

        char *const argv[] = {
                BS(go_binary),
                (char *)program_invocation_short_name,
                (char *)is_debug,
                BS(bdata->name.full),
                BS(bdata->name.path),
                BS(bdata->topdir->pathname),
                (char *)0
        };

        struct cmd_info *info = getinfo(bdata);
        bstring         *rd   = get_command_output(BS(go_binary), argv, tmp, &retval);
        if (retval != 0)
                errx(1, "Fuck! (%d)", retval);
        if (!rd || !rd->data || rd->slen == 0) {
                b_free(rd);
                retval = (-1);
                goto cleanup;
        }

        b_list *data = separate_and_sort(rd);

        parse_go_output(bdata, info, data);
        b_destroy_all(tmp, rd, go_binary);
        b_list_destroy(data);
        talloc_free(info);

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
parse_go_output(Buffer *bdata, struct cmd_info *info, b_list *output)
{
        struct go_output data;
        bstring const   *group;
        mpack_arg_array *calls = new_arg_array();

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        B_LIST_FOREACH (output, tok, i) {
                sscanf(BS(tok), "%c\t%u\t%u\t%u\t%u\t%u\t%s", &data.ch,
                       &data.start.line, &data.start.column, &data.end.line,
                       &data.end.column, &data.ident.len, data.ident.str);

                {
                        bstring tmp = bt_fromblk(data.ident.str, data.ident.len);
                        if (ident_is_ignored(bdata, &tmp))
                                continue;
                }

                switch (data.ch) {
                case 'p': case 'f': case 'm': case 'c':
                case 't': case 's': case 'i': case 'v':
                        group = find_group(bdata->ft, info, info->num, data.ch);
                        if (group) {
                                line_data const ln = {data.start.line, data.start.column, data.end.column};
                                add_hl_call(calls, bdata->num, bdata->hl_id, group, &ln);
                        }
                        break;
                default:
                        SHOUT("Unidentifiable string... %c\n", data.ch);
                        break;
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
        /* free(bak); */
        return ret;
}

static inline bool
ident_is_ignored(Buffer *bdata, bstring const *tok)
{
        if (!bdata->ft->ignored_tags)
                return false;
        return B_LIST_BSEARCH_FAST(bdata->ft->ignored_tags, tok) != NULL;
}


static int
b_list_remove_dups_2(b_list **listp)
{
        if (!listp || !*listp || !(*listp)->lst || (*listp)->qty == 0)
                return (-1);

        b_list *toks = *listp;

        qsort(toks->lst, toks->qty, sizeof(bstring *), &b_strcmp_fast_wrap);

        b_list *uniq = b_list_create_alloc(toks->qty);
        uniq->lst[0] = talloc_move(uniq->lst, &toks->lst[0]);
        uniq->qty    = 1;

        for (unsigned i = 1; i < toks->qty; ++i) {
                int const ret = b_iseq(toks->lst[i], toks->lst[i-1]);
                if (ret == (-1)) {
                        //ECHO("bstrlib returned an error comparing: \"%s\"  and  \"%s\"", toks->lst[i], toks->lst[i-1]);
                } else if (ret == 0) {
                        uniq->lst[uniq->qty] = talloc_move(uniq->lst, &toks->lst[i]);
                        ++uniq->qty;
                }
        }

        talloc_free(*listp);
        *listp = uniq;
        return BSTR_OK;
}
