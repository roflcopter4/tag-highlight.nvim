// +build ignore

/* Above lines are necessary to keep go from trying to include this file. */
#include "lang/common.h"
#include "my_p99_common.h"

extern void try_go_crap(struct bufdata *bdata);

struct go_output {
        char ch;
        struct {
                unsigned line, column;
        } start, end;
};

/* #define GO_BINARY_THING "/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/lang/golang/src/tag_highlight_go/tag_highlight_go" */
/* #define GO_BINARY_THING "/home/bml/.vim/dein/repos/github.com/roflcopter4/tag-highlight.nvim/tag_highlight/src/lang/golang/src/tag_highlight_go/tag_highlight_go" */
#define READ_FD  (0)
#define WRITE_FD (1)

#define ADD_CALL(CH)                                                                 \
        do {                                                                         \
                if ((group = find_group(bdata->ft, info, info->num, (CH))))          \
                        add_hl_call(calls, bdata->num, bdata->hl_id, group,          \
                                    &(line_data){data.start.line, data.start.column, \
                                                 data.end.column});                  \
        } while (0)

static void parse_go_output(struct bufdata *bdata, struct cmd_info *info, bstring *output);

/*======================================================================================*/

void
highlight_go(struct bufdata *bdata)
{
        pthread_mutex_lock(&bdata->lines->lock);   
        bstring *tmp = ll_join(bdata->lines, '\n');
        pthread_mutex_unlock(&bdata->lines->lock); 

        bstring *go_binary = b_format("%s/.vim_tags/bin/Go_Cmd", HOME);

        struct cmd_info *info   = getinfo(bdata);
        char *const      argv[] = {"tag_highlight_go", BS(bdata->name.full), (char *)0};
        bstring         *rd     = get_command_output(BS(go_binary), argv, tmp);
        b_destroy(tmp);
        parse_go_output(bdata, info, rd);
        b_destroy(go_binary);
}

void *
highlight_go_pthread_wrapper(void *vdata)
{
        highlight_go((struct bufdata *)vdata);
        pthread_exit();
}

static void
parse_go_output(struct bufdata *bdata, struct cmd_info *info, bstring *output)
{
        const bstring  *group;
        bstring        *tok   = &(bstring){0, 0, NULL, 0};
        uchar          *bak   = output->data;
        nvim_arg_array *calls = new_arg_array();

        if (bdata->hl_id == 0)
                bdata->hl_id = nvim_buf_add_highlight(,bdata->num);
        else
                add_clr_call(calls, bdata->num, bdata->hl_id, 0, -1);

        while (b_memsep(tok, output, '\n')) {
                struct go_output data;
                sscanf(BS(tok), "%c\t%u\t%u\t%u\t%u", &data.ch, &data.start.line,
                       &data.start.column, &data.end.line, &data.end.column);

                switch (data.ch) {
                case 'p': case 'f': case 'm': case 'c':
                case 't': case 's': case 'i': case 'v':
                        ADD_CALL(data.ch);
                        break;
#if 0
                case 'p':
                        ADD_CALL(CTAGS_PACKAGE);
                        break;
                case 'f':
                        ADD_CALL(CTAGS_FUNCTION);
                        break;
                case 'm':
                        ADD_CALL(CTAGS_MEMBER);
                        break;
                case 'c':
                        ADD_CALL(EXTENSION_CONSTANT);
                        break;
                case 't':
                        ADD_CALL(CTAGS_TYPE);
                        break;
                case 's':
                        ADD_CALL(CTAGS_STRUCT);
                        break;
                case 'i':
                        ADD_CALL(CTAGS_INTERFACE);
                        break;
#endif
                default:
                        SHOUT("Unidentifiable string... %c\n", data.ch);
                        break;
                }
        }

        nvim_call_atomic(,calls);
        _nvim_destroy_arg_array(calls);
        xfree(bak);
        xfree(output);
}
