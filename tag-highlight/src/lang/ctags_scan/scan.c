#include "scan.h"
#include <stdlib.h>

#if defined(DOSISH) || defined(__MINGW32__) || defined(__MINGW64__)
#  include <malloc.h>
#  define CONST__
#  define SEPCHAR ';'
#else
#  define CONST__ const
#  define SEPCHAR ':'
#endif

#define b_iseql_caseless(BSTR, LIT) (b_iseq_caseless((BSTR), b_tmp(LIT)))

#define CTX tok_scan_talloc_ctx_
void *tok_scan_talloc_ctx_ = NULL;

/*============================================================================*/

static unsigned num_threads = 0;

__attribute__((constructor(10000)))
static void init(void)
{
        num_threads = find_num_cpus();
        if (num_threads == 0)
                num_threads = 4;
}


static void *do_tok_search(void *vdata);
static int   tag_cmp(void const *vA, const void *vB);
static void  remove_duplicate_tags(struct taglist **taglist_p);

static inline void add_tag_to_list(struct taglist **listp, struct tag *tag);
static inline bool in_order(b_list const *equiv, const bstring *order, uchar *kind);
static inline bool is_correct_lang(bstring const *lang, bstring CONST__ *match_lang, bool is_c_or_cpp);
static inline bool skip_tag(b_list const *skip, const bstring *find);


/*-==========================================================================-*/


static void
add_tag_to_list(struct taglist **listp, struct tag *tag)
{
        if (!listp || !*listp || !(*listp)->lst || !tag)
                return;
        struct taglist *const list = *listp;

        if (list->qty >= (list->mlen - 1))
                list->lst = talloc_realloc(list, list->lst, struct tag *, (list->mlen <<= 1));
                                           
        list->lst[list->qty++] = talloc_steal(list->lst, tag);
}


static int
tag_cmp(void const *vA, const void *vB)
{
        int ret;
        struct tag const *sA = *(struct tag *const *)(vA);
        struct tag const *sB = *(struct tag *const *)(vB);

        if (sA->kind == sB->kind) {
                if (sA->b->slen == sB->b->slen)
                        ret = memcmp(sA->b->data, sB->b->data, sA->b->slen);
                else
                        ret = (int)(sA->b->slen - sB->b->slen);
        } else {
                ret = sA->kind - sB->kind;
        }

        return ret;
}


static void
remove_duplicate_tags(struct taglist **taglist_p)
{
        struct taglist *list = *taglist_p;
        struct taglist *repl = talloc(CTX, struct taglist);
        struct tag     *last;

        last  = (struct tag[]){{.b = B(""), .kind = 0}};
        *repl = (struct taglist){
                .lst = talloc_array(repl, struct tag *, (list->qty) ? list->qty : 1),
                .qty = 0,
                .mlen = list->qty
        };

#define TAG (list->lst[i])
        for (unsigned i = 0; i < list->qty; ++i) {
                if (TAG->kind != last->kind || !b_iseq(TAG->b, last->b))
                        last = repl->lst[repl->qty++] = talloc_move(repl->lst, &TAG);
        }
#undef TAG

        talloc_free(*taglist_p);
        *taglist_p = repl;
}


/* ========================================================================== */


static inline bool
in_order(b_list const *equiv, const bstring *order, uchar *kind)
{
        /* `kind' is actually a pointer to a char, not a C bstring. */
        if (equiv) {
                for (unsigned i = 0; i < equiv->qty && equiv->lst[i]; ++i) {
                        if (*kind == equiv->lst[i]->data[0]) {
                                *kind = (char)equiv->lst[i]->data[1];
                                break;
                        }
                }
        }

        return strchr(BS(order), *kind) != NULL;
}


static inline bool
is_correct_lang(bstring const *lang, CONST__ bstring *match_lang, bool const is_c_or_cpp)
{
#ifdef DOSISH
        if (match_lang->data[match_lang->slen - 1] == '\r')
                match_lang->data[--match_lang->slen] = '\0';
#endif
        if (b_iseq_caseless(match_lang, lang))
                return true;

        return (is_c_or_cpp && (b_iseql_caseless(match_lang, "C") ||
                                b_iseql_caseless(match_lang, "C++")));
}


static inline bool
skip_tag(b_list const *skip, const bstring *find)
{
        if (skip && skip->lst && skip->qty)
                for (unsigned i = 0; i < skip->qty; ++i)
                        if (b_iseq(skip->lst[i], find))
                                return true;
        return false;
}


/*============================================================================*/


#if (defined __has_attribute && __has_attribute(__designated_init__)) || \
    (defined __GNUC__ && !defined __clang__)
#  define aDESINIT_ __attribute__((__designated_init__))
#else
#  define aDESINIT_
#endif

struct aDESINIT_ pdata {
        b_list  const  *vim_buf;
        b_list  const  *skip;
        b_list  const  *equiv;
        bstring const  *lang;
        bstring const  *order;
        bstring const  *filename;
        bstring       **lst;
        unsigned        num;
        bool            c_family;
} /*__attribute__((aligned(64)))*/;

struct aDESINIT_ tag_vector {
      struct taglist **data;
      unsigned         size;
};


static inline b_list            *tok_search_launch_threads(Buffer const *bdata, b_list *vimbuf, pthread_t *tid, size_t nthreads);
static inline struct tag_vector *tok_search_join_threads(b_list *uniq, pthread_t const *tid, size_t nthreads);
static inline struct taglist    *tok_search_combine_data(struct tag_vector *vec, size_t nthreads);


struct taglist *
process_tags(Buffer const *bdata, b_list *vimbuf)
{
      if (!bdata)
            errx(1, "bdata is null\n");
      if (!bdata->topdir)
            errx(1, "bdata->topdir is NULL\n");
      if (!vimbuf)
            errx(1, "vimbuf is NULL\n");
      if (vimbuf->qty == 0)
            return NULL;
      if (!bdata->topdir->tags || bdata->topdir->tags->qty == 0) {
            warnx("No tags found in ctags file.");
            return NULL;
      }

      warnd("Sorting through %d tags with %d cpus.", bdata->topdir->tags->qty, num_threads);

      pthread_t         *tid  = calloc((size_t)num_threads, sizeof(pthread_t));
      b_list            *uniq = tok_search_launch_threads(bdata, vimbuf, tid, num_threads);
      struct tag_vector *out  = tok_search_join_threads(uniq, tid, num_threads);
      struct taglist    *ret  = out ? tok_search_combine_data(out, num_threads) : NULL;

      free(tid);
      return ret;
}


static inline b_list *
tok_search_launch_threads(Buffer const *bdata, b_list *vimbuf, pthread_t *tid, size_t const nthreads)
{
      bool const is_c_or_cpp = (bdata->ft->id == FT_C || bdata->ft->id == FT_CXX);
      b_list *tags = bdata->topdir->tags;

      /* Because we may have examined multiple tags files, it's very possible
       * for there to be duplicate tags. Sort the list and remove any. */
      qsort(vimbuf->lst, vimbuf->qty, sizeof(bstring *), &b_strcmp_fast_wrap);

      b_list *uniq = b_list_create_alloc(vimbuf->qty);
      uniq->lst[0] = talloc_move(uniq->lst, &vimbuf->lst[0]);
      uniq->qty    = 1;

      for (unsigned i = 1; i < vimbuf->qty; ++i)
            if (!b_iseq(vimbuf->lst[i], vimbuf->lst[i-1]))
                    uniq->lst[uniq->qty++] = talloc_move(uniq->lst, &vimbuf->lst[i]);

      /* Launch the actual search in separate threads, with each handling as
       * close to an equal number of tags as the math allows. */
      for (unsigned i = 0; i < nthreads; ++i) {
            struct pdata  *tmp  = malloc(sizeof(struct pdata));
            assert(tmp);
            unsigned const quot = tags->qty / nthreads;
            unsigned const num  = (i == nthreads - 1)
                                     ? (tags->qty - ((nthreads - 1) * quot))
                                     : quot;

            *tmp = (struct pdata){.vim_buf  =  uniq,
                                  .skip     =  bdata->ft->ignored_tags,
                                  .equiv    =  bdata->ft->equiv,
                                  .lang     = &bdata->ft->ctags_name,
                                  .order    =  bdata->ft->order,
                                  .filename =  bdata->name.full,
                                  .lst      = &tags->lst[(int)(i * quot)],
                                  .num      =  num,
                                  .c_family = is_c_or_cpp};

            if (pthread_create(tid + i, NULL, &do_tok_search, tmp) != 0)
                  err(1, "pthread_create failed");
      }

      return uniq;
}


static inline struct tag_vector *
tok_search_join_threads(b_list *uniq, pthread_t const *tid, size_t const nthreads)
{
        struct tag_vector *ret = talloc(CTX, struct tag_vector);

        ret->data = talloc_array(ret, struct taglist *, nthreads);
        ret->size = 0;

        /* Collect the threads. */
        for (unsigned i = 0; i < nthreads; ++i) {
              pthread_join(tid[i], (void **)(&ret->data[i]));
              talloc_steal(ret, ret->data[i]);
        }

        talloc_free(uniq);

        for (unsigned T = 0; T < nthreads; ++T)
                if (ret->data[T])
                        ret->size += ret->data[T]->qty;
        if (ret->size == 0) {
                warnx("No tags found in this buffer.");
                talloc_free(ret->data);
                return NULL;
        }

        return ret;
}


static inline struct taglist *
tok_search_combine_data(struct tag_vector *vec, size_t const nthreads)
{
        /* Combine the returned data from all threads into one array, which is
         * then sorted and returned. */
        size_t          offset  = 0;
        struct taglist *ret     = talloc(CTX, struct taglist);
        struct tag    **alldata = talloc_array(ret, struct tag *, vec->size);
        *ret = (struct taglist){ alldata, vec->size, vec->size };

        for (unsigned T = 0; T < nthreads; ++T) {
                if (vec->data[T]) {
                        for (uint i = 0; i < vec->data[T]->qty; ++i)
                                alldata[offset++] = talloc_move(alldata, &vec->data[T]->lst[i]);

                        TALLOC_FREE(vec->data[T]->lst);
                        TALLOC_FREE(vec->data[T]);
                }
        }

        qsort(alldata, vec->size, sizeof(struct tag *), &tag_cmp);
        remove_duplicate_tags(&ret);

        TALLOC_FREE(vec->data);
        talloc_free(vec);
        return ret;
}


/*============================================================================*/


#if 0
static struct taglist *
tok_search(Buffer const *bdata, b_list *vimbuf)
{
        if (!bdata)
                errx(1, "bdata is null\n");
        if (!bdata->topdir)
                errx(1, "bdata->topdir is NULL\n");
        if (!vimbuf)
                errx(1, "vimbuf is NULL\n");
        if (vimbuf->qty == 0)
                return NULL;

        if (!bdata->topdir->tags || bdata->topdir->tags->qty == 0) {
                warnx("No tags found.");
                return NULL;
        }

        struct top_dir  *topdir = bdata->topdir;
        b_list          *tags   = topdir->tags;
        pthread_t       *tid    = alloca(sizeof(pthread_t) * (size_t)num_threads);

        ECHO("Sorting through %d tags with %d cpus.", tags->qty, num_threads);

        /* Because we may have examined multiple tags files, it's very possible
         * for there to be duplicate tags. Sort the list and remove any. */
        qsort(vimbuf->lst, vimbuf->qty, sizeof(bstring *), &b_strcmp_fast_wrap);

        b_list *uniq = b_list_create_alloc(vimbuf->qty);
        uniq->qty = 0;
        uniq->lst[uniq->qty++] = talloc_move(uniq->lst, &vimbuf->lst[0]);

        for (unsigned i = 1; i < vimbuf->qty; ++i)
                if (!b_iseq(vimbuf->lst[i], vimbuf->lst[i-1]))
                        uniq->lst[uniq->qty++] = talloc_move(uniq->lst, &vimbuf->lst[i]);


        /* Launch the actual search in separate threads, with each handling as
         * close to an equal number of tags as the math allows. */
        for (unsigned i = 0; i < num_threads; ++i) {
                struct pdata  *tmp  = malloc(sizeof(struct pdata));
                assert(tmp);
                unsigned const quot = tags->qty / num_threads;
                unsigned const num  = (i == num_threads - 1)
                                         ? (tags->qty - ((num_threads - 1) * quot))
                                         : quot;

                *tmp = (struct pdata){.vim_buf  =  uniq,
                                      .skip     =  bdata->ft->ignored_tags,
                                      .equiv    =  bdata->ft->equiv,
                                      .lang     = &bdata->ft->ctags_name,
                                      .order    =  bdata->ft->order,
                                      .filename =  bdata->name.full,
                                      .lst      = &tags->lst[(int)(i * quot)],
                                      .num      =  num};

                if (pthread_create(tid + i, NULL, &do_tok_search, tmp) != 0)
                        err(1, "pthread_create failed");
        }

        struct taglist **out = talloc_array(CTX, struct taglist *, num_threads);

        /* Collect the threads. */
        for (unsigned i = 0; i < num_threads; ++i) {
                pthread_join(tid[i], (void **)(&out[i]));
        }

        /* talloc_free(tid); */
        talloc_free(uniq);
        unsigned total = 0, offset = 0;

        for (unsigned T = 0; T < num_threads; ++T)
                if (out[T])
                        total += out[T]->qty;
        if (total == 0) {
                warnx("No tags found in buffer.");
                talloc_free(out);
                return NULL;
        }

        /* Combine the returned data from all threads into one array, which is
         * then sorted and returned. */
        struct taglist *ret     = talloc(CTX, struct taglist);
        struct tag    **alldata = talloc_array(ret, struct tag *, total);
        *ret = (struct taglist){ alldata, total, total };

        for (unsigned T = 0; T < num_threads; ++T) {
                if (out[T]) {
                        for (uint i = 0; i < out[T]->qty; ++i)
                                alldata[offset++] = talloc_move(alldata, &out[T]->lst[i]);

                        talloc_free(out[T]->lst);
                        talloc_free(out[T]);
                }
        }

        qsort(alldata, total, sizeof(struct tag *), &tag_cmp);
        remove_duplicate_tags(&ret);

        talloc_free(out);
        return ret;
}
#endif


/*============================================================================*/


#define INIT_VAL  ((data->num * 2) / 3)
#define INIT_MAX  ((INIT_VAL >= 32) ? INIT_VAL : 32)
#define SIZE_LANG (sizeof("language:") - 1)

static void *
do_tok_search(void *vdata)
{
      struct pdata *data = vdata;
      if (data->num == 0) {
            free(data);
            pthread_exit(NULL);
      }

      struct taglist *ret = talloc(CTX, struct taglist);
      *ret = (struct taglist){talloc_array(ret, struct tag *, INIT_MAX), 0, INIT_MAX};

      for (unsigned i = 0; i < data->num; ++i) {
            /* Skip empty lines and comments. */
            if (!data->lst[i] || !data->lst[i]->data || data->lst[i]->slen < 1)
                  continue;
            if (data->lst[i]->data[0] == '!')
                  continue;

            bstring  name[]       = {BSTR_STATIC_INIT};
            bstring  tok[]        = {BSTR_STATIC_INIT};
            bstring  match_file[] = {BSTR_STATIC_INIT};
            bstring  match_lang[] = {BSTR_STATIC_INIT};
            bstring *namep        = name;
            bstring *cpy          = b_strcpy(data->lst[i]);
            uchar   *cpy_data     = cpy->data;
            uchar    kind         = '\0';

            /* The name is first, followed by two fields we don't need. */
            b_memsep(name, cpy, '\t');
            b_memsep(match_file, cpy, '\t');
            int64_t const pos = b_strchr(cpy, '\t');
            cpy->data += pos;
            cpy->slen -= pos;

            /* Extract the 'kind' and 'language' fields. The former is the
             * only one that is 1 character long, and the latter is prefaced. */
            while (b_memsep(tok, cpy, '\t')) {
                  if (tok->slen == 1) {
                        kind = tok->data[0];
                  } else if (strncmp(BTS(*tok), "language:", SIZE_LANG) == 0) {
                        match_lang[0].data = tok[0].data + SIZE_LANG;
                        match_lang[0].slen = tok[0].slen - SIZE_LANG;
                  }
            }

            /*
             * Prune tags. Include them only if we actually identified the
             * kind and match_lang, and only tags that are:
             *    1) of a type in the `order' list,
             *    2) of the correct language,
             *    3) are not included in the `skip' list, and
             *    4) are present in the current vim buffer.
             * If invalid, just move on.
             */
#if 1
                if ( ( kind && match_lang[0].data )                          &&
                     in_order(data->equiv, data->order, &kind)               &&
                     is_correct_lang(data->lang, match_lang, data->c_family) &&
                    !skip_tag(data->skip, name)                              &&
                     ( b_iseq(data->filename, match_file) ||
                       bsearch(&namep, data->vim_buf->lst, data->vim_buf->qty,
                               sizeof(bstring *), &b_strcmp_fast_wrap) )
                   )
                {
                        bstring    *tmp = b_fromblk(name->data, name->slen);
                        struct tag *tag = talloc(CTX, struct tag);
                        *tag            = (struct tag){.b = talloc_steal(tag, tmp), .kind = kind};
                        add_tag_to_list(&ret, tag);
                }

#else
#  define SUPER_DUMB(cond, FMT, ...)                                                         \
      do {                                                                         \
            if (!(cond)) {                                                         \
                  eprintf("Condition failed for `%*s`: (%s):    " FMT "\n", BSC(name), #cond, ##__VA_ARGS__); \
                  goto skip;                                                       \
            }                                                                      \
      } while (0)

            SUPER_DUMB(kind, "%d", kind);
            SUPER_DUMB(match_lang[0].data, "%p", match_lang[0].data);
            SUPER_DUMB(in_order(data->equiv, data->order, &kind), "order='%*s', kind='%c'", BSC(data->order), kind);
            SUPER_DUMB(is_correct_lang(data->lang, match_lang, data->c_family), "%*s vs %*s", BSC(data->lang), BSC(match_lang));
            SUPER_DUMB(!skip_tag(data->skip, name), "");
            SUPER_DUMB(b_iseq(data->filename, match_file) ||
                       bsearch(&namep, data->vim_buf->lst, data->vim_buf->qty,
                               sizeof(bstring *), &b_strcmp_fast_wrap), "");

            {
                  bstring    *tmp = b_fromblk(name->data, name->slen);
                  struct tag *tag = talloc(CTX, struct tag);
                  *tag = (struct tag){.b = talloc_steal(tag, tmp), .kind = kind};
                  add_tag_to_list(&ret, tag);
            }

#  undef SUPER_DUMB
      skip:
#endif

            cpy->data = NULL;
            talloc_steal(CTX, cpy_data);
            talloc_free(cpy_data);
            talloc_free(cpy);
      }

      free(data);
      pthread_exit(ret);
}
