/*
 * Do I need a copyright here?
 */

#include "private.h"

/*============================================================================*/
/* SOME CRAPPY ADDITIONS! */
/*============================================================================*/


void
__b_fputs(FILE *fp, bstring *bstr, ...)
{
        va_list va;
        va_start(va, bstr);
        for (;;bstr = va_arg(va, bstring *)) {
                if (bstr) {
                        if (bstr->flags & BSTR_LIST_END)
                                break;
                        if (bstr->data && bstr->slen > 0)
                                fwrite(bstr->data, 1, bstr->slen, fp);
                }
        }
        va_end(va);
}


void
__b_write(int fd, bstring *bstr, ...)
{
        va_list va;
        va_start(va, bstr);
        for (;;bstr = va_arg(va, bstring *)) {
                if (bstr) {
                        if (bstr->flags & BSTR_LIST_END)
                                break;
                        if (bstr->data && bstr->slen > 0) {
                                ssize_t n, total = 0;
                                do {
                                        n = write(fd, bstr->data, bstr->slen);
                                } while (n >= 0 && (total += n) != bstr->slen);
                        }
                }
        }
        va_end(va);
}


void
__b_free_all(bstring **bstr, ...)
{
        va_list va;
        va_start(va, bstr);
        for (;;bstr = va_arg(va, bstring **)) {
                if (bstr && *bstr) {
                        if ((*bstr)->flags & BSTR_LIST_END)
                                break;
                        if ((*bstr)->data) {
                                b_destroy(*bstr);
                                *bstr = NULL;
                        }
                }
        }
        va_end(va);
}


bstring *
__b_concat_all(const bstring *join, const int join_end, ...)
{
        uint size = 0;
        uint j_size = (join && join->data) ? join->slen : 0;

        va_list va, va2;
        va_start(va, join_end);
        for (;;) {
                const bstring *src = va_arg(va, const bstring *);
                if (src) {
                        if (src->flags & BSTR_LIST_END)
                                break;
                        if (src->data)
                                size += src->slen + j_size;
                }
        }
        va_end(va);

        bstring *dest = b_alloc_null(size + 1);
        dest->slen = 0;
        va_start(va2, join_end);

        for (;;) {
                const bstring *src = va_arg(va2, const bstring *);
                if (src) {
                        if (src->flags & BSTR_LIST_END)
                                break;
                        if (src->data) {
                                memcpy((dest->data + dest->slen), src->data, src->slen);
                                dest->slen += src->slen;
                                if (j_size) {
                                        memcpy((dest->data + dest->slen), join->data,
                                               join->slen);
                                        dest->slen += src->slen;
                                }
                        }
                }
        }
        va_end(va2);

        if (dest->slen != size) {
                b_destroy(dest);
                RETURN_NULL();
        }

        if (join_end || !join) 
                dest->data[dest->slen] = '\0';
        else
                dest->data[(dest->slen -= join->slen)] = '\0';
        if (join && !join_end)
                size -= join->slen;

        return dest;
}


int
__b_append_all(bstring *dest, const bstring *join, const int join_end, ...)
{
        uint size   = dest->slen;
        uint j_size = (join && join->data) ? join->slen : 0;

        va_list va, va2;
        va_start(va, join_end);
        /* va_copy(va2, va); */

        for (;;) {
                const bstring *src = va_arg(va, const bstring *);
                if (src) {
                        if (src->flags & BSTR_LIST_END)
                                break;
                        if (src->data)
                                size += src->slen + j_size;
                }
        }
        va_end(va);

        b_alloc(dest, size + 1);
        va_start(va2, join_end);

        for (;;) {
                const bstring *src = va_arg(va2, const bstring *);
                if (src) {
                        if (src->flags & BSTR_LIST_END)
                                break;
                        if (src->data) {
                                memcpy((dest->data + dest->slen), src->data, src->slen);
                                dest->slen += src->slen;
                                if (j_size) {
                                        memcpy((dest->data + dest->slen), join->data,
                                               join->slen);
                                        dest->slen += join->slen;
                                }
                        }
                }
        }
        va_end(va2);

        if (join_end || !join) 
                dest->data[dest->slen] = '\0';
        else
                dest->data[(dest->slen -= join->slen)] = '\0';
        if (join && !join_end)
                size -= join->slen;

        return (dest->slen == size) ? BSTR_OK : BSTR_ERR;
}


/*============================================================================*/


int
b_strcmp_fast_wrap(const void *vA, const void *vB)
{
        const bstring *sA = *(bstring **)(vA);
        const bstring *sB = *(bstring **)(vB);

        if (sA->slen == sB->slen)
                return memcmp(sA->data, sB->data, sA->slen);
        else
                return sA->slen - sB->slen;
}


int
b_strcmp_wrap(const void *const vA, const void *const vB)
{
        return b_strcmp((*(bstring const*const*const)(vA)),
                        (*(bstring const*const*const)(vB)));
}


bstring *
b_refblk(void *blk, const uint len)
{
        if (!blk || len == 0)
                RETURN_NULL();
        bstring *ret = xmalloc(sizeof *ret);
        *ret = (bstring){
            .slen  = len,
            .mlen  = len,
            .data  = (uchar *)blk,
            .flags = BSTR_WRITE_ALLOWED | BSTR_FREEABLE
        };
        return ret;
}


bstring *
b_clone(const bstring *const src)
{
        if (INVALID(src))
                RETURN_NULL();

        bstring *ret = xmalloc(sizeof *ret);
        *ret = (bstring){.slen  = src->slen,
                         .mlen  = src->mlen,
                         .flags = (src->flags & (~((uint8_t)BSTR_DATA_FREEABLE))),
                         .data  = src->data};

        b_writeprotect(ret);
        return ret;
}


bstring *
b_clone_swap(bstring *src)
{
        if (INVALID(src) || NO_WRITE(src))
                RETURN_NULL();

        bstring *ret = xmalloc(sizeof *ret);
        *ret = (bstring){.slen  = src->slen,
                         .mlen  = src->mlen,
                         .flags = src->flags,
                         .data  = src->data};

        src->flags &= (~((uint8_t)BSTR_DATA_FREEABLE));
        b_writeprotect(src);

        return ret;
}


/* A 64 bit integer is at most 19 decimal digits long. That, plus one for the
 * null byte and plus one for a '+' or '-' sign gives a max size of 21. */
#define INT64_MAX_CHARS 21

bstring *
b_ll2str(const long long value)
{
        /* Generate the (reversed) string representation. */
        uint64_t inv = (value < 0) ? (-value) : (value);
        bstring *ret = b_alloc_null(INT64_MAX_CHARS);
        uchar   *ptr = ret->data;

        do {
                *ptr++ = (uchar)('0' + (inv % 10));
                inv    = (inv / 10);
        } while (inv > 0);

        if (value < 0)
                *ptr++ = (uchar)'-';

        /* Compute length and add null term. */
        ret->slen = (ptr - ret->data - 1);
        *ptr      = (uchar)'\0';

        /* Reverse the string. */
        --ptr;
        uchar *tmp = ret->data;
        while (ret->data < ptr) {
                char aux = *ret->data;
                *tmp     = *ptr;
                *ptr     = aux;
                ++tmp;
                --ptr;
        }

        return ret;
}


void
__b_dump_list(FILE *fp, const b_list *list, const char *listname)
{
        fprintf(fp, "Dumping list \"%s\"\n", listname);
        for (unsigned i = 0; i < list->qty; ++i)
                b_fputs(fp, list->lst[i], b_tmp("\n"));
        fputc('\n', fp);
}


int
b_add_to_list(b_list **listp, bstring *bstr)
{
        if (!listp || !*listp || !(*listp)->lst)
                RUNTIME_ERROR();

        if ((*listp)->qty == ((*listp)->mlen - 1))
                (*listp)->lst = xrealloc((*listp)->lst, ((*listp)->mlen *= 2) *
                                                          sizeof(*(*listp)->lst));
        (*listp)->lst[(*listp)->qty++] = bstr;

        return BSTR_OK;
}


b_list *
b_list_copy(const b_list *list)
{
        if (!list || !list->lst)
                RETURN_NULL();

        b_list *ret = b_list_create_alloc(list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                ret->lst[ret->qty] = b_strcpy(list->lst[i]);
                b_writeallow(ret->lst[ret->qty]);
                ++ret->qty;
        }

        return ret;
}

b_list *
b_list_clone(const b_list *const list)
{
        if (!list || !list->lst)
                RETURN_NULL();

        b_list *ret = b_list_create_alloc(list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                ret->lst[ret->qty] = b_clone(list->lst[i]);
                ++ret->qty;
        }

        return ret;
}

int
b_list_merge(b_list **dest, b_list *src, const int flags)
{
        if (!dest || !*dest || !(*dest)->lst)
                RUNTIME_ERROR();
        if (!src || !src->lst)
                RUNTIME_ERROR();
        if (src->qty == 0)
                return BSTR_ERR;

        const unsigned size = ((*dest)->qty + src->qty);
        if ((*dest)->mlen < size)
                (*dest)->lst = xrealloc((*dest)->lst,
                                (size_t)((*dest)->mlen = size) * sizeof(bstring *));

        for (unsigned i = 0; i < src->qty; ++i)
                (*dest)->lst[(*dest)->qty++] = src->lst[i];

        if (flags & BSTR_M_DEL_SRC) {
                free(src->lst);
                free(src);
        }
        if (flags & BSTR_M_DEL_DUPS)
                b_list_remove_dups(dest);
        else if (flags & BSTR_M_SORT_FAST)
                B_LIST_SORT_FAST(*dest);
        if (!(flags & BSTR_M_SORT_FAST) && (flags & BSTR_M_SORT))
                B_LIST_SORT(*dest);

        return BSTR_OK;
}


int
b_list_remove_dups(b_list **listp)
{
        if (!listp || !*listp || !(*listp)->lst || (*listp)->qty == 0)
                RUNTIME_ERROR();

        b_list *list = *listp;
        b_list *toks = b_list_create_alloc(list->qty * 10);

        for (unsigned i = 0; i < list->qty; ++i) {
                b_list *tmp = b_split(list->lst[i], ' ');
                for (unsigned x = 0; x < tmp->qty; ++x)
                        b_add_to_list(&toks, tmp->lst[x]);
                free(tmp->lst);
                free(tmp);
        }

        b_list_destroy(list);
        list = toks;

        qsort(list->lst, list->qty, sizeof(bstring *), &b_strcmp_fast_wrap);

        b_list *uniq = b_list_create_alloc(list->qty);
        uniq->lst[0] = list->lst[0];
        uniq->qty    = 1;
        b_writeprotect(uniq->lst[0]);

        for (unsigned i = 1; i < list->qty; ++i) {
                if (!b_iseq(list->lst[i], list->lst[i-1])) {
                        uniq->lst[uniq->qty] = list->lst[i];
                        b_writeprotect(uniq->lst[uniq->qty]);
                        ++uniq->qty;
                }
        }

        b_list_destroy(list);
        for (unsigned i = 0; i < uniq->qty; ++i)
                b_writeallow(uniq->lst[i]);

        *listp = uniq;
        return BSTR_OK;
}


/*============================================================================*/
/* Proper strstr replacements */
/*============================================================================*/


int64_t
b_strstr(const bstring *const haystack, const bstring *needle, const uint pos)
{
        if (INVALID(haystack) || INVALID(needle))
                RUNTIME_ERROR();
        if (haystack->slen < needle->slen || pos > haystack->slen)
                return (-1);

        char *ptr = strstr((char *)(haystack->data + pos), BS(needle));

        if (!ptr)
                return (-1);

        return (int64_t)((ptrdiff_t)ptr - (ptrdiff_t)haystack->data);
}


b_list *
b_strsep(bstring *str, const char *const delim, const int refonly)
{
        if (INVALID(str) || NO_WRITE(str) || !delim)
                RETURN_NULL();

        b_list  *ret  = b_list_create();
        char    *data = (char *)str->data;
        char    *tok;

        if (refonly)
                while ((tok = strsep(&data, delim)))
                        b_add_to_list(&ret, (data) ? b_refblk(tok, (psub(data, tok) - 1u))
                                                   : b_refcstr(tok));
        else
                while ((tok = strsep(&data, delim)))
                        b_add_to_list(&ret, (data) ? b_fromblk(tok, (psub(data, tok) - 1u))
                                                   : b_fromcstr(tok));

        return ret;
}
