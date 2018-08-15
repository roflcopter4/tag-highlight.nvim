#include "util.h"

#include "generic_list.h"

#define RUNTIME_ERROR() abort();
#define RETURN_NULL() abort();


static unsigned
snapUpSize(unsigned i)
{
        if (i < 8) {
                i = 8;
        } else {
                unsigned j = i;
                j |= (j >> 1);
                j |= (j >> 2);
                j |= (j >> 4);
                j |= (j >> 8); /* Ok, since int >= 16 bits */
#if (UINT_MAX != 0xFFFF)
                j |= (j >> 16); /* For 32 bit int systems */
#if (UINT_MAX > 0xFFFFFFFFllu)
                j |= (j >> 32); /* For 64 bit int systems */
#endif
#endif
                /* Least power of two greater than i */
                j++;
                if (j >= i)
                        i = j;
        }

        return i;
}


genlist *
genlist_create(void)
{
        genlist *sl = xmalloc(sizeof(genlist));
        sl->lst     = xmalloc(1 * sizeof(void *));
        sl->qty     = 0;
        sl->mlen    = 1;

        return sl;
}

genlist *
genlist_create_alloc(const unsigned msz)
{
        genlist *sl = xmalloc(sizeof(genlist));
        sl->lst     = xmalloc(msz * sizeof(void *));
        sl->qty     = 0;
        sl->mlen    = msz;

        return sl;
}


int
genlist_destroy(genlist *sl)
{
        if (!sl)
                return (-1);
        for (unsigned i = 0; i < sl->qty; ++i)
                if (sl->lst[i])
                        free(sl->lst[i]);

        sl->qty  = 0;
        sl->mlen = 0;
        free(sl->lst);
        sl->lst = NULL;
        free(sl);

        return 0;
}


int
genlist_alloc(genlist *sl, const unsigned msz)
{
        void **  ptr;
        unsigned smsz;
        size_t   nsz;

        if (!sl || msz == 0 || !sl->lst || sl->mlen == 0 || sl->qty > sl->mlen)
                RUNTIME_ERROR();
        if (sl->mlen >= msz)
                return 0;

        smsz = snapUpSize(msz);
        nsz  = ((size_t)smsz) * sizeof(void *);

        if (nsz < (size_t)smsz)
                RUNTIME_ERROR();

        ptr = xrealloc(sl->lst, nsz);

        sl->mlen = smsz;
        sl->lst  = ptr;
        return 0;
}


int
genlist_append(genlist **listp, void *item)
{
        if (!listp || !*listp || !(*listp)->lst)
                RUNTIME_ERROR();

        if ((*listp)->qty == ((*listp)->mlen - 1))
                (*listp)->lst = xrealloc((*listp)->lst, ((*listp)->mlen *= 2) *
                                                         sizeof(*(*listp)->lst));
        (*listp)->lst[(*listp)->qty++] = item;

        return 0;
}

int
genlist_remove(genlist *list, const unsigned index)
{
        if (!list || !list->lst || index >= list->qty)
                RUNTIME_ERROR();

        free(list->lst[index]);
        list->lst[index] = NULL;

        memmove(list->lst + index, list->lst + index + 1, --list->qty - index);
        return 0;
}


genlist *
genlist_copy(const genlist *list, const genlist_copy_func cpy)
{
        if (!list || !list->lst)
                RETURN_NULL();

        genlist *ret = genlist_create_alloc(list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                cpy(&ret->lst[ret->qty], list->lst[i]);
                ++ret->qty;
        }

        return ret;
}


/*======================================================================================*/


struct argument_vector *
argv_create(const unsigned len)
{
        struct argument_vector *ret = xmalloc(sizeof(struct argument_vector));
        ret->lst             = nmalloc(sizeof(char *), len);
        ret->qty             = 0;
        ret->mlen            = len;
        return ret;
}


void
argv_append(struct argument_vector *lst, const char *str, const bool cpy)
{
        if (lst->qty == (lst->mlen - 1))
                lst->lst = nrealloc(lst->lst, sizeof(char *), (lst->mlen *= 2));

        if (cpy)
                lst->lst[lst->qty++] = strdup(str);
        else
                lst->lst[lst->qty++] = (char *)str;
}


char *
argv_fmt(const char *const restrict fmt, ...)
{
        char *_buf = NULL;
        va_list _ap;
        va_start(_ap, fmt);
#ifdef HAVE_VASPRINTF
        vasprintf(&_buf, fmt, _ap);
#else
        bstring *tmp = b_vformat(fmt, ap);
        _buf         = BS(tmp);
        free(tmp);
#endif
        va_end(_ap);
        return _buf;
}


void
argv_destroy(struct argument_vector *lst)
{
        for (unsigned i = 0; i < lst->qty; ++i)
                if (lst->lst[i])
                        free(lst->lst[i]);
        free(lst->lst);
        free(lst);
}
