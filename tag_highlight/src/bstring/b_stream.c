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
        xfree(buf);

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
                                bstring t = b_static_fromblk((buff->data + i + 1), buff->slen - (i + 1));
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
                                bstring t = b_static_fromblk(buff->data, ret);
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
