// +build ignore

/* Above lines (including the blank line) are necessary to keep go from trying
 * to include this file in its build. */
#include "Common.h"
#include "lang/lang.h"
#include "my_p99_common.h"
#include <sys/stat.h>

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
static inline bool ident_is_ignored(Buffer *bdata, const bstring *tok);

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

        pthread_mutex_lock(&bdata->lock.update);
        pthread_mutex_lock(&bdata->lines->lock);   
        bstring *tmp = ll_join(bdata->lines, '\n');
        pthread_mutex_unlock(&bdata->lines->lock);

        char buf[8192];
        snprintf(buf, 8192, "%u", tmp->slen);

        bstring *go_binary = nvim_call_function(, B(PKG "install_info#GetBinaryPath"), E_STRING).ptr;
        b_catlit(go_binary, "/golang" CMD_SUFFIX);
        struct stat st;
        if (stat(BS(go_binary), &st) != 0) {
                retval = errno;
                goto cleanup;
        }
        
        ECHO("Using binary %s\n", go_binary);

        struct cmd_info *info      = getinfo(bdata);
        char *const      argv[]    = {BS(go_binary), BS(bdata->name.full), buf, (char *)0};
        bstring         *rd        = get_command_output(BS(go_binary), argv, tmp);
        b_list          *data      = separate_and_sort(rd);

        parse_go_output(bdata, info, data);
        b_destroy_all(tmp, rd);
        b_list_destroy(data);

cleanup:
        b_destroy(tmp);
        p99_count_dec(&bdata->lock.num_workers);
        pthread_mutex_unlock(&bdata->lock.update);
        return retval;
}

void *
highlight_go_pthread_wrapper(void *vdata)
{
        highlight_go((Buffer *)vdata);
        pthread_exit();
}

static void
parse_go_output(Buffer *bdata, struct cmd_info *info, b_list *output)
{
        struct go_output data;
        const bstring   *group;
        mpack_arg_array *calls = new_arg_array();

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(,bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        B_LIST_FOREACH_2 (output, tok, i) {
                sscanf(BS(tok), "%c\t%u\t%u\t%u\t%u\t%u\t%s", &data.ch,
                       &data.start.line, &data.start.column, &data.end.line,
                       &data.end.column, &data.ident.len, data.ident.str);

                if (ident_is_ignored(bdata, &(bstring){data.ident.len, 0,
                                                       (uchar *)data.ident.str, 0}))
                        continue;

                switch (data.ch) {
                case 'p': case 'f': case 'm': case 'c':
                case 't': case 's': case 'i': case 'v':
                        ADD_CALL(data.ch);
                        break;
                default:
                        SHOUT("Unidentifiable string... %c\n", data.ch);
                        break;
                }
        }

        nvim_call_atomic(,calls);
        mpack_destroy_arg_array(calls);
}

/*======================================================================================*/

static b_list *
separate_and_sort(bstring *output)
{
        uint8_t *bak = output->data;
        b_list  *ret = b_list_create();
        bstring *tok = BSTR_NULL_INIT;
        while (b_memsep(tok, output, '\n'))
                b_list_append(&ret, b_refblk(tok->data, tok->slen));
        b_list_remove_dups(&ret);
        output->data = bak;
        return ret;
}

static inline bool
ident_is_ignored(Buffer *bdata, const bstring *tok)
{
        if (!bdata->ft->ignored_tags)
                return false;
        return B_LIST_BSEARCH_FAST(bdata->ft->ignored_tags, tok) != NULL;
}
