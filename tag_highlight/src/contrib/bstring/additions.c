/*
 * Do I need a copyright here?
 */

#include "private.h"
#include <assert.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "bstring.h"


/*============================================================================*/
/*============================================================================*/
#ifdef _WIN32
/*-
* SPDX-License-Identifier: BSD-3-Clause
*
* Copyright (c) 1990, 1993
*	The Regents of the University of California.  All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. Neither the name of the University nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

static inline char *
strsep(char **stringp, const char *delim)
{
        const char *delimp;
        char       *ptr, *tok;
        char        src_ch, del_ch;

        if ((ptr = tok = *stringp) == NULL)
                return (NULL);

        for (;;) {
                src_ch = *ptr++;
                delimp = delim;
                do {
                        if ((del_ch = *delimp++) == src_ch) {
                                if (src_ch == '\0')
                                        ptr = NULL;
                                else
                                        ptr[-1] = '\0';
                                *stringp = ptr;
                                return (tok);
                        }
                } while (del_ch != '\0');
        }
        /* NOTREACHED */
}
#else
typedef error_t errno_t;
#endif

int
b_memsep(bstring *dest, bstring *stringp, const char delim)
{
        if (!dest || !stringp || NO_WRITE(stringp))
                errx(1, "invalid input strings");

        dest->data = stringp->data;

        if (!stringp->data || stringp->slen == 0)
                return 0;

        const int64_t pos = b_strchr(stringp, delim);
        int           ret = 1;

        if (pos >= 0) {
                dest->data[pos] = '\0';
                dest->slen      = pos;
                stringp->data   = stringp->data + pos + 1U;
                stringp->slen  -= pos + 1U;
        } else {
                dest->slen    = stringp->slen;
                stringp->data = NULL;
                stringp->slen = 0;
        }

        return ret;
}

b_list *
b_split_char(bstring *tosplit, const int delim, const bool destroy)
{
        if (INVALID(tosplit) || (!destroy && NO_WRITE(tosplit)))
                RETURN_NULL();

        bstring *split = destroy ? tosplit : b_strcpy(tosplit);
        b_list  *ret   = b_list_create();

        bstring *tok = &(bstring){0, 0, NULL, 0};
        bstring *buf = &(bstring){split->slen, 0, split->data, split->flags};

        while (b_memsep(tok, buf, (char)delim)) {
                b_list_append(&ret, b_fromblk(tok->data, tok->slen));
        }

        b_destroy(split);
        return ret;
}


/*============================================================================*/
/* SOME CRAPPY ADDITIONS! */
/*============================================================================*/


void
_b_fwrite(FILE *fp, bstring *bstr, ...)
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


int
_b_write(const int fd, bstring *bstr, ...)
{
        va_list ap;
        va_start(ap, bstr);
        for (;;bstr = va_arg(ap, bstring *)) {
                if (bstr) {
                        if (bstr->flags & BSTR_LIST_END)
                                break;
                        if (bstr->data && bstr->slen > 0) {
                                ssize_t n, total = 0;
                                errno_t tmp;
                                errno = 0;
                                do {
                                        n = write(fd, bstr->data, bstr->slen);
                                } while (n > 0 && (unsigned)(total += n) < bstr->slen);
                                if ((tmp = errno)) {
                                        va_end(ap);
                                        return tmp;
                                }
                        }
                }
        }
        va_end(ap);

        return BSTR_OK;
}


void
_b_free_all(bstring **bstr, ...)
{
        va_list va;
        va_start(va, bstr);
        for (;;bstr = va_arg(va, bstring **)) {
                if (bstr && *bstr) {
                        if ((*bstr)->flags & BSTR_LIST_END)
                                break;
                        if ((*bstr)->data)
                                b_destroy(*bstr);
                }
        }
        va_end(va);
}


bstring *
_b_concat_all(const bstring *join, const int join_end, ...)
{
        unsigned       size   = 0;
        const unsigned j_size = (join && join->data) ? join->slen : 0;

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

        bstring *dest = b_alloc_null(size + 1 + j_size);
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
                                        dest->slen += j_size;
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
                dest->slen -= join->slen;

        return dest;
}


int
_b_append_all(bstring *dest, const bstring *join, const int join_end, ...)
{
        unsigned       size   = dest->slen;
        const unsigned j_size = (join && join->data) ? join->slen : 0;

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
        const bstring *sA = *(bstring const*const*const)(vA);
        const bstring *sB = *(bstring const*const*const)(vB);

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
b_steal(void *blk, const unsigned len)
{
        if (!blk || len == 0)
                RETURN_NULL();
        bstring *ret = malloc(sizeof *ret);
        *ret = (bstring){
            .slen  = len,
            .mlen  = len + 1,
            .data  = (uchar *)blk,
            .flags = BSTR_STANDARD,
        };
        return ret;
}


bstring *
b_refblk(void *blk, const unsigned len)
{
        if (!blk || len == 0)
                RETURN_NULL();
        bstring *ret = malloc(sizeof *ret);
        *ret = (bstring){
            .slen  = len,
            .mlen  = len,
            .data  = (uchar *)blk,
            .flags = BSTR_WRITE_ALLOWED | BSTR_FREEABLE /* | BSTR_CLONE */,
        };
        return ret;
}


bstring *
b_clone(const bstring *const src)
{
        if (INVALID(src))
                RETURN_NULL();

        bstring *ret = malloc(sizeof *ret);
        memcpy(ret, src, sizeof(bstring));
#if 0
        *ret = (bstring){.slen  = src->slen,
                         .mlen  = src->mlen,
                         .flags = (src->flags & (~((uint8_t)BSTR_DATA_FREEABLE))),
                         .data  = src->data};
#endif
        ret->flags &= (~((uint8_t)BSTR_DATA_FREEABLE));
        ret->flags |= BSTR_CLONE;
        b_writeprotect(ret);

        return ret;
}


bstring *
b_clone_swap(bstring *src)
{
        if (INVALID(src) || NO_WRITE(src))
                RETURN_NULL();

        bstring *ret = malloc(sizeof *ret);
        memcpy(ret, src, sizeof(bstring));
#if 0
        *ret = (bstring){.slen  = src->slen,
                         .mlen  = src->mlen,
                         .flags = src->flags,
                         .data  = src->data};
#endif

        src->flags &= (~((uint8_t)BSTR_DATA_FREEABLE));
        src->flags |= BSTR_CLONE;
        b_writeprotect(src);

        return ret;
}


/*============================================================================*/


void
_b_list_dump(FILE *fp, const b_list *list, const char *listname)
{
        fprintf(fp, "Dumping list \"%s\"\n", listname);
        for (unsigned i = 0; i < list->qty; ++i)
                b_fwrite(fp, list->lst[i], b_tmp("\n"));
        fputc('\n', fp);
}


void
_b_list_dump_fd(const int fd, const b_list *list, const char *listname)
{
        dprintf(fd, "Dumping list \"%s\"\n", listname);
        for (unsigned i = 0; i < list->qty; ++i) {
                if (!list->lst[i] || list->lst[i]->slen == 0)
                        b_write(fd, B("(NULL)\n"));
                else
                        b_write(fd, list->lst[i], b_tmp("\n"));
        }
        if (write(fd, "\n", 1) != 1)
                warn("write error");
}


int
b_list_append(b_list **listp, bstring *bstr)
{
        if (!listp || !*listp || !(*listp)->lst)
                RUNTIME_ERROR();

        if ((*listp)->qty >= ((*listp)->mlen)) {
                bstring **tmp = reallocarray((*listp)->lst, ((*listp)->mlen *= 2), sizeof(bstring *));
                (*listp)->lst = tmp;
        }
        (*listp)->lst[(*listp)->qty++] = bstr;

        return BSTR_OK;
}

int
b_list_remove(b_list *list, const unsigned index)
{
        if (!list || !list->lst || index >= list->qty)
                RUNTIME_ERROR();

        b_destroy(list->lst[index]);
        list->lst[index] = NULL;

        memmove(list->lst + index, list->lst + index + 1, --list->qty - index);
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


b_list *
b_list_clone_swap(b_list *list)
{
        if (!list || !list->lst)
                RETURN_NULL();

        b_list *ret = b_list_create_alloc(list->qty);

        for (unsigned i = 0; i < list->qty; ++i) {
                ret->lst[ret->qty] = b_clone_swap(list->lst[i]);
                ++ret->qty;
        }

        return ret;
}

int
b_list_writeprotect(b_list *list)
{
        if (!list || !list->lst)
                RUNTIME_ERROR();

        unsigned i;
        B_LIST_FOREACH(list, bstr, i)
                if (!INVALID(bstr))
                        b_writeprotect(bstr);

        return BSTR_OK;
}

int
b_list_writeallow(b_list *list)
{
        if (!list || !list->lst)
                RUNTIME_ERROR();

        unsigned i;
        B_LIST_FOREACH(list, bstr, i)
                if (!INVALID(bstr))
                        b_writeallow(bstr);

        return BSTR_OK;
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
                (*dest)->lst = realloc((*dest)->lst,
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

        b_list *toks = b_list_create_alloc((*listp)->qty * 10);

        for (unsigned i = 0; i < (*listp)->qty; ++i) {
                b_list *tmp = b_strsep((*listp)->lst[i], " ", 0);
                if (!tmp)
                        continue;
                for (unsigned x = 0; x < tmp->qty; ++x)
                        b_list_append(&toks, tmp->lst[x]);
                free(tmp->lst);
                free(tmp);
        }

        b_list_destroy(*listp);

        qsort(toks->lst, toks->qty, sizeof(bstring *), &b_strcmp_fast_wrap);

        b_list *uniq = b_list_create_alloc(toks->qty);
        uniq->lst[0] = toks->lst[0];
        uniq->qty    = 1;
        b_writeprotect(uniq->lst[0]);

        for (unsigned i = 1; i < toks->qty; ++i) {
                if (!b_iseq(toks->lst[i], toks->lst[i-1])) {
                        uniq->lst[uniq->qty] = toks->lst[i];
                        b_writeprotect(uniq->lst[uniq->qty]);
                        ++uniq->qty;
                }
        }

        b_list_destroy(toks);
        for (unsigned i = 0; i < uniq->qty; ++i)
                b_writeallow(uniq->lst[i]);

        *listp = uniq;
        return BSTR_OK;
}


bstring *
b_list_join(const b_list *list, const bstring *sep)
{
        if (!list || !list->lst || list->qty == 0)
                RETURN_NULL();
        unsigned i;
        unsigned total = 1;
        unsigned seplen = (sep != NULL) ? sep->slen : 0;

        B_LIST_FOREACH (list, str, i)
                total += str->slen + seplen;
        
        bstring *ret = b_alloc_null(total);

        B_LIST_FOREACH (list, str, i) {
                memcpy(ret->data + ret->slen, str->data, str->slen);
                ret->slen += str->slen;
                if (sep != NULL) {
                        memcpy(ret->data + ret->slen, sep->data, sep->slen);
                        ret->slen += sep->slen;
                }
        }

        ret->data[ret->slen] = '\0';
        return ret;
}


/*============================================================================*/
/* Proper strstr replacements */
/*============================================================================*/


int64_t
b_strstr(const bstring *const haystack, const bstring *needle, const unsigned pos)
{
        if (INVALID(haystack) || INVALID(needle))
                RUNTIME_ERROR();
        if (haystack->slen < needle->slen || pos > haystack->slen)
                return (-1);

        char *ptr = strstr((char *)(haystack->data + pos), BS(needle));

        if (!ptr)
                return (-1);

        return (int64_t)psub(ptr, haystack->data);
}


b_list *
b_strsep(bstring *ostr, const char *const delim, const int refonly)
{
        if (INVALID(ostr) || NO_WRITE(ostr) || !delim)
                RETURN_NULL();

        b_list *ret   = b_list_create();
        bstring tok[] = {{0, 0, NULL, 0}};
        bstring str[] = {{ostr->slen, 0, ostr->data, ostr->flags}};

        if (refonly)
                while (b_memsep(tok, str, delim[0]))
                        b_list_append(&ret, b_refblk(tok->data, tok->slen));
        else
                while (b_memsep(tok, str, delim[0]))
                        b_list_append(&ret, b_fromblk(tok->data, tok->slen));

        return ret;
}


/*============================================================================*/
/* strpbrk */
/*============================================================================*/


int64_t
b_strpbrk_pos(const bstring *bstr, const unsigned pos, const bstring *delim)
{
        if (INVALID(bstr) || INVALID(delim) || bstr->slen == 0 ||
                    delim->slen == 0 || pos > bstr->slen)
                RUNTIME_ERROR();

        for (unsigned i = pos; i < bstr->slen; ++i)
                for (unsigned x = 0; x < delim->slen; ++x)
                        if (bstr->data[i] == delim->data[x])
                                return (int64_t)i;

        return (-1LL);
}


int64_t
b_strrpbrk_pos(const bstring *bstr, const unsigned pos, const bstring *delim)
{
        if (INVALID(bstr) || INVALID(delim) || bstr->slen == 0 ||
                    delim->slen == 0 || pos > bstr->slen)
                RUNTIME_ERROR();

        unsigned i = pos;
        do {
                for (unsigned x = 0; x < delim->slen; ++x)
                        if (bstr->data[i] == delim->data[x])
                                return (int64_t)i;
        } while (i-- > 0);

        return (-1LL);
}


int
b_advance(bstring *bstr, const unsigned n)
{
        if (n == 0)
                return BSTR_OK;
        if (INVALID(bstr) || NO_WRITE(bstr) || n > bstr->slen)
                RUNTIME_ERROR();

        bstr->slen  += n;
        bstr->data  += n;
        bstr->flags |= BSTR_BASE_MOVED;

        return BSTR_OK;
}


/*============================================================================*/
/* Path operations */
/*============================================================================*/


bstring *
b_dirname(const bstring *path)
{
        if (INVALID(path))
                RETURN_NULL();
        int64_t pos;

#ifdef _WIN32
        pos = b_strrpbrk(path, B("/\\"));
#else
        pos = b_strrchr(path, '/');
        if (pos == 0)
                ++pos;
#endif

        if (pos >= 0)
                return b_fromblk(path->data, pos);

        RETURN_NULL();
}


bstring *
b_basename(const bstring *path)
{
        if (INVALID(path))
                RETURN_NULL();
        int64_t pos;

#ifdef _WIN32
        pos = b_strrpbrk(path, B("/\\"));
#else
        pos = b_strrchr(path, '/');
        if (pos == 0)
                ++pos;
#endif

        if (pos >= 0)
                return b_fromblk(path->data + pos + 1U, path->slen - pos - 1U);

        RETURN_NULL();
}


int
b_regularize_path(bstring *path)
{
        if (INVALID(path) || NO_WRITE(path))
                RUNTIME_ERROR();
        
#if defined(_WIN32) || defined(_WIN64)
        for (unsigned i = 0; i < path->slen; ++i)
                if (path->data[i] == '/')
                        path->data[i] = '\\';
#endif

        return BSTR_OK;
}


bstring *
b_quickread(const char *const __restrict fmt, ...)
{
        if (!fmt)
                RETURN_NULL();
        va_list ap;
        char buf[PATH_MAX + 1];
        va_start(ap, fmt);
        vsnprintf(buf, PATH_MAX + 1, fmt, ap);
        va_end(ap);

        struct stat st;
        FILE       *fp = fopen(buf, "rb");
        if (!fp)
                RETURN_NULL();
        fstat(fileno(fp), &st);
        if (st.st_size == 0) {
                fclose(fp);
                return NULL;
        }

        bstring      *ret   = b_alloc_null(st.st_size + 1);
        const ssize_t nread = fread(ret->data, 1, st.st_size, fp);
        fclose(fp);
        if (nread < 0) {
                b_free(ret);
                RETURN_NULL();
        }

        ret->slen        = (unsigned)nread;
        ret->data[nread] = '\0';
        return ret;
}

#if 0
#define INIT_READ ((size_t)(1 << 20))

static size_t
getstdin(char **dest, FILE *fp)
{
        size_t  total = 0;
        char   *buf   = malloc(INIT_READ+1LLU);
        
        for (;;) {
                size_t nread = fread(buf + total, 1, INIT_READ, fp);
                if (nread > 0) {
                        total += nread;
                        if (feof(fp))
                                break;
                        char *tmp = realloc(buf, total + INIT_READ);
                        buf       = tmp;
                } else {
                        break;
                }
        }

        buf[total] = '\0';
        *dest      = buf;
        return total;
}

bstring *
b_read_fd(const int fd)
{
        FILE *fp = fdopen(fd, "rb");
        char *buf;
        size_t total = getstdin(&buf, fp);
        fclose(fp);

        bstring *ret = malloc(sizeof *ret);
        *ret = (bstring){total, total+1, (uchar *)buf, BSTR_STANDARD};
        return ret;
}
#endif

#define INIT_READ ((size_t)(8192LLU))
#ifdef DOSISH
#  define SSIZE_T size_t
#else
#  define SSIZE_T ssize_t
#endif

bstring *
b_read_fd(const int fd)
{
        bstring *ret = b_alloc_null(INIT_READ + 1U);

        for (;;) {
                SSIZE_T nread = read(fd, (ret->data + ret->slen), INIT_READ);
                if (nread > 0) {
                        ret->slen += nread;
                        if ((size_t)nread < INIT_READ) {
                                /* eprintf("breaking\n"); */
                                break;
                        }
                        b_growby(ret, INIT_READ);
                } else {
                        break;
                }
        }

        ret->data[ret->slen] = '\0';
        return ret;
}



#if 0
bstring *
b_read_fd(const int fd)
{
        /* size_t   total = 0; */
        bstring *ret   = b_alloc_null(INIT_READ+1LLU);
        /* FILE    *fp    = fdopen(fd, "rb"); */
        /* bstring *ret   = malloc(sizeof *ret); */
        /* *ret = (bstring){0, INIT_READ+1LLU, calloc(1, INIT_READ+1LLU), BSTR_STANDARD}; */

        int ch = 0;
        while (read(fd, &ch, 1) != EOF)
                b_conchar(ret, ch);
#if 0
        for (;;) {
                size_t nread = fread(ret->data + total, 1, INIT_READ, fp);
                /* ssize_t nread = read(fd, ret->data + total, INIT_READ); */
                /* if (nread >= 0) { */
                total += nread;
                if (feof(fp))
                        break;
                /* b_alloc(ret, total + INIT_READ); */
                b_growby(ret, INIT_READ);
                /* } else { */
                        /* break; */
                /* } */
        }
#endif

        /* fclose(fp); */
        /* ret->slen        = total; */
        return ret;
}
#endif

/*============================================================================*/
/* Minor helper functions */
/*============================================================================*/


/* A 64 bit integer is at most 19 decimal digits long. That, plus one for the
 * null byte and plus one for a '+' or '-' sign gives a max size of 21. */
#define INT64_MAX_CHARS 21

bstring *
b_ll2str(const long long value)
{
        /* Generate the (reversed) string representation. */
        uchar *rev, *fwd;
        uint64_t inv = (value < 0) ? (-value) : (value);
        bstring *ret = b_alloc_null(INT64_MAX_CHARS + 1);
        rev = fwd = ret->data;

        do {
                *rev++ = (uchar)('0' + (inv % 10));
                inv    = (inv / 10);
        } while (inv);

        if (value < 0)
                *rev++ = (uchar)'-';

        /* Compute length and add null term. */
        *rev--    = (uchar)'\0';
        ret->slen = psub(rev, ret->data) + 1U;

        /* Reverse the string. */
        while (fwd < rev) {
                const uchar swap = *fwd;
                *fwd++           = *rev;
                *rev--           = swap;
        }

        return ret;
}


static unsigned
_tmp_ll2bstr(bstring *bstr, const long long value)
{
        uchar *rev, *fwd;
        unsigned long long inv = (value < 0) ? (-value) : (value);
        rev = fwd = bstr->data;

        do {
                *rev++ = (uchar)('0' + (inv % 10LLU));
                inv    = (inv / 10LLU);
        } while (inv);
        if (value < 0)
                *rev++ = (uchar)'-';

        *rev--     = (uchar)'\0';
        bstr->slen = psub(rev, bstr->data) + 1U;
        while (fwd < rev) {
                const uchar swap = *fwd;
                *fwd++           = *rev;
                *rev--           = swap;
        }

        return bstr->slen;
}


static unsigned
_tmp_ull2bstr(bstring *bstr, const unsigned long long value)
{
        uchar *rev, *fwd;
        unsigned long long inv = value;
        rev = fwd = bstr->data;

        do {
                *rev++ = (uchar)('0' + (inv % 10LLU));
                inv    = (inv / 10LLU);
        } while (inv);

        *rev--     = (uchar)'\0';
        bstr->slen = psub(rev, bstr->data) + 1U;
        while (fwd < rev) {
                const uchar swap = *fwd;
                *fwd++           = *rev;
                *rev--           = swap;
        }

        return bstr->slen;
}

/*============================================================================*/
/* Simple string manipulation.. */
/*============================================================================*/


int
b_chomp(bstring *bstr)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();

        if (bstr->slen > 0) {
                if (bstr->data[bstr->slen - 1] == '\n')
                        bstr->data[--bstr->slen] = '\0';
                if (bstr->data[bstr->slen - 1] == '\r')
                        bstr->data[--bstr->slen] = '\0';
        }

        return BSTR_OK;
}

int
b_replace_ch(bstring *bstr, const int find, const int replacement)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();

        unsigned  len = bstr->slen;
        uint8_t  *dat = bstr->data;
        uint8_t  *ptr = NULL;

        while ((ptr = memchr(dat, find, len))) {
                *ptr = replacement;
                len -= (unsigned)psub(dat, ptr);
                dat  = ptr + 1;
        }

        return BSTR_OK;
}

int
b_catblk_nonul(bstring *bstr, void *blk, const unsigned len)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !blk || len == 0)
                RUNTIME_ERROR();
        b_alloc(bstr, bstr->slen + 1 + len);
        memcpy(bstr->data + bstr->slen + 1, blk, len);
        return BSTR_OK;
}

bool
b_starts_with(const bstring *b0, const bstring *b1)
{
        if (INVALID(b0) || INVALID(b1))
                RUNTIME_ERROR();
        if (b0->slen < b1->slen)
                return false;
        return b_strncmp(b0, b1, b1->slen) == 0;
}


/*============================================================================*/
/* Simple printf analogues. */
/*============================================================================*/


bstring *
_b_sprintf(const bstring *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        bstring *ret = _b_vsprintf(fmt, ap);
        va_end(ap);
        return ret;
}


bstring *
_b_vsprintf(const bstring *fmt, va_list args)
{
        static bstring nullstring = bt_init("(null)");
        if (INVALID(fmt))
                RETURN_NULL();

        b_list  *c_strings = NULL;
        unsigned c_str_ctr = 0;

        va_list  cpy;
        int64_t  pos[2048];
        unsigned len  = fmt->slen;
        int64_t  pcnt = 0;
        int64_t  i    = 0;
        memset(pos, 0, sizeof(pos));
        va_copy(cpy, args);

        for (; i < fmt->slen; ++pcnt) {
                int     islong = 0;
                pos[pcnt] = b_strchrp(fmt, '%', i) + 1LL;
                if (pos[pcnt] == 0)
                        break;

                int ch = fmt->data[pos[pcnt]];

        len_restart:
                switch (ch) {
                case 's': {
                        bstring *next = va_arg(cpy, bstring *);
                        if (!next || !next->data)
                                len = (len - 2U) + (sizeof("(null)") - 1U);
                        else
                                len = (len - 2U) + next->slen;
                        i   = pos[pcnt] + 2LL;

                        break;
                }
                case 'n': {
                        if (!c_strings)
                                c_strings = b_list_create();
                        const char *next = va_arg(cpy, const char *);
                        bstring    *tmp;
                        if (next)
                                tmp = b_fromcstr(next);
                        else
                                tmp = b_fromlit("(null)");
                        b_list_append(&c_strings, tmp);

                        len = (len - 2U) + tmp->slen;
                        i   = pos[pcnt] + 2;
                        break;
                }
                case 'd':
                case 'u':
                        switch (islong) {
                        case 0:
                                (void)va_arg(cpy, int);
                                len = (len - 2U) + 11U;
                                i   = pos[pcnt] + 2;
                                break;
                        case 1: (void)va_arg(cpy, long);
#if INT_MAX == LONG_MAX
                                len = (len - 3U) + 11U;
#else
                                len = (len - 3U) + 21U;
#endif
                                i   = pos[pcnt] + 3;
                                break;
                        case 2:
                                (void)va_arg(cpy, long long);
                                len = (len - 4U) + 21U;
                                i   = pos[pcnt] + 4;
                                break;
                        case 3:
                                (void)va_arg(cpy, size_t);
                                len = (len - 3U) + 21U;
                                i   = pos[pcnt] + 3;
                                break;
                        default:
                                abort();
                        }

                        break;
                case 'l':
                        ++islong;
                        ch = fmt->data[pos[pcnt] + islong];
                        goto len_restart;
                case 'z':
                        islong = 3;
                        ch = fmt->data[pos[pcnt] + 1];
                        goto len_restart;
                case 'c':
                        (void)va_arg(cpy, int);
                        --len;
                        i = pos[pcnt] + 2;
                        break;
                case '%':
                        --len;
                        i = pos[pcnt] + 2;
                        break;
                default:
                        errx(1, "Value '%c' is not legal.", ch);
                }
        }

        va_end(cpy);
        bstring *ret = b_alloc_null(snapUpSize(len + 1U));
        int64_t  x;
        pcnt = i = x = 0;

        for (; ; ++pcnt) {
                const int64_t diff = (pos[pcnt] == 0) ? (int64_t)fmt->slen - x
                                                      : pos[pcnt] - x - 1LL;
                if (diff >= 0)
                        memcpy(ret->data + i, fmt->data + x, diff);
                
                int islong = 0;
                int ch     = fmt->data[pos[pcnt]];
                i += diff;
                x += diff;

                if (pos[pcnt] == 0)
                        break;

        str_restart:
                switch (ch) {
                case 's':
                case 'n': {
                        bstring *next;

                        if (ch == 's') {
                                next = va_arg(args, bstring *);
                                if (!next || !next->data)
                                        next = &nullstring;
                        } else {
                                if (!c_strings)
                                        abort();
                                (void)va_arg(args, const char *);
                                next = c_strings->lst[c_str_ctr++];
                        }

                        memcpy(ret->data + i, next->data, next->slen);
                        i += next->slen;
                        x = pos[pcnt] + 1;
                        break;
                }
                case 'd': {
                        uchar buf[INT64_MAX_CHARS + 1];
                        int n = 0;
                        bstring tmp = {0, 0, buf, 0};

                        switch (islong) {
                        case 0: {
                                const int next = va_arg(args, int);
                                n = _tmp_ll2bstr(&tmp, (long long)next);
                                x = pos[pcnt] + 1;
                                break;
                        }
                        case 1: {
                                const long next = va_arg(args, long);
                                n = _tmp_ll2bstr(&tmp, (long long)next);
                                x = pos[pcnt] + 2;
                                break;
                        }
                        case 2: {
                                const long long next = va_arg(args, long long);
                                n = _tmp_ll2bstr(&tmp, next);
                                x = pos[pcnt] + 3;
                                break;
                        }
                        case 3: {
                                const ssize_t next = va_arg(args, ssize_t);
                                n = _tmp_ll2bstr(&tmp, next);
                                x = pos[pcnt] + 2;
                                break;
                        }
                        default:
                                abort();
                        }

                        memcpy(ret->data + i, buf, n);
                        i += n;
                        break;
                }
                case 'u': {
                        uchar buf[INT64_MAX_CHARS + 1];
                        int n = 0;
                        bstring tmp[] = {{0, 0, buf, 0}};

                        switch (islong) {
                        case 0: {
                                const unsigned next = va_arg(args, unsigned);
                                n = _tmp_ull2bstr(tmp, (long long unsigned)next);
                                x = pos[pcnt] + 1;
                                break;
                        }
                        case 1: {
                                const long unsigned next = va_arg(args, long unsigned);
                                n = _tmp_ull2bstr(tmp, (long long unsigned)next);
                                x = pos[pcnt] + 2;
                                break;
                        }
                        case 2: {
                                const long long unsigned next = va_arg(args, long long unsigned);
                                n = _tmp_ull2bstr(tmp, next);
                                x = pos[pcnt] + 3;
                                break;
                        }
                        case 3: {
                                const size_t next = va_arg(args, size_t);
                                n = _tmp_ull2bstr(tmp, next);
                                x = pos[pcnt] + 2;
                                break;
                        }
                        default:
                                abort();
                        }

                        memcpy(ret->data + i, buf, n);
                        i += n;
                        break;
                }
                case 'c': {
                        const int next = va_arg(args, int);
                        ret->data[i++] = (uchar)next;
                        x += 2;
                        break;
                }
                case 'l':
                        ++islong;
                        ch = fmt->data[pos[pcnt] + islong];
                        goto str_restart;
                case 'z':
                        islong = 3;
                        ch = fmt->data[pos[pcnt] + 1];
                        goto str_restart;
                case '%':
                        ret->data[i++] = '%';
                        x += 2;
                        break;
                default:
                        errx(1, "Value '%c' is not legal.", ch);
                }
        }

        if (c_strings)
                b_list_destroy(c_strings);

        ret->data[(ret->slen = i)] = '\0';
        return ret;
}


int
_b_fprintf(FILE *out_fp, const bstring *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        const int ret = _b_vfprintf(out_fp, fmt, ap);
        va_end(ap);

        return ret;
}


int
_b_vfprintf(FILE *out_fp, const bstring *fmt, va_list args)
{
        if (INVALID(fmt) || !out_fp)
                RUNTIME_ERROR();

        bstring *toprint = _b_vsprintf(fmt, args);
        if (!toprint)
                RUNTIME_ERROR();

        const int ret = fwrite(toprint->data, 1, toprint->slen, out_fp);
        b_free(toprint);
        return ret;
}


int
_b_dprintf(const int out_fd, const bstring *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        const int ret = _b_vdprintf(out_fd, fmt, ap);
        va_end(ap);

        return ret;
}


int
_b_vdprintf(const int out_fd, const bstring *fmt, va_list args)
{
        if (INVALID(fmt) || out_fd < 0)
                RUNTIME_ERROR();

        bstring *toprint = _b_vsprintf(fmt, args);
        if (!toprint)
                RUNTIME_ERROR();

        const int ret = write(out_fd, toprint->data, toprint->slen);
        b_free(toprint);
        return ret;
}


int
_b_sprintfa(bstring *dest, const bstring *fmt, ...)
{
        if (INVALID(dest) || NO_WRITE(dest) || INVALID(fmt))
                RUNTIME_ERROR();
        va_list ap;
        va_start(ap, fmt);
        const int ret = _b_vsprintfa(dest, fmt, ap);
        va_end(ap);

        return ret;
}


int
_b_vsprintfa(bstring *dest, const bstring *fmt, va_list args)
{
        if (INVALID(dest) || NO_WRITE(dest) || INVALID(fmt))
                RUNTIME_ERROR();

        bstring *app = _b_vsprintf(fmt, args);
        if (INVALID(app))
                RUNTIME_ERROR();

        const unsigned newlen = snapUpSize(dest->slen + app->slen + 1U);

        if (dest->mlen >= newlen) {
                memcpy(dest->data + dest->slen, app->data, app->slen);
        } else {
                uchar *buf = malloc(newlen);
                memcpy(buf, dest->data, dest->slen);
                memcpy(buf + dest->slen, app->data, app->slen);
                free(dest->data);
                dest->data = buf;
        }

        dest->slen += app->slen;
        dest->data[dest->slen] = '\0';
        dest->mlen = newlen;
        b_free(app);

        return BSTR_OK;
}
