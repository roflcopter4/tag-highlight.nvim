#include "tag_highlight.h"

#include "util/list.h"

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
#  if (UINT_MAX > 0xFFFFFFFFllu)
                j |= (j >> 32); /* For 64 bit int systems */
#  endif
#endif
                /* Least power of two greater than i */
                j++;
                if (j >= i)
                        i = j;
        }

        return i;
}

/* static pthread_mutex_t sl->mut = PTHREAD_MUTEX_INITIALIZER; */

genlist *
genlist_create(void)
{
        genlist *sl = xmalloc(sizeof(genlist));
        sl->lst     = xmalloc(2 * sizeof(void *));
        sl->qty     = 0;
        sl->mlen    = 2;
        /* pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&sl->mut, &attr); */
        pthread_rwlock_init(&sl->lock);

        return sl;
}

genlist *
genlist_create_alloc(const unsigned msz)
{
        const unsigned size = (msz <= 2) ? 2 : msz;
        genlist *sl = xmalloc(sizeof(genlist));
        sl->lst     = xmalloc(size * sizeof(void *));
        sl->qty     = 0;
        sl->mlen    = size;
        /* pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&sl->mut, &attr); */
        pthread_rwlock_init(&sl->lock);

        return sl;
}


int
genlist_destroy(genlist *sl)
{
        if (!sl)
                return (-1);
        /* pthread_rwlock_wrlock(&sl->lock); */

        for (unsigned i = 0; i < sl->qty; ++i)
                if (sl->lst[i])
                        xfree(sl->lst[i]);

        sl->qty  = 0;
        sl->mlen = 0;
        xfree(sl->lst);
        sl->lst = NULL;
        xfree(sl);

        /* pthread_rwlock_unlock(&sl->lock); */
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

        pthread_rwlock_wrlock(&sl->lock);

        smsz = snapUpSize(msz);
        nsz  = ((size_t)smsz) * sizeof(void *);

        if (nsz < (size_t)smsz) {
                pthread_rwlock_unlock(&sl->lock);
                RUNTIME_ERROR();
        }

        ptr = xrealloc(sl->lst, nsz);

        sl->mlen = smsz;
        sl->lst  = ptr;

        pthread_rwlock_unlock(&sl->lock);
        return 0;
}


/* int                                                                               */
/* genlist_append(genlist **listp, void *item)                                       */
/* {                                                                                 */
/*         if (!listp || !*listp || !(*listp)->lst)                                  */
/*                 RUNTIME_ERROR();                                                  */
/*                                                                                   */
/*         if ((*listp)->qty == ((*listp)->mlen - 1))                                */
/*                 (*listp)->lst = xrealloc((*listp)->lst, ((*listp)->mlen *= 2) *   */
/*                                                          sizeof(*(*listp)->lst)); */
/*         (*listp)->lst[(*listp)->qty++] = item;                                    */
/*                                                                                   */
/*         return 0;                                                                 */
/* }                                                                                 */

int
genlist_append(genlist *list, void *item)
{
        if (!list || !list->lst)
                RUNTIME_ERROR();
        pthread_rwlock_wrlock(&list->lock);

        if (list->qty == (list->mlen - 1))
                list->lst = xrealloc(list->lst, (list->mlen *= 2) * sizeof(void *));
        list->lst[list->qty++] = item;

        pthread_rwlock_unlock(&list->lock);
        return 0;
}

int 
genlist_remove(genlist *list, const void *obj)
{
        if (!list || !list->lst || !obj)
                RUNTIME_ERROR();
        pthread_rwlock_wrlock(&list->lock);

        for (unsigned i = 0; i < list->qty; ++i) {
                if (list->lst[i] == obj) {
                        pthread_rwlock_unlock(&list->lock);
                        genlist_remove_index(list, i);
                        return 0;
                }
        }

        pthread_rwlock_unlock(&list->lock);
        return (-1);
}

int
genlist_remove_index(genlist *list, const unsigned index)
{
        if (!list || !list->lst || index >= list->qty)
                RUNTIME_ERROR();
        pthread_rwlock_wrlock(&list->lock);

        xfree(list->lst[index]);
        list->lst[index] = NULL;

        if (index == list->qty - 1)
                --list->qty;
        else
                list->lst[index] = list->lst[--list->qty];

        /* memmove(list->lst + index, list->lst + index + 1, --list->qty - index); */
        pthread_rwlock_unlock(&list->lock);
        return 0;
}

genlist *
genlist_copy(genlist *list, const genlist_copy_func cpy)
{
        if (!list || !list->lst)
                RETURN_NULL();
        pthread_rwlock_wrlock(&list->lock);

        genlist *ret = genlist_create_alloc(list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                cpy(&ret->lst[ret->qty], list->lst[i]);
                ++ret->qty;
        }

        pthread_rwlock_unlock(&list->lock);
        return ret;
}


/*======================================================================================*/


struct argument_vector *
argv_create(const unsigned len)
{
        struct argument_vector *argv = xmalloc(sizeof(struct argument_vector));
        argv->lst                    = nmalloc(((len) ? len : 1), sizeof(char *));
        argv->qty                    = 0;
        argv->mlen                   = len;
        return argv;
}


void
argv_append(struct argument_vector *argv, const char *str, const bool cpy)
{
        if (argv->qty == (argv->mlen - 1)) {
                auto_type tmp = nrealloc(argv->lst, (argv->mlen *= 2), sizeof(char *));
                argv->lst = tmp;
        }

        if (cpy)
                argv->lst[argv->qty++] = strdup(str);
        else
                argv->lst[argv->qty++] = (char *)str;
}


void
argv_fmt(struct argument_vector *argv, const char *const __restrict fmt, ...)
{
        char *_buf = NULL;
        va_list _ap;
        va_start(_ap, fmt);
#ifdef HAVE_VASPRINTF
        if (vasprintf(&_buf, fmt, _ap) == (-1))
                err(1, "vasprintf failed");
#else
        bstring *tmp = b_vformat(fmt, _ap);
        _buf         = BS(tmp);
        xfree(tmp);
#endif
        va_end(_ap);

        argv_append(argv, _buf, false);
}


void
argv_destroy(struct argument_vector *argv)
{
        for (unsigned i = 0; i < argv->qty; ++i)
                if (argv->lst[i])
                        xfree(argv->lst[i]);
        xfree(argv->lst);
        xfree(argv);
}


void
argv_dump__(FILE *                              fp,
            const struct argument_vector *const restrict argv,
            const char *const restrict listname,
            const char *const restrict file,
            const int line)
{
        fprintf(fp, "Dumping list \"%s\" (%s at %d)\n", listname, file, line);
        for (unsigned i = 0; i < argv->qty; ++i)
                fprintf(fp, "%s\n", argv->lst[i]);
        fputc('\n', fp);
}


void
argv_dump_fd__(const int    fd,
               const struct argument_vector *const restrict argv,
               const char *const restrict listname,
               const char *const restrict file,
               const int line)
{
        dprintf(fd, "Dumping list \"%s\" (%s at %d)\n", listname, file, line);
        for (unsigned i = 0; i < argv->qty; ++i)
                dprintf(fd, "%s\n", argv->lst[i]);
        if (write(fd, "\n", 1) != 1)
                err(1, "write");
}
