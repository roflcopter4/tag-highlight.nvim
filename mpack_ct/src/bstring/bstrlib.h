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
 * CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * GNU General Public License Version 2 (the "GPL").
 */

/**
 * \file
 * \brief C implementaion of bstring functions
 *
 * This file is the header file for the core module for implementing the
 * bstring functions.
 */

#ifndef BSTRLIB_H
#define BSTRLIB_H

#if __GNUC__ >= 4
#  define BSTR_PUBLIC  __attribute__((visibility("default")))
#  define BSTR_PRIVATE __attribute__((visibility("hidden")))
#  define INLINE       __attribute__((always_inline)) static inline
#  define PURE         __attribute__((pure))
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#else
#  define BSTR_PUBLIC
#  define BSTR_PRIVATE
#  define INLINE static inline
#  define PURE
#endif

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#  define BSTR_PRINTF(format, argument) __attribute__((__format__(__printf__, format, argument)))
#  define BSTR_UNUSED __attribute__((__unused__))
#else
#  define BSTR_PRINTF(format, argument)
#  define BSTR_UNUSED
#endif

#ifndef __GNUC__
#  define __attribute__(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char uchar;

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BSTR_ERR (-1)
#define BSTR_OK (0)
#define BSTR_BS_BUFF_LENGTH_GET (0)

#define BSTR_WRITE_ALLOWED 0x01u
#define BSTR_FREEABLE      0x02u
#define BSTR_DATA_FREEABLE 0x04u
#define BSTR_LIST_END      0x08u

#if defined(__GNUC__)
#  define PACK(...) __VA_ARGS__ __attribute__((__packed__))
#elif defined(_MSC_VER)
#  define PACK(...) __pragma( pack(push, 1) ) __VA_ARGS__ __pragma( pack(pop) )
#else
#  define PACK(...) __VA_ARGS__
#endif


#pragma pack(push, 1)
struct tagbstring {
        unsigned slen;
        unsigned mlen;
        uchar *data;
        uint8_t flags;
};
#pragma pack(pop)


struct bstring_list {
        unsigned qty;
        unsigned mlen;
        struct tagbstring **lst;
};

typedef struct tagbstring   bstring;
typedef struct bstring_list b_list;


/*============================================================================*/
/* Copy functions */


#define cstr2bstr b_fromcstr

/**
 * Take a standard C library style '\0' terminated char buffer and generate a
 * bstring with the same contents as the char buffer.
 *
 * If an error occurs #NULL is returned.
 *
 * \code
 * bstring *b = b_fromcstr("Hello");
 * if(!b)
 *         fprintf(stderr, "Out of memory");
 * else
 *         puts((char *)b->data);
 * \endcode
 */
BSTR_PUBLIC bstring *b_fromcstr(const char *str);

/**
 * Create a bstring which contains the contents of the '\0' terminated char
 * *buffer str.
 *
 * The memory buffer backing the bstring is at least mlen characters in length.
 * If an error occurs NULL is returned.
 *
 * \code
 * bstring *b = b_fromcstr_alloc(64, someCstr);
 * if(b)
 *         b->data[63] = 'x';
 * \endcode
 *
 * The idea is that this will set the 64th character of b to 'x' if it is at
 * least 64 characters long otherwise do nothing. And we know this is well
 * defined so long as b was successfully created, since it will have been
 * allocated with at least 64 characters.
 */
BSTR_PUBLIC bstring *b_fromcstr_alloc(unsigned mlen, const char *str);


/**
 * Create an empty bstring of size len. The string pointed to by bstr->data will
 * be uninitialized with the exception of the very first byte, which is set to
 * NUL just in case. The size of the string is 0, so this NUL will be
 * overwritten by any of the standard bstring api functions (and even the
 * standard c library functions).
 */
BSTR_PUBLIC bstring *b_alloc_null(unsigned len);

/**
 * Create a bstring whose contents are described by the contiguous buffer
 * pointing to by blk with a length of len bytes.
 *
 * Note that this function creates a copy of the data in blk, rather than simply
 * referencing it. Compare with the blk2tbstr macro. If an error occurs NULL is
 * returned.
 */
BSTR_PUBLIC bstring *b_blk2bstr(const void *blk, unsigned len);

#define b_fromblk(BLK_, LENGTH_) b_blk2bstr((BLK_), (LENGTH_))

/**
 * Create a '\0' terminated char buffer which contains the contents of the
 * bstring *s, except that any contained '\0' characters are converted to the
 * character in z.
 *
 * This returned value should be freed with b_cstrfree(), by the caller. If an
 * error occurs NULL is returned.
 */
BSTR_PUBLIC char *b_bstr2cstr(const bstring *bstr, char nul);

/**
 * Frees a C-string generated by bstr2cstr().
 *
 * This is normally unnecessary since it just wraps a call to free(), however,
 * if malloc() and free() have been redefined as a macros within the bstrlib
 * module (via macros in the memdbg.h backdoor) with some difference in
 * behaviour from the std library functions, then this allows a correct way of
 * freeing the memory that allows higher level code to be independent from these
 * macro redefinitions.
 */
BSTR_PUBLIC int b_cstrfree(char * buf);

/**
 * Make a copy of the passed in bstring.
 *
 * The copied bstring is returned if there is no error, otherwise NULL is returned.
 */
BSTR_PUBLIC bstring *b_strcpy(const bstring *bstr);

/**
 * Overwrite the bstring *a with the contents of bstring *b.
 *
 * Note that the bstring *a must be a well defined and writable bstring. If an
 * error occurs BSTR_ERR is returned and a is not overwritten.
 */
BSTR_PUBLIC int b_assign(bstring *a, const bstring *bstr);

/**
 * Overwrite the bstring *a with the middle of contents of bstring *b
 * starting from position left and running for a length len.
 *
 * left and len are clamped to the ends of b as with the function bmidstr.
 * Note that the bstring *a must be a well defined and writable bstring. If
 * an error occurs BSTR_ERR is returned and a is not overwritten.
 */
BSTR_PUBLIC int b_assign_midstr(bstring *a, const bstring *bstr,
                                int64_t left, unsigned len);

/**
 * Overwrite the string a with the contents of char * string str.
 *
 * Note that the bstring a must be a well defined and writable bstring. If
 * an error occurs BSTR_ERR is returned and a may be partially overwritten.
 */
BSTR_PUBLIC int b_assign_cstr(bstring *a, const char *str);

/**
 * Overwrite the bstring a with the middle of contents of bstring b
 * starting from position left and running for a length len.
 *
 * left and len are clamped to the ends of b as with the function bmidstr.
 * Note that the bstring a must be a well defined and writable bstring. If
 * an error occurs BSTR_ERR is returned and a is not overwritten.
 */
BSTR_PUBLIC int b_assign_blk(bstring *a, const void *s, unsigned len);


/*============================================================================*/
/* Destroy function */


/**
 * Deallocate the bstring passed.
 *
 * Passing NULL in as a parameter will have no effect. Note that both the
 * header and the data portion of the bstring will be freed. No other
 * bstring function which modifies one of its parameters will free or
 * reallocate the header. Because of this, in general, bdestroy cannot be
 * called on any declared bstring even if it is not write
 * protected. A bstring which is write protected cannot be destroyed via the
 * bdestroy call. Any attempt to do so will result in no action taken, and
 * BSTR_ERR will be returned.
 */
BSTR_PUBLIC int b_free(bstring *bstr);

INLINE int __b_destroy(bstring **bstr)
{
        int ret = b_free(*bstr);
        if (ret == BSTR_OK)
                *bstr = NULL;
        return ret;
}

#define b_destroy(BSTR) (__b_destroy(&(BSTR)))


/* Space allocation hinting functions */

/**
 * Increase the allocated memory backing the data buffer for the bstring b
 * to a length of at least length.
 *
 * If the memory backing the bstring b is already large enough, not action is
 * performed. This has no effect on the bstring b that is visible to the
 * bstring API. Usually this function will only be used when a minimum buffer
 * size is required coupled with a direct access to the ->data member of the
 * bstring structure.
 *
 * Be warned that like any other bstring function, the bstring must be well
 * defined upon lst to this function, i.e., doing something like:
 *
 * \code
 * b->slen *= 2;
 * b_alloc(b, b->slen);
 * \endcode
 *
 * is invalid, and should be implemented as:
 *
 * \code
 * int t;
 * if (BSTR_OK == balloc (b, t = (b->slen * 2))) {
 *     b->slen = t;
 * }
 * \endcode
 *
 * This function will return with BSTR_ERR if b is not detected as a valid
 * bstring or length is not greater than 0, otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_alloc(bstring *bstr, unsigned olen);

/**
 * Change the amount of memory backing the bstring b to at least length.
 *
 * This operation will never truncate the bstring data including the extra
 * terminating '\0' and thus will not decrease the length to less than
 * b->slen + 1. Note that repeated use of this function may cause
 * performance problems (realloc may be called on the bstring more than the
 * O(log(INT_MAX)) times). This function will return with BSTR_ERR if b is not
 * detected as a valid bstring or length is not greater than 0, otherwise
 * BSTR_OK is returned.
 *
 * So for example:
 *
 * \code
 * if (BSTR_OK == ballocmin (b, 64)) {
 *     b->data[63] = 'x';
 * }
 * \endcode
 *
 * The idea is that this will set the 64th character of b to 'x' if it is at
 * least 64 characters long otherwise do nothing. And we know this is well
 * defined so long as the ballocmin call was successfully, since it will
 * ensure that b has been allocated with at least 64 characters.
 */
BSTR_PUBLIC int b_allocmin(bstring *bstr, unsigned len);


/*============================================================================*/
/* Substring extraction */


/**
 * Create a bstring which is the substring of b starting from position left
 * and running for a length len (clamped by the end of the bstring b).
 *
 * If there was no error, the value of this constructed bstring is returned
 * otherwise NULL is returned.
 */
BSTR_PUBLIC bstring *b_midstr(const bstring *bstr, int64_t left, unsigned len);

/*Various standard manipulations */

/**
 * Concatenate the bstring b1 to the end of bstring b0.
 *
 * The value BSTR_OK is returned if the operation is successful, otherwise
 * BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_concat(bstring *b0, const bstring *b1);

/**
 * Concatenate the character c to the end of bstring b.
 *
 * The value BSTR_OK is returned if the operation is successful, otherwise
 * BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_conchar(bstring *bstr, char c);

/**
 * Concatenate the char * string s to the end of bstring b.
 *
 * The value BSTR_OK is returned if the operation is successful, otherwise
 * BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_catcstr(bstring *bstr, const char * buf);

/**
 * Concatenate a fixed length buffer (s, len) to the end of bstring b.
 *
 * The value BSTR_OK is returned if the operation is successful, otherwise
 * BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_catblk(bstring *bstr, const void * buf, unsigned len);

/**
 * Inserts the bstring s2 into s1 at position pos.
 *
 * If the position pos is past the end of s1, then the character "fill" is
 * appended as necessary to make up the gap between the end of s1 and pos. The
 * value BSTR_OK is returned if the operation is successful, otherwise BSTR_ERR
 * is returned.
 */
BSTR_PUBLIC int b_insert(bstring *b1, unsigned pos, const bstring *b2, uchar fill);

/**
 * Inserts the character fill repeatedly into s1 at position pos for a
 * length len.
 *
 * If the position pos is past the end of s1, then the character "fill" is
 * appended as necessary to make up the gap between the end of s1 and the
 * position pos + len (exclusive). The value BSTR_OK is returned if the
 * operation is successful, otherwise BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_insertch(bstring *bstr, unsigned pos, unsigned len, uchar fill);

/**
 * Replace a section of a bstring from pos for a length len with the bstring b2.
 *
 * If the position pos is past the end of b1 then the character "fill" is
 * appended as necessary to make up the gap between the end of b1 and pos.
 */
BSTR_PUBLIC int b_replace(bstring *b1, unsigned pos, unsigned len,
                          const bstring *b2, uchar fill);

/**
 * Removes characters from pos to pos+len-1 and shifts the tail of the bstring
 * starting from pos+len to pos.
 *
 * len must be positive for this call to have any effect. The section of the
 * bstring described by (pos, len) is clamped to boundaries of the bstring b.
 * The value BSTR_OK is returned if the operation is successful, otherwise
 * BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_delete(bstring *bstr, int64_t pos, unsigned len);

/**
 * Overwrite the bstring b0 starting at position pos with the bstring b1.
 *
 * If the position pos is past the end of b0, then the character "fill" is
 * appended as necessary to make up the gap between the end of b0 and pos.  If
 * b1 is NULL, it behaves as if it were a 0-length bstring. The value BSTR_OK is
 * returned if the operation is successful, otherwise BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_setstr(bstring *b0, unsigned pos, const bstring *b1, uchar fill);

/**
 * Truncate the bstring to at most n characters.
 *
 * This function will return with BSTR_ERR if b is not detected as a valid
 * bstring or n is less than 0, otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_trunc(bstring *bstr, unsigned n);


/*============================================================================*/
/* Scan/search functions */


/**
 * Compare two bstrings without differentiating between case.
 *
 * The return value is the difference of the values of the characters where the
 * two bstrings first differ, otherwise 0 is returned indicating that the
 * bstrings are equal. If the lengths are different, then a difference from 0
 * is given, but if the first extra character is '\0', then it is taken to be
 * the value UCHAR_MAX + 1.
 */
BSTR_PUBLIC int b_stricmp(const bstring *b0, const bstring *b1);

/**
 * Compare two bstrings without differentiating between case for at most n
 * characters.
 *
 * If the position where the two bstrings first differ is before the nth
 * position, the return value is the difference of the values of the
 * characters, otherwise 0 is returned. If the lengths are different and less
 * than n characters, then a difference from 0 is given, but if the first extra
 * character is '\0', then it is taken to be the value UCHAR_MAX + 1.
 */
BSTR_PUBLIC int b_strnicmp(const bstring *b0, const bstring *b1, unsigned n);

/**
 * Compare two bstrings for equality without differentiating between case.
 *
 * If the bstrings differ other than in case, 0 is returned, if the bstrings
 * are the same, 1 is returned, if there is an error, -1 is returned. If
 * the length of the bstrings are different, this function is O(1). '\0'
 * termination characters are not treated in any special way.
 */
BSTR_PUBLIC int b_iseq_caseless(const bstring *b0, const bstring *b1);

/**
 * Compare beginning of bstring b0 with a block of memory of length len without
 * differentiating between case for equality.
 *
 * If the beginning of b0 differs from the memory block other than in case (or
 * if b0 is too short), 0 is returned, if the bstrings are the same, 1 is
 * returned, if there is an error, -1 is returned.
 */
BSTR_PUBLIC int b_is_stem_eq_caseless_blk(const bstring *b0, const void *blk, unsigned len);

/**
 * Compare the bstring b0 and b1 for equality.
 *
 * If the bstrings differ, 0 is returned, if the bstrings are the same, 1 is
 * returned, if there is an error, -1 is returned. If the length of the bstrings
 * are different, this function has O(1) complexity. Contained '\0' characters
 * are not treated as a termination character.
 *
 * Note that the semantics of biseq are not completely compatible with bstrcmp
 * because of its different treatment of the '\0' character.
 */
BSTR_PUBLIC int b_iseq(const bstring *b0, const bstring *b1);

/**
 * Compare beginning of bstring b0 with a block of memory of length len for equality.
 *
 * If the beginning of b0 differs from the memory block (or if b0 is too short),
 * 0 is returned, if the bstrings are the same, 1 is returned, if there is an
 * error, -1 is returned.
 */
BSTR_PUBLIC int b_is_stem_eq_blk(const bstring *b0, const void *blk, unsigned len);

/**
 * Compare the bstring b and char * string s.
 *
 * The C string s must be '\0' terminated at exactly the length of the bstring
 * b, and the contents between the two must be identical with the bstring b with
 * no '\0' characters for the two contents to be considered equal. This is
 * equivalent to the condition that their current contents will be always be
 * equal when comparing them in the same format after converting one or the
 * other. If they are equal 1 is returned, if they are unequal 0 is returned and
 * if there is a detectable error BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_iseq_cstr(const bstring *bstr, const char *buf);

/**
 * Compare the bstring b and char * string s.
 *
 * The C string s must be '\0' terminated at exactly the length of the bstring
 * b, and the contents between the two must be identical except for case with
 * the bstring b with no '\0' characters for the two contents to be considered
 * equal. This is equivalent to the condition that their current contents will
 * be always be equal ignoring case when comparing them in the same format after
 * converting one or the other. If they are equal, except for case, 1 is
 * returned, if they are unequal regardless of case 0 is returned and if there
 * is a detectable error BSTR_ERR is returned.
 */
BSTR_PUBLIC int b_iseq_cstr_caseless(const bstring *bstr, const char *buf);

/**
 * Compare the bstrings b0 and b1 for ordering.
 *
 * If there is an error, SHRT_MIN is returned, otherwise a value less than or
 * greater than zero, indicating that the bstring pointed to by b0 is
 * lexicographically less than or greater than the bstring pointed to by b1 is
 * returned. If the bstring lengths are unequal but the characters up until the
 * length of the shorter are equal then a value less than, or greater than zero,
 * indicating that the bstring pointed to by b0 is shorter or longer than the
 * bstring pointed to by b1 is returned. 0 is returned if and only if the two
 * bstrings are the same. If the length of the bstrings are different, this
 * function is O(n). Like its standard C library counter part, the comparison
 * does not proceed past any '\0' termination characters encountered.
 *
 * The seemingly odd error return value, merely provides slightly more
 * granularity than the undefined situation given in the C library function
 * strcmp. The function otherwise behaves very much like strcmp().
 *
 * Note that the semantics of bstrcmp are not completely compatible with biseq
 * because of its different treatment of the '\0' termination character.
 */
BSTR_PUBLIC int b_strcmp(const bstring *b0, const bstring *b1);

/**
 * Compare the bstrings b0 and b1 for ordering for at most n characters.
 *
 * If there is an error, SHRT_MIN is returned, otherwise a value is returned as
 * if b0 and b1 were first truncated to at most n characters then bstrcmp was
 * called with these new bstrings are paremeters. If the length of the bstrings
 * are different, this function is O(n). Like its standard C library counter
 * part, the comparison does not proceed past any '\0' termination characters
 * encountered.
 *
 * The seemingly odd error return value, merely provides slightly more
 * granularity than the undefined situation given in the C library function
 * strncmp. The function otherwise behaves very much like strncmp().
 */
BSTR_PUBLIC int b_strncmp(const bstring *b0, const bstring *b1, unsigned n);

/**
 * Search for the bstring s2 in s1 starting at position pos and looking in a
 * forward (increasing) direction.
 *
 * If it is found then it returns with the first position after pos where it is
 * found, otherwise it returns BSTR_ERR.  The algorithm used is brute force;
 * O(m*n).
 */
BSTR_PUBLIC int64_t b_instr(const bstring *b1, unsigned pos, const bstring *b2);

/**
 * Search for the bstring s2 in s1 starting at position pos and looking in a
 * backward (decreasing) direction.
 *
 * If it is found then it returns with the first position after pos where it is
 * found, otherwise return BSTR_ERR.  Note that the current position at pos is
 * tested as well -- so to be disjoint from a previous forward search it is
 * recommended that the position be backed up (decremented) by one position.
 * The algorithm used is brute force; O(m*n).
 */
BSTR_PUBLIC int64_t b_instrr(const bstring *b1, unsigned pos, const bstring *b2);

/**
 * Search for the bstring s2 in s1 starting at position pos and looking in a
 * forward (increasing) direction but without regard to case.
 *
 * If it is found then it returns with the first position after pos where it is
 * found, otherwise it returns BSTR_ERR. The algorithm used is brute force;
 * O(m*n).
 */
BSTR_PUBLIC int64_t b_instr_caseless(const bstring *b1, unsigned pos, const bstring *b2);

/**
 * Search for the bstring s2 in s1 starting at position pos and looking in a
 * backward (decreasing) direction but without regard to case.
 *
 * If it is found then it returns with the first position after pos where it is
 * found, otherwise return BSTR_ERR. Note that the current position at pos is
 * tested as well -- so to be disjoint from a previous forward search it is
 * recommended that the position be backed up (decremented) by one position.
 * The algorithm used is brute force; O(m*n).
 */
BSTR_PUBLIC int64_t b_instrr_caseless(const bstring *b1, unsigned pos, const bstring *b2);

/**
 * Search for the character c in b forwards from the position pos
 * (inclusive).
 *
 * Returns the position of the found character or BSTR_ERR if it is not found.
 */
BSTR_PUBLIC int64_t b_strchrp(const bstring *bstr, int ch, unsigned pos);

/**
 * Search for the character c in b backwards from the position pos in bstring
 * (inclusive).
 *
 * Returns the position of the found character or BSTR_ERR if it is not found.
 */
BSTR_PUBLIC int64_t b_strrchrp(const bstring *bstr, int ch, unsigned pos);

/**
 * Search for the character c in the bstring b forwards from the start of
 * the bstring.
 *
 * Returns the position of the found character or BSTR_ERR if it is not found.
 */
#define b_strchr(b, c) b_strchrp((b), (c), 0)

/**
 * Search for the character c in the bstring b backwards from the end of the
 * bstring.
 *
 * Returns the position of the found character or BSTR_ERR if it is not found.
 */
#define b_strrchr(b, c) b_strrchrp((b), (c), b_length(b) - 1)

/**
 * Search for the first position in b0 starting from pos or after, in which
 * one of the characters in b1 is found.
 *
 * This function has an execution time of O(b0->slen + b1->slen). If such a
 * position does not exist in b0, then BSTR_ERR is returned.
 */
BSTR_PUBLIC int64_t b_inchr(const bstring *b0, unsigned pos, const bstring *b1);

/**
 * Search for the last position in b0 no greater than pos, in which one of
 * the characters in b1 is found.
 *
 * This function has an execution time of O(b0->slen + b1->slen). If such a
 * position does not exist in b0, then BSTR_ERR is returned.
 */
BSTR_PUBLIC int64_t b_inchrr(const bstring *b0, unsigned pos, const bstring *b1);

/**
 * Search for the first position in b0 starting from pos or after, in which
 * none of the characters in b1 is found and return it.
 *
 * This function has an execution time of O(b0->slen + b1->slen). If such a
 * position does not exist in b0, then BSTR_ERR is returned.
 */
BSTR_PUBLIC int64_t b_ninchr(const bstring *b0, unsigned pos, const bstring *b1);

/**
 * Search for the last position in b0 no greater than pos, in which none of
 * the characters in b1 is found and return it.
 *
 * This function has an execution time of O(b0->slen + b1->slen). If such a
 * position does not exist in b0, then BSTR_ERR is returned.
 */
BSTR_PUBLIC int64_t b_ninchrr(const bstring *b0, unsigned pos, const bstring *b1);

/**
 * Replace all occurrences of the find substring with a replace bstring after a
 * given position in the bstring b.
 *
 * The find bstring must have a length > 0 otherwise BSTR_ERR is returned. This
 * function does not perform recursive per character replacement; that is to say
 * successive searches resume at the position after the last replace.
 *
 * So for example:
 *
 * \code
 * b_findreplace(a0 = b_fromcstr("aabaAb"),
 *               a1 = b_fromcstr("a"),
 *               a2 = b_fromcstr("aa"), 0);
 * \endcode
 *
 * Should result in changing a0 to "aaaabaaAb".
 *
 * This function performs exactly (b->slen - position) bstring comparisons, and
 * data movement is bounded above by character volume equivalent to size of the
 * output bstring.
 */
BSTR_PUBLIC int64_t b_findreplace(bstring *bstr, const bstring *find,
                                  const bstring *repl, unsigned pos);

/**
 * Replace all occurrences of the find substring, ignoring case, with a replace
 * bstring after a given position in the bstring b.
 *
 * The find bstring must have a length > 0 otherwise BSTR_ERR is returned. This
 * function does not perform recursive per character replacement; that is to say
 * successive searches resume at the position after the last replace.
 *
 * So for example:
 *
 * \code
 * b_findreplacecaseless(a0 = b_fromcstr("AAbaAb"), a1 = b_fromcstr("a"),
 * a2 = b_fromcstr("aa"), 0);
 * \endcode
 *
 * Should result in changing a0 to "aaaabaaaab".
 *
 * This function performs exactly (b->slen - position) bstring comparisons, and
 * data movement is bounded above by character volume equivalent to size of the
 * output bstring.
 */
BSTR_PUBLIC int64_t b_findreplace_caseless(bstring *bstr, const bstring *find,
                                           const bstring *repl, unsigned pos);


/*============================================================================*/
/* List of string container functions */


/**
 * Create an empty b_list.
 *
 * The b_list output structure is declared as follows:
 *
 * \code
 * b_list {
 *     int qty, mlen;
 *     bstring * *lst;
 * };
 * \endcode
 *
 * The lst field actually is an array with qty number entries. The mlen record
 * counts the maximum number of bstring's for which there is memory in the lst
 * record.
 *
 * The Bstrlib API does *NOT* include a comprehensive set of functions for full
 * management of b_list in an abstracted way. The reason for this is because
 * aliasing semantics of the list are best left to the user of this function,
 * and performance varies wildly depending on the assumptions made.
 */
BSTR_PUBLIC b_list *b_list_create(void);

/**
 * Create a b_list with maximum size msz, as though b_list_create and
 * b_list_alloc had been called in succession.
 */
BSTR_PUBLIC b_list *b_list_create_alloc(unsigned msz);

/**
 * Destroy a b_list structure that was returned by the bsplit function. Note
 * that this will destroy each bstring in the ->lst array as well. See
 * b_list_create() above for structure of b_list.
 */
BSTR_PUBLIC int b_list_destroy(b_list *sl);

/**
 * Ensure that there is memory for at least msz number of entries for the
 * list.
 */
BSTR_PUBLIC int b_list_alloc(b_list *sl, unsigned msz);

/**
 * Try to allocate the minimum amount of memory for the list to include at
 * least msz entries or sl->qty whichever is greater.
 */
BSTR_PUBLIC int b_list_allocmin(b_list *sl, unsigned msz);


/*============================================================================*/
/* String split and join functions */

typedef int (*b_cbfunc)(void *parm, unsigned ofs, unsigned len);

/**
 * Create an array of substrings from str divided by the character splitChar.
 *
 * Successive occurrences of the splitChar will be divided by empty bstring
 * entries, following the semantics from the Python programming language. To
 * reclaim the memory from this output structure, b_list_destroy() should be
 * called. See b_list_create() above for structure of b_list.
 */
BSTR_PUBLIC b_list *b_split(const bstring *str, uchar splitChar);

/**
 * Create an array of sequential substrings from str divided by any
 * character contained in splitStr.
 *
 * An empty splitStr causes a single lst b_list containing a copy of str to
 * be returned. See b_list_create() above for structure of b_list.
 */
BSTR_PUBLIC b_list *b_splits(const bstring *str, const bstring *splitStr);

/**
 * Create an array of sequential substrings from str divided by the entire
 * substring splitStr.
 *
 * An empty splitStr causes a single lst b_list containing a copy of str to
 * be returned. See b_list_create() above for structure of b_list.
 */
BSTR_PUBLIC b_list *b_splitstr(const bstring *str, const bstring *splitStr);

/**
 * Join the entries of a b_list into one bstring by sequentially concatenating
 * them with the sep bstring in between.
 *
 * If sep is NULL, it is treated as if it were the empty bstring. Note that:
 *
 * \code
 * bjoin (l = bsplit (b, s->data[0]), s);
 * \endcode
 *
 * should result in a copy of b, if s->slen is 1. If there is an error NULL is
 * returned, otherwise a bstring with the correct result is returned. See
 * b_list_create() above for structure of b_list.
 */
BSTR_PUBLIC bstring *b_join(const b_list *bl, const bstring *sep);

/**
 * Iterate the set of disjoint sequential substrings over str starting at
 * position pos divided by the character splitChar.
 *
 * The parm passed to bsplitcb is passed on to cb. If the function cb returns a
 * value < 0, then further iterating is halted and this value is returned by
 * bsplitcb.
 *
 * Note: Non-destructive modification of str from within the cb function
 * while performing this split is not undefined. bsplitcb behaves in
 * sequential lock step with calls to cb. I.e., after returning from a cb
 * that return a non-negative integer, bsplitcb continues from the position
 * 1 character after the last detected split character and it will halt
 * immediately if the length of str falls below this point. However, if the
 * cb function destroys str, then it *must* return with a negative value,
 * otherwise bsplitcb will continue in an undefined manner.
 *
 * This function is provided as an incremental alternative to bsplit that is
 * abortable and which does not impose additional memory allocation.
 */
BSTR_PUBLIC int b_splitcb(const bstring *str, uchar splitChar, unsigned pos,
                          b_cbfunc cb, void *parm);

/**
 * Iterate the set of disjoint sequential substrings over str starting at
 * position pos divided by any of the characters in splitStr.
 *
 * An empty splitStr causes the whole str to be iterated once. The parm passed
 * to bsplitcb is passed on to cb. If the function cb returns a value < 0, then
 * further iterating is halted and this value is returned by bsplitcb.
 *
 * Note: Non-destructive modification of str from within the cb function
 * while performing this split is not undefined. bsplitscb behaves in
 * sequential lock step with calls to cb. I.e., after returning from a cb
 * that return a non-negative integer, bsplitscb continues from the position
 * 1 character after the last detected split character and it will halt
 * immediately if the length of str falls below this point. However, if the
 * cb function destroys str, then it *must* return with a negative value,
 * otherwise bsplitscb will continue in an undefined manner.
 *
 * This function is provided as an incremental alternative to bsplits that
 * is abortable and which does not impose additional memory allocation.
 */
BSTR_PUBLIC int b_splitscb(const bstring *str, const bstring *splitStr, unsigned pos,
                           b_cbfunc cb, void *parm);

/**
 * Iterate the set of disjoint sequential substrings over str starting at
 * position pos divided by the entire substring splitStr.
 *
 * An empty splitStr causes each character of str to be iterated. The parm
 * passed to bsplitcb is passed on to cb. If the function cb returns a value <
 * 0, then further iterating is halted and this value is returned by bsplitcb.
 *
 * Note: Non-destructive modification of str from within the cb function
 * while performing this split is not undefined. bsplitstrcb behaves in
 * sequential lock step with calls to cb. I.e., after returning from a cb
 * that return a non-negative integer, bsplitstrcb continues from the position
 * 1 character after the last detected split character and it will halt
 * immediately if the length of str falls below this point. However, if the
 * cb function destroys str, then it *must* return with a negative value,
 * otherwise bsplitscb will continue in an undefined manner.
 *
 * This function is provided as an incremental alternative to bsplitstr that
 * is abortable and which does not impose additional memory allocation.
 */
BSTR_PUBLIC int b_splitstrcb(const bstring *str, const bstring *splitStr,
                             unsigned pos, b_cbfunc cb, void *parm);


/*============================================================================*/
/* Miscellaneous functions */


/**
 * Replicate the starting bstring, b, end to end repeatedly until it
 * surpasses len characters, then chop the result to exactly len characters.
 *
 * This function operates in-place. This function will return with BSTR_ERR
 * if b is NULL or of length 0, otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_pattern(bstring *bstr, unsigned len);

/**
 * Convert contents of bstring to upper case.
 *
 * This function will return with BSTR_ERR if b is NULL or of length 0,
 * otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_toupper(bstring *bstr);

/**
 * Convert contents of bstring to lower case.
 *
 * This function will return with BSTR_ERR if b is NULL or of length 0,
 * otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_tolower(bstring *bstr);

/**
 * Delete whitespace contiguous from the left end of the bstring.
 *
 * This function will return with BSTR_ERR if b is NULL or of length 0,
 * otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_ltrimws(bstring *bstr);

/**
 * Delete whitespace contiguous from the right end of the bstring.
 *
 * This function will return with BSTR_ERR if b is NULL or of length 0,
 * otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_rtrimws(bstring *bstr);

/**
 * Delete whitespace contiguous from both ends of the bstring.
 *
 * This function will return with BSTR_ERR if b is NULL or of length 0,
 * otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_trimws(bstring *bstr);

/* *printf format functions */
/**
 * Takes the same parameters as printf(), but rather than outputting
 * results to stdio, it forms a bstring which contains what would have been
 * output.
 *
 * Note that if there is an early generation of a '\0' character, the bstring
 * will be truncated to this end point.
 *
 * Note that %s format tokens correspond to '\0' terminated char * buffers,
 * not bstrings. To print a bstring, first dereference data element of the
 * the bstring:
 *
 * b1->data needs to be '\0' terminated, so tagbstrings generated by
 * blk2tbstr() might not be suitable.
 *
 * \code
 * b0 = bformat("Hello, %s", b1->data);
 * \endcode
 */
BSTR_PUBLIC bstring *b_format(const char *fmt, ...) BSTR_PRINTF(1, 2);

BSTR_PUBLIC bstring *b_vformat(const char *fmt, va_list arglist);

/**
 * In addition to the initial output buffer b, bformata takes the same
 * parameters as printf (), but rather than outputting results to stdio, it
 * appends the results to the initial bstring parameter.
 *
 * Note that if there is an early generation of a '\0' character, the bstring
 * will be truncated to this end point.
 *
 * Note that %s format tokens correspond to '\0' terminated char * buffers,
 * not bstrings. To print a bstring, first dereference data element of the
 * the bstring:
 *
 * b1->data needs to be '\0' terminated, so tagbstrings generated by
 * blk2tbstr() might not be suitable.
 *
 * \code
 * bformata(b0 = bfromcstr ("Hello"), ", %s", b1->data);
 * \endcode
 */
BSTR_PUBLIC int b_formata(bstring *bstr, const char *fmt, ...) BSTR_PRINTF(2, 3);

/**
 * After the first parameter, it takes the same parameters as printf(), but
 * rather than outputting results to stdio, it outputs the results to the
 * bstring parameter b.
 *
 * Note that if there is an early generation of a '\0' character, the bstring
 * will be truncated to this end point.
 *
 * Note that %s format tokens correspond to '\0' terminated c strings, not
 * bstrings. To print a bstring, dereference data element of the the bstring.
 *
 * b1->data needs to be '\0' terminated, so tagbstrings generated by blk2tbstr()
 * might not be suitable.
 *
 * \code
 * bassignformat(b0 = bfromcstr ("Hello"), ", %s", b1->data);
 * \endcode
 */
BSTR_PUBLIC int b_assign_format(bstring *bstr, const char *fmt, ...) BSTR_PRINTF(2, 3);

/**
 * The bvcformata function formats data under control of the format control
 * string fmt and attempts to append the result to b.
 *
 * The fmt parameter is the same as that of the printf function. The variable
 * argument list is replaced with arglist, which has been initialized by the
 * va_start macro.  The size of the output is upper bounded by count. If the
 * required output exceeds count, the string b is not augmented with any
 * contents and a value below BSTR_ERR is returned. If a value below -count is
 * returned then it is recommended that the negative of this value be used as an
 * update to the count in a subsequent pass. On other errors, such as running
 * out of memory, parameter errors or numeric wrap around BSTR_ERR is returned.
 * BSTR_OK is returned when the output is successfully generated and appended to b.
 *
 * Note: There is no sanity checking of arglist, and this function is
 * destructive of the contents of b from the b->slen point onward. If there is
 * an early generation of a '\0' character, the bstring will be truncated to
 * this end point.
 *
 * Although this function is part of the public API for Bstrlib, the interface
 * and semantics (length limitations, and unusual return codes) are fairly
 * atypical. The real purpose for this function is to provide an engine for the
 * bvformata macro.
 */
BSTR_PUBLIC int b_vcformata(bstring *bstr, unsigned count, const char *fmt, va_list arg);

/**
 * Append the bstring b with printf like formatting with the format control
 * string, and the arguments taken from the list of arguments after lastarg
 * passed to the containing function.
 *
 * If the containing function does not have extra parameters or lastarg is not
 * the last named parameter before the extra parameters then the results are
 * undefined. If successful, the results are appended to b and BSTR_OK is
 * assigned to ret.  Otherwise BSTR_ERR is assigned to ret.
 *
 * Example:
 *
 * \code
 * void dbgerror(FILE *fp, const char *fmt, ...)
 * {
 *     int ret;
 *     bstring *b;
 *     b_vformata(ret, b = bfromcstr ("DBG: "), fmt, fmt);
 *     if (BSTR_OK == ret) {
 *         fputs ((char *) bdata (b), fp);
 *     }
 *     bdestroy (b);
 * }
 * \endcode
 *
 * DISCLAIMER: Useless!
 */
#define b_vformata(ret, b, fmt, lastarg)                                       \
        do {                                                                   \
                bstring *bstrtmp_b = (b);                                      \
                const char *bstrtmp_fmt = (fmt);                               \
                int bstrtmp_r = BSTR_ERR, bstrtmp_sz = 16;                     \
                for (;;) {                                                     \
                        va_list bstrtmp_arglist;                               \
                        va_start(bstrtmp_arglist, lastarg);                    \
                        bstrtmp_r = b_vcformata(bstrtmp_b, bstrtmp_sz,         \
                                                bstrtmp_fmt, bstrtmp_arglist); \
                        va_end(bstrtmp_arglist);                               \
                        if (bstrtmp_r >= 0) {                                  \
                                /* Everything went ok */                       \
                                bstrtmp_r = BSTR_OK;                           \
                                break;                                         \
                        } else if (-bstrtmp_r <= bstrtmp_sz) {                 \
                                /* A real error? */                            \
                                bstrtmp_r = BSTR_ERR;                          \
                                break;                                         \
                        }                                                      \
                        /* Doubled or target size */                           \
                        bstrtmp_sz = -bstrtmp_r;                               \
                }                                                              \
                ret = bstrtmp_r;                                               \
        } while (0);

typedef int (*bNgetc)(void *parm);
typedef size_t (*bNread)(void *buff, size_t elsize, size_t nelem, void *parm);


/*============================================================================*/
/* Input functions */


/**
 * Read a bstring from a stream.
 *
 * As many bytes as is necessary are read until the terminator is consumed or no
 * more characters are available from the stream. If read from the stream, the
 * terminator character will be appended to the end of the returned bstring. The
 * getcPtr function must have the same semantics as the fgetc C library function
 * (i.e., returning an integer whose value is negative when there are no more
 * characters available, otherwise the value of the next available ucharacter
 * from the stream.)  The intention is that parm would contain the stream data
 * context/state required (similar to the role of the FILE* I/O stream parameter
 * of fgets.)  If no characters are read, or there is some other detectable
 * error, NULL is returned.
 *
 * bgets will never call the getcPtr function more often than necessary to
 * construct its output (including a single call, if required, to determine that
 * the stream contains no more characters.)
 *
 * Abstracting the character stream function and terminator character allows for
 * different stream devices and string formats other than '\n' terminated lines
 * in a file if desired (consider \032 terminated email messages, in a UNIX
 * mailbox for example.)
 *
 * For files, this function can be used analogously as fgets as follows:
 *
 * \code
 * fp = fopen( ... );
 * if (fp) b = b_gets((bNgetc) fgetc, fp, '\n');
 * \endcode
 *
 * (Note that only one terminator character can be used, and that '\0' is not
 * assumed to terminate the stream in addition to the terminator character. This
 * is consistent with the semantics of fgets.)
 */
BSTR_PUBLIC bstring *b_gets(bNgetc getc_ptr, void *parm, char terminator);


/**
 * Read an entire stream into a bstring, verbatum.
 *
 * The readPtr function pointer is compatible with fread sematics, except that
 * it need not obtain the stream data from a file. The intention is that parm
 * would contain the stream data context/state required (similar to the role of
 * the FILE* I/O stream parameter of fread.)
 *
 * Abstracting the block read function allows for block devices other than file
 * streams to be read if desired. Note that there is an ANSI compatibility issue
 * if "fread" is used directly; see the ANSI issues section below.
 */
BSTR_PUBLIC bstring *b_read(bNread read_ptr, void *parm);

/**
 * Read from a stream and concatenate to a bstring.
 *
 * Behaves like bgets, except that it appends it results to the bstring *b. The
 * value 1 is returned if no characters are read before a negative result is
 * returned from getcPtr. Otherwise BSTR_ERR is returned on error, and 0 is
 * returned in other normal cases.
 */
BSTR_PUBLIC int b_getsa(bstring *bstr, bNgetc getc_ptr, void *parm,
                        char terminator);

/**
 * Read from a stream and concatenate to a bstring.
 *
 * Behaves like bgets, except that it assigns the results to the bstring *b. The
 * value 1 is returned if no characters are read before a negative result is
 * returned from getcPtr. Otherwise BSTR_ERR is returned on error, and 0 is
 * returned in other normal cases.
 */
BSTR_PUBLIC int b_assign_gets(bstring *bstr, bNgetc getc_ptr, void *parm,
                              char terminator);

/**
 * Read an entire stream and append it to a bstring, verbatim.
 *
 * Behaves like bread, except that it appends it results to the bstring *b.
 * BSTR_ERR is returned on error, otherwise 0 is returned.
 */
BSTR_PUBLIC int b_reada(bstring *bstr, bNread read_ptr, void *parm);


/*============================================================================*/
/* Stream functions */


typedef unsigned (*bs_cbfunc)(void *parm, unsigned ofs, const bstring *lst);

/**
 * Wrap a given open stream (described by a fread compatible function pointer
 * and stream handle) into an open bStream suitable for the bstring library
 * streaming functions.
 */
BSTR_PUBLIC struct bStream *bs_open(bNread read_ptr, void *parm);

/**
 * Close the bStream, and return the handle to the stream that was originally
 * used to open the given stream.
 *
 * If s is NULL or detectably invalid, NULL will be returned.
 */
BSTR_PUBLIC void *bs_close(struct bStream * buf);

/**
 * Set the length of the buffer used by the bStream.
 *
 * If sz is the macro BSTR_BS_BUFF_LENGTH_GET (which is 0), the length is not
 * set. If s is NULL or sz is negative, the function will return with BSTR_ERR,
 * otherwise this function returns with the previous length.
 */
BSTR_PUBLIC int bs_bufflength(struct bStream * buf, unsigned sz);

/**
 * Read a bstring terminated by the terminator character or the end of the
 * stream from the bStream (s) and return it into the parameter r.
 *
 * The matched terminator, if found, appears at the end of the line read. If the
 * stream has been exhausted of all available data, before any can be read,
 * BSTR_ERR is returned. This function may read additional characters into the
 * stream buffer from the core stream that are not returned, but will be
 * retained for subsequent read operations. When reading from high speed
 * streams, this function can perform significantly faster than bgets.
 */
BSTR_PUBLIC int bs_readln(bstring *r, struct bStream * buf, char terminator);

/**
 * Read a bstring terminated by any character in the terminators bstring or the
 * end of the stream from the bStream (s) and return it into the parameter r.
 *
 * This function may read additional characters from the core stream that are
 * not returned, but will be retained for subsequent read operations.
 */
BSTR_PUBLIC int bs_readlns(bstring *r, struct bStream * buf, const bstring *term);

/**
 * Read a bstring of length n (or, if it is fewer, as many bytes as is
 * remaining) from the bStream.
 *
 * This function will read the minimum required number of additional characters
 * from the core stream. When the stream is at the end of the file BSTR_ERR is
 * returned, otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int bs_read(bstring *r, struct bStream * buf, unsigned n);

/**
 * Read a bstring terminated by the terminator character or the end of the
 * stream from the bStream (s) and concatenate it to the parameter r.
 *
 * The matched terminator, if found, appears at the end of the line read. If the
 * stream has been exhausted of all available data, before any can be read,
 * BSTR_ERR is returned. This function may read additional characters into the
 * stream buffer from the core stream that are not returned, but will be
 * retained for subsequent read operations. When reading from high speed
 * streams, this function can perform significantly faster than bgets.
 */
BSTR_PUBLIC int bs_readlna(bstring *r, struct bStream * buf, char terminator);

/**
 * Read a bstring terminated by any character in the terminators bstring or the
 * end of the stream from the bStream (s) and concatenate it to the parameter r.
 *
 * If the stream has been exhausted of all available data, before any can be
 * read, BSTR_ERR is returned. This function may read additional characters from
 * the core stream that are not returned, but will be retained for subsequent
 * read operations.
 */
BSTR_PUBLIC int bs_readlnsa(bstring *r, struct bStream *buf, const bstring *term);

/**
 * Read a bstring of length n (or, if it is fewer, as many bytes as is
 * remaining) from the bStream and concatenate it to the parameter r.
 *
 * This function will read the minimum required number of additional characters
 * from the core stream. When the stream is at the end of the file BSTR_ERR is
 * returned, otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int bs_reada(bstring *r, struct bStream *buf, unsigned n);

/**
 * Insert a bstring into the bStream at the current position.
 *
 * These characters will be read prior to those that actually come from the core
 * stream.
 */
BSTR_PUBLIC int bs_unread(struct bStream *buf, const bstring *bstr);

/**
 * Return the number of currently buffered characters from the bStream that will
 * be read prior to reads from the core stream, and append it to the the
 * parameter r.
 */
BSTR_PUBLIC int bs_peek(bstring *r, const struct bStream *buf);

/**
 * Iterate the set of disjoint sequential substrings over the stream s divided
 * by any character from the bstring *splitStr.
 *
 * The parm passed to bssplitscb is passed on to cb. If the function cb returns
 * a value < 0, then further iterating is halted and this return value is
 * returned by bssplitscb.
 *
 * Note: At the point of calling the cb function, the bStream pointer is pointed
 * exactly at the position right after having read the split character. The cb
 * function can act on the stream by causing the bStream pointer to move, and
 * bssplitscb will continue by starting the next split at the position of the
 * pointer after the return from cb.
 *
 * However, if the cb causes the bStream s to be destroyed then the cb must
 * return with a negative value, otherwise bssplitscb will continue in an
 * undefined manner.
 *
 * This function is provided as way to incrementally parse through a file or
 * other generic stream that in total size may otherwise exceed the practical or
 * desired memory available. As with the other split callback based functions
 * this is abortable and does not impose additional memory allocation.
 */
BSTR_PUBLIC int bs_splitscb(struct bStream *buf, const bstring *split_str,
                            bs_cbfunc cb, void *parm);

/**
 * Iterate the set of disjoint sequential substrings over the stream s divided
 * by the entire substring splitStr.
 *
 * The parm passed to bssplitstrcb is passed on to cb. If the function cb
 * returns a value < 0, then further iterating is halted and this return value
 * is returned by bssplitstrcb.
 *
 * Note: At the point of calling the cb function, the bStream pointer is pointed
 * exactly at the position right after having read the split character. The cb
 * function can act on the stream by causing the bStream pointer to move, and
 * bssplitstrcb will continue by starting the next split at the position of the
 * pointer after the return from cb.
 *
 * However, if the cb causes the bStream s to be destroyed then the cb must
 * return with a negative value, otherwise bssplitscb will continue in an
 * undefined manner.
 *
 * This function is provided as way to incrementally parse through a file or
 * other generic stream that in total size may otherwise exceed the practical or
 * desired memory available. As with the other split callback based functions
 * this is abortable and does not impose additional memory allocation.
 */
BSTR_PUBLIC int bs_splitstrcb(struct bStream *buf, const bstring *split_str,
                              bs_cbfunc cb, void *parm);

/**
 * Return the defacto "EOF" (end of file) state of a stream (1 if the bStream is
 * in an EOF state, 0 if not, and BSTR_ERR if stream is closed or detectably
 * erroneous).
 *
 * When the readPtr callback returns a value <= 0 the stream reaches its "EOF"
 * state. Note that bunread with non-empty content will essentially turn off
 * this state, and the stream will not be in its "EOF" state so long as its
 * possible to read more data out of it.
 *
 * Also note that the semantics of b_seof() are slightly different from
 * something like feof(), i.e., reaching the end of the stream does not
 * necessarily guarantee that b_seof() will return with a value indicating that
 * this has happened. b_seof() will only return indicating that it has reached
 * the "EOF" and an attempt has been made to read past the end of the bStream.
 */
BSTR_PUBLIC int bs_eof(const struct bStream * buf);


/*============================================================================*/
/* Accessor macros */


/**
 * Returns the length of the bstring.
 *
 * If the bstring is NULL err is returned.
 */
#define b_lengthe(b, e) \
        (((b) == NULL) ? (unsigned)(e) : ((b)->slen))

/**
 * Returns the length of the bstring.
 *
 * If the bstring is NULL, the length returned is 0.
 */
#define b_length(b) (b_lengthe((b), 0))

#define BLEN(b) b_length(b)

/**
 * Returns the char * data portion of the bstring *b offset by off.
 *
 * If b is NULL, err is returned.
 */
#define b_dataoffe(b, o, e)                 \
        (((b) == NULL || (b)->data == NULL) \
             ? (char *)(e)                  \
             : ((char *)(b)->data) + (o))

/**
 * Returns the char * data portion of the bstring *b offset by off.
 *
 * If b is NULL, NULL is returned.
 */
#define b_dataoff(b, o) (b_dataoffe((b), (o), NULL))

/**
 * Returns the char * data portion of the bstring *b.
 *
 * If b is NULL, err is returned.
 */
#define b_datae(b, e) (b_dataoffe(b, 0, e))

/**
 * Returns the char * data portion of the bstring *b.
 *
 * If b is NULL, NULL is returned.
 */
#define b_data(b) (b_dataoff(b, 0))

/**
 * Dumber macro which simply returns the data of a bstring cast to char *. No
 * checking is performed. The program is hopeless anyway if it is accessing NULL
 * memory and should crash.
 */
#ifdef __GNUC__
#  define BS(BSTR_)                                                 \
        __extension__({                                             \
                _Static_assert(sizeof(*BSTR_) == sizeof(bstring) && \
                               sizeof((BSTR_)->flags) == 1,         \
                               "Pointer is not a bstring");         \
                (char *)((BSTR_)->data);                            \
        })
#  define BTS(BSTR_)                                               \
        __extension__({                                            \
                _Static_assert(sizeof(BSTR_) == sizeof(bstring) && \
                               sizeof((BSTR_).flags) == 1,         \
                               "Pointer is not a bstring");        \
                (char *)((BSTR_).data);                            \
        })
#else
#  define BS(BSTR_)  ((char *)((BSTR_)->data))
#  define BTS(BSTR_) ((char *)((BSTR_).data))
#endif

/**
 * Returns the p'th character of the bstring *b.
 *
 * If the position p refers to a position that does not exist in the bstring or
 * the bstring is NULL, then c is returned.
 */
#define b_chare(b, p, e) \
        ((((unsigned)(p)) < (unsigned)b_length(b)) ? ((b)->data[(p)]) : (e))

/**
 * Returns the p'th character of the bstring *b.
 *
 * If the position p refers to a position that does not exist in the bstring or
 * the bstring is NULL, then '\0' is returned.
 */
#define b_char(b, p) b_chare((b), (p), '\0')


/*============================================================================*/
/* Static constant string initialization macro */

/**
 * The bsStaticBlkParms macro emits a pair of comma seperated parameters
 * corresponding to the block parameters for the block functions in Bstrlib
 * (i.e., b_blk2bstr, bcatblk, blk2tbstr, bisstemeqblk, bisstemeqcaselessblk).
 *
 * Note that this macro is only well defined for string literal arguments.
 *
 * Examples:
 *
 * \code
 * bstring * b = b_blk2bstr(b_staticBlkParms("Fast init."));
 * b_catblk(b, b_staticBlkParms("No frills fast concatenation."));
 * \endcode
 *
 * These are faster than using b_fromcstr() and b_catcstr() respectively
 * because the length of the inline string is known as a compile time
 * constant. Also note that seperate bstring declarations for
 */
#define b_staticBlkParms(cstr) ((void *)("" cstr "")), (sizeof(cstr) - 1)


/*============================================================================*/
/* MY ADDITIONS */

/**
 * Initialize a bstring without any casting. seful when a constant expression is
 * required, such as in global or static variable initializers. The argument
 * MUST be a literal string, so double evaluation shouldn't be a problem..
 */
#define bt_init(CSTR)                             \
        {                                         \
                .slen  = (sizeof(CSTR) - 1),      \
                .mlen  = 0,                       \
                .data  = (uchar *)("" CSTR ""),   \
                .flags = 0x00u                    \
        }

/**
 * This useful macro creates a valid pointer to a static bstring object by
 * casting bt_init() to (bstring[]). Used most often to supply literal string
 * arguments to functions that expect a bstring pointer. Like bt_init, the
 * argument must be a literal string.
 *
 * All bstring functions will refuse to modify the return from this macro,
 * including b_free(). The object must not otheriwise be free'd.
 */
#define b_tmp(CSTR) (bstring[]){ bt_init(CSTR) }


#define b_litsiz                   b_staticBlkParms
#define b_lit2bstr(LIT_STR)        b_blk2bstr(b_staticBlkParms(LIT_STR))
#define b_assignlit(BSTR, LIT_STR) b_assign_blk((BSTR), b_staticBlkParms(LIT_STR))
#define b_catlit(BSTR, LIT_STR)    b_catblk((BSTR), b_staticBlkParms(LIT_STR))


/**
 * Allocates a reference to the data of an existing bstring without copying the
 * data. The bstring itself must be free'd, however b_free will not free the
 * data. The clone is write protected, but the original is not, so caution must
 * be taken when modifying it lest the clone's length fields become invalid.
 *
 * This is rarely useful.
 */
BSTR_PUBLIC bstring *b_clone(const bstring *const src);

/**
 * Similar to b_clone() except the original bstring is designated as the clone
 * and is write protected. Useful in situations where some routine will destroy
 * the original, but you want to keep its data.
 */
BSTR_PUBLIC bstring *b_clone_swap(bstring *src);
BSTR_PUBLIC bstring *b_ll2str(const long long value);


/*----------------------------------------------------------------------------*/


/**
 * Allocates a bstring object that references the supplied memory. The memory is
 * not copied, so the user must ensure that it will not go out of scope. The
 * bstring object itself must be freed, but the memory it references will not be
 * freed by b_destroy or b_free by default. The user must either manually set
 * the BSTR_DATA_FREEABLE flag or ensure that the memory is freed independently.
 */
BSTR_PUBLIC bstring *b_refblk(void *blk, unsigned len);

/**
 * The same as b_refblk with the exception that the size is derived by strlen().
 * Required to be an inline function to avoid evaluating the arguments twice via
 * the necessary strlen() call.
 */
INLINE PURE bstring *b_refcstr(char *str)
{
        return b_refblk(str, strlen(str));
}


/**
 * Creates a static bstring reference to existing memory without copying it.
 * Unlike the return from b_tmp, this will accept non-literal strings, and the
 * data is modifyable by default. However, b_free will refuse to free the data,
 * and the object itself is stack memory and therefore also not freeable.
 */
#define bt_fromblk(BLK, LEN) \
        (bstring[]){{ (LEN), 0, ((uchar *)(BLK)), BSTR_WRITE_ALLOWED }}

/**
 * Return a static bstring derived from a cstring. Identical to bt_fromblk
 * except that the length field is derived through a call to strlen(). This is
 * defined as an inline function to avoid any potential double evaluation of
 * side effects that may happen if it were a macro.
 */
INLINE PURE bstring *bt_fromcstr(void *str)
{
        return (bstring[]){{
                .slen  = strlen(str),
                .mlen  = 0,
                .data  = (uchar *)(str),
                .flags = 0x00u
        }};
}


/*----------------------------------------------------------------------------*/
/* Read wrappers */

/**
 * Simple wrapper for fgetc that casts the void * paramater to a FILE * object
 * to avoid compiler warnings.
 */
INLINE int b_fgetc(void *param)
{
        return fgetc((FILE *)param);
}

/**
 * Simple wrapper for fread that casts the void * paramater to a FILE * object
 * to avoid compiler warnings.
 */
INLINE size_t b_fread(void *buf, const size_t size, const size_t nelem, void *param)
{
        return fread(buf, size, nelem, (FILE *)param);
}

#define B_GETS(PARAM, TERM) b_gets(&b_fgetc, (PARAM), (TERM))
#define B_READ(PARAM)       b_read(&b_fread, (PARAM))


/*----------------------------------------------------------------------------*/


/**
 * Safely free several bstrings.
 */
BSTR_PUBLIC void __b_free_all(bstring **bstr, ...);

/**
 * Write bstrings to files/stdout/stderr without the calls to strlen that the
 * standard c library would make. These call fwrite on the supplied stream,
 * passing the slen of the bstring as the length.
 */
BSTR_PUBLIC void __b_fputs(FILE *fp, bstring *bstr, ...);

/**
 * Same as __b_fputs but writes to a file descriptor using the write(2) function
 * rather than a FILE * object and fwrite(3);
 */
BSTR_PUBLIC void __b_write(int fd, bstring *bstr, ...);


#define b_free_all(...)    __b_free_all(__VA_ARGS__, NULL)
#define b_puts(...)        __b_fputs(stdout, __VA_ARGS__, ((bstring[]){{0, 0, NULL, BSTR_LIST_END}}))
#define b_warn(...)        __b_fputs(stderr, __VA_ARGS__, ((bstring[]){{0, 0, NULL, BSTR_LIST_END}}))
#define b_fputs(__FP, ...) __b_fputs(__FP, __VA_ARGS__,   ((bstring[]){{0, 0, NULL, BSTR_LIST_END}}))
#define b_write(__FD, ...) __b_write(__FD, __VA_ARGS__,   ((bstring[]){{0, 0, NULL, BSTR_LIST_END}}))


BSTR_PUBLIC void __b_dump_list(FILE *fp, const b_list *list, const char *listname);
BSTR_PUBLIC void __b_add_to_list(b_list **list, bstring *bstr);

#define b_dump_list(FP_, LST_) __b_dump_list((FP_), (LST_), #LST_)
#define b_add_to_list(LST_, BSTR) __b_add_to_list((&(LST_)), (BSTR))



/*============================================================================*/

/**
 * The bsStatic macro allows for static declarations of literal string
 * constants as bstring structures.
 *
 * The resulting tagbstring does not need to be freed or destroyed. Note that
 * this macro is only well defined for string literal arguments. For more
 * general string pointers, use the btfromcstr macro.
 *
 * The resulting bstring is permanently write protected. Attempts
 * to write to this bstring from any bstrlib function will lead to
 * BSTR_ERR being returned. Invoking the bwriteallow macro onto this struct
 * tagbstring has no effect.
 */
/**
 * Fill in the tagbstring t with the '\0' terminated char buffer s.
 *
 * This action is purely reference oriented; no memory management is done. The
 * data member is just assigned s, and slen is assigned the strlen of s.  The s
 * parameter is accessed exactly once in this macro.
 *
 * The resulting bstring is initially write protected. Attempts
 * to write to this bstring in a write protected state from any
 * bstrlib function will lead to BSTR_ERR being returned. Invoke the
 * bwriteallow on this bstring to make it writeable (though this
 * requires that s be obtained from a function compatible with malloc.)
 */
/**
 * Fill in the tagbstring t with the data buffer s with length len.
 *
 * This action is purely reference oriented; no memory management is done. The
 * data member of t is just assigned s, and slen is assigned len. Note that the
 * buffer is not appended with a '\0' character. The s and len parameters are
 * accessed exactly once each in this macro.
 *
 * The resulting bstring is initially write protected. Attempts to write to this
 * bstring in a write protected state from any bstrlib function will lead to
 * BSTR_ERR being returned. Invoke the bwriteallow on this bstring to make it
 * writeable (though this requires that s be obtained from a function compatible
 * with malloc.)
 */
#define b_static_refblk(BLK, LEN) \
        ((bstring){ (LEN), 0, ((uchar *)(BLK)), 0x00u })

#define blk2tbstr(s, l) b_static_refblk(s, l)

INLINE PURE bstring b_static_refcstr(char *str)
{
        return (bstring){
                .slen  = strlen(str),
                .mlen  = 0,
                .data  = (uchar *)(str),
                .flags = 0x00u
        };
}

/**
 * Fill the tagbstring t with the substring from b, starting from position pos
 * with a length len.
 *
 * The segment is clamped by the boundaries of the bstring b. This action is
 * purely reference oriented; no memory management is done. Note that the buffer
 * is not appended with a '\0' character. Note that the t parameter to this
 * macro may be accessed multiple times. Note that the contents of t will become
 * undefined if the contents of b change or are destroyed.
 *
 * The resulting bstring is permanently write protected. Attempts to write to
 * this bstring in a write protected state from any bstrlib function will lead
 * to BSTR_ERR being returned. Invoking the bwriteallow macro on this bstring
 * will have no effect.
 */
#define bmid2tbstr(t, b, p, l)                                                \
        do {                                                                  \
                const bstring *bstrtmp_s = (b);                               \
                if (bstrtmp_s && bstrtmp_s->data && bstrtmp_s->slen >= 0) {   \
                        int bstrtmp_left = (p);                               \
                        int bstrtmp_len = (l);                                \
                        if (bstrtmp_left < 0) {                               \
                                bstrtmp_len += bstrtmp_left;                  \
                                bstrtmp_left = 0;                             \
                        }                                                     \
                        if (bstrtmp_len > bstrtmp_s->slen - bstrtmp_left)     \
                                bstrtmp_len = bstrtmp_s->slen - bstrtmp_left; \
                        if (bstrtmp_len <= 0) {                               \
                                (t).data = (uchar *)"";                       \
                                (t).slen = 0;                                 \
                        } else {                                              \
                                (t).data = bstrtmp_s->data + bstrtmp_left;    \
                                (t).slen = bstrtmp_len;                       \
                        }                                                     \
                } else {                                                      \
                        (t).data = (uchar *)"";                               \
                        (t).slen = 0;                                         \
                }                                                             \
                (t).mlen  = 0;                                        \
                (t).flags = 0x00u;                                            \
        } while (0);

/**
 * Fill in the tagbstring t with the data buffer s with length len after it
 * has been left trimmed.
 *
 * This action is purely reference oriented; no memory management is done. The
 * data member of t is just assigned to a pointer inside the buffer s. Note
 * that the buffer is not appended with a '\0' character. The s and len
 * parameters are accessed exactly once each in this macro.
 *
 * The resulting bstring is permanently write protected. Attempts
 * to write to this bstring from any bstrlib function will lead to
 * BSTR_ERR being returned. Invoking the bwriteallow macro onto this struct
 * tagbstring has no effect.
 */
#define bt_fromblkltrimws(t, s, l)                                       \
        do {                                                             \
                unsigned bstrtmp_idx  = 0;                               \
                unsigned bstrtmp_len  = (l);                             \
                uchar    *bstrtmp_s   = (s);                             \
                if (bstrtmp_s && bstrtmp_len >= 0)                       \
                        for (; bstrtmp_idx < bstrtmp_len; bstrtmp_idx++) \
                                if (!isspace(bstrtmp_s[bstrtmp_idx]))    \
                                        break;                           \
                (t).data  = bstrtmp_s + bstrtmp_idx;                     \
                (t).slen  = bstrtmp_len - bstrtmp_idx;                   \
                (t).mlen  = 0;                                           \
                (t).flags = 0x00u;                                       \
        } while (0);

/**
 * Fill in the tagbstring t with the data buffer s with length len after it
 * has been right trimmed.
 *
 * This action is purely reference oriented; no memory management is done. The
 * data member of t is just assigned to a pointer inside the buffer s. Note
 * that the buffer is not appended with a '\0' character. The s and len
 * parameters are accessed exactly once each in this macro.
 *
 * The resulting bstring is permanently write protected. Attempts
 * to write to this bstring from any bstrlib function will lead to
 * BSTR_ERR being returned. Invoking the bwriteallow macro onto this struct
 * tagbstring has no effect.
 */
#define bt_fromblkrtrimws(t, s, l)                                    \
        do {                                                          \
                unsigned bstrtmp_len  = (l)-1;                        \
                uchar    *bstrtmp_s   = (s);                          \
                if (bstrtmp_s && bstrtmp_len >= 0)                    \
                        for (; bstrtmp_len >= 0; bstrtmp_len--)       \
                                if (!isspace(bstrtmp_s[bstrtmp_len])) \
                                        break;                        \
                (t).data  = bstrtmp_s;                                \
                (t).slen  = bstrtmp_len + 1;                          \
                (t).mlen  = 0;                                        \
                (t).flags = 0x00u;                                    \
        } while (0);

/**
 * Fill in the tagbstring t with the data buffer s with length len after it
 * has been left and right trimmed.
 *
 * This action is purely reference oriented; no memory management is done. The
 * data member of t is just assigned to a pointer inside the buffer s. Note
 * that the buffer is not appended with a '\0' character. The s and len
 * parameters are accessed exactly once each in this macro.
 *
 * The resulting bstring is permanently write protected. Attempts
 * to write to this bstring from any bstrlib function will lead to
 * BSTR_ERR being returned. Invoking the bwriteallow macro onto this struct
 * tagbstring has no effect.
 */
#define bt_fromblktrimws(t, s, l)                                         \
        do {                                                              \
                unsigned bstrtmp_idx = 0;                                 \
                unsigned bstrtmp_len = (l)-1;                             \
                uchar    *bstrtmp_s  = (s);                               \
                if (bstrtmp_s && bstrtmp_len >= 0) {                      \
                        for (; bstrtmp_idx <= bstrtmp_len; bstrtmp_idx++) \
                                if (!isspace(bstrtmp_s[bstrtmp_idx]))     \
                                        break;                            \
                        for (; bstrtmp_len >= bstrtmp_idx; bstrtmp_len--) \
                                if (!isspace(bstrtmp_s[bstrtmp_len]))     \
                                        break;                            \
                }                                                         \
                (t).data  = bstrtmp_s + bstrtmp_idx;                      \
                (t).slen  = bstrtmp_len + 1 - bstrtmp_idx;                \
                (t).mlen  = 0;                                            \
                (t).flags = 0x00u;                                        \
        } while (0);


/*============================================================================*/
/* Write protection macros */


/**
 * Disallow bstring from being written to via the bstrlib API.
 *
 * Attempts to write to the resulting tagbstring from any bstrlib function will
 * lead to BSTR_ERR being returned.
 *
 * Note: bstrings which are write protected cannot be destroyed via bdestroy.
 */
#define b_writeprotect(BSTR_) \
        ((BSTR_) ? ((BSTR_)->flags &= (~((uint8_t)BSTR_WRITE_ALLOWED))) : 0)

/**
 * Allow bstring to be written to via the bstrlib API.
 *
 * Note that such an action makes the bstring both writable and destroyable. If
 * the bstring is not legitimately writable (as is the case for struct
 * tagbstrings initialized with a bsStatic value), the results of this are
 * undefined.
 *
 * Note that invoking the bwriteallow macro may increase the number of reallocs
 * by one more than necessary for every call to bwriteallow interleaved with any
 * bstring API which writes to this bstring.
 */
#define b_writeallow(BSTR_) \
        ((BSTR_) ? ((BSTR_)->flags |= BSTR_WRITE_ALLOWED) : 0)

/**
 * Returns 1 if the bstring is write protected, otherwise 0 is returned.
 */
#define b_iswriteprotected(BSTR_) \
        ((BSTR_) && (((BSTR_)->flags & BSTR_WRITE_ALLOWED) == 0))

/* 
 * Cleanup
 */
#undef BSTR_PRIVATE
#undef BSTR_PUBLIC
#undef INLINE

#ifdef __cplusplus
}
#endif

#endif /* BSTRLIB_H */
