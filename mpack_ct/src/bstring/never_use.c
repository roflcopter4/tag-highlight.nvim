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

#define upcase(c)   (toupper((uchar)(c)))
#define downcase(c) (tolower((uchar)(c)))
#define wspace(c)   (isspace((uchar)(c)))


//=============================================================================
// strstr replacements and friends
//============================================================================


int64_t
b_instr(const bstring *haystack, const uint pos, const bstring *needle)
{
        if (INVALID(haystack) || INVALID(needle))
                RUNTIME_ERROR();
        if (haystack->slen == pos)
                return (needle->slen == 0) ? (int64_t)pos : BSTR_ERR;
        if (haystack->slen < pos)
                RUNTIME_ERROR();
        if (needle->slen == 0)
                return pos;

        /* No space to find such a string? */
        if ((haystack->slen - needle->slen + 1) <= pos)
                RUNTIME_ERROR();

        /* An obvious alias case */
        if (haystack->data == needle->data && pos == 0)
                return 0;

        uint i  = pos;

        /* Peel off the needle->slen == 1 case */
        if (needle->slen == 1) {
                for (uint lf = (haystack->slen - needle->slen + 1); i < lf; ++i)
                        if (needle->data[0] == haystack->data[i])
                                return i;
                return (-1);
        }

        uchar ch = needle->data[0];
        uint  x  = 0;
        uint  ii = (-1);
        uint  lf = (haystack->slen - 1);

        if (i < lf) {
                do {
                        /* Unrolled current character test */
                        if (ch != haystack->data[i]) {
                                if (ch != haystack->data[i+1]) {
                                        i += 2;
                                        continue;
                                }
                                ++i;
                        }

                        /* Take note if this is the start of a potential match */
                        if (0 == x)
                                ii = i;

                        /* Shift the test character down by one */
                        ++x;
                        ++i;

                        /* If this isn't past the last character continue */
                        if (x < needle->slen) {
                                ch = needle->data[x];
                                continue;
                        }
                N0:
                        /* If no characters mismatched, then we matched */
                        if (i == ii + x)
                                return ii;

                        /* Shift back to the beginning */
                        i -= x;
                        x = 0;
                        ch = needle->data[0];
                } while (i < lf);
        }

        /* Deal with last case if unrolling caused a misalignment */
        if ((i == lf) && (needle->slen == x + 1) && (ch == haystack->data[i]))
                goto N0;

        return (-1);
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


//=============================================================================
// Misc
//============================================================================


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
                        memmove((char *)(bstr->data + pos),
                                    (char *)(bstr->data + pos + len),
                                    bstr->slen - (pos + len));
                        bstr->slen -= len;
                }
                bstr->data[bstr->slen] = (uchar)'\0';
        }

        return BSTR_OK;
}


//=============================================================================
// Find/replace and friends
//============================================================================


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
                memmove((char *)(b0->data + pos), (char *)aux->data, aux->slen);
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
                memmove(b1->data + blen, b1->data + pos, d - blen);
                b1->slen = d;
        }

        memmove(b1->data + pos, aux->data, aux->slen);
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
