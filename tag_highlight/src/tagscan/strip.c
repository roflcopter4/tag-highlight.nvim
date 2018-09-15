#include "util/util.h"
#include <ctype.h>

#include "data.h"
#include "highlight.h"


static const struct lang_s {
        const enum filetype_id id;
        const enum basic_types { C_LIKE, PYTHON } type;
} lang_comment_groups[] = {
    { FT_C,      C_LIKE, },
    { FT_CPP,    C_LIKE, },
    { FT_CSHARP, C_LIKE, },
    { FT_GO,     C_LIKE, },
    { FT_JAVA,   C_LIKE, },
    { FT_RUST,   C_LIKE, },
    { FT_PYTHON, PYTHON, },
};

static const struct comment_s {
        const int type;
        const char delim;
} comments[] = {{0, '\0'}, {1, '#'}, {2, ';'}, {3, '"'}};


static void handle_cstyle(bstring **vim_bufp);
static void handle_python(bstring *vim_buf);

/* These functions perform a rather crude stripping of comments and string
 * literals from the a few languages. This means fewer words to search though
 * when the buffer is searched for applicable tags later on, and avoids any
 * false positives caused by tag names appearing in comments and strings. */

bstring *
strip_comments(struct bufdata *bdata)
{
        const struct comment_s *com = NULL;
        /* unsigned               *bytenum = nvim_call_function_args(
            0, B("wordcount()"), MPACK_NUM, B("bytes"), 1, "d", bdata->num); */

        unsigned bytenum = 0;

        LL_FOREACH_F (bdata->lines, line)
                bytenum += line->data->slen + 1;

        /* warnx("Buffer size is %u", bytenum); */
        bstring *joined = b_alloc_null(bytenum);
        /* xfree(bytenum); */

        LL_FOREACH_F (bdata->lines, line) {
                b_concat(joined, line->data);
                b_conchar(joined, '\n');
        }

        /* warnx("buffer size is definitely %u\n", joined->slen); */

        for (unsigned i = 0; i < ARRSIZ(lang_comment_groups); ++i) {
                if (bdata->ft->id == lang_comment_groups[i].id) {
                        com = &comments[lang_comment_groups[i].type];
                        break;
                }
        }
        if (com) {
                switch (com->type) {
                case C_LIKE: handle_cstyle(&joined); break;
                case PYTHON: handle_python(joined); break;
                default:     abort();
                }
        } else
                warnx("Failed to identify language \"%s\".",
                      BTS(bdata->ft->vim_name));

        return joined;
}


/*============================================================================*/
/* C style languages */


#if 0
#define QUOTE() (single_q || double_q)

#define check_quote(CHECK, OTHER)                      \
    do {                                               \
            if (!(OTHER)) {                            \
                    if (CHECK) {                       \
                            if (!escape)               \
                                    (CHECK) = false,   \
                                    skip = true;       \
                    } else                             \
                            (CHECK) = true;            \
            }                                          \
            slash = false;                             \
    } while (0)

#define ustrchr(USTR, CH)  ((uchar *)(strchr((char *)(USTR), (CH))))
#define ustrstr(USTR, STR) ((uchar *)(strstr((char *)(USTR), (STR))))

enum c_com_type { NONE, BLOCK, LINE };


static void
handle_cstyle(bstring *vim_buf)
{
        /* enum c_com_type comment = NONE; */
        unsigned     space = 0;
        const uchar *pos   = vim_buf->data;

        bool double_q, single_q, slash, escape, skip, header;
        uchar *buf, *buf_orig;

        double_q = single_q = slash = escape = skip = header = false;
        buf_orig = buf = xmalloc(vim_buf->slen + 32);

        if (!*pos) {
                warnx("whole buf \"%s\"", pos);
                errx(1, "Empty vim buffer!");
        }

        /* Add a non-offensive character to the buffer so we never have to worry
         * about going out of bounds when checking 1 character backwards. */
        for (int i = 0; i < 5; ++i)
                *buf++ = ' ';

        do {
                switch (*pos) {
                case '/':
                        if (!QUOTE() && slash) {
                                const uchar *tmp = pos + 1;
                                --buf;
                                /* Find the end of comment, but keep in mind
                                 * that 'single line' C comments can be multiple
                                 * lines long if the newline is escaped. */
                                do
                                        tmp = ustrchr(tmp + 1, '\n');
                                while (tmp && *(tmp - 1) == '\\');

                                if (!tmp) {
                                        warnx("Couldn't find end of line comment.");
                                        break;
                                }
                                pos = tmp;
                                /* Add the newline only if the last char in the
                                 * output buffer was not also a newline. */
                                if (*(buf - 1) == '\n')
                                        skip = true;
                        } else if (!QUOTE())
                                slash = true;
                        break;

                case '*':
                        if (!QUOTE() && slash) {
                                const uchar *tmp;
                                --buf;
                                if (!(tmp = ustrstr(pos, "*/")))
                                        errx(1, "Couldn't find end of block comment.");
                                pos = tmp + 2;
                                /* Don't add newlines after infixed comments. */
                                if (*pos == '\n' && *(buf - 1) == '\n')
                                        skip = true;
                                slash = false;
                        }
                        break;

                case '\n':
                        if (!escape) {
                                slash = double_q = false;
                                if (*(buf - 1) == '\n')
                                        skip = true;
                                header = false;
                        }
                        break;

                case '#':;
                        /* Strip out include directives as well. */
                        const uchar *endln;
                        if (*(buf - 1) == '\n' && (endln = (uchar*)strchr((char*)pos, '\n'))) {
                                const uchar *tmp = pos + 1;
                                while (tmp < endln && isblank(*tmp))
                                        ++tmp;
                                if (strncmp((char*)tmp, SLS("include")) == 0) {
                                        header = true;
                                        pos = endln - 1;
                                        continue;
                                }
                        }
                        slash = false;
                        break;

                case '\\': break;
                case '"':  check_quote(double_q, single_q); break;
                case '\'': check_quote(single_q, double_q); break;
                default:   slash = false;
                }

                escape = (*pos == '\\') ? !escape : false;
                /* Avoid adding spaces at the start of lines, and don't add more
                 * than one space or newline in succession. */
                space  = (isblank(*pos) &&
                          !(skip = (skip) ? true : *(buf - 1) == '\n')
                         ) ? space + 1 : 0;

                if (skip)
                        skip = false;
                else if (!QUOTE() && !header && space < 2)
                        *buf++ = *pos;

        } while (*pos++);

        *buf = '\0';

        xfree(vim_buf->data);
        vim_buf->data = buf_orig;
        vim_buf->slen = vim_buf->mlen = buf - buf_orig - 1;
}

#undef QUOTE
#undef check_quote
#endif


/*============================================================================*/

#define GUESS_AVERAGE_LINELEN (45llu)
#define WANT_IF_ZERO          (UCHAR_MAX + 1)


static void
handle_cstyle(bstring **vim_bufp)
{
        if (!vim_bufp || !*vim_bufp || !(*vim_bufp)->data)
                errx(1, "Null paramater");
        bstring        *data     = *vim_bufp;
        uint8_t        *bak      = data->data;
        bstring         tok[1]   = {{0, 0, NULL, 0}};
        int             ifcount  = 0;
        bool            esc      = 0;
        unsigned short  want     = 0;
        b_list         *list     = b_list_create_alloc(data->slen / GUESS_AVERAGE_LINELEN);

        while (data && *data->data && b_memsep(tok, data, '\n')) {
                char     *repl  = xmalloc(tok->slen + 2ll);
                char     *line  = (char *)tok->data;
                unsigned  len   = tok->slen;
                unsigned  i     = 0;
                unsigned  x     = 0;
                bool      empty = true;

                while (isblank(line[i]))
                        ++i;

                if (want == WANT_IF_ZERO) {
                        if (line[i] == '#') {
                                unsigned tmp = i + 1u;
                                while (isblank(line[tmp]))
                                        ++tmp;
                                if (strncmp(&line[tmp], SLS("if")) == 0)
                                        ++ifcount;
                                else if (strncmp(&line[tmp], SLS("endif")) == 0)
                                        --ifcount;

                                if (ifcount == 0)
                                        want = 0;
                        }
                        goto next_line;

                } else if (!want && line[i] == '#') {
                        unsigned tmp = i + 1u;
                        while (isblank(line[tmp]))
                                ++tmp;
                        if (strncmp(&line[tmp], SLS("if 0")) == 0) {
                                ++ifcount;
                                want = WANT_IF_ZERO;
                                goto next_line;
                        }
                }

                for (; i < len; ++i) {
                        if (isblank(line[i])) {
                                /* No need to do anything. */
                        } else if (want) {
                                if (want == '\n')
                                        goto line_comment;
                                if (line[i] == want) {
                                        switch (want) {
                                        case '*':
                                                if (line[i+1] == '/') {
                                                        want = 0;
                                                        i   += 2;
                                                }
                                                break;
                                        case '\'':
                                        case '"':
                                                if (!esc) {
                                                        ++i;
                                                        want = 0;
                                                }
                                                break;
                                        /* Clang whines if I don't have a default */
                                        default:;
                                        }
                                }
                        } else {
                                switch (line[i]) {
                                case '\'':
                                        want = '\'';
                                        break;
                                case '"':
                                        want = '"';
                                        break;
                                case '/':
                                        if (line[i+1] == '*') {
                                                want      = '*';
                                                repl[x++] = ' ';
                                                empty     = false;
                                                ++i;
                                        } else if (line[i+1] == '/') {
                                        line_comment:
                                                if (line[len-1] == '\\' &&
                                                    (len > 3 && line[len-2] != '\\'))
                                                        want = '\n';
                                                else
                                                        want = 0;
                                                goto next_line;
                                        }
                                        break;
                                /* But it doesn't whine here. Go figure. */
                                }
                        }

                        if (!want && line[i] /* && */
                            /* (!isblank(line[i]) || (x > 0 && !isblank(repl[x-1]))) */)
                        {
                                repl[x++] = line[i];
                                empty     = false;
                        }

                        esc = (line[i] == '\\') ? !esc : false;
                }

        next_line:
                esc = false;
                if (!empty) {
                        repl[x++] = '\n';
                        repl[x]   = '\0';
                        b_list_append(&list, b_steal(repl, x));
                } else {
                        xfree(repl);
                }
        }

        xfree(bak);
        xfree(*vim_bufp);
        *vim_bufp = b_list_join(list, NULL);

        FILE *fp = safe_fopen_fmt("%s/.tag_highlight_log/strip.log", "wb", HOME);
        b_fputs(fp, *vim_bufp);
        fclose(fp);

        b_list_destroy(list);
}


/*============================================================================*/
/* Python */


#define QUOTE() (Single.Q || Double.Q || in_docstring)

#define check_docstring(AA, BB)                                      \
    do {                                                             \
            if (in_docstring) {                                      \
                    if ((AA).cnt == 3)                               \
                            --(AA).cnt;                              \
                    else if (*(pos - 1) == (AA).ch)                  \
                            --(AA).cnt;                              \
                    else                                             \
                            (AA).cnt = 3;                            \
                                                                     \
                    in_docstring = ((AA).cnt != 0) ? (AA).val        \
                                                   : NO_DOCSTRING;   \
                    if (!in_docstring)                               \
                            skip = true;                             \
            } else {                                                 \
                    if ((AA).cnt == 0 && !((AA).Q || (BB).Q))        \
                            ++(AA).cnt;                              \
                    else if (*(pos - 1) == (AA).ch)                  \
                            ++(AA).cnt;                              \
                                                                     \
                    in_docstring = ((AA).cnt == 3) ? (AA).val        \
                                                   : NO_DOCSTRING;   \
                                                                     \
                    if (in_docstring) {                              \
                            (AA).Q = (BB).Q = false;                 \
                            skip = true;                             \
                    } else if (!(BB).Q && !comment) {                \
                            if ((AA).Q) {                            \
                                    if (!escape)                     \
                                            (AA).Q = false,          \
                                            skip = true;             \
                            } else                                   \
                                    (AA).Q = true;                   \
                    }                                                \
            }                                                        \
    } while (0)


enum docstring_e {
        NO_DOCSTRING,
        SINGLE_DOCSTRING,
        DOUBLE_DOCSTRING
};

struct py_quote {
        bool Q;
        int cnt;
        char ch;
        enum docstring_e val;
};


static void
handle_python(bstring *vim_buf)
{
        enum docstring_e in_docstring = NO_DOCSTRING;
        struct py_quote  Single = { false, 0, '\'', SINGLE_DOCSTRING };
        struct py_quote  Double = { false, 0, '"',  DOUBLE_DOCSTRING };
        const uchar     *pos    = vim_buf->data;
        unsigned short   space  = 0;

        uchar *buf, *buf_orig;
        bool escape, comment, skip;

        buf    = buf_orig = xmalloc(vim_buf->slen + 2LLU);
        escape = comment  = skip = false;

        if (*pos == '\0')
                errx(1, "Empty vim buffer!");

        /* Add a non-offensive character to the buffer so we never have to worry
         * about going out of bounds when checking 1 character backwards. */
        *buf++ = ' ';

        do {
                if (!comment && !QUOTE() && !escape && *pos == '#') {
                        comment = true;
                        space   = 0;
                        continue;
                }

                if (comment && *pos != '\n')
                        continue;

                switch (*pos) {
                case '\n':
                        if (*(buf - 1) == '\n')
                                skip = true;
                        comment = false;
                        space = 0;
                        break;

                case '"':
                        if (in_docstring != SINGLE_DOCSTRING)
                                check_docstring(Double, Single);
                        space = 0;
                        break;

                case '\'':
                        if (in_docstring != DOUBLE_DOCSTRING)
                                check_docstring(Single, Double);
                        space = 0;
                        break;

                case '\t':
                case ' ':
                        if (*(buf - 1) == '\n')
                                skip = true;
                        else
                                ++space;
                        break;

                default:
                        space = 0;
                }

                /* If less than 3 of any type of quote appear in a row, reset
                 * the corresponding counter to 0. */
                switch (in_docstring) {
                case SINGLE_DOCSTRING:
                        if (Single.cnt < 3 && *pos != Single.ch)
                                Single.cnt = 3;
                        Double.cnt = 0;
                        break;

                case DOUBLE_DOCSTRING:
                        if (Double.cnt < 3 && *pos != Double.ch)
                                Double.cnt = 3;
                        Single.cnt = 0;
                        break;

                case NO_DOCSTRING:
                        if (Single.cnt > 0 && *pos != Single.ch)
                                Single.cnt = 0;
                        if (Double.cnt > 0 && *pos != Double.ch)
                                Double.cnt = 0;
                        break;

                default: /* not reachable */ abort();
                }

                if (skip)
                        skip = false;
                else if (!QUOTE() && space < 2)
                        *buf++ = *pos;

                escape = (*pos == '\\' ? !escape : false);

        } while (*pos++);

        *buf = '\0';

        xfree(vim_buf->data);
        vim_buf->data = buf_orig;
        vim_buf->slen = vim_buf->mlen = buf - buf_orig - 1;
}
