#include "util.h"

#include "data.h"
#include "highlight.h"
#include "mpack.h"

#define THE_WORD "int"

static struct taglist * analyze_tags(struct bufdata *bdata);
static bstring * get_hl_group(bstring *buffer, struct bufdata *bdata, struct tag *tag);
static void add_hl_call(struct atomic_call_array **calls,
                        const int bufnum,     const int hl_id,
                        const bstring *group, const unsigned column,
                        const unsigned start, const int end);


static b_list * breakdown_file(const struct bufdata *bdata);

#ifdef DEBUG
#  define B_LOG b_fputs
#else
#  define B_LOG(...)
#endif


/*======================================================================================*/

int
my_highlight(const int bufnum, struct bufdata *bdata)
{
        bdata = null_find_bufdata(bufnum, bdata);
        unsigned lineno = 0;
        int      hl_id  = 0;

        LL_FOREACH_F (bdata->lines, node) {
                char *tmp = BS(node->data);

                if ((tmp = strstr(tmp, THE_WORD))) {
                        const unsigned start = (unsigned)(((ptrdiff_t)tmp) -
                                                          ((ptrdiff_t)node->data->data));
                        const unsigned end   = start + (sizeof(THE_WORD) - 1llu);

                        hl_id = nvim_buf_add_highlight(0, bdata->num, hl_id, B("Keyword"), lineno, start, end);
                }

                ++lineno;
        }

        return hl_id;
}

/*======================================================================================*/

extern int main_hl_id;
int main_hl_id;

static inline void free_char(char **arg) { free(*arg); }


void
my_parser(const int bufnum, struct bufdata *bdata)
{
        bdata               = null_find_bufdata(bufnum, bdata);
        /* const b_list *tags  = bdata->topdir->tags; */
        FILE         *aaaaa = safe_fopen_fmt("%s/parselog.log", "w", HOME);
        /* const uchar  *last  = NULL; */
        struct taglist *tags = analyze_tags(bdata);

        struct atomic_call_array *calls = NULL;
        main_hl_id = nvim_buf_add_highlight(0, bdata->num, 0, B(""), 0, 0, 0);

        b_list *stripped = breakdown_file(bdata);
        for (unsigned i = 0; i < stripped->qty; ++i)
                B_LOG(aaaaa, stripped->lst[i], B("\n"));

        for (unsigned i = 0; i < tags->qty; ++i) {
                /* const char   *tagend = strchr(BS(tags->lst[i]), '\t');
                const size_t  taglen = (ptrdiff_t)tagend - (ptrdiff_t)tags->lst[i]->data;
                uchar        *tag    = malloc(taglen + 1); */

                /* memcpy(tag, tags->lst[i]->, taglen);
                tag[taglen] = (uchar)'\0'; */

                /* if (last && strcmp((char *)tag, (char *)last) == 0)
                        goto skip_dup; */

#define CURTAG (tags->lst[i])

                int      lineno = (-1);
                bstring *buffer = b_alloc_null(1024);

                for (unsigned x = 0; x < stripped->qty; ++x) {
                        ++lineno;
                        bstring *line = stripped->lst[x];
                        uchar   *tok  = line->data;

                        const unsigned taglen = CURTAG->b->slen;
                        while ((tok = (uchar *)strstr((char *)tok, BS(CURTAG->b)))) {
                                if (tok != line->data &&
                                                (isalnum((*(tok - 1))) ||
                                                 *(tok - 1) == '_'))
                                        goto next;
                                if (*(tok + taglen) &&
                                                (isalnum(*(tok + taglen)) ||
                                                 *(tok + taglen) == '_'))
                                        goto next;

                                const unsigned start = (unsigned)((ptrdiff_t)tok -
                                                                  (ptrdiff_t)line->data);
                                const unsigned end   = start + taglen;

                                /* fprintf(aaaaa, "Found tag \"%s\" on line %u at %u to %u (%c)\n",
                                        BS(CURTAG->b), lineno+1, start, end, *(tok + taglen));
                                fprintf(aaaaa, "Found at: %s\n", tok);
                                fprintf(aaaaa, "whole line: %s\n\n", BS(node->data)); */

                                add_hl_call(&calls, bdata->num, main_hl_id,
                                            get_hl_group(buffer, bdata, CURTAG),
                                            lineno, start, end);
next:
                                tok += 1;
                        }

                }
        }

        b_list_destroy(stripped);

        if (calls)
                echo("Finished creating calls, sending %u calls to nvim.", calls->qty);

        nvim_call_atomic(0, calls);
        destroy_call_array(calls);

#undef CURTAG

        for (unsigned i = 0; i < tags->qty; ++i) {
                b_free(tags->lst[i]->b);
                free(tags->lst[i]);
        }
        free(tags->lst);
        free(tags);

        fclose(aaaaa);
}


static struct taglist *
analyze_tags(struct bufdata *bdata)
{
        bstring        *joined = strip_comments(bdata);
        b_list         *toks   = tokenize(bdata, joined);
        struct taglist *tags   = process_tags(bdata, toks);

        b_list_destroy(toks);
        b_destroy(joined);
        return tags;
}


static bstring *
get_hl_group(bstring *buffer, struct bufdata *bdata, struct tag *tag)
{
#if 0
        static _Thread_local bstring ret;
        static _Thread_local uchar   buf[1024];
#   if 39
        bstring *group = nvim_get_var_fmt(0, MPACK_STRING, B("group"), 1, PKG "#%s#%c",
                                          BTS(bdata->ft->vim_name), tag->kind);
#   endif

        size_t len = snprintf((char*)buf, 1024, "_tag_higlight_%s_%c_%s", BTS(bdata->ft->vim_name), tag->kind, BS(group));
        b_free(group);

        ret = (bstring){ .data = buf, .slen = len, .mlen = 0, .flags = 0 };
        return &ret;
#endif
        switch (tag->kind) {
        case 'f': b_assign(buffer, B("CFuncTag"));   break;
        case 'd': b_assign(buffer, B("PreProc"));    break;
        case 'm': b_assign(buffer, B("CMember"));    break;
        case 't': b_assign(buffer, B("Type"));       break;
        case 's': b_assign(buffer, B("Type"));       break;
        case 'g': b_assign(buffer, B("Type"));       break;
        case 'v': b_assign(buffer, B("cGlobalVar")); break;
        case 'e': b_assign(buffer, B("Enum"));       break;
        case 'u': b_assign(buffer, B("Type"));       break;
        default:  b_assign(buffer, B("Keyword"));    break;
        }

        return buffer;
}


static void
add_hl_call(struct atomic_call_array **calls,
            const int       bufnum,
            const int       hl_id,
            const bstring  *group,
            const unsigned  column,
            const unsigned  start,
            const int       end)
{
        assert(calls);
        if (!*calls) {
                echo("allocating calls...");
                (*calls)            = xmalloc(sizeof **calls);
                (*calls)->mlen      = 32;
                (*calls)->fmt       = calloc(sizeof(char *), (*calls)->mlen);
                (*calls)->args      = calloc(sizeof(union atomic_call_args *), (*calls)->mlen);
                (*calls)->qty       = 0;
        } else if ((*calls)->qty >= (*calls)->mlen-1) {
                (*calls)->mlen     *= 2;
                (*calls)->fmt       = nrealloc((*calls)->fmt, sizeof(char *), (*calls)->mlen);
                (*calls)->args      = nrealloc((*calls)->args, sizeof(union atomic_call_args *), (*calls)->mlen);
        }

        (*calls)->fmt[(*calls)->qty]         = strdup("s[dd,s,ddd]");
        (*calls)->args[(*calls)->qty]        = nmalloc(sizeof(union atomic_call_args), 7);
        (*calls)->args[(*calls)->qty][0].str = b_lit2bstr("nvim_buf_add_highlight");
        (*calls)->args[(*calls)->qty][1].num = bufnum;
        (*calls)->args[(*calls)->qty][2].num = hl_id;
        (*calls)->args[(*calls)->qty][3].str = b_strcpy(group);
        (*calls)->args[(*calls)->qty][4].num = column;
        (*calls)->args[(*calls)->qty][5].num = start;
        (*calls)->args[(*calls)->qty][6].num = end;

        ++(*calls)->qty;
}


/*======================================================================================*/

#define WANT_IF_ZERO (UCHAR_MAX + 1)


static b_list *
breakdown_file(const struct bufdata *const bdata)
{
        b_list *list    = b_list_create_alloc(bdata->lines->qty);
        int     want    = 0;
        int     ifcount = 0;
        bool    esc     = 0;

        LL_FOREACH_F (bdata->lines, node) {
                const unsigned     len  = node->data->slen;
                const uchar *const str  = node->data->data;
                bstring           *repl = b_alloc_null(len + 1u);
                repl->slen              = len;
                unsigned i              = 0;

                memset(repl->data, ' ', len);
                repl->data[len] = '\0';

                while (isspace(str[i]))
                        ++i;

                if (want == WANT_IF_ZERO) {
                        if (str[i] == '#') {
                                do {
                                        ++i;
                                } while (isspace(str[i]));

                                if (strncmp((char *)(&str[i]), SLS("if")) == 0)
                                        ++ifcount;
                                else if (strncmp((char *)(&str[i]), SLS("endif")) == 0)
                                        --ifcount;

                                if (ifcount == 0)
                                        want = 0;
                        }
                        goto next_line;
                } else if (!want && str[i] == '#') {
                        do {
                                ++i;
                        } while (isspace(str[i]));

                        if (strncmp((char *)(&str[i]), SLS("if 0")) == 0) {
                                ++ifcount;
                                want = WANT_IF_ZERO;
                                goto next_line;
                        }
                }

                for (; i < len; ++i) {
                        if (isspace(str[i])) {
                                ; /* No need to do anything. */
                        } else if (want) {
                                if (want == '\n')
                                        goto line_comment;
                                if (str[i] == want) {
                                        switch (want) {
                                        case '*':
                                                if (str[i+1] == '/') {
                                                        want = 0;
                                                        ++i;
                                                }
                                                break;
                                        case '\'':
                                        case '"':
                                                if (!esc)
                                                        want = 0;
                                                break;
                                        }
                                }
                        } else {
                                switch (str[i]) {
                                case '\'':
                                        want = '\'';
                                        goto next_char;
                                case '"':
                                        want = '"';
                                        goto next_char;
                                case '/':
                                        if (str[i+1] == '*') {
                                                want = '*';
                                                ++i;
                                                goto next_char;
                                        } else if (str[i+1] == '/') {
                                        line_comment:
                                                if (str[len-1] == '\\')
                                                        want = '\n';
                                                else
                                                        want = 0;
                                                goto next_line;
                                        }
                                        break;
                                }

                                repl->data[i] = str[i];
                        }

                next_char:
                        esc = (str[i] == '\\') ? !esc : false;
                }

        next_line:
                b_list_append(&list, repl);
                esc = false;
        }

        return list;
}
