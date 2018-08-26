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

#include "private.h"
#include <assert.h>
#include <inttypes.h>

#include "bstring.h"

/* 
 * This file was broken off from the main bstrlib.c file in a forlorn effort to
 * keep things a little neater.
 */


/*============================================================================*/
/* General operations */
/*============================================================================*/

b_list *
b_list_create(void)
{
        b_list *sl = xmalloc(sizeof(b_list));
        sl->lst    = calloc(4, sizeof(bstring *));
        sl->qty    = 0;
        sl->mlen   = 1;
        if (!sl->lst)
                FATAL_ERROR("calloc failed");

        return sl;
}

b_list *
b_list_create_alloc(const uint msz)
{
        const int safesize = (msz == 0) ? 1 : msz;

        b_list *sl = xmalloc(sizeof(b_list));
        sl->lst    = xmalloc(safesize * sizeof(bstring *));
        sl->qty    = 0;
        sl->mlen   = safesize;
        sl->lst[0] = NULL;

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

        blen = xrealloc(sl->lst, nsz);
#if 0
        blen = realloc(sl->lst, nsz);
        if (!blen) {
                smsz = msz;
                nsz = ((size_t)smsz) * sizeof(bstring *);
                blen = realloc(sl->lst, nsz);
                if (!blen)
                        ALLOCATION_ERROR(BSTR_ERR);
        }
#endif

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


#if 0
/*============================================================================*/
/* Splitting */
/*============================================================================*/


/*PRIVATE*/ int
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
#endif


bstring *
b_join(const b_list *bl, const bstring *sep)
{
        if (!bl || INVALID(sep))
                RETURN_NULL();
        int64_t total = 1;

        for (uint i = 0; i < bl->qty; ++i) {
                const uint v = bl->lst[i]->slen;
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
                const uint v = bl->lst[i]->slen;
                memcpy(bstr->data + total, bl->lst[i]->data, v);
                total += v;
        }
        bstr->data[total] = (uchar)'\0';

        return bstr;
}


bstring *
b_join_quote(const b_list *bl, const bstring *sep, const int ch)
{
        if (!bl || INVALID(sep) || !ch)
                RETURN_NULL();
        uint       i;
        const uint sepsize = (sep) ? sep->slen : 0;
        int64_t    total   = 1;

        B_LIST_FOREACH(bl, bstr, i)
                total += bstr->slen + sepsize + 2u;
        if (total > UINT32_MAX)
                RETURN_NULL();

        bstring *bstr = xmalloc(sizeof(bstring));
        bstr->mlen    = total - (2 * sepsize);
        bstr->slen    = 0;
        bstr->data    = xmalloc(total);
        bstr->flags   = BSTR_STANDARD;

        B_LIST_FOREACH(bl, cur, i) {
                if (sep && i > 0) {
                        memcpy(bstr->data + bstr->slen, sep->data, sep->slen);
                        bstr->slen += sep->slen;
                }

                bstr->data[bstr->slen++] = (uchar)ch;
                memcpy((bstr->data + bstr->slen), cur->data, cur->slen);
                bstr->slen += cur->slen;
                bstr->data[bstr->slen++] = (uchar)ch;
        }

        bstr->data[bstr->slen] = (uchar)'\0';

        if (bstr->slen != bstr->mlen)
                warnx("Failed operation, %u is smaller than %"PRId64"!", bstr->slen, total);

        return bstr;
}
