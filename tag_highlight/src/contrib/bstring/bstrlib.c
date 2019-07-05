/*
 * Copyright 2002-2010 Paul Hsieh
 * This file is part of Bstrlib.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of bstrlib nor the names of its contributors may be
 *       used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * GNU General Public License Version 2 (the "GPL").
 */

/*
 * This file is the core module for implementing the bstring functions.
 */

#include "private.h"

#include "bstring.h"


/**
 * Compute the snapped size for a given requested size.
 * By snapping to powers of 2 like this, repeated reallocations are avoided.
 */
/*PRIVATE*/ unsigned
snapUpSize(unsigned i)
{
        if (i < 8) {
                i = 8;
        } else {
                unsigned j = i;
                j |= (j >> 1);
                j |= (j >> 2);
                j |= (j >> 4);
                j |= (j >> 8);  /* Ok, since int >= 16 bits */
#if (UINT_MAX != 0xFFFF)
                j |= (j >> 16); /* For 32 bit int systems */
#   if (UINT_MAX > 0xFFFFFFFFllu)
                j |= (j >> 32); /* For 64 bit int systems */
#   endif
#endif
                /* Least power of two greater than i */
                j++;
                if (j >= i)
                        i = j;
        }

        return i;
}


#if 0
int
b_alloc(bstring *bstr, const unsigned olen)
{
        if (INVALID(bstr) || olen == 0)
                RUNTIME_ERROR();
        if (NO_ALLOC(bstr))
                FATAL_ERROR("Error, attempt to reallocate a static bstring.\n");
        if (NO_WRITE(bstr))
                RUNTIME_ERROR();

        if (olen >= bstr->mlen) {
                uchar *tmp;
                unsigned len = snapUpSize(olen);
                if (len <= bstr->mlen)
                        return BSTR_OK;

                /* Assume probability of a non-moving realloc is 0.125 */
                if (7 * bstr->mlen < 8 * bstr->slen) {
                        /* If slen is close to mlen in size then use realloc
                         * to reduce the memory defragmentation */
                retry:
                        tmp = realloc(bstr->data, len);
                        if (!tmp) {
                                /* Since we failed, try mallocating the tighest
                                 * possible allocation */
                                len = olen;
                                tmp = realloc(bstr->data, len);
                                if (!tmp)
                                        ALLOCATION_ERROR(BSTR_ERR);
                        }
                } else {
                        /* If slen is not close to mlen then avoid the penalty
                         * of copying the extra bytes that are allocated, but
                         * not considered part of the string */
                        tmp = malloc(len);
                        /* Perhaps there is no available memory for the two
                         * allocations to be in memory at once */
                        if (!tmp)
                                goto retry;

                        if (bstr->slen)
                                memcpy(tmp, bstr->data, bstr->slen);
                        xfree(bstr->data);
                }

                bstr->data             = tmp;
                bstr->mlen             = len;
                bstr->data[bstr->slen] = (uchar)'\0';
                bstr->flags            = BSTR_STANDARD;
        }
        return BSTR_OK;
}
#endif


int
b_alloc(bstring *bstr, const unsigned olen)
{
        if (INVALID(bstr) || olen == 0)
                RUNTIME_ERROR();
        if (NO_ALLOC(bstr))
                FATAL_ERROR("Error, attempt to reallocate a static bstring.\n");
        if (NO_WRITE(bstr))
                RUNTIME_ERROR();

        if (olen >= bstr->mlen) {
                uchar *tmp;
                unsigned len = snapUpSize(olen);
                if (len <= bstr->mlen)
                        return BSTR_OK;

                /* Assume probability of a non-moving realloc is 0.125 */
                if (7 * bstr->mlen < 8 * bstr->slen) {
                        /* If slen is close to mlen in size then use realloc
                         * to reduce the memory defragmentation */
                        tmp = xrealloc(bstr->data, len);
                } else {
                        /* If slen is not close to mlen then avoid the penalty
                         * of copying the extra bytes that are allocated, but
                         * not considered part of the string */
                        tmp = xmalloc(len);
                        if (bstr->slen)
                                memcpy(tmp, bstr->data, bstr->slen);
                        xfree(bstr->data);
                }

                bstr->data             = tmp;
                bstr->mlen             = len;
                bstr->data[bstr->slen] = (uchar)'\0';
                bstr->flags            = BSTR_STANDARD;
        }
        return BSTR_OK;
}


int
b_allocmin(bstring *bstr, unsigned len)
{
        if (IS_NULL(bstr))
                RUNTIME_ERROR();
        if (NO_ALLOC(bstr))
                errx(1, "Error, attempt to reallocate a static bstring");
        if (NO_WRITE(bstr) || len == 0)
                RUNTIME_ERROR();

        if (len < bstr->slen + 1)
                len = bstr->slen + 1;

        if (len != bstr->mlen) {
                uchar *buf      = xrealloc(bstr->data, (size_t)len);
                buf[bstr->slen] = (uchar)'\0';
                bstr->data      = buf;
                bstr->mlen      = len;
                bstr->flags     = BSTR_STANDARD;
        }

        return BSTR_OK;
}


int
b_free(bstring *bstr)
{
        if (!bstr)
                return BSTR_ERR;
                /* RUNTIME_ERROR(); */
        if (!(bstr->flags & BSTR_WRITE_ALLOWED) && !IS_CLONE(bstr))
                return BSTR_ERR;
                /* RUNTIME_ERROR(); */

        if (bstr->data && (bstr->flags & BSTR_DATA_FREEABLE))
                xfree(bstr->data);

        bstr->data = NULL;
        bstr->slen = bstr->mlen = (-1);

        if (!(bstr->flags & BSTR_FREEABLE))
                return BSTR_ERR;
                /* RUNTIME_ERROR(); */

        xfree(bstr);
        return BSTR_OK;
}


bstring *
b_fromcstr(const char *const str)
{
        if (!str)
                RETURN_NULL();

        const size_t   size = strlen(str);
        const unsigned max  = snapUpSize((size + (2 - (size != 0))));

        if (max <= size)
                RETURN_NULL();

        bstring *bstr = xmalloc(sizeof *bstr);
        bstr->slen    = size;
        bstr->mlen    = max;
        bstr->data    = xmalloc(bstr->mlen);
        bstr->flags   = BSTR_STANDARD;

        memcpy(bstr->data, str, size + 1);
        return bstr;
}


bstring *
b_fromcstr_alloc(const unsigned mlen, const char *const str)
{
        if (!str)
                RETURN_NULL();

        const size_t size = strlen(str);
        unsigned     max  = snapUpSize((size + (2 - (size != 0))));

        if (max <= size)
                RETURN_NULL();

        bstring *bstr = xmalloc(sizeof *bstr);
        bstr->slen    = size;
        if (max < mlen)
                max = mlen;

        bstr->mlen  = max;
        bstr->data  = xmalloc(bstr->mlen);
        bstr->flags = BSTR_STANDARD;

        memcpy(bstr->data, str, size + 1);
        return bstr;
}


bstring *
b_fromblk(const void *blk, const unsigned len)
{
        if (!blk)
                RETURN_NULL();

        bstring *bstr = xmalloc(sizeof *bstr);
        bstr->slen    = len;
        bstr->mlen    = snapUpSize(len + (2 - (len != 0)));;
        bstr->data    = xmalloc(bstr->mlen);
        bstr->flags   = BSTR_STANDARD;

        if (len > 0)
                memcpy(bstr->data, blk, len);
        bstr->data[len] = (uchar)'\0';

        return bstr;
}


bstring *
b_alloc_null(const unsigned len)
{
        uint safelen  = len + 1;
        bstring *bstr = xmalloc(sizeof *bstr);
        bstr->slen    = 0;
        bstr->mlen    = safelen;
        bstr->flags   = BSTR_STANDARD;
        bstr->data    = xmalloc(safelen);
        bstr->data[0] = (uchar)'\0';

        return bstr;
}


char *
b_bstr2cstr(const bstring *bstr, const char nul)
{
        if (INVALID(bstr))
                RETURN_NULL();
        char *buf = xmalloc(bstr->slen + 1);

        if (nul == 0) {
                /* Don't bother trying to replace nul characters with anything,
                 * just copy as efficiently as possible. */
                memcpy(buf, bstr->data, bstr->slen + 1);
        } else {
                for (unsigned i = 0; i < bstr->slen; ++i)
                        buf[i] = (bstr->data[i] == '\0') ? nul : bstr->data[i];

                buf[bstr->slen] = '\0';
        }

        return buf;
}


int
b_cstrfree(char *buf)
{
        xfree(buf);
        return BSTR_OK;
}


int
b_concat(bstring *b0, const bstring *b1)
{
        if (INVALID(b0) || NO_WRITE(b0) || INVALID(b1))
                RUNTIME_ERROR();

        bstring *aux = (bstring *)b1;
        const unsigned d   = b0->slen;
        const unsigned len = b1->slen;

        if (b0->mlen <= d + len + 1) {
                const ptrdiff_t pd = b1->data - b0->data;
                if (0 <= pd && pd < b0->mlen) {
                        aux = b_strcpy(b1);
                        if (!aux)
                                RUNTIME_ERROR();
                }
                if (b_alloc(b0, d + len + 1) != BSTR_OK) {
                        if (aux != b1)
                                b_free(aux);
                        RUNTIME_ERROR();
                }
        }

        memmove(&b0->data[d], &aux->data[0], len);
        b0->data[d + len] = (uchar)'\0';
        b0->slen          = d + len;
        if (aux != b1)
                b_free(aux);

        return BSTR_OK;
}


#if 0
int
b_conchar(bstring *bstr, const char ch)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();

        if (bstr->mlen < (bstr->slen + 2))
                if (b_alloc(bstr, bstr->slen + 2) != BSTR_OK)
                        RUNTIME_ERROR();

        bstr->data[bstr->slen++] = (uchar)ch;
        bstr->data[bstr->slen]   = (uchar)'\0';

        return BSTR_OK;
}
#endif


int
b_catcstr(bstring *bstr, const char *buf)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !buf)
                RUNTIME_ERROR();

        /* Optimistically concatenate directly */
        unsigned i;
        const unsigned blen = bstr->mlen - bstr->slen;
        char          *d    = (char *)(&bstr->data[bstr->slen]);

        for (i = 0; i < blen; ++i) {
                if ((*d++ = *buf++) == '\0') {
                        bstr->slen += i;
                        return BSTR_OK;
                }
        }

        bstr->slen += i;

        /* Need to explicitly resize and concatenate tail */
        return b_catblk(bstr, buf, strlen(buf));
}


int
b_catblk(bstring *bstr, const void *buf, const unsigned len)
{
        uint64_t nl;

        if (INVALID(bstr) || NO_WRITE(bstr) || !buf)
                RUNTIME_ERROR();
        if ((nl = (uint64_t)bstr->slen + (uint64_t)len) > UINT_MAX)
                RUNTIME_ERROR();
        if (bstr->mlen <= nl && b_alloc(bstr, nl + 1) == BSTR_ERR)
                RUNTIME_ERROR();

        memmove(&bstr->data[bstr->slen], buf, len);
        bstr->slen     = nl;
        bstr->data[nl] = (uchar)'\0';

        return BSTR_OK;
}


bstring *
b_strcpy(const bstring *bstr)
{
        if (INVALID(bstr))
                RETURN_NULL();

        bstring  *b0   = xmalloc(sizeof(bstring));
        unsigned  size = snapUpSize(bstr->slen + 1);
        b0->data       = xmalloc(size);

        if (!b0->data) {
                size     = bstr->slen + 1;
                b0->data = xmalloc(size);
        }
        b0->mlen  = size;
        b0->slen  = bstr->slen;
        b0->flags = BSTR_STANDARD;

        if (size)
                memcpy(b0->data, bstr->data, bstr->slen);
        b0->data[b0->slen] = (uchar)'\0';

        return b0;
}


int
b_assign(bstring *a, const bstring *bstr)
{
        if (INVALID(bstr) || NO_WRITE(a))
                RUNTIME_ERROR();
        if (bstr->slen != 0) {
                if (b_alloc(a, bstr->slen) != BSTR_OK)
                        RUNTIME_ERROR();
                memmove(a->data, bstr->data, bstr->slen);
        } else if (INVALID(a))
                RUNTIME_ERROR();

        a->data[bstr->slen] = (uchar)'\0';
        a->slen  = bstr->slen;
        a->flags = BSTR_STANDARD;

        return BSTR_OK;
}


int
b_assign_cstr(bstring *a, const char *str)
{
        unsigned i;
        if (INVALID(a) || NO_WRITE(a) || !str)
                RUNTIME_ERROR();

        for (i = 0; i < a->mlen; ++i) {
                if ('\0' == (a->data[i] = str[i])) {
                        a->slen = i;
                        return BSTR_OK;
                }
        }

        a->slen = i;
        const size_t len = strlen(str + i);
        if (len > INT_MAX || i + len + 1 > INT_MAX || 0 > b_alloc(a, (i + len + 1)))
                RUNTIME_ERROR();
        memmove(a->data + i, str + i, len + UINTMAX_C(1));
        a->slen += len;
        a->flags = BSTR_STANDARD;

        return BSTR_OK;
}


int
b_assign_blk(bstring *a, const void *buf, const unsigned len)
{
        if (INVALID(a) || NO_WRITE(a) || !buf || len + 1 < 1)
                RUNTIME_ERROR();
        if (len + 1 > a->mlen && 0 > b_alloc(a, len + 1))
                RUNTIME_ERROR();

        memmove(a->data, buf, len);
        a->data[len] = (uchar)'\0';
        a->slen      = len;
        a->flags     = BSTR_STANDARD;

        return BSTR_OK;
}


int
b_trunc(bstring *bstr, const unsigned n)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        if (bstr->slen > n) {
                bstr->slen = n;
                bstr->data[n] = (uchar)'\0';
        }

        return BSTR_OK;
}


#define upcase(c)   (toupper((uchar)(c)))
#define downcase(c) (tolower((uchar)(c)))
#define wspace(c)   (isspace((uchar)(c)))

int
b_toupper(bstring *bstr)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        for (unsigned i = 0, len = bstr->slen; i < len; ++i)
                bstr->data[i] = (uchar)upcase(bstr->data[i]);

        return BSTR_OK;
}


int
b_tolower(bstring *bstr)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        for (unsigned i = 0, len = bstr->slen; i < len; ++i)
                bstr->data[i] = (uchar)downcase(bstr->data[i]);

        return BSTR_OK;
}


int
b_strcmp(const bstring *b0, const bstring *b1)
{
        /* if (INVALID(b0)) */
                /* errx(1, "ERROR: string b0 is invalid -> %p, %u, %u, 0x%02X %s", */
                     /* (void *)b0, b0->slen, b0->mlen, b0->flags, BS(b0)); */
        /* if (INVALID(b1)) */
                /* errx(1, "ERROR: string b1 is invalid -> %p, %u, %u, 0x%02X %s", */
                     /* (void *)b1, b1->slen, b1->mlen, b1->flags, BS(b1)); */
        if (INVALID(b0) || INVALID(b1))
                abort();
                /* return SHRT_MIN; */

        /* Return zero if two strings are both empty or point to the same data. */
        if (b0->slen == b1->slen && (b0->data == b1->data || b0->slen == 0))
                return 0;

        const unsigned n = MIN(b0->slen, b1->slen);

        return memcmp(b0->data, b1->data, n);

#if 0

        for (unsigned i = 0; i < n; ++i) {
                int v = ((char)b0->data[i]) - ((char)b1->data[i]);
                if (v != 0)
                        return v;
                if (b0->data[i] == (uchar)'\0')
                        return 0;
        }

        if (b0->slen > n)
                return 1;
        if (b1->slen > n)
                return -1;

        return 0;
#endif
}


int
b_strncmp(const bstring *b0, const bstring *b1, const unsigned n)
{
        if (INVALID(b0) || INVALID(b1))
                return SHRT_MIN;

        const unsigned m = MIN(n, MIN(b0->slen, b1->slen));

#if 0
        if (b0->data != b1->data) {
                for (unsigned i = 0; i < m; ++i) {
                        int v = ((char)b0->data[i]) - ((char)b1->data[i]);
                        if (v != 0)
                                return v;
                        if (b0->data[i] == (uchar)'\0')
                                return 0;
                }
        }

        if (n == m || b0->slen == b1->slen)
                return 0;
        if (b0->slen > m)
                return 1;
#endif

        /* RUNTIME_ERROR(); */
        return memcmp(b0->data, b1->data, m);
}


int
b_stricmp(const bstring *b0, const bstring *b1)
{

        unsigned n;
        if (INVALID(b0) || INVALID(b1))
                return SHRT_MIN;
        if ((n = b0->slen) > b1->slen)
                n = b1->slen;
        else if (b0->slen == b1->slen && b0->data == b1->data)
                return BSTR_OK;

#if defined(HAVE_STRCASECMP)
        return strcasecmp(BS(b0), BS(b1));
#elif defined(HAVE_STRICMP)
        return stricmp(BS(b0), BS(b1));
#else
        for (unsigned i = 0; i < n; ++i) {
                const int v = (char)downcase(b0->data[i]) - (char)downcase(b1->data[i]);
                if (v != 0)
                        return v;
        }
        if (b0->slen > n) {
                const int v = (char)downcase(b0->data[n]);
                if (v)
                        return v;
                return UCHAR_MAX + 1;
        }
        if (b1->slen > n) {
                const int v = -(char)downcase(b1->data[n]);
                if (v)
                        return v;
                return -(UCHAR_MAX + 1);
        }

        return BSTR_OK;
#endif
}


int
b_strnicmp(const bstring *b0, const bstring *b1, const unsigned n)
{
        if (INVALID(b0) || INVALID(b1))
                return SHRT_MIN;
        unsigned v;
        unsigned m = n;

        if (m > b0->slen)
                m = b0->slen;
        if (m > b1->slen)
                m = b1->slen;
        if (b0->data != b1->data) {
                for (unsigned i = 0; i < m; ++i) {
                        v = (char)downcase(b0->data[i]);
                        v -= (char)downcase(b1->data[i]);
                        if (v != 0)
                                return b0->data[i] - b1->data[i];
                }
        }

        if (n == m || b0->slen == b1->slen)
                return BSTR_OK;
        if (b0->slen > m) {
                v = (char)downcase(b0->data[m]);
                if (v)
                        return v;
                return UCHAR_MAX + 1;
        }
        v = -(char)downcase(b1->data[m]);
        if (v)
                return v;

        return -(UCHAR_MAX + 1);
}


int
b_iseq(const bstring *b0, const bstring *b1)
{
        if (INVALID(b0) || INVALID(b1))
                RUNTIME_ERROR();
        /* if (INVALID(b0)) */
                /* errx(1, "ERROR: string b0 is invalid -> %p, %u, %u, 0x%02X %s", */
                     /* (void *)b0, b0->slen, b0->mlen, b0->flags, BS(b0)); */
        /* if (INVALID(b1)) */
                /* errx(1, "ERROR: string b1 is invalid -> %p, %u, %u, 0x%02X %s", */
                     /* (void *)b1, b1->slen, b1->mlen, b1->flags, BS(b1)); */

        if (b0->slen != b1->slen)
                return 0;
        if (b0->data == b1->data || b0->slen == 0)
                return 1;

        return !memcmp(b0->data, b1->data, b0->slen);
}


int
b_iseq_caseless(const bstring *b0, const bstring *b1)
{
        if (INVALID(b0) || INVALID(b1))
                RUNTIME_ERROR();
        if (b0->slen != b1->slen)
                return BSTR_OK;
        if (b0->data == b1->data || b0->slen == 0)
                return 1;

        for (unsigned i = 0, n = b0->slen; i < n; ++i) {
                if (b0->data[i] != b1->data[i]) {
                        const uchar c = (uchar)downcase(b0->data[i]);
                        if (c != (uchar)downcase(b1->data[i]))
                                return 0;
                }
        }

        return 1;
}


int
b_iseq_cstr(const bstring *bstr, const char *buf)
{
        if (!buf || INVALID(bstr))
                RUNTIME_ERROR();
#if 0
        unsigned i;

        for (i = 0; i < bstr->slen; ++i)
                if (buf[i] == '\0' || bstr->data[i] != (uchar)buf[i])
                        return 0;

        return buf[i] == '\0';
#endif
        const size_t len = strlen(buf);

        if (bstr->slen != len)
                return 0;
        if ((char *)bstr->data == buf)
                return 1;

        return !memcmp(bstr->data, (uchar *)buf, len);
}


int
b_iseq_cstr_caseless(const bstring *bstr, const char *buf)
{
        unsigned i;
        if (!buf || INVALID(bstr))
                RUNTIME_ERROR();
        for (i = 0; i < bstr->slen; ++i)
                if (buf[i] == '\0' || (bstr->data[i] != (uchar)buf[i] &&
                                downcase(bstr->data[i]) != (uchar)downcase(buf[i])))
                        return 0;

        return buf[i] == '\0';
}


int64_t
b_strchrp(const bstring *bstr, const int ch, const unsigned pos)
{
        if (IS_NULL(bstr) || bstr->slen < pos)
                RUNTIME_ERROR();
        if (bstr->slen == pos)
                return (-1);

        char *ptr = memchr((bstr->data + pos), ch, (bstr->slen - pos));

        if (ptr)
                return (int64_t)((ptrdiff_t)ptr - (ptrdiff_t)bstr->data);

        return (-1);
}


int64_t
b_strrchrp(const bstring *bstr, const int ch, const unsigned pos)
{
        if (IS_NULL(bstr) || bstr->slen <= pos)
                RUNTIME_ERROR();
#ifdef HAVE_MEMRCHR
        char *ptr = memrchr(bstr->data, ch, pos);

        if (ptr)
                return (int64_t)((ptrdiff_t)ptr - (ptrdiff_t)bstr->data);
#else
        for (int64_t i = pos; i >= 0; --i)
                if (bstr->data[i] == (uchar)ch)
                        return i;
#endif
        return (-1);
}


/* ============================================================================
 * READ
 * ============================================================================ */


int
b_reada(bstring *bstr, const bNread read_ptr, void *parm)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !read_ptr)
                RUNTIME_ERROR();

        unsigned i = bstr->slen;
        for (unsigned n = (i + 16);; n += ((n < BS_BUFF_SZ) ? n : BS_BUFF_SZ)) {
                if (BSTR_OK != b_alloc(bstr, n + 1))
                        RUNTIME_ERROR();
                const int bytes_read = read_ptr((void *)(bstr->data + i), 1, n - i, parm);
                i += bytes_read;
                bstr->slen = i;
                if (i < n)
                        break;
        }
        bstr->data[i] = (uchar)'\0';

        return BSTR_OK;
}


bstring *
b_read(const bNread read_ptr, void *parm)
{
        bstring *buff;
        if (0 > b_reada((buff = b_fromcstr("")), read_ptr, parm)) {
                b_free(buff);
                RETURN_NULL();
        }
        return buff;
}


/* ============================================================================
 * GETS
 * ============================================================================ */


inline static int
do_gets(bstring *bstr, const bNgetc getc_ptr, void *parm, const int terminator,
        const bool keepend, const unsigned init)
{
        int      ch;
        unsigned i = init;

        while ((ch = getc_ptr(parm)) >= 0) {
                if (i > (bstr->mlen - 2)) {
                        bstr->slen = i;
                        if (b_alloc(bstr, i + 2) != BSTR_OK)
                                RUNTIME_ERROR();
                }
                bstr->data[i++] = (uchar)ch;
                if (ch == terminator)
                        break;
        }

        bstr->slen    = ((keepend || i == 0) ? i : --i);
        bstr->data[i] = (uchar)'\0';

        return (i == 0 && ch < 0);
}


int
b_assign_gets(bstring *bstr, const bNgetc getc_ptr, void *parm,
              const int terminator, const bool keepend)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !getc_ptr)
                RUNTIME_ERROR();
        
        return do_gets(bstr, getc_ptr, parm, terminator, keepend, 0);
}


int
b_getsa(bstring *bstr, const bNgetc getc_ptr, void *parm,
        const int terminator, const bool keepend)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !getc_ptr)
                RUNTIME_ERROR();
        
        return do_gets(bstr, getc_ptr, parm, terminator, keepend, bstr->slen);
}


bstring *
b_gets(const bNgetc getc_ptr, void *parm,
       const int terminator, const bool keepend)
{
        bstring *buf = b_alloc_null(128);

        if (do_gets(buf, getc_ptr, parm, terminator, keepend, 0) != BSTR_OK)
                b_destroy(buf);

        return buf;
}


/* 
 * ============================================================================
 * FORMATTING FUNCTIONS
 * ============================================================================
 */

#define START_VSNBUFF (16)
/* On IRIX vsnprintf returns n-1 when the operation would overflow the target
 * buffer, WATCOM and MSVC both return -1, while C99 requires that the value be
 * exactly what the length would be if the buffer would be large enough.  This
 * leads to the idea that if the retval is larger than n, then changing n to the
 * retval will reduce the number of iterations required. */


bstring *
b_format(const char *const fmt, ...)
{
        if (!fmt)
                RETURN_NULL();
        va_list va;
        va_start(va, fmt);
        bstring *ret = b_vformat(fmt, va);
        va_end(va);

        return ret;
}

int
b_format_assign(bstring *bstr, const char *const fmt, ...)
{
        if (!fmt || NO_WRITE(bstr)) 
                RUNTIME_ERROR();
        va_list va;
        va_start(va, fmt);
        bstring *buff = b_vformat(fmt, va);
        va_end(va);

        const int ret = b_assign(bstr, buff);
        b_free(buff);
        return ret;
}

int
b_formata(bstring *bstr, const char *const fmt, ...)
{
        if (!fmt || INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        va_list va;
        va_start(va, fmt);
        bstring *buff = b_vformat(fmt, va);
        va_end(va);

        const int ret = b_concat(bstr, buff);
        b_free(buff);
        return ret;
}

bstring *
b_vformat(const char *const fmt, va_list arglist)
{
        if (!fmt || !arglist)
                RETURN_NULL();
        unsigned total;
        bstring *buff;

#ifdef HAVE_VASPRINTF
        char *tmp   = NULL;
        total       = xvasprintf(&tmp, fmt, arglist);
        buff        = xmalloc(sizeof *buff);
        buff->data  = (uchar *)tmp;
        buff->slen  = total;
        buff->mlen  = total + 1;
        buff->flags = BSTR_STANDARD;
#else
        /*
         * Without asprintf, because we can't determine the length of the
         * resulting string beforehand, a search has to be performed using the
         * truncating "vsnprintf" call (to avoid buffer overflows) on increasing
         * potential sizes for the output result. The function is supposed to
         * return the result that would have been printed if enough space were
         * available, so in theory this should take at most two attempts.
         */
        if ((total = (2 * strlen(fmt))) < START_VSNBUFF)
                total = START_VSNBUFF;
        buff = b_alloc_null(total + 2);
        if (!buff) {
                total = 1;
                buff = b_alloc_null(total + 2);
                if (!buff)
                        RETURN_NULL();
        }

        for (;;) {
                const unsigned ret = vsnprintf(BS(buff), total + 1,
                                               fmt, arglist);

                buff->data[total] = (uchar)'\0';
                buff->slen        = strlen((char *)buff->data);

                if (buff->slen < total)
                        break;
                if (ret > total)
                        total = ret;
                else
                        total += total;

                if (BSTR_OK != b_alloc(buff, total + 2)) {
                        b_free(buff);
                        RETURN_NULL();
                }
        }
#endif

        return buff;
}

int
b_vformat_assign(bstring *bstr, const char *const fmt, va_list arglist)
{
        if (!fmt || NO_WRITE(bstr)) 
                RUNTIME_ERROR();

        bstring  *buff = b_vformat(fmt, arglist);
        const int ret  = b_assign(bstr, buff);
        b_free(buff);
        return ret;
}

int
b_vformata(bstring *bstr, const char *const fmt, va_list arglist)
{
        if (!fmt || NO_WRITE(bstr)) 
                RUNTIME_ERROR();

        bstring  *buff = b_vformat(fmt, arglist);
        const int ret  = b_concat(bstr, buff);
        b_free(buff);
        return ret;
}
