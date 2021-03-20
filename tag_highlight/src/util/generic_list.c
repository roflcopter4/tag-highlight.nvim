#include "Common.h"

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

/* static pthread_mutex_t list->mut = PTHREAD_MUTEX_INITIALIZER; */

#define RWLOCK_LOCK(GENLIST)                                   \
        __extension__({                                        \
                genlist * list_ = (GENLIST);                   \
                pthread_t self_ = pthread_self();              \
                if (!pthread_equal(self_, list_->whodunnit)) { \
                        pthread_mutex_lock(&list_->lock);   \
                        list_->whodunnit = self_;              \
                }                                              \
        })
#define mutex_unlock(GENLIST)                      \
        __extension__({                             \
                genlist *list_ = (GENLIST);         \
                pthread_mutex_unlock(&list_->lock); \
                list_->whodunnit = 0;               \
        })
        
genlist *
genlist_create(void *talloc_ctx)
{
        genlist *list = talloc(talloc_ctx, genlist);
        list->lst     = talloc_array(list, void *, 2);
        list->qty     = 0;
        list->mlen    = 2;
        /* talloc_set_destructor(list, genlist_destroy); */
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&list->mut, &attr);
        /* pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NP);
        pthread_rwlock_init(&list->lock, &attr); */

        return list;
}

genlist *
genlist_create_alloc(void *talloc_ctx, const unsigned msz)
{
        const unsigned size = (msz <= 2) ? 2 : msz;
        genlist *list = talloc(talloc_ctx, genlist);
        list->lst     = talloc_array(list, void *, size);
        list->qty     = 0;
        list->mlen    = size;
        /* talloc_set_destructor(list, genlist_destroy); */
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&list->mut, &attr);
        /* pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NP);
        pthread_rwlock_init(&list->lock, &attr); */

        return list;
}


int
genlist_destroy(genlist *list)
{
        if (!list)
                return (-1);
        talloc_free(list);
        return 0;
#if 0
        /* pthread_mutex_lock(&list->lock); */

        for (unsigned i = 0; i < list->qty; ++i)
                if (list->lst[i])
                        free(list->lst[i]);

        list->qty  = 0;
        list->mlen = 0;
        free(list->lst);
        list->lst = NULL;
        free(list);

        /* pthread_mutex_unlock(&list->lock); */
#endif
}


int
genlist_alloc(genlist *list, const unsigned msz)
{
        void **  ptr;
        unsigned smsz;
        size_t   nsz;

        if (!list || msz == 0 || !list->lst || list->mlen == 0 || list->qty > list->mlen)
                RUNTIME_ERROR();
        if (list->mlen >= msz)
                return 0;

        pthread_mutex_lock(&list->mut);

        smsz = snapUpSize(msz);
        nsz  = ((size_t)smsz) * sizeof(void *);

        if (nsz < (size_t)smsz) {
                pthread_mutex_unlock(&list->mut);
                RUNTIME_ERROR();
        }

        ptr = talloc_realloc(NULL, list->lst, void *, nsz);

        list->mlen = smsz;
        list->lst  = ptr;

        pthread_mutex_unlock(&list->mut);
        return 0;
}


/* int                                                                               */
/* genlist_append(genlist **list, void *item)                                       */
/* {                                                                                 */
/*         if (!list || !*list || !(*list)->lst)                                  */
/*                 RUNTIME_ERROR();                                                  */
/*                                                                                   */
/*         if ((*list)->qty == ((*list)->mlen - 1))                                */
/*                 (*list)->lst = realloc((*list)->lst, ((*list)->mlen *= 2) *   */
/*                                                          sizeof(*(*list)->lst)); */
/*         (*list)->lst[(*list)->qty++] = item;                                    */
/*                                                                                   */
/*         return 0;                                                                 */
/* }                                                                                 */

int
(genlist_append)(genlist *list, void *item /*, const bool copy, const size_t size*/)
{
        if (!list || !list->lst)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

        if (list->qty >= (list->mlen - 1)) {
                void **ptr = talloc_realloc(list, list->lst, void *, (list->mlen *= 2));
                assert(ptr != NULL);
                list->lst = ptr;
        }
#if 0

        if (copy) {
                list->lst[list->qty] = malloc(size);
                memcpy(list->lst[list->qty++], item, size);
        } else {
                list->lst[list->qty++] = item;
        }

#endif

        list->lst[list->qty++] = talloc_move(list, &item);

        pthread_mutex_unlock(&list->mut);
        return 0;
}

int 
genlist_remove(genlist *list, const void *obj)
{
        if (!list || !list->lst || !obj)
                RUNTIME_ERROR();

        pthread_mutex_lock(&list->mut);

        assert(list->qty > 0);
        eprintf("qty -> %u\n", list->qty);
        int ret = (-1);

        for (unsigned i = 0; i < list->qty; ++i) {
                if (list->lst[i] == obj) {
                        talloc_free(list->lst[i]);
                        list->lst[i] = NULL;
                        assert(list->qty > 0);
                        --list->qty;

                        if (i != list->qty && list->qty > 0) {
                                eprintf("qty -> %u, i ->%u\n", list->qty, i);
                                memmove(&list->lst[i], &list->lst[i+1],
                                                (list->qty - i) * sizeof(void *));
                        }

                        ret = 0;
                        break;
                }
        }

        pthread_mutex_unlock(&list->mut);
        return ret;
}

int
genlist_remove_index(genlist *list, const unsigned index)
{
        if (!list || !list->lst || index >= list->qty)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

#if 0
        free(list->lst[index]);
        list->lst[index] = NULL;

        if (index == list->qty - 1)
                --list->qty;
        else
                memmove(list->lst + index, list->lst + index + 1, --list->qty - index);
#endif

        talloc_free(list->lst[index]);
        list->lst[index] = NULL;

        if (index == --list->qty)
                memmove(list->lst + index, list->lst + (index + 1),
                        (list->qty - index) * sizeof(void *));

        pthread_mutex_unlock(&list->mut);
        return 0;
}

genlist *
genlist_copy(genlist *list, const genlist_copy_func cpy)
{
        if (!list || !list->lst)
                RETURN_NULL();
        pthread_mutex_lock(&list->mut);

        genlist *ret = genlist_create_alloc(NULL, list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                cpy(&ret->lst[ret->qty], list->lst[i]);
                ++ret->qty;
        }

        pthread_mutex_unlock(&list->mut);
        return ret;
}

void *
genlist_pop(genlist *list)
{
        if (!list || !list->lst || list->qty == 0)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

        void *ret = talloc_move(NULL, &list->lst[--list->qty]);

        pthread_mutex_unlock(&list->mut);
        return ret;
}

void *
genlist_dequeue(genlist *list)
{
        if (!list || !list->lst || list->qty == 0)
                RUNTIME_ERROR();
        pthread_mutex_lock(&list->mut);

        void *ret = talloc_move(NULL, &list->lst[0]);

        if (list->qty > 1)
                memmove(list->lst, list->lst + 1, (list->qty - 1) * sizeof(void *));
        --list->qty;

        pthread_mutex_unlock(&list->mut);
        return ret;
}


/*======================================================================================*/


struct argument_vector *
argv_create(const unsigned len)
{
        struct argument_vector *argv = talloc(NULL, struct argument_vector);
        argv->mlen                   = (len) ? len : 1;
        argv->qty                    = 0;
        argv->lst                    = talloc_array(argv, char *, argv->mlen);
        return argv;
}


void
argv_append(struct argument_vector *argv, const char *str, const bool cpy)
{
        if (argv->qty == (argv->mlen - 1)) {
                auto_type tmp = talloc_realloc(argv, argv->lst, char *, (argv->mlen *= 2));
                argv->lst     = tmp;
        }

        if (cpy) {
                argv->lst[argv->qty++] = talloc_strdup(argv->lst, str);
        } else {
                argv->lst[argv->qty++] = (char *)talloc_move(argv->lst, &str);
        }
}


void
argv_fmt(struct argument_vector *argv, const char *const __restrict fmt, ...)
{
        char *_buf = NULL;
        va_list _ap;
        va_start(_ap, fmt);
        _buf = talloc_vasprintf(NULL, fmt, _ap);
#if 0
#ifdef HAVE_VASPRINTF
        if (vasprintf(&_buf, fmt, _ap) == (-1))
                err(1, "vasprintf failed");
#else
        bstring *tmp = b_vformat(fmt, _ap);
        _buf         = talloc_move(NULL, &tmp->data);
        talloc_free(tmp);
#endif
#endif
        va_end(_ap);

        argv_append(argv, _buf, false);
}


void
argv_destroy(struct argument_vector *argv)
{
        talloc_free(argv);
#if 0
        for (unsigned i = 0; i < argv->qty; ++i)
                if (argv->lst[i])
                        free(argv->lst[i]);
        free(argv->lst);
        free(argv);
#endif
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
        fflush(fp);
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
