#include "util.h"

#include "data.h"
#include "highlight.h"

#ifdef DOSISH
#  include <malloc.h>
#  define __CONST__
#  define SEPCHAR ';'
#else
#  include <alloca.h>
#  define __CONST__ const
#  define SEPCHAR ':'
#endif

#define REQUIRED_INPUT 9
#define free_list(LST)                               \
        do {                                         \
                for (int i = 0; i < (LST)->num; ++i) \
                        free((LST)->lst[i]);         \
        } while (0)

#define b_iseql(BSTR, LIT)          (b_iseq((BSTR), b_tmp(LIT)))
#define b_iseql_caseless(BSTR, LIT) (b_iseq_caseless((BSTR), b_tmp(LIT)))

static bool is_c_or_cpp;

static void print_tags(const struct taglist *list, const char *ft);
static void print_tags_vim(const struct taglist *list, const char *ft);

static struct taglist * tok_search(const struct bufdata *bdata, b_list *vimbuf);
static void *do_tok_search(void *vdata);

static FILE *thislog;


struct taglist *
findemtagers(struct bufdata *bdata, b_list *toks)
{
        thislog     = safe_fopen_fmt("%s/rejectlog.log", "wb", HOME);
        is_c_or_cpp = (bdata->ft->id == FT_C || bdata->ft->id == FT_CPP);

        struct taglist *list = tok_search(bdata, toks);
        if (!list)
                return NULL;

        /* if (bdata->ft->id == FT_VIM) */
                /* print_tags_vim(list, BTS(bdata->ft->vim_name)); */
        /* else */
                /* print_tags(list, BTS(bdata->ft->vim_name)); */

        fclose(thislog);
        return list;

}


static inline void
add_tag_to_list(struct taglist **list, struct tag *tag)
{
        if ((*list)->qty == ((*list)->mlen - 1))
                (*list)->lst = nrealloc((*list)->lst, ((*list)->mlen <<= 1),
                                        sizeof(*(*list)->lst));
        (*list)->lst[(*list)->qty++] = tag;
}


/* ========================================================================== */


static int
tag_cmp(const void *vA, const void *vB)
{
        int ret;
        const struct tag *sA = *(struct tag **)(vA);
        const struct tag *sB = *(struct tag **)(vB);

        if (sA->kind == sB->kind) {
                if (sA->b->slen == sB->b->slen)
                        ret = memcmp(sA->b->data, sB->b->data, sA->b->slen);
                else
                        ret = sA->b->slen - sB->b->slen;
                /* ret = b_strcmp(sA->b, sB->b); */
                /* ret = strcmp(BS(sA->b), BS(sB->b)); */
                /* ret = memcmp(BS(sA->b), BS(sB->b), MIN(sA->b->slen, sB->b->slen)); */
        } else
                ret = sA->kind - sB->kind;

        return ret;
}


static int
b_strcmp_wrap(const void *vA, const void *vB)
{
        return b_strcmp((*(bstring **)(vA)),
                        (*(bstring **)(vB)));
        /* return strcmp(BS((*(bstring **)(vA))),
                      BS((*(bstring **)(vB)))); */
}


/* ========================================================================== */


#define DATA (list->lst)
#define PRINT(IT) (fprintf(fp, "%s#%c\t%s\n", ft, DATA[IT]->kind, BS(DATA[IT]->b)))

static void
print_tags(const struct taglist *list, const char *ft)
{
        FILE *fp = safe_fopen_fmt("%s/final_output.log", "a", HOME);

        /* Always print the first tag. */
        PRINT(0);

        for (unsigned i = 1; i < list->qty; ++i)
                if (!b_iseq(DATA[i]->b, DATA[i-1]->b))
                        PRINT(i);

        fputs("\n\n\n", fp);
        fclose(fp);
}

#undef PRINT


static void
print_tags_vim(const struct taglist *list, const char *ft)
{
        FILE *fp = safe_fopen_fmt("%s/final_output.log", "a", HOME);
        char *tmp;

        /* Always print the first tag. */
        if (DATA[0]->kind == 'f' && (tmp = strchr(BS(DATA[0]->b), ':')))
                fprintf(fp, "%s#%c\t%s\n", ft, DATA[0]->kind, tmp + 1);
        else
                fprintf(fp, "%s#%c\t%s\n", ft, DATA[0]->kind, BS(DATA[0]->b));

        for (unsigned i = 1; i < list->qty; ++i)
                if (!b_iseq(DATA[i]->b, DATA[i - 1]->b)) {
                        if (DATA[i]->kind == 'f' && (tmp = strchr(BS(DATA[i]->b), ':')))
                                fprintf(fp, "%s#%c\t%s\n", ft, DATA[i]->kind, tmp + 1);
                        else
                                fprintf(fp, "%s#%c\t%s\n", ft, DATA[i]->kind, BS(DATA[i]->b));
                }
}

#undef DATA


/* ========================================================================== */


static bool
in_order(const b_list *equiv, const bstring *order, char *kind)
{
        /* `kind' is actually a pointer to a char, not a C bstring. */
        if (equiv)
                for (unsigned i = 0; i < equiv->qty && equiv->lst[i]; ++i)
                        if (*kind == equiv->lst[i]->data[0]) {
                                *kind = equiv->lst[i]->data[1];
                                break;
                        }

        return strchr(BS(order), *kind) != NULL;
}


static bool
is_correct_lang(const bstring *lang, __CONST__ bstring *match_lang)
{
#ifdef DOSISH
        if (match_lang->data[match_lang->len - 1] == '\r')
                match_lang->data[match_lang->len - 1] = '\0';
#endif
        if (b_iseq_caseless(match_lang, lang))
                return true;

        return (is_c_or_cpp && (b_iseql_caseless(match_lang, "C") ||
                                b_iseql_caseless(match_lang, "C++")));
}


static bool
skip_tag(const b_list *skip, const bstring *find)
{
        if (skip && skip->lst && skip->qty)
                for (unsigned i = 0; i < skip->qty; ++i)
                        if (b_iseq(skip->lst[i], find))
                                return true;

        return false;
}


/*============================================================================*/


struct pdata {
        uint8_t thnum;
        const b_list *vim_buf;
        const b_list *skip;
        const b_list *equiv;
        const bstring *lang;
        const bstring *order;
        const bstring *filename;
        bstring **lst;
        unsigned num;
};

/* FILE *logfile; */


static struct taglist *
tok_search(const struct bufdata *bdata, b_list *vimbuf)
{
        struct top_dir *topdir = bdata->topdir;
        b_list         *tags   = topdir->tags;

        if (tags->qty == 0)
                errx(1, "No tags found!");

        int num_threads = find_num_cpus();
        if (num_threads <= 0)
                num_threads = 4;

        pthread_t       *tid = nmalloc(num_threads, sizeof(*tid));
        struct taglist **out = nmalloc(num_threads, sizeof(*out));

        warnx("Sorting through %d tags with %d cpus.", tags->qty, num_threads);

        /* Because we may have examined multiple tags files, it's very possible
         * for there to be duplicate tags. Sort the list and remove any. */
        qsort(vimbuf->lst, vimbuf->qty, sizeof(*vimbuf->lst), &b_strcmp_wrap);

        b_list *uniq = b_list_create_alloc(vimbuf->qty);
        uniq->qty = 0;
        uniq->lst[uniq->qty++] = vimbuf->lst[0];

        for (unsigned i = 1; i < vimbuf->qty; ++i)
                if (!b_iseq(vimbuf->lst[i], vimbuf->lst[i-1]))
                        uniq->lst[uniq->qty++] = vimbuf->lst[i];


        /* Launch the actual search in separate threads, with each handling as
         * close to an equal number of tags as the math allows. */
        for (int i = 0; i < num_threads; ++i) {
                struct pdata *tmp = xmalloc(sizeof(*tmp));
                unsigned quot = tags->qty / num_threads;
                unsigned num  = (i == num_threads - 1)
                                   ? (tags->qty - ((num_threads - 1) * quot))
                                   : quot;

                *tmp = (struct pdata){i,
                                      uniq,
                                      bdata->ft->ignored_tags,
                                      bdata->ft->equiv,
                                     &bdata->ft->ctags_name,
                                      bdata->ft->order,
                                      bdata->filename,
                                     &tags->lst[i * quot],
                                      num};

                if (pthread_create(tid + i, NULL, &do_tok_search, tmp) != 0)
                        err(1, "pthread_create failed");
        }

        /* Collect the threads. */
        for (int i = 0; i < num_threads; ++i)
                pthread_join(tid[i], (void **)(&out[i]));

        free_all(uniq->lst, uniq);
        unsigned total = 0, offset = 0;

        for (int T = 0; T < num_threads; ++T)
                total += out[T]->qty;
        if (total == 0) {
                warnx("No tags found in buffer.");
                free_all(tid, out);
                return NULL;
        }

        /* Combine the returned data from all threads into one array, which is
         * then sorted and returned. */
        struct tag    **alldata = nmalloc(total, sizeof(*alldata));
        struct taglist *ret     = xmalloc(sizeof(*ret));
        *ret = (struct taglist){ alldata, total, total };

        for (int T = 0; T < num_threads; ++T) {
                if (out[T]->qty > 0) {
                        memcpy(alldata + offset, out[T]->lst,
                               out[T]->qty * sizeof(*out));
                        offset += out[T]->qty;
                }
                free_all(out[T]->lst, out[T]);
        }

        qsort(alldata, total, sizeof(*alldata), &tag_cmp);

        free_all(tid, out);
        return ret;
}

#define INIT_MAX ((data->num / 2) * 3)
/* #define cur_str  (data->lst[i]->data) */
#define cur_str  (cpy->data)
#define STRSEP(BSTR, SEP) ((uchar *)(strsep((char **)(&(BSTR)), (SEP))))
#define STRCHR(BSTR, CH)  ((uchar *)(strchr((char *)(BSTR), (CH))))

static void *
do_tok_search(void *vdata)
{
        struct pdata   *data = vdata;
        struct taglist *ret  = xmalloc(sizeof(*ret));
        *ret = (struct taglist){
                .lst = nmalloc(INIT_MAX, sizeof(*ret->lst)),
                .qty = 0, .mlen = INIT_MAX
        };

        for (unsigned i = 0; i < data->num; ++i) {
                /* Skip empty lines and comments. */
                if (!data->lst[i] || !data->lst[i]->data || !data->lst[i]->data[0] ) {
                        /* b_fputs(logfile, b_tmp("Skipping a NULL tag.\n")); */
                        continue;
                }
                if (data->lst[i]->data[0] == '!')
                        continue;

                bstring *cpy      = b_strcpy(data->lst[i]);
                uchar   *cpy_data = cpy->data;

                bstring *name       = (bstring[]){{ 0, 0, NULL, 0u }};
                bstring *match_file = (bstring[]){{ 0, 0, NULL, 0u }};

                /* The name is first, followed by two fields we don't need. */
                name->data       = STRSEP(cur_str, "\t");
                name->slen       = cur_str - name->data - 1;
                match_file->data = STRSEP(cur_str, "\t");
                match_file->slen = cur_str - match_file->data - 1;
                cur_str          = STRCHR(cur_str, '\t');

                char *tok, kind = '\0';
                bstring *match_lang = NULL;

                /* Extract the 'kind' and 'language' fields. The former is the
                 * only one that is 1 character long, and the latter is prefaced. */
                while ((tok = strsep((char**)(&cur_str), "\t"))) {
                        if (tok[0] && !tok[1])
                                kind = *tok;
                        else if (strncmp(tok, "language:", 9) == 0)
                                match_lang = (cur_str)
                                           ? bt_fromblk(tok+9, (cur_str - (uchar*)(tok+9)) - 1)
                                           : bt_fromcstr(tok+9);
                }

                if (!match_lang || !kind || !match_lang->data) {
                        /* b_fputs(logfile, b_tmp("Couldn't find lang/kind for tag "), name, b_tmp(".\n")); */
                        free(cpy_data);
                        free(cpy);
                        continue;
                }

#if 0
                fprintf(thislog,
                        "thread %2u:\tname\t\t%p\tslen: %2d\tmlen: %3d\tflg: 0x%02X\t%s\n"
                        "thread %2u:\tmatch_file\t%p\tslen: %2d\tmlen: %3d\tflg: 0x%02X\t%s\n"
                        "thread %2u:\tmatch_lang\t%p\tslen: %2d\tmlen: %3d\tflg: 0x%02X\t%s\n\n",
                        data->thnum, (void*)name, name->slen, name->mlen, name->flags, BS(name),
                        data->thnum, (void*)match_file, match_file->slen, match_file->mlen, match_file->flags, BS(match_file),
                        data->thnum, (void*)match_lang, match_lang->slen, match_lang->mlen, match_lang->flags, BS(match_lang));
#endif

                /*
                 * Prune tags. Include only those that are:
                 *    1) of a type in the `order' list,
                 *    2) of the correct language,
                 *    3) are not included in the `skip' list, and
                 *    4) are present in the current vim buffer.
                 * If invalid, just move on. 
                 */
#if 0
                if ( in_order(data->equiv, data->order, &kind) &&
                     is_correct_lang(data->lang, match_lang) &&
                    !skip_tag(data->skip, name) &&
                     (b_iseq(data->filename, match_file) ||
                      bsearch(&name, data->vim_buf->lst, data->vim_buf->qty,
                              sizeof(*data->vim_buf->lst), &b_strcmp_wrap)))
                {
                        bstring    *tmp = b_fromblk(name->data, name->slen);
                        struct tag *tag = xmalloc(sizeof(*tag));
                        *tag            = (struct tag){.b = tmp, .kind = kind};
                        add_tag_to_list(&ret, tag);
                }
#endif
#define REJECT_TAG(REASON) (fprintf(thislog, "Rejecting tag %c - %-20s - %-40s - (%d)-\t%s.\n", \
                                    kind, BS(match_lang), BS(name), name->slen, (REASON)))

                if (!in_order(data->equiv, data->order, &kind))
                        REJECT_TAG("not in order");
                else if (!is_correct_lang(data->lang, match_lang))
                        REJECT_TAG("wrong language");
                else if (skip_tag(data->skip, name))
                        REJECT_TAG("in skip list");
                /* else if (!(b_iseq(data->filename, match_file) || */
                           /* bsearch(&name, data->vim_buf->lst, data->vim_buf->qty, */
                                   /* sizeof(*data->vim_buf->lst), &b_strcmp_wrap))) */
                        /* REJECT_TAG("not in buffer"); */
                else if (!(b_iseq(data->filename, match_file))) {
                        REJECT_TAG("not in specified file");
                        if (bsearch(&name, data->vim_buf->lst, data->vim_buf->qty,
                                     sizeof(bstring *), &b_strcmp_wrap))
                                goto lazy;
                        /* for (unsigned i = 0; i < data->vim_buf->qty; ++i) {
                                fprintf(thislog, "Comparing \"%s\" with \"%s\".\n", BS(name), BS(data->vim_buf->lst[i]));
                                if (b_iseq(name, data->vim_buf->lst[i]))
                                        goto lazy;
                        } */
                        REJECT_TAG("also not in buffer");
                } else {
lazy:
                        fprintf(thislog, "Accepting tag %c - %-20s - %-40s\n",
                                kind, BS(match_lang), BS(name));
                        bstring    *tmp = b_fromblk(name->data, name->slen);
                        struct tag *tag = xmalloc(sizeof(*tag));
                        *tag            = (struct tag){.b = tmp, .kind = kind};
                        add_tag_to_list(&ret, tag);
                }

                free(cpy_data);
                free(cpy);
        }

        free(vdata);

#ifndef DOSISH
        pthread_exit(ret);
#else
        return ret;
#endif
}
