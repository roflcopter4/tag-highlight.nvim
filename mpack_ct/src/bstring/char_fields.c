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


/*
 * Convert a bstring to charField
 */
/*PRIVATE*/ int
build_char_field(struct char_field *cf, const bstring *bstr)
{
        if (IS_NULL(bstr) || bstr->slen <= 0)
                RUNTIME_ERROR();
        memset(cf->content, 0, sizeof(struct char_field));
        for (uint i = 0; i < bstr->slen; ++i)
                setInCharField(cf, bstr->data[i]);

        return BSTR_OK;
}


/*PRIVATE*/ void
invert_char_field(struct char_field *cf)
{
        for (uint i = 0; i < CFCLEN; ++i)
                cf->content[i] = ~cf->content[i];
}


/*
 * Inner engine for binchr
 */
/*PRIVATE*/ int
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


/*
 * Inner engine for binchrr
 */
/*PRIVATE*/ int
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
