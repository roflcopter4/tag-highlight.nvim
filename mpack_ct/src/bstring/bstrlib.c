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
 * This file is the core module for implementing the bstring * functions.
 */

#include "private.h"


/*
 * There were some pretty horrifying if statements in this file. I've tried to
 * make them at least somewhat saner with these macros that at least explain
 * what the checks are trying to accomplish.
 */
#define IS_NULL(BSTR)   (!(BSTR) || !(BSTR)->data)
#define INVALID(BSTR)   (IS_NULL(BSTR))
#define NO_WRITE(BSTR)  (((BSTR)->flags & BSTR_WRITE_ALLOWED) == 0)
#define NO_ALLOC(BSTR)  (((BSTR)->flags & BSTR_DATA_FREEABLE) == 0)
#define IS_STATIC(BSTR) (NO_WRITE(BSTR) && NO_ALLOC(BSTR))

#define BSTR_STANDARD (BSTR_WRITE_ALLOWED | BSTR_FREEABLE | BSTR_DATA_FREEABLE)

typedef unsigned int uint;


/**
 * Compute the snapped size for a given requested size.
 * By snapping to powers of 2 like this, repeated reallocations are avoided.
 */
static uint
snapUpSize(uint i)
{
        if (i < 8)
                i = 8;
        else {
                uint j = i;
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


int
b_alloc(bstring *bstr, const uint olen)
{
        if (INVALID(bstr) || olen == 0)
                RUNTIME_ERROR();
        if (NO_ALLOC(bstr))
                errx(1, "Error, attempt to reallocate a static bstring");
        if (NO_WRITE(bstr))
                RUNTIME_ERROR();

        if (olen >= bstr->mlen) {
                uchar *tmp;
                uint len = snapUpSize(olen);
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
                         * of copying the extra bytes that are mallocated, but
                         * not considered part of the string */
                        tmp = malloc(len);
                        /* Perhaps there is no available memory for the two
                         * mallocations to be in memory at once */
                        if (!tmp)
                                goto retry;

                        if (bstr->slen)
                                memcpy(tmp, bstr->data, bstr->slen);
                        free(bstr->data);
                }
                bstr->data = tmp;
                bstr->mlen = len;
                bstr->data[bstr->slen] = (uchar)'\0';
                bstr->flags = BSTR_STANDARD;
        }
        return BSTR_OK;
}


int
b_allocmin(bstring *bstr, uint len)
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


bstring *
b_fromcstr(const char *const str)
{
        if (!str)
                RETURN_NULL();

        const size_t size = strlen(str);
        const uint   max  = snapUpSize((size + (2 - (size != 0))));

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
b_fromcstr_alloc(const uint mlen, const char *const str)
{
        if (!str)
                RETURN_NULL();

        const size_t size = strlen(str);
        uint         max  = snapUpSize((size + (2 - (size != 0))));

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
b_blk2bstr(const void *blk, const uint len)
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
b_alloc_null(uint len)
{
        if (len == 0)
                len = 1;

        bstring *bstr = xmalloc(sizeof *bstr);
        bstr->slen    = 0;
        bstr->mlen    = len;
        bstr->flags   = BSTR_STANDARD;
        bstr->data    = xmalloc(len);
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
                for (uint i = 0; i < bstr->slen; ++i)
                        buf[i] = (bstr->data[i] == '\0') ? nul : bstr->data[i];

                buf[bstr->slen] = '\0';
        }

        return buf;
}


int
b_cstrfree(char *buf)
{
        free(buf);
        return BSTR_OK;
}


int
b_concat(bstring *b0, const bstring *b1)
{
        if (INVALID(b0) || NO_WRITE(b0) || INVALID(b1))
                RUNTIME_ERROR();

        bstring *aux = (bstring *)b1;
        uint     d   = b0->slen;
        uint     len = b1->slen;

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

        b_BlockCopy(&b0->data[d], &aux->data[0], len);
        b0->data[d + len] = (uchar)'\0';
        b0->slen          = d + len;
        if (aux != b1)
                b_free(aux);

        return BSTR_OK;
}


int
b_conchar(bstring *bstr, const char c)
{
        if (!bstr)
                RUNTIME_ERROR();

        const uint d = bstr->slen;

        if (b_alloc(bstr, d + 2) != BSTR_OK)
                RUNTIME_ERROR();

        bstr->data[d] = (uchar)c;
        bstr->data[d + 1] = (uchar)'\0';
        bstr->slen++;

        return BSTR_OK;
}


int
b_catcstr(bstring *bstr, const char *buf)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !buf)
                RUNTIME_ERROR();

        /* Optimistically concatenate directly */
        uint i;
        const uint blen = bstr->mlen - bstr->slen;
        char *d         = (char *)(&bstr->data[bstr->slen]);

        for (i = 0; i < blen; ++i) {
                if ((*d++ = *buf++) == '\0') {
                        bstr->slen += i;
                        return BSTR_OK;
                }
        }

        bstr->slen += i;

        /* Need to explicitely resize and concatenate tail */
        return b_catblk(bstr, buf, strlen(buf));
}


int
b_catblk(bstring *bstr, const void *buf, const uint len)
{
        uint64_t nl;
        if (INVALID(bstr) || NO_WRITE(bstr) || !buf)
                RUNTIME_ERROR();
        if ((nl = bstr->slen + len) > UINT_MAX)
                RUNTIME_ERROR();
        if (bstr->mlen <= nl && b_alloc(bstr, nl + 1) < 0)
                RUNTIME_ERROR();

        b_BlockCopy(&bstr->data[bstr->slen], buf, len);
        bstr->slen     = nl;
        bstr->data[nl] = (uchar)'\0';

        return BSTR_OK;
}


bstring *
b_strcpy(const bstring *bstr)
{
        if (INVALID(bstr))
                RETURN_NULL();

        bstring *b0 = xmalloc(sizeof(bstring));
        uint i      = bstr->slen;
        uint j      = snapUpSize(i + 1);
        b0->data    = malloc(j);

        if (!b0->data) {
                j        = i + 1;
                b0->data = xmalloc(j);
        }
        b0->mlen  = j;
        b0->slen  = i;
        b0->flags = BSTR_STANDARD;

        if (i)
                memcpy(b0->data, bstr->data, i);
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
b_assign_midstr(bstring *a, const bstring *bstr, int64_t left, uint len)
{
        if (INVALID(bstr) || INVALID(a) || NO_WRITE(a))
                RUNTIME_ERROR();
        if (left < 0) {
                len += left;
                left = 0;
        }
        if (len > bstr->slen - left)
                len = bstr->slen - left;

        if (len > 0) {
                if (b_alloc(a, len) != BSTR_OK)
                        RUNTIME_ERROR();
                memmove(a->data, bstr->data + left, len);
                a->slen = len;
        } else
                a->slen = 0;

        a->data[a->slen] = (uchar)'\0';
        a->flags         = BSTR_STANDARD;

        return BSTR_OK;
}


int
b_assign_cstr(bstring *a, const char *str)
{
        uint i;
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
        b_BlockCopy(a->data + i, str + i, (size_t)len + 1);
        a->slen += len;
        a->flags = BSTR_STANDARD;

        return BSTR_OK;
}


int
b_assign_blk(bstring *a, const void *buf, const uint len)
{
        if (INVALID(a) || NO_WRITE(a) || !buf || len + 1 < 1)
                RUNTIME_ERROR();
        if (len + 1 > a->mlen && 0 > b_alloc(a, len + 1))
                RUNTIME_ERROR();

        b_BlockCopy(a->data, buf, len);
        a->data[len] = (uchar)'\0';
        a->slen      = len;
        a->flags     = BSTR_STANDARD;

        return BSTR_OK;
}


int
b_trunc(bstring *bstr, const uint n)
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
        for (uint i = 0, len = bstr->slen; i < len; ++i)
                bstr->data[i] = (uchar)upcase(bstr->data[i]);

        return BSTR_OK;
}


int
b_tolower(bstring *bstr)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        for (uint i = 0, len = bstr->slen; i < len; ++i)
                bstr->data[i] = (uchar)downcase(bstr->data[i]);

        return BSTR_OK;
}


int
b_stricmp(const bstring *b0, const bstring *b1)
{
        uint n;
        if (INVALID(b0) || INVALID(b1))
                return SHRT_MIN;
        if ((n = b0->slen) > b1->slen)
                n = b1->slen;
        else if (b0->slen == b1->slen && b0->data == b1->data)
                return BSTR_OK;

        for (uint i = 0; i < n; ++i) {
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
}


int
b_strnicmp(const bstring *b0, const bstring *b1, const uint n)
{
        if (INVALID(b0) || INVALID(b1))
                return SHRT_MIN;
        uint v, m = n;

        if (m > b0->slen)
                m = b0->slen;
        if (m > b1->slen)
                m = b1->slen;
        if (b0->data != b1->data) {
                for (uint i = 0; i < m; ++i) {
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
b_iseq_caseless(const bstring *b0, const bstring *b1)
{
        if (INVALID(b0) || INVALID(b1))
                RUNTIME_ERROR();
        if (b0->slen != b1->slen)
                return BSTR_OK;
        if (b0->data == b1->data || b0->slen == 0)
                return 1;

        for (uint i = 0, n = b0->slen; i < n; ++i) {
                if (b0->data[i] != b1->data[i]) {
                        const uchar c = (uchar)downcase(b0->data[i]);
                        if (c != (uchar)downcase(b1->data[i]))
                                return 0;
                }
        }

        return 1;
}


int
b_is_stem_eq_caseless_blk(const bstring *b0, const void *blk, const uint len)
{
        if (INVALID(b0) || !blk)
                RUNTIME_ERROR();
        if (b0->slen < len)
                return BSTR_OK;
        if (b0->data == (const uchar *)blk || len == 0)
                return 1;

        for (uint i = 0; i < len; ++i)
                if (b0->data[i] != ((const uchar *)blk)[i])
                        if (downcase(b0->data[i]) != downcase(((const uchar *)blk)[i]))
                                return 0;

        return 1;
}


int
b_ltrimws(bstring *bstr)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        for (uint len = bstr->slen, i = 0; i < len; ++i)
                if (!wspace(bstr->data[i]))
                        return b_delete(bstr, 0, i);

        bstr->data[0] = (uchar)'\0';
        bstr->slen = 0;

        return BSTR_OK;
}


int
b_rtrimws(bstring *bstr)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        for (int64_t i = bstr->slen - 1; i >= 0; --i) {
                if (!wspace(bstr->data[i])) {
                        if (bstr->mlen > i)
                                bstr->data[i + 1] = (uchar)'\0';
                        bstr->slen = i + 1;
                        return BSTR_OK;
                }
        }

        bstr->data[0] = (uchar)'\0';
        bstr->slen = 0;

        return BSTR_OK;
}


int
b_trimws(bstring *bstr)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        for (int64_t i = bstr->slen - 1; i >= 0; --i) {
                if (!wspace(bstr->data[i])) {
                        int j;
                        if (bstr->mlen > i)
                                bstr->data[i + 1] = (uchar)'\0';
                        bstr->slen = i + 1;
                        for (j = 0; wspace(bstr->data[j]); j++)
                                ;
                        return b_delete(bstr, 0, j);
                }
        }

        bstr->data[0] = (uchar)'\0';
        bstr->slen = 0;

        return BSTR_OK;
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
b_is_stem_eq_blk(const bstring *b0, const void *blk, const uint len)
{
        if (INVALID(b0) || !blk)
                RUNTIME_ERROR();
        if (b0->slen < len)
                return 0;
        if (b0->data == (const uchar *)blk || len == 0)
                return 1;
        for (uint i = 0; i < len; ++i)
                if (b0->data[i] != ((const uchar *)blk)[i])
                        return 0;

        return 1;
}


int
b_iseq_cstr(const bstring *bstr, const char *buf)
{
        if (!buf || INVALID(bstr))
                RUNTIME_ERROR();
        uint i;

        for (i = 0; i < bstr->slen; ++i)
                if (buf[i] == '\0' || bstr->data[i] != (uchar)buf[i])
                        return 0;

        return buf[i] == '\0';
}


int
b_iseq_cstr_caseless(const bstring *bstr, const char *buf)
{
        uint i;
        if (!buf || INVALID(bstr))
                RUNTIME_ERROR();
        for (i = 0; i < bstr->slen; ++i)
                if (buf[i] == '\0' || (bstr->data[i] != (uchar)buf[i] &&
                                downcase(bstr->data[i]) != (uchar)downcase(buf[i])))
                        return 0;

        return buf[i] == '\0';
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
                return SHRT_MIN;

        /* Return zero if two strings are both empty or point to the same data. */
        if (b0->slen == b1->slen && (b0->data == b1->data || b0->slen == 0))
                return 0;

        const uint n = MIN(b0->slen, b1->slen);
#if 0

        for (uint i = 0; i < n; ++i) {
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

        return memcmp(b0->data, b1->data, n);
}


int
b_strncmp(const bstring *b0, const bstring *b1, const uint n)
{
        if (INVALID(b0) || INVALID(b1))
                return SHRT_MIN;

        const uint m = MIN(n, MIN(b0->slen, b1->slen));

#if 0
        if (b0->data != b1->data) {
                for (uint i = 0; i < m; ++i) {
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


bstring *
b_midstr(const bstring *bstr, int64_t left, uint len)
{
        if (INVALID(bstr))
                RETURN_NULL();
        if (left < 0) {
                len += left;
                left = 0;
        }
        if (len > bstr->slen - left)
                len = bstr->slen - left;
        if (len <= 0)
                return b_fromcstr("");

        return b_blk2bstr(bstr->data + left, len);
}


int
b_delete(bstring *bstr, int64_t pos, uint len)
{
        /* Clamp to left side of bstring * */
        if (pos < 0) {
                len += pos;
                pos = 0;
        }
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();

        if (len > 0 && pos < bstr->slen) {
                if (pos + len >= bstr->slen)
                        bstr->slen = pos;
                else {
                        b_BlockCopy((char *)(bstr->data + pos),
                                    (char *)(bstr->data + pos + len),
                                    bstr->slen - (pos + len));
                        bstr->slen -= len;
                }
                bstr->data[bstr->slen] = (uchar)'\0';
        }

        return BSTR_OK;
}


int
b_free(bstring *bstr)
{
        if (!bstr)
                RUNTIME_ERROR();
        if (NO_WRITE(bstr))
                return BSTR_ERR;
                /* RUNTIME_ERROR(); */

        if (bstr->data && (bstr->flags & BSTR_DATA_FREEABLE))
                free(bstr->data);

        /* In case there is any stale usage, there is one more chance to notice this error. */
        bstr->data = NULL;
        bstr->slen = bstr->mlen = (-1);

        if (!(bstr->flags & BSTR_FREEABLE))
                return BSTR_ERR;
                /* RUNTIME_ERROR(); */

        free(bstr);
        return BSTR_OK;
}


int64_t
b_instr(const bstring *b1, const uint pos, const bstring *b2)
{
        if (INVALID(b1) || INVALID(b2))
                RUNTIME_ERROR();
        if (b1->slen == pos)
                return (b2->slen == 0) ? (int64_t)pos : BSTR_ERR;
        if (b1->slen < pos)
                RUNTIME_ERROR();
        if (b2->slen == 0)
                return pos;

        /* No space to find such a string? */
        if ((b1->slen - b2->slen + 1) <= pos)
                RUNTIME_ERROR();

        /* An obvious alias case */
        if (b1->data == b2->data && pos == 0)
                return 0;

        uint   i  = pos;
        uint   ll = b2->slen;
        uchar *d0 = b2->data;
        uchar *d1 = b1->data;

        /* Peel off the b2->slen == 1 case */
        uchar c0 = d0[0];
        if (1 == ll) {
                for (uint lf = (b1->slen - b2->slen + 1); i < lf; ++i)
                        if (c0 == d1[i])
                                return i;
                RUNTIME_ERROR();
        }

        uchar c1 = c0;
        uint  j  = 0;
        uint  ii = -1;
        uint  lf = b1->slen - 1;

        if (i < lf) {
                do {
                        /* Unrolled current character test */
                        if (c1 != d1[i]) {
                                if (c1 != d1[1 + i]) {
                                        i += 2;
                                        continue;
                                }
                                i++;
                        }
                        /* Take note if this is the start of a potential match */
                        if (0 == j)
                                ii = i;
                        /* Shift the test character down by one */
                        j++;
                        i++;
                        /* If this isn't past the last character continue */
                        if (j < ll) {
                                c1 = d0[j];
                                continue;
                        }
                N0:
                        /* If no characters mismatched, then we matched */
                        if (i == ii + j)
                                return ii;

                        /* Shift back to the beginning */
                        i -= j;
                        j = 0;
                        c1 = c0;
                } while (i < lf);
        }

        /* Deal with last case if unrolling caused a misalignment */
        if (i == lf && ll == j + 1 && c1 == d1[i])
                goto N0;

        return BSTR_ERR;
}


int64_t
b_instrr(const bstring *b1, const uint pos, const bstring *b2)
{
        if (INVALID(b1) || INVALID(b2))
                RUNTIME_ERROR();
        if (b1->slen == pos && b2->slen == 0)
                return pos;
        if (b1->slen < pos)
                RUNTIME_ERROR();
        if (b2->slen == 0)
                return pos;

        /* Obvious alias case */
        if (b1->data == b2->data && pos == 0 && b2->slen <= b1->slen)
                return 0;
        if (((int64_t)b1->slen - (int64_t)b2->slen) < 0)
                RUNTIME_ERROR();

        int64_t i = pos;
        uint blen = b1->slen - b2->slen;

        /* If no space to find such a string then snap back */
        if (blen + 1 <= i)
                i = blen;

        uint   j  = 0;
        uchar *d0 = b2->data;
        uchar *d1 = b1->data;
        blen      = b2->slen;

        for (;;) {
                if (d0[j] == d1[i + j]) {
                        j++;
                        if (j >= blen)
                                return i;
                } else {
                        i--;
                        if (i < 0)
                                break;
                        j = 0;
                }
        }

        return BSTR_ERR;
}


int64_t
b_instr_caseless(const bstring *b1, const uint pos, const bstring *b2)
{
        if (INVALID(b1) || INVALID(b2))
                RUNTIME_ERROR();
        if (b1->slen == pos)
                return (b2->slen == 0) ? (int64_t)pos : BSTR_ERR;
        if (b1->slen < pos)
                RUNTIME_ERROR();
        if (b2->slen == 0)
                return pos;

        uint blen = b1->slen - b2->slen + 1;
        /* No space to find such a string? */
        if (blen <= pos)
                RUNTIME_ERROR();
        /* An obvious alias case */
        if (b1->data == b2->data && pos == 0)
                return BSTR_OK;

        uint   i  = pos;
        uint   j  = 0;
        uint   ll = b2->slen;
        uchar *d0 = b2->data;
        uchar *d1 = b1->data;

        for (;;) {
                if (d0[j] == d1[i + j] ||
                    downcase(d0[j]) == downcase(d1[i + j])) {
                        j++;
                        if (j >= ll)
                                return i;
                } else {
                        i++;
                        if (i >= blen)
                                break;
                        j = 0;
                }
        }

        return BSTR_ERR;
}


int64_t
b_instrr_caseless(const bstring *b1, const uint pos, const bstring *b2)
{
        if (INVALID(b1) || INVALID(b2))
                RUNTIME_ERROR();
        if (b1->slen == pos && b2->slen == 0)
                return pos;
        if (b1->slen < pos)
                RUNTIME_ERROR();
        if (b2->slen == 0)
                return pos;
        /* Obvious alias case */
        if (b1->data == b2->data && pos == 0 && b2->slen <= b1->slen)
                return BSTR_OK;

        int64_t blen, i = pos;
        if ((blen = (int64_t)b1->slen - (int64_t)b2->slen) < 0)
                RUNTIME_ERROR();
        /* If no space to find such a string then snap back */
        if ((blen + 1) <= i)
                i = blen;

        uint   j  = 0;
        uchar *d0 = b2->data;
        uchar *d1 = b1->data;
        blen      = b2->slen;

        for (;;) {
                if (d0[j] == d1[i + j] ||
                    downcase(d0[j]) == downcase(d1[i + j])) {
                        j++;
                        if (j >= blen)
                                return i;
                } else {
                        i--;
                        if (i < 0)
                                break;
                        j = 0;
                }
        }

        return BSTR_ERR;
}


int64_t
b_strchrp(const bstring *bstr, const int ch, const uint pos)
{
        if (IS_NULL(bstr) || bstr->slen <= pos)
                RUNTIME_ERROR();

        uchar *p = (uchar *)memchr((bstr->data + pos), (uchar)ch,
                                   (bstr->slen - pos));
        if (p)
                return (p - bstr->data);

        return BSTR_ERR;
}


int64_t
b_strrchrp(const bstring *bstr, const int ch, const uint pos)
{
        if (IS_NULL(bstr) || bstr->slen <= pos)
                RUNTIME_ERROR();
        for (int64_t i = pos; i >= 0; --i)
                if (bstr->data[i] == (uchar)ch)
                        return i;

        return BSTR_ERR;
}


#ifndef BSTRLIB_AGGRESSIVE_MEMORY_FOR_SPEED_TRADEOFF
#   define LONG_LOG_BITS_QTY (3)
#   define LONG_BITS_QTY (1 << LONG_LOG_BITS_QTY)
#   define LONG_TYPE uchar
#   define CFCLEN ((1 << CHAR_BIT) / LONG_BITS_QTY)
    struct char_field {
            LONG_TYPE content[CFCLEN];
    };
#   define testInCharField(cf, c)                  \
        ((cf)->content[(c) >> LONG_LOG_BITS_QTY] & \
         ((1ll) << ((c) & (LONG_BITS_QTY - 1))))

#   define setInCharField(cf, idx)                                  \
        do {                                                        \
                int c = (uint)(idx);                                \
                (cf)->content[c >> LONG_LOG_BITS_QTY] |=            \
                    (LONG_TYPE)(1llu << (c & (LONG_BITS_QTY - 1))); \
        } while (0)
#else
#   define CFCLEN (1 << CHAR_BIT)
    struct charField {
            uchar content[CFCLEN];
    };
#   define testInCharField(cf, c)  ((cf)->content[(uchar)(c)])
#   define setInCharField(cf, idx) (cf)->content[(uint int)(idx)] = ~0
#endif


/* Convert a bstring * to charField */
static int
build_char_field(struct char_field *cf, const bstring *bstr)
{
        if (IS_NULL(bstr) || bstr->slen <= 0)
                RUNTIME_ERROR();
        memset(cf->content, 0, sizeof(struct char_field));
        for (uint i = 0; i < bstr->slen; ++i)
                setInCharField(cf, bstr->data[i]);

        return BSTR_OK;
}


static void
invert_char_field(struct char_field *cf)
{
        for (uint i = 0; i < CFCLEN; ++i)
                cf->content[i] = ~cf->content[i];
}


/* Inner engine for binchr */
static int
b_inchrCF(const uchar *data, const uint len, const uint pos, const struct char_field *cf)
{
        for (uint i = pos; i < len; ++i) {
                const uchar c = data[i];
                if (testInCharField(cf, c))
                        return i;
        }

        return BSTR_ERR;
}


int64_t
b_inchr(const bstring *b0, const uint pos, const bstring *b1)
{
        struct char_field chrs;
        if (INVALID(b0) || INVALID(b1))
                RUNTIME_ERROR();
        if (1 == b1->slen)
                return b_strchrp(b0, b1->data[0], pos);
        if (0 > build_char_field(&chrs, b1))
                RUNTIME_ERROR();

        return b_inchrCF(b0->data, b0->slen, pos, &chrs);
}


/* Inner engine for binchrr */
static int
b_inchrrCF(const uchar *data, const uint pos, const struct char_field *cf)
{
        for (int64_t i = pos; i >= 0; --i) {
                const uint c = (uint)data[i];
                if (testInCharField(cf, c))
                        return i;
        }

        return BSTR_ERR;
}


int64_t
b_inchrr(const bstring *b0, uint pos, const bstring *b1)
{
        struct char_field chrs;
        if (INVALID(b0) || !b1)
                RUNTIME_ERROR();
        if (pos == b0->slen)
                pos--;
        if (1 == b1->slen)
                return b_strrchrp(b0, b1->data[0], pos);
        if (0 > build_char_field(&chrs, b1))
                RUNTIME_ERROR();

        return b_inchrrCF(b0->data, pos, &chrs);
}


int64_t
b_ninchr(const bstring *b0, const uint pos, const bstring *b1)
{
        struct char_field chrs;
        if (INVALID(b0))
                RUNTIME_ERROR();
        if (build_char_field(&chrs, b1) < 0)
                RUNTIME_ERROR();
        invert_char_field(&chrs);

        return b_inchrCF(b0->data, b0->slen, pos, &chrs);
}


int64_t
b_ninchrr(const bstring *b0, uint pos, const bstring *b1)
{
        struct char_field chrs;
        if (INVALID(b0))
                RUNTIME_ERROR();
        if (pos == b0->slen)
                pos--;
        if (build_char_field(&chrs, b1) < 0)
                RUNTIME_ERROR();
        invert_char_field(&chrs);

        return b_inchrrCF(b0->data, pos, &chrs);
}


int
b_setstr(bstring *b0, const uint pos, const bstring *b1, uchar fill)
{
        if (INVALID(b0) || NO_WRITE(b0))
                RUNTIME_ERROR();
        if (b1 && !b1->data)
                RUNTIME_ERROR();

        bstring *aux = (bstring *)b1;
        uint     d   = pos;

        /* Aliasing case */
        if (aux) {
                ptrdiff_t pd = (ptrdiff_t)(b1->data - b0->data);
                if (pd >= 0 && pd < (ptrdiff_t)b0->mlen)
                        if (!(aux = b_strcpy(b1)))
                                RUNTIME_ERROR();
                d += aux->slen;
        }

        /* Increase memory size if necessary */
        if (b_alloc(b0, d + 1) != BSTR_OK) {
                if (aux != b1)
                        b_free(aux);
                RUNTIME_ERROR();
        }
        uint newlen = b0->slen;

        /* Fill in "fill" character as necessary */
        if (pos > newlen) {
                memset(b0->data + b0->slen, fill, (size_t)(pos - b0->slen));
                newlen = pos;
        }

        /* Copy b1 to position pos in b0. */
        if (aux) {
                b_BlockCopy((char *)(b0->data + pos), (char *)aux->data, aux->slen);
                if (aux != b1)
                        b_free(aux);
        }

        /* Indicate the potentially increased size of b0 */
        if (d > newlen)
                newlen = d;
        b0->slen = newlen;
        b0->data[newlen] = (uchar)'\0';

        return BSTR_OK;
}


int
b_insert(bstring *b1, const uint pos, const bstring *b2, uchar fill)
{
        ptrdiff_t pd;
        bstring *aux = (bstring *)b2;

        if (INVALID(b1) || INVALID(b2) || NO_WRITE(b1))
                RUNTIME_ERROR();

        /* Aliasing case */
        if ((pd = (ptrdiff_t)(b2->data - b1->data)) >= 0 &&
            pd < (ptrdiff_t)b1->mlen) {
                if (!(aux = b_strcpy(b2)))
                        RUNTIME_ERROR();
        }

        /* Compute the two possible end pointers */
        if ((int64_t)b1->slen + (int64_t)aux->slen > UINT32_MAX) {
                if (aux != b2)
                        b_free(aux);
                RUNTIME_ERROR();
        }

        uint d    = b1->slen + aux->slen;
        uint blen = pos + aux->slen;

        if (blen > d) {
                /* Inserting past the end of the string */
                if (b_alloc(b1, blen + 1) != BSTR_OK) {
                        if (aux != b2)
                                b_free(aux);
                        RUNTIME_ERROR();
                }
                memset(b1->data + b1->slen, fill,
                       (size_t)(pos - b1->slen));
                b1->slen = blen;
        } else {
                /* Inserting in the middle of the string */
                if (b_alloc(b1, d + 1) != BSTR_OK) {
                        if (aux != b2)
                                b_free(aux);
                        RUNTIME_ERROR();
                }
                b_BlockCopy(b1->data + blen, b1->data + pos, d - blen);
                b1->slen = d;
        }

        b_BlockCopy(b1->data + pos, aux->data, aux->slen);
        b1->data[b1->slen] = (uchar)'\0';
        if (aux != b2)
                b_free(aux);

        return BSTR_OK;
}


int
b_replace(bstring *b1, const uint pos, const uint len, const bstring *b2, uchar fill)
{
        int64_t ret;
        ptrdiff_t pd;
        bstring *aux = (bstring *)b2;

        if ((int64_t)pos + (int64_t)len > UINT32_MAX || INVALID(b1) || INVALID(b2) || NO_WRITE(b1))
                RUNTIME_ERROR();
        uint pl = pos + len;

        /* Straddles the end? */
        if (pl >= b1->slen) {
                if ((ret = b_setstr(b1, pos, b2, fill)) < 0)
                        return ret;
                if (pos + b2->slen < b1->slen) {
                        b1->slen = pos + b2->slen;
                        b1->data[b1->slen] = (uchar)'\0';
                }
                return ret;
        }

        /* Aliasing case */
        pd = (ptrdiff_t)(b2->data - b1->data);
        if (pd >= 0 && pd < (ptrdiff_t)b1->slen) {
                aux = b_strcpy(b2);
                if (!aux)
                        RUNTIME_ERROR();
        }
        if (aux->slen > len) {
                if (b_alloc(b1, b1->slen + aux->slen - len) != BSTR_OK) {
                        if (aux != b2)
                                b_free(aux);
                        RUNTIME_ERROR();
                }
        }

        if (aux->slen != len)
                memmove(b1->data + pos + aux->slen, b1->data + pos + len,
                        b1->slen - (pos + len));
        memcpy(b1->data + pos, aux->data, aux->slen);

        b1->slen += aux->slen - len;
        b1->data[b1->slen] = (uchar)'\0';
        if (aux != b2)
                b_free(aux);

        return BSTR_OK;
}


typedef int64_t (*instr_fnptr)(const bstring *s1, const uint pos, const bstring *s2);
#define INITIAL_STATIC_FIND_INDEX_COUNT 32

/*
 *  findreplaceengine is used to implement bfindreplace and
 *  bfindreplacecaseless. It works by breaking the three cases of
 *  expansion, reduction and replacement, and solving each of these
 *  in the most efficient way possible.
 */
static int64_t
findreplaceengine(bstring *bstr, const bstring *find, const bstring *repl,
                  uint pos, instr_fnptr instr)
{
        int64_t ret;
        int64_t slen, mlen, delta, acc;
        int *d;
        /* This +1 is unnecessary, but it shuts up LINT. */
        int static_d[INITIAL_STATIC_FIND_INDEX_COUNT + 1];
        ptrdiff_t pd;
        bstring *auxf = (bstring *)find;
        bstring *auxr = (bstring *)repl;
        int64_t lpos  = pos;

        if (IS_NULL(find) || find->slen == 0 || INVALID(bstr) || INVALID(repl) || NO_WRITE(bstr))
                RUNTIME_ERROR();

        if (lpos > bstr->slen - find->slen)
                return BSTR_OK;

        /* Alias with find string */
        pd = (ptrdiff_t)(find->data - bstr->data);
        if ((ptrdiff_t)(lpos - find->slen) < pd && pd < (ptrdiff_t)bstr->slen) {
                auxf = b_strcpy(find);
                if (!auxf)
                        RUNTIME_ERROR();
        }

        /* Alias with repl string */
        pd = (ptrdiff_t)(repl->data - bstr->data);
        if ((ptrdiff_t)(lpos - repl->slen) < pd && pd < (ptrdiff_t)bstr->slen) {
                auxr = b_strcpy(repl);
                if (!auxr) {
                        if (auxf != find)
                                b_free(auxf);
                        RUNTIME_ERROR();
                }
        }

        /* in-place replacement since find and replace strings are of equal length */
        delta = auxf->slen - auxr->slen;
        if (delta == 0) {
                while ((lpos = instr(bstr, lpos, auxf)) >= 0) {
                        memcpy(bstr->data + lpos, auxr->data, auxr->slen);
                        lpos += auxf->slen;
                }
                if (auxf != find)
                        b_free(auxf);
                if (auxr != repl)
                        b_free(auxr);
                return BSTR_OK;
        }

        /* shrinking replacement since auxf->slen > auxr->slen */
        if (delta > 0) {
                int64_t li;
                acc = 0;
                while ((li = instr(bstr, lpos, auxf)) >= 0) {
                        if (acc && li > lpos)
                                memmove(bstr->data + lpos - acc, bstr->data + lpos, li - lpos);
                        if (auxr->slen)
                                memcpy(bstr->data + li - acc, auxr->data, auxr->slen);
                        acc += delta;
                        lpos = li + auxf->slen;
                }

                if (acc) {
                        li = bstr->slen;
                        if (li > lpos)
                                memmove((bstr->data + lpos - acc), bstr->data + lpos, li - lpos);
                        bstr->slen -= acc;
                        bstr->data[bstr->slen] = (uchar)'\0';
                }

                if (auxf != find)
                        b_free(auxf);
                if (auxr != repl)
                        b_free(auxr);
                return BSTR_OK;
        }

        /*
         * Expanding replacement since find->slen < repl->slen. Its a lot more
         * complicated. This works by first finding all the matches and storing
         * them to a growable array, then doing at most one resize of the
         * destination bstring * and then performing the direct memory transfers
         * of the string segment pieces to form the final result. The growable
         * array of matches uses a deferred doubling reallocing strategy. What
         * this means is that it starts as a reasonably fixed sized auto array
         * in the hopes that many if not most cases will never need to grow this
         * array. But it switches as soon as the bounds of the array will be
         * exceeded. An extra find result is always appended to this array that
         * corresponds to the end of the destination string, so slen is checked
         * against mlen - 1 rather than mlen before resizing.
         */
        mlen = INITIAL_STATIC_FIND_INDEX_COUNT;
        d = (int *)static_d; /* Avoid malloc for trivial/initial cases */
        acc = slen = 0;
        while ((lpos = instr(bstr, lpos, auxf)) >= 0) {
                if (slen >= mlen - 1) {
                        mlen += mlen;
                        int64_t sl = sizeof(int *) * mlen;
                        if (static_d == d) /* static_d cannot be realloced */
                                d = NULL;
                        if (mlen <= 0 || sl < mlen) {
                                ret = BSTR_ERR;
                                goto done;
                        }
                        int *t = xrealloc(d, sl);
                        if (!d)
                                memcpy(t, static_d, sizeof(static_d));
                        d = t;
                }

                d[slen] = lpos;
                slen++;
                acc -= delta;
                lpos += auxf->slen;
                if (acc < 0) {
                        ret = BSTR_ERR;
                        goto done;
                }
        }
        /* slen <= INITIAL_STATIC_INDEX_COUNT-1 or mlen-1 here. */
        d[slen] = bstr->slen;
        ret = b_alloc(bstr, bstr->slen + acc + 1);
        if (BSTR_OK == ret) {
                bstr->slen += acc;
                for (int64_t i = (slen - 1); i >= 0; --i) {
                        int64_t buf, blen;
                        buf = d[i] + auxf->slen;
                        blen = d[i + 1] - buf; /* d[slen] may be accessed here. */
                        if (blen)
                                memmove(bstr->data + buf + acc,
                                        bstr->data + buf, blen);
                        if (auxr->slen)
                                memmove(bstr->data + buf + acc - auxr->slen,
                                        auxr->data, auxr->slen);
                        acc += delta;
                }
                bstr->data[bstr->slen] = (uchar)'\0';
        }
done:
        if (static_d == d)
                d = NULL;
        free(d);
        if (auxf != find)
                b_free(auxf);
        if (auxr != repl)
                b_free(auxr);

        return ret;
}


int64_t
b_findreplace(bstring *bstr, const bstring *find, const bstring *repl, const uint pos)
{
        return findreplaceengine(bstr, find, repl, pos, b_instr);
}


int64_t
b_findreplace_caseless(bstring *bstr, const bstring *find, const bstring *repl, const uint pos)
{
        return findreplaceengine(bstr, find, repl, pos, b_instr_caseless);
}


int
b_insertch(bstring *bstr, uint pos, const uint len, const uchar fill)
{
        if (INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();

        /* Compute the two possible end pointers */
        const uint end    = bstr->slen + len;
        const uint newlen = pos + len;

        if (newlen > end) {
                /* Inserting past the end of the string */
                if (b_alloc(bstr, newlen + 1) != BSTR_OK)
                        RUNTIME_ERROR();
                pos = bstr->slen;
                bstr->slen = newlen;
        } else {
                /* Inserting in the middle of the string */
                if (b_alloc(bstr, end + 1) != BSTR_OK)
                        RUNTIME_ERROR();
                for (uint i = end - 1; i >= newlen; --i)
                        bstr->data[i] = bstr->data[i - len];
                bstr->slen = end;
        }

        for (uint i = pos; i < newlen; ++i)
                bstr->data[i] = fill;
        bstr->data[bstr->slen] = (uchar)'\0';

        return BSTR_OK;
}


int
b_pattern(bstring *bstr, const uint len)
{
        /* const int d = b_length(bstr); */
        if (INVALID(bstr))
                RUNTIME_ERROR();

        const unsigned d = bstr->slen;

        if (d == 0 || b_alloc(bstr, len + 1) != BSTR_OK)
                RUNTIME_ERROR();

        if (len > 0) {
                if (d == 1)
                        return b_setstr(bstr, len, NULL, bstr->data[0]);
                for (uint i = d; i < len; ++i)
                        bstr->data[i] = bstr->data[i - d];
        }

        bstr->data[len] = (uchar)'\0';
        bstr->slen = len;

        return BSTR_OK;
}


#define BS_BUFF_SZ (1024)

int
b_reada(bstring *bstr, const bNread read_ptr, void *parm)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !read_ptr)
                RUNTIME_ERROR();

        uint i = bstr->slen;
        for (uint n = (i + 16);; n += ((n < BS_BUFF_SZ) ? n : BS_BUFF_SZ)) {
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


int
b_assign_gets(bstring *bstr, const bNgetc getc_ptr, void *parm, const char terminator)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !getc_ptr)
                RUNTIME_ERROR();
        uint d = 0;
        uint e = bstr->mlen - 2;
        int ch;

        while ((ch = getc_ptr(parm)) >= 0) {
                if (d > e) {
                        bstr->slen = d;
                        if (b_alloc(bstr, d + 2) != BSTR_OK)
                                RUNTIME_ERROR();
                        e = bstr->mlen - 2;
                }
                bstr->data[d] = (uchar)ch;
                d++;
                if (ch == terminator)
                        break;
        }

        bstr->data[d] = (uchar)'\0';
        bstr->slen = d;

        return (d == 0 && ch < 0);
}


int
b_getsa(bstring *bstr, const bNgetc getc_ptr, void *parm, const char terminator)
{
        if (INVALID(bstr) || NO_WRITE(bstr) || !getc_ptr)
                RUNTIME_ERROR();
        int ch;
        uint d = bstr->slen;
        uint e = bstr->mlen - 2;

        while ((ch = getc_ptr(parm)) >= 0) {
                if (d > e) {
                        bstr->slen = d;
                        if (b_alloc(bstr, d + 2) != BSTR_OK)
                                RUNTIME_ERROR();
                        e = bstr->mlen - 2;
                }
                bstr->data[d] = (uchar)ch;
                d++;
                if (ch == terminator)
                        break;
        }

        bstr->data[d] = (uchar)'\0';
        bstr->slen = d;

        return (d == 0 && ch < 0);
}


bstring *
b_gets(const bNgetc getc_ptr, void *parm, const char terminator)
{
        bstring *buff = b_alloc_null(128);
        if (b_getsa(buff, getc_ptr, parm, terminator) != BSTR_OK) {
                b_free(buff);
                buff = NULL;
        }
        return buff;
}


struct bStream {
        bstring *buff;    /* Buffer for over-reads */
        void *parm;       /* The stream handle for core stream */
        bNread readFnPtr; /* fread compatible fnptr for core stream */
        int isEOF;        /* track file'buf EOF state */
        int maxBuffSz;
};

struct bStream *
bs_open(const bNread read_ptr, void *parm)
{
        if (!read_ptr)
                RETURN_NULL();

        struct bStream *buf = xmalloc(sizeof(struct bStream));

        buf->parm      = parm;
        buf->buff      = b_fromcstr("");
        buf->readFnPtr = read_ptr;
        buf->maxBuffSz = BS_BUFF_SZ;
        buf->isEOF     = 0;

        return buf;
}


int
bs_bufflength(struct bStream *buf, const uint sz)
{
        uint old_sz;
        if (!buf)
                RUNTIME_ERROR();
        old_sz = buf->maxBuffSz;
        if (sz > 0)
                buf->maxBuffSz = sz;

        return old_sz;
}


int
bs_eof(const struct bStream *buf)
{
        if (!buf || !buf->readFnPtr)
                RUNTIME_ERROR();
        return buf->isEOF && (buf->buff->slen == 0);
}


void *
bs_close(struct bStream *buf)
{
        void *parm;
        if (!buf)
                RETURN_NULL();
        buf->readFnPtr = NULL;
        if (buf->buff)
                b_destroy(buf->buff);
        buf->buff = NULL;
        parm = buf->parm;
        buf->parm = NULL;
        buf->isEOF = 1;
        free(buf);

        return parm;
}


int
bs_readlna(bstring *r, struct bStream *buf, const char terminator)
{
        uint i, rlo;
        bstring tmp;

        if (!buf || !buf->buff || INVALID(r) || NO_WRITE(r))
                RUNTIME_ERROR();

        uint blen = buf->buff->slen;
        if (BSTR_OK != b_alloc(buf->buff, buf->maxBuffSz + 1))
                RUNTIME_ERROR();
        char *str = (char *)buf->buff->data;
        tmp.data = (uchar *)str;

        /* First check if the current buffer holds the terminator */
        str[blen] = terminator; /* Set sentinel */
        for (i = 0; str[i] != terminator; ++i)
                ;
        if (i < blen) {
                tmp.slen = i + 1;
                const int64_t ret = b_concat(r, &tmp);
                buf->buff->slen = blen;
                if (BSTR_OK == ret)
                        b_delete(buf->buff, 0, i + 1);
                return BSTR_OK;
        }
        rlo = r->slen;
        /* If not then just concatenate the entire buffer to the output */
        tmp.slen = blen;
        if (BSTR_OK != b_concat(r, &tmp))
                RUNTIME_ERROR();

        /* Perform direct in-place reads into the destination to allow for
         * the minimum of data-copies */
        for (;;) {
                if (BSTR_OK != b_alloc(r, r->slen + buf->maxBuffSz + 1))
                        RUNTIME_ERROR();
                str = (char *)(r->data + r->slen);
                blen = buf->readFnPtr(str, 1, buf->maxBuffSz, buf->parm);
                if (blen <= 0) {
                        r->data[r->slen] = (uchar)'\0';
                        buf->buff->slen = 0;
                        buf->isEOF = 1;
                        /* If nothing was read return with an error message */
                        return BSTR_ERR & -(r->slen == rlo);
                }
                str[blen] = terminator; /* Set sentinel */
                for (i = 0; str[i] != terminator; ++i)
                        ;
                if (i < blen)
                        break;
                r->slen += blen;
        }

        /* Terminator found, push over-read back to buffer */
        r->slen += (++i);
        buf->buff->slen = blen - i;

        memcpy(buf->buff->data, str + i, blen - i);

        r->data[r->slen] = (uchar)'\0';
        r->flags = BSTR_STANDARD;

        return BSTR_OK;
}


int
bs_readlnsa(bstring *r, struct bStream *buf, const bstring *term)
{
        uint i;
        bstring tmp;
        struct char_field cf;

        if (!buf || !buf->buff || INVALID(term) || INVALID(r) || NO_WRITE(r))
                RUNTIME_ERROR();
        if (term->slen == 1)
                return bs_readlna(r, buf, term->data[0]);
        if (term->slen < 1 || build_char_field(&cf, term))
                RUNTIME_ERROR();

        uint blen = buf->buff->slen;
        if (BSTR_OK != b_alloc(buf->buff, buf->maxBuffSz + 1))
                RUNTIME_ERROR();

        uchar *bstr = (uchar *)buf->buff->data;
        tmp.data    = bstr;

        /* First check if the current buffer holds the terminator */
        bstr[blen] = term->data[0]; /* Set sentinel */
        for (i = 0; !testInCharField(&cf, bstr[i]); ++i)
                ;
        if (i < blen) {
                tmp.slen = i + 1;
                int64_t ret  = b_concat(r, &tmp);
                buf->buff->slen = blen;
                if (BSTR_OK == ret)
                        b_delete(buf->buff, 0, i + 1);
                return BSTR_OK;
        }
        int64_t rlo = r->slen;

        /* If not then just concatenate the entire buffer to the output */
        tmp.slen = blen;
        if (BSTR_OK != b_concat(r, &tmp))
                RUNTIME_ERROR();

        /* Perform direct in-place reads into the destination to allow for
         * the minimum of data-copies */
        for (;;) {
                if (BSTR_OK != b_alloc(r, r->slen + buf->maxBuffSz + 1))
                        RUNTIME_ERROR();

                bstr = (uchar *)(r->data + r->slen);
                blen = buf->readFnPtr(bstr, 1, buf->maxBuffSz, buf->parm);

                if (blen <= 0) {
                        r->data[r->slen] = (uchar)'\0';
                        buf->buff->slen = 0;
                        buf->isEOF = 1;
                        /* If nothing was read return with an error message */
                        return BSTR_ERR & -(r->slen == rlo);
                }

                bstr[blen] = term->data[0]; /* Set sentinel */
                for (i = 0; !testInCharField(&cf, bstr[i]); ++i)
                        ;
                if (i < blen)
                        break;
                r->slen += blen;
        }

        /* Terminator found, push over-read back to buffer */
        r->slen        += (++i);
        buf->buff->slen = blen - i;

        memcpy(buf->buff->data, bstr + i, blen - i);

        r->data[r->slen] = (uchar)'\0';
        r->flags         = BSTR_STANDARD;

        return BSTR_OK;
}


int
bs_reada(bstring *r, struct bStream *buf, uint n)
{
        int64_t ret;
        int64_t blen, orslen;
        char *bstr;
        bstring tmp;

        if (!buf || !buf->buff || INVALID(r) || NO_WRITE(r) || n <= 0)
                RUNTIME_ERROR();
        n += r->slen;

        if (n <= 0)
                RUNTIME_ERROR();
        blen = buf->buff->slen;
        orslen = r->slen;

        if (0 == blen) {
                if (buf->isEOF)
                        RUNTIME_ERROR();
                if (r->mlen > n) {
                        blen = buf->readFnPtr((r->data + r->slen), 1,
                                                   (n - r->slen), buf->parm);
                        if (0 >= blen || blen > n - r->slen) {
                                buf->isEOF = 1;
                                RUNTIME_ERROR();
                        }
                        r->slen += blen;
                        r->data[r->slen] = (uchar)'\0';
                        return 0;
                }
        }

        if (BSTR_OK != b_alloc(buf->buff, buf->maxBuffSz + 1))
                RUNTIME_ERROR();
        bstr = (char *)buf->buff->data;
        tmp.data = (uchar *)bstr;

        do {
                if (blen + r->slen >= n) {
                        tmp.slen = n - r->slen;
                        ret = b_concat(r, &tmp);
                        buf->buff->slen = blen;
                        if (BSTR_OK == ret)
                                b_delete(buf->buff, 0, tmp.slen);
                        return BSTR_ERR & -(r->slen == orslen);
                }
                tmp.slen = blen;
                if (BSTR_OK != b_concat(r, &tmp))
                        break;
                blen = n - r->slen;
                if (blen > buf->maxBuffSz)
                        blen = buf->maxBuffSz;
                blen = buf->readFnPtr(bstr, 1, blen, buf->parm);

        } while (blen > 0);

        if (blen < 0)
                blen = 0;
        if (blen == 0)
                buf->isEOF = 1;
        buf->buff->slen = blen;
        buf->buff->flags = BSTR_STANDARD;

        return BSTR_ERR & -(r->slen == orslen);
}


int
bs_readln(bstring *r, struct bStream *buf, char terminator)
{
        if (!buf || !buf->buff || INVALID(r) || NO_WRITE(r))
                RUNTIME_ERROR();
        if (BSTR_OK != b_alloc(buf->buff, buf->maxBuffSz + 1))
                RUNTIME_ERROR();
        r->slen = 0;

        return bs_readlna(r, buf, terminator);
}


int
bs_readlns(bstring *r, struct bStream *buf, const bstring *term)
{
        if (!buf || !buf->buff || INVALID(r) || NO_WRITE(r) || INVALID(term))
                RUNTIME_ERROR();
        if (term->slen == 1)
                return bs_readln(r, buf, term->data[0]);
        if (term->slen < 1)
                RUNTIME_ERROR();
        if (BSTR_OK != b_alloc(buf->buff, buf->maxBuffSz + 1))
                RUNTIME_ERROR();
        r->slen = 0;

        return bs_readlnsa(r, buf, term);
}


int
bs_read(bstring *r, struct bStream *buf, const uint n)
{
        if (!buf || !buf->buff || INVALID(r) || NO_WRITE(r) || n <= 0)
                RUNTIME_ERROR();
        if (BSTR_OK != b_alloc(buf->buff, buf->maxBuffSz + 1))
                RUNTIME_ERROR();
        r->slen = 0;

        return bs_reada(r, buf, n);
}


int
bs_unread(struct bStream *buf, const bstring *bstr)
{
        if (!buf || !buf->buff)
                RUNTIME_ERROR();
        return b_insert(buf->buff, 0, bstr, (uchar)'?');
}


int
bs_peek(bstring *r, const struct bStream *buf)
{
        if (!buf || !buf->buff)
                RUNTIME_ERROR();
        return b_assign(r, buf->buff);
}


bstring *
b_join(const b_list *bl, const bstring *sep)
{
        if (!bl || INVALID(sep))
                RETURN_NULL();
        int64_t total = 1;

        for (uint i = 0; i < bl->qty; ++i) {
                uint v = bl->lst[i]->slen;
                total += v;
                if (total > UINT32_MAX)
                        RETURN_NULL();
        }

        if (sep)
                total += (bl->qty - 1) * sep->slen;

        bstring *bstr = xmalloc(sizeof(bstring));

        bstr->mlen  = total;
        bstr->slen  = total - 1;
        bstr->data  = xmalloc(total);
        bstr->flags = BSTR_STANDARD;
        total       = 0;

        for (uint i = 0; i < bl->qty; ++i) {
                if (i > 0 && sep) {
                        memcpy(bstr->data + total, sep->data, sep->slen);
                        total += sep->slen;
                }
                uint v = bl->lst[i]->slen;
                memcpy(bstr->data + total, bl->lst[i]->data, v);
                total += v;
        }
        bstr->data[total] = (uchar)'\0';

        return bstr;
}


#define BSSSC_BUFF_LEN (256)

int
bs_splitscb(struct bStream *buf, const bstring *splitStr, bs_cbfunc cb, void *parm)
{
        struct char_field chrs;
        bstring *buff;
        int64_t i, p;
        int64_t ret;

        if (!cb || !buf || !buf->readFnPtr || INVALID(splitStr))
                RUNTIME_ERROR();
        buff = b_fromcstr("");
        if (!buff)
                RUNTIME_ERROR();

        if (splitStr->slen == 0) {
                while (bs_reada(buff, buf, BSSSC_BUFF_LEN) >= 0)
                        ;
                if ((ret = cb(parm, 0, buff)) > 0)
                        ret = 0;
        } else {
                build_char_field(&chrs, splitStr);
                ret = p = i = 0;
                for (;;) {
                        if (i >= buff->slen) {
                                bs_reada(buff, buf, BSSSC_BUFF_LEN);
                                if (i >= buff->slen) {
                                        if (0 < (ret = cb(parm, p, buff)))
                                                ret = 0;
                                        break;
                                }
                        }
                        if (testInCharField(&chrs, buff->data[i])) {
                                uchar c;
                                bstring t = blk2tbstr((buff->data + i + 1), buff->slen - (i + 1));
                                if ((ret = bs_unread(buf, &t)) < 0)
                                        break;
                                buff->slen = i;
                                c = buff->data[i];
                                buff->data[i] = (uchar)'\0';
                                if ((ret = cb(parm, p, buff)) < 0)
                                        break;
                                buff->data[i] = c;
                                buff->slen = 0;
                                p += i + 1;
                                i = -1;
                        }
                        i++;
                }
        }

        b_free(buff);
        return ret;
}


int
bs_splitstrcb(struct bStream *buf, const bstring *splitStr, bs_cbfunc cb, void *parm)
{
        bstring *buff;
        int64_t ret;

        if (!cb || !buf || !buf->readFnPtr || INVALID(splitStr))
                RUNTIME_ERROR();
        if (splitStr->slen == 1)
                return bs_splitscb(buf, splitStr, cb, parm);
        if (!(buff = b_fromcstr("")))
                RUNTIME_ERROR();

        if (splitStr->slen == 0) {
                for (uint i = 0; bs_reada(buff, buf, BSSSC_BUFF_LEN) >= 0; ++i) {
                        if ((ret = cb(parm, 0, buff)) < 0) {
                                b_free(buff);
                                return ret;
                        }
                        buff->slen = 0;
                }
                b_free(buff);
                return BSTR_OK;
        } else {
                for (;;) {
                        int64_t p = 0, i = 0;
                        ret = b_instr(buff, 0, splitStr);

                        if (ret >= 0) {
                                bstring t = blk2tbstr(buff->data, ret);
                                i = ret + splitStr->slen;
                                ret = cb(parm, p, &t);
                                if (ret < 0)
                                        break;
                                p += i;
                                b_delete(buff, 0, i);
                        } else {
                                bs_reada(buff, buf, BSSSC_BUFF_LEN);
                                if (bs_eof(buf)) {
                                        ret = cb(parm, p, buff);
                                        if (ret > 0)
                                                ret = 0;
                                        break;
                                }
                        }
                }
        }

        b_free(buff);
        return ret;
}


b_list *
b_list_create(void)
{
        b_list *sl = xmalloc(sizeof(b_list));
        sl->lst    = xmalloc(1 * sizeof(bstring *));
        sl->qty    = 0;
        sl->mlen   = 1;

        return sl;
}

b_list *
b_list_create_alloc(const uint msz)
{
        b_list *sl = xmalloc(sizeof(b_list));
        sl->lst    = xmalloc(msz * sizeof(bstring *));
        sl->qty    = 0;
        sl->mlen   = msz;

        return sl;
}


int
b_list_destroy(b_list *sl)
{
        if (!sl)
                return BSTR_ERR;
        for (uint i = 0; i < sl->qty; ++i)
                if (sl->lst[i])
                        b_destroy(sl->lst[i]);

        sl->qty  = 0;
        sl->mlen = 0;
        free(sl->lst);
        sl->lst  = NULL;
        free(sl);

        return BSTR_OK;
}


int
b_list_alloc(b_list *sl, const uint msz)
{
        bstring **blen;
        uint smsz;
        size_t nsz;
        if (!sl || msz == 0 || !sl->lst || sl->mlen == 0 || sl->qty > sl->mlen)
                RUNTIME_ERROR();
        if (sl->mlen >= msz)
                return BSTR_OK;

        smsz = snapUpSize(msz);
        nsz = ((size_t)smsz) * sizeof(bstring *);

        if (nsz < (size_t)smsz)
                RUNTIME_ERROR();

        blen = realloc(sl->lst, nsz);
        if (!blen) {
                smsz = msz;
                nsz = ((size_t)smsz) * sizeof(bstring *);
                blen = realloc(sl->lst, nsz);
                if (!blen)
                        ALLOCATION_ERROR(BSTR_ERR);
        }

        sl->mlen = smsz;
        sl->lst  = blen;
        return BSTR_OK;
}


int
b_list_allocmin(b_list *sl, uint msz)
{
        bstring **blen;
        size_t nsz;

        if (!sl || msz == 0 || !sl->lst || sl->mlen == 0 || sl->qty > sl->mlen)
                RUNTIME_ERROR();
        if (msz < sl->qty)
                msz = sl->qty;
        if (sl->mlen == msz)
                return BSTR_OK;

        nsz = ((size_t)msz) * sizeof(bstring *);

        if (nsz < (size_t)msz)
                RUNTIME_ERROR();

        blen = realloc(sl->lst, nsz);
        if (!blen)
                RUNTIME_ERROR();

        sl->mlen = msz;
        sl->lst  = blen;

        return BSTR_OK;
}


int
b_splitcb(const bstring *str, uchar splitChar, const uint pos,
          b_cbfunc cb, void *parm)
{
        int64_t ret;
        uint i, p;

        if (!cb || INVALID(str) || pos > str->slen)
                RUNTIME_ERROR();
        p = pos;

        do {
                for (i = p; i < str->slen; ++i)
                        if (str->data[i] == splitChar)
                                break;
                if ((ret = cb(parm, p, i - p)) < 0)
                        return ret;
                p = i + 1;
        } while (p <= str->slen);

        return BSTR_OK;
}


int
b_splitscb(const bstring *str, const bstring *splitStr, const uint pos,
           b_cbfunc cb, void *parm)
{
        struct char_field chrs;
        uint i, p;
        int64_t ret;

        if (!cb || INVALID(str) || INVALID(splitStr) || pos > str->slen)
                RUNTIME_ERROR();
        if (splitStr->slen == 0) {
                if ((ret = cb(parm, 0, str->slen)) > 0)
                        ret = 0;
                return ret;
        }
        if (splitStr->slen == 1)
                return b_splitcb(str, splitStr->data[0], pos, cb, parm);

        build_char_field(&chrs, splitStr);
        p = pos;
        do {
                for (i = p; i < str->slen; ++i)
                        if (testInCharField(&chrs, str->data[i]))
                                break;
                if ((ret = cb(parm, p, i - p)) < 0)
                        return ret;
                p = i + 1;
        } while (p <= str->slen);

        return BSTR_OK;
}


int
b_splitstrcb(const bstring *str, const bstring *splitStr, const uint pos,
             b_cbfunc cb, void *parm)
{
        uint i, p;
        int64_t ret;

        if (!cb || INVALID(str) || INVALID(splitStr) || pos > str->slen)
                RUNTIME_ERROR();

        if (0 == splitStr->slen) {
                for (i = pos; i < str->slen; ++i) {
                        ret = cb(parm, i, 1);
                        if (ret < 0)
                                return ret;
                }
                return BSTR_OK;
        }
        if (splitStr->slen == 1)
                return b_splitcb(str, splitStr->data[0], pos, cb, parm);
        i = p = pos;
        while (i <= str->slen - splitStr->slen) {
                ret = memcmp(splitStr->data, str->data + i, splitStr->slen);
                if (0 == ret) {
                        ret = cb(parm, p, i - p);
                        if (ret < 0)
                                return ret;
                        i += splitStr->slen;
                        p = i;
                } else
                        i++;
        }
        ret = cb(parm, p, str->slen - p);
        if (ret < 0)
                return ret;

        return BSTR_OK;
}


struct gen_b_list {
        bstring *bstr;
        b_list *bl;
};


static int
b_scb(void *parm, const uint ofs, const uint len)
{
        struct gen_b_list *g = (struct gen_b_list *)parm;

        if (g->bl->qty >= g->bl->mlen) {
                uint mlen = g->bl->mlen * 2;
                bstring **tbl;

                while (g->bl->qty >= mlen) {
                        if (mlen < g->bl->mlen)
                                RUNTIME_ERROR();
                        mlen += mlen;
                }

                tbl         = xrealloc(g->bl->lst, sizeof(bstring *) * mlen);
                g->bl->lst  = tbl;
                g->bl->mlen = mlen;
        }

        g->bl->lst[g->bl->qty] = b_midstr(g->bstr, ofs, len);
        g->bl->qty++;

        return BSTR_OK;
}


b_list *
b_split(const bstring *str, uchar splitChar)
{
        struct gen_b_list g;
        if (INVALID(str))
                RETURN_NULL();

        g.bl       = xmalloc(sizeof(b_list));
        g.bl->mlen = 4;
        g.bl->lst  = xmalloc(g.bl->mlen * sizeof(bstring *));
        g.bstr     = (bstring *)str;
        g.bl->qty  = 0;

        if (b_splitcb(str, splitChar, 0, b_scb, &g) < 0) {
                b_list_destroy(g.bl);
                RETURN_NULL();
        }

        return g.bl;
}


b_list *
b_splitstr(const bstring *str, const bstring *splitStr)
{
        struct gen_b_list g;

        if (INVALID(str))
                RETURN_NULL();

        g.bl       = xmalloc(sizeof(b_list));
        g.bl->mlen = 4;
        g.bl->lst  = xmalloc(g.bl->mlen * sizeof(bstring *));
        g.bstr     = (bstring *)str;
        g.bl->qty  = 0;

        if (b_splitstrcb(str, splitStr, 0, b_scb, &g) < 0) {
                b_list_destroy(g.bl);
                RETURN_NULL();
        }

        return g.bl;
}


b_list *
b_splits(const bstring *str, const bstring *splitStr)
{
        struct gen_b_list g;
        if (INVALID(str) || INVALID(splitStr))
                RETURN_NULL();

        g.bl       = xmalloc(sizeof(b_list));
        g.bl->mlen = 4;
        g.bl->lst  = xmalloc(g.bl->mlen * sizeof(bstring *));
        g.bstr     = (bstring *)str;
        g.bl->qty  = 0;

        if (b_splitscb(str, splitStr, 0, b_scb, &g) < 0) {
                b_list_destroy(g.bl);
                RETURN_NULL();
        }

        return g.bl;
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



#if 0
bstring *
b_format(const char *fmt, ...)
{
        va_list arglist;
        if (!fmt)
                RETURN_NULL();

#ifdef HAVE_VASPRINTF
        bstring *buff = xmalloc(sizeof *buff);

        va_start(arglist, fmt);
        uint n = xvasprintf((char **)(&buff->data), fmt, arglist);
        va_end(arglist);

        buff->slen  = n;
        buff->mlen  = n;
        buff->flags = BSTR_STANDARD;
#else
        /*
         * Since the length is not determinable beforehand, a search is
         * performed using the truncating "vsnprintf" call (to avoid buffer
         * overflows) on increasing potential sizes for the output result.
         */
        uint n = (2 * strlen(fmt));
        if (n < START_VSNBUFF)
                n = START_VSNBUFF;
        bstring *buff = b_fromcstr_alloc(n + 2, "");

        if (!buff) {
                n = 1;
                buff = b_fromcstr_alloc(n + 2, "");
                if (!buff)
                        RETURN_NULL();
        }

        for (;;) {
                va_start(arglist, fmt);
                uint r = vsnprintf((char *)buff->data, n + 1, fmt, arglist);
                va_end(arglist);

                buff->data[n] = (uchar)'\0';
                buff->slen    = strlen((char *)buff->data);

                if (buff->slen < n)
                        break;
                if (r > n)
                        n = r;
                else
                        n += n;

                if (BSTR_OK != b_alloc(buff, n + 2)) {
                        b_free(buff);
                        RETURN_NULL();
                }
        }
#endif

        return buff;
}
#endif
#if 0
int
b_format_assign(bstring *bstr, const char *fmt, ...)
{
        va_list arglist;
        int64_t total, ret;
        if (!fmt || INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();

#ifdef HAVE_VASPRINTF
        bstring *buff = xmalloc(sizeof *buff);

        va_start(arglist, fmt);
        total = xvasprintf((char **)(&buff->data), fmt, arglist);
        va_end(arglist);

        buff->slen  = total;
        buff->mlen  = total;
        buff->flags = BSTR_STANDARD;
#else
        /*
         * Since the length is not determinable beforehand, a search is
         * performed using the truncating "vsnprintf" call (to avoid buffer
         * overflows) on increasing potential sizes for the output result.
         */
        total = (2 * strlen(fmt));
        if (total < START_VSNBUFF)
                total = START_VSNBUFF;

        bstring *buff = b_fromcstr_alloc(total + 2, "");

        if (!buff) {
                total = 1;
                buff = b_fromcstr_alloc(total + 2, "");
                if (!buff)
                        RUNTIME_ERROR();
        }

        for (;;) {
                va_start(arglist, fmt);
                ret = vsnprintf((char *)buff->data, total + 1, fmt, arglist);
                va_end(arglist);

                buff->data[total] = (uchar)'\0';
                buff->slen = strlen((char *)buff->data);

                if (buff->slen < total)
                        break;
                if (ret > total)
                        total = ret;
                else
                        total += total;

                if (BSTR_OK != b_alloc(buff, total + 2)) {
                        b_free(buff);
                        RUNTIME_ERROR();
                }
        }
#endif

        ret = b_assign(bstr, buff);
        b_free(buff);

        return ret;
}
#endif


bstring *
b_format(const char *fmt, ...)
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
b_format_assign(bstring *bstr, const char *fmt, ...)
{
        if (!fmt || NO_WRITE(bstr)) 
                RUNTIME_ERROR();
        va_list va;
        va_start(va, fmt);
        bstring *buff = b_vformat(fmt, va);
        va_end(va);

        int ret = b_assign(bstr, buff);
        b_free(buff);
        return ret;
}

int
b_formata(bstring *bstr, const char *fmt, ...)
{
        if (!fmt || INVALID(bstr) || NO_WRITE(bstr))
                RUNTIME_ERROR();
        va_list va;
        va_start(va, fmt);
        bstring *buff = b_vformat(fmt, va);
        va_end(va);

        int ret = b_concat(bstr, buff);
        b_free(buff);
        return ret;
}

bstring *
b_vformat(const char *fmt, va_list arglist)
{
        if (!fmt || !arglist)
                RETURN_NULL();
        uint     total;
        bstring *buff;

#ifdef HAVE_VASPRINTF
        char *tmp   = NULL;
        total       = xvasprintf(&tmp, fmt, arglist);
        /* buff        = b_fromblk(tmp, total + 1); */

        buff        = xmalloc(sizeof *buff);
       /* buff       = (bstring){ .mlen = 0, .slen = 0, .flags = 0, .data = NULL };  */
        /* total       = xvasprintf(&(buff->data), fmt, arglist); */
        buff->data  = (uchar *)tmp;
        buff->slen  = total;
        buff->mlen  = total + 1;
        buff->flags = BSTR_STANDARD;
#else
        /*
         * Without asprintf, because we can't determine the length of the
         * resulting string beforehand, a serch has to be performed using the
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
                uint ret = vsnprintf((char *)buff->data, total + 1, fmt, arglist);

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
b_vformat_assign(bstring *bstr, const char *fmt, va_list arglist)
{
        if (!fmt || NO_WRITE(bstr)) 
                RUNTIME_ERROR();

        bstring *buff = b_vformat(fmt, arglist);
        int      ret  = b_assign(bstr, buff);
        b_free(buff);
        return ret;
}

int
b_vformata(bstring *bstr, const char *fmt, va_list arglist)
{
        if (!fmt || NO_WRITE(bstr)) 
                RUNTIME_ERROR();

        bstring *buff = b_vformat(fmt, arglist);
        int      ret  = b_concat(bstr, buff);
        b_free(buff);
        return ret;
}


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
                        if (bstr->data && bstr->slen > 0)
                                write(fd, bstr->data, bstr->slen);
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
__b_concat_all(const bstring *join, ...)
{
        uint size = 0;
        uint j_size = (join && join->data) ? join->slen : 0;

        va_list va, va2;
        va_start(va, join);
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

        bstring *dest = b_alloc_null(size);
        --dest->slen;
        va_start(va2, join);

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

        dest->data[dest->slen] = '\0';
        return dest;
}


int
__b_append_all(bstring *dest, const bstring *join, ...)
{
        uint size   = dest->slen + 1;
        uint j_size = (join && join->data) ? join->slen : 0;

        va_list va, va2;
        va_start(va, join);
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

        b_alloc(dest, size);
        va_start(va2, join);

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

        dest->data[dest->slen] = '\0';
        return (dest->slen == size) ? BSTR_OK : BSTR_ERR;
}


/*============================================================================*/
/* SOME CRAPPY ADDITIONS! */
/*============================================================================*/


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
}


void
__b_add_to_list(b_list **list, bstring *bstr)
{
        if ((*list)->qty == ((*list)->mlen - 1))
                (*list)->lst = xrealloc((*list)->lst, ((*list)->mlen *= 2) *
                                                          sizeof(*(*list)->lst));
        (*list)->lst[(*list)->qty++] = bstr;
}
