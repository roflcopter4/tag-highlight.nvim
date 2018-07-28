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

#if (__GNUC__ >= 4)
#  define BSTR_PUBLIC  __attribute__((__visibility__("default")))
#  define BSTR_PRIVATE __attribute__((__visibility__("hidden")))
#  define INLINE       __attribute__((__always_inline__)) static inline
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#else
#  define BSTR_PUBLIC
#  define BSTR_PRIVATE
#  define INLINE static inline
#endif

#if (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#  define BSTR_PRINTF(format, argument) __attribute__((__format__(__printf__, format, argument)))
#  define BSTR_UNUSED __attribute__((__unused__))
#else
#  define BSTR_PRINTF(format, argument)
#  define BSTR_UNUSED
#endif

#if !defined(__GNUC__)
#  define __attribute__(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char uchar;

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BSTR_ERR (-1)
#define BSTR_OK (0)
#define BSTR_BS_BUFF_LENGTH_GET (0)

#define BSTR_WRITE_ALLOWED 0x01u
#define BSTR_FREEABLE      0x02u
#define BSTR_DATA_FREEABLE 0x04u
#define BSTR_LIST_END      0x08u
#define BSTR_CLONE         0x10u

#define BSTR_STANDARD (BSTR_WRITE_ALLOWED | BSTR_FREEABLE | BSTR_DATA_FREEABLE)


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
BSTR_PUBLIC bstring *b_fromblk(const void *blk, unsigned len);

#define b_blk2bstr(BLK_, LENGTH_) b_fromblk((BLK_), (LENGTH_))

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
BSTR_PUBLIC int b_assign_blk(bstring *a, const void *buf, unsigned len);


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

INLINE int
__b_destroy(bstring **bstr)
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
/* BSTR_PUBLIC int b_conchar(bstring *bstr, char c); */

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
#define b_strrchr(b, c) b_strrchrp((b), (c), ((b)->slen - 1u))

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


/*============================================================================*/
/* Miscellaneous functions */

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


/*============================================================================*/
/* printf format functions */

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
BSTR_PUBLIC int b_format_assign(bstring *bstr, const char *fmt, ...) BSTR_PRINTF(2, 3);

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
 * Like b_format but takes a va_list instead a varargs. b_format and
 * b_format_assign simply pass through to this function.
 */
BSTR_PUBLIC bstring *b_vformat(const char *fmt, va_list arglist);

BSTR_PUBLIC int b_vformat_assign(bstring *bstr, const char *fmt, va_list arglist);
BSTR_PUBLIC int b_vformata(bstring *bstr, const char *fmt, va_list arglist);


/*============================================================================*/
/* Input functions */


typedef int (*bNgetc)(void *parm);
typedef size_t (*bNread)(void *buff, size_t elsize, size_t nelem, void *parm);

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
BSTR_PUBLIC bstring *b_gets(bNgetc getc_ptr, void *parm, int terminator, bool keepend);


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
                        int terminator, bool keepend);

/**
 * Read from a stream and concatenate to a bstring.
 *
 * Behaves like bgets, except that it assigns the results to the bstring *b. The
 * value 1 is returned if no characters are read before a negative result is
 * returned from getcPtr. Otherwise BSTR_ERR is returned on error, and 0 is
 * returned in other normal cases.
 */
BSTR_PUBLIC int b_assign_gets(bstring *bstr, bNgetc getc_ptr, void *parm,
                              int terminator, bool keepend);

/**
 * Read an entire stream and append it to a bstring, verbatim.
 *
 * Behaves like bread, except that it appends it results to the bstring *b.
 * BSTR_ERR is returned on error, otherwise 0 is returned.
 */
BSTR_PUBLIC int b_reada(bstring *bstr, bNread read_ptr, void *parm);


/*============================================================================*/
/* Static constant string initialization macro */

/**
 * The bsStaticBlkParms macro emits a pair of comma seperated parameters
 * corresponding to the block parameters for the block functions in Bstrlib
 * (i.e., b_fromblk, bcatblk, blk2tbstr, bisstemeqblk, bisstemeqcaselessblk).
 *
 * Note that this macro is only well defined for string literal arguments.
 *
 * Examples:
 *
 * \code
 * bstring * b = b_fromblk(b_staticBlkParms("Fast init."));
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
#define B(CSTR)     b_tmp(CSTR)


#if 0
/**
 * Creates a static bstring reference to existing memory without copying it.
 * Unlike the return from b_tmp, this will accept non-literal strings, and the
 * data is modifyable by default. However, b_free will refuse to free the data,
 * and the object itself is stack memory and therefore also not freeable.
 */
#define bt_fromblk(BLK, LEN) \
        ((bstring[]){{ (LEN), 0, ((uchar *)(BLK)), BSTR_WRITE_ALLOWED }})

/**
 * Return a static bstring derived from a cstring. Identical to bt_fromblk
 * except that the length field is derived through a call to strlen(). Beware
 * that this macro evaluates its argument twice!
 */
#define bt_fromcstr(STR_) \
        ((bstring[]){{ strlen(STR_), 0, ((uchar *)(STR_)), BSTR_WRITE_ALLOWED }})

#define bt_fromarray(CSTR) \
        ((bstring[]){ (sizeof(CSTR) - 1), 0, (uchar *)(CSTR), 0x00u })


#define b_static_fromblk(BLK, LEN) \
        ((bstring){ (LEN), 0, ((uchar *)(BLK)), 0x00u })

#define b_static_fromcstr(CSTR) \
        ((bstring){ strlen(CSTR), 0, ((uchar *)(CSTR)), 0x00u })

#define b_static_fromarray(CSTR) \
        ((bstring){ (sizeof(CSTR) - 1), 0, (uchar *)(CSTR), 0x00u })
#endif

#define bt_fromblk(BLK, LEN) \
        ((bstring){ (LEN), 0, ((uchar *)(BLK)), 0x00u })

#define bt_fromcstr(CSTR) \
        ((bstring){ strlen(CSTR), 0, ((uchar *)(CSTR)), 0x00u })

#define bt_fromarray(CSTR) \
        ((bstring){ (sizeof(CSTR) - 1), 0, (uchar *)(CSTR), 0x00u })


#define btp_fromblk(BLK, LEN) \
        ((bstring[]){{ (LEN), 0, ((uchar *)(BLK)), 0x00u }})

#define btp_fromcstr(STR_) \
        ((bstring[]){{ strlen(STR_), 0, ((uchar *)(STR_)), 0x00u }})

#define btp_fromarray(CSTR) \
        ((bstring[]){{ (sizeof(CSTR) - 1), 0, (uchar *)(CSTR), 0x00u }})



#define b_litsiz                   b_staticBlkParms
#define b_lit2bstr(LIT_STR)        b_fromblk(b_staticBlkParms(LIT_STR))
#define b_assignlit(BSTR, LIT_STR) b_assign_blk((BSTR), b_staticBlkParms(LIT_STR))
#define b_catlit(BSTR, LIT_STR)    b_catblk((BSTR), b_staticBlkParms(LIT_STR))
#define b_fromlit(LIT_STR)         b_lit2bstr(LIT_STR)


/**
 * Allocates a reference to the data of an existing bstring without copying the
 * data. The bstring itself must be free'd, however b_free will not free the
 * data. The clone is write protected, but the original is not, so caution must
 * be taken when modifying it lest the clone's length fields become invalid.
 *
 * This is rarely useful.
 */
BSTR_PUBLIC bstring *b_clone(const bstring *src);

/**
 * Similar to b_clone() except the original bstring is designated as the clone
 * and is write protected. Useful in situations where some routine will destroy
 * the original, but you want to keep its data.
 */
BSTR_PUBLIC bstring *b_clone_swap(bstring *src);
BSTR_PUBLIC bstring *b_ll2str(const long long value);
BSTR_PUBLIC int      b_strcmp_fast_wrap(const void *vA, const void *vB);
BSTR_PUBLIC int      b_strcmp_wrap(const void *vA, const void *vB);


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
INLINE bstring *b_refcstr(char *str)
{
        return b_refblk(str, strlen(str));
}


/*----------------------------------------------------------------------------*/
/* Read wrappers */

/**
 * Simple wrapper for fgetc that casts the void * paramater to a FILE * object
 * to avoid compiler warnings.
 */
INLINE int
b_fgetc(void *param)
{
        return fgetc((FILE *)param);
}

/**
 * Simple wrapper for fread that casts the void * paramater to a FILE * object
 * to avoid compiler warnings.
 */
INLINE size_t
b_fread(void *buf, const size_t size, const size_t nelem, void *param)
{
        return fread(buf, size, nelem, (FILE *)param);
}

#define B_GETS(PARAM, TERM, END_) b_gets(&b_fgetc, (PARAM), (TERM), (END_))
#define B_READ(PARAM, END_)       b_read(&b_fread, (PARAM), (END_))


/*----------------------------------------------------------------------------*/
/* Some additional list operations. */

/**
 * Signifies the end of a list of bstring varargs.
 */
#define B_LIST_END_MARK ((bstring[]){{0, 0, NULL, BSTR_LIST_END}})


/**
 * Concatenate a series of bstrings.
 */
BSTR_PUBLIC bstring *__b_concat_all(const bstring *join, int join_end, ...);
BSTR_PUBLIC int      __b_append_all(bstring *dest, const bstring *join, int join_end, ...);

#define b_concat_all(...) \
        __b_concat_all(NULL, 0, __VA_ARGS__, B_LIST_END_MARK)
#define b_append_all(BDEST, ...) \
        __b_append_all((BDEST), NULL, 0, __VA_ARGS__, B_LIST_END_MARK)

#define b_join_all(JOIN_, END_, ...) \
        __b_concat_all((JOIN_), (END_), __VA_ARGS__, B_LIST_END_MARK)
#define b_join_append_all(BDEST, JOIN_, END_, ...) \
        __b_append_all((BDEST), (JOIN_), (END_), __VA_ARGS__, B_LIST_END_MARK)


BSTR_PUBLIC bstring * b_join_quote(const b_list *bl, const bstring *sep, int ch);


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
BSTR_PUBLIC void __b_dump_list(FILE *fp, const b_list *list, const char *listname);

#define b_free_all(...)        __b_free_all(__VA_ARGS__, B_LIST_END_MARK)
#define b_puts(...)            __b_fputs(stdout, __VA_ARGS__, B_LIST_END_MARK)
#define b_warn(...)            __b_fputs(stderr, __VA_ARGS__, B_LIST_END_MARK)
#define b_fputs(__FP, ...)     __b_fputs(__FP, __VA_ARGS__,   B_LIST_END_MARK)
#define b_write(__FD, ...)     __b_write(__FD, __VA_ARGS__,   B_LIST_END_MARK)
#define b_dump_list(FP_, LST_) __b_dump_list((FP_), (LST_), #LST_)


#define B_LIST_FOREACH(BLIST, VAR, CTR)                                    \
        for (bstring *VAR = ((BLIST)->lst[((CTR) = 0)]);                   \
             (CTR) < (BLIST)->qty && (((VAR) = (BLIST)->lst[(CTR)]) || 1); \
             ++(CTR))

#define B_LIST_SORT_FAST(BLIST)                                    \
        qsort((BLIST)->lst, (BLIST)->qty, sizeof(*((BLIST)->lst)), \
              &b_strcmp_fast_wrap)

#define B_LIST_BSEARCH_FAST(BLIST, ITEM_)             \
        bsearch(&(ITEM_), (BLIST)->lst, (BLIST)->qty, \
                sizeof(*((BLIST)->lst)), &b_strcmp_fast_wrap)

#define B_LIST_SORT(BLIST) \
        qsort((BLIST)->lst, (BLIST)->qty, sizeof(*((BLIST)->lst)), &b_strcmp_wrap)


#define BSTR_M_DEL_SRC   0x01
#define BSTR_M_SORT      0x02
#define BSTR_M_SORT_FAST 0x04
#define BSTR_M_DEL_DUPS  0x08

BSTR_PUBLIC int     b_list_append(b_list **list, bstring *bstr);
BSTR_PUBLIC int     b_list_merge(b_list **dest, b_list *src, int flags);
BSTR_PUBLIC int     b_list_remove_dups(b_list **listp);
BSTR_PUBLIC b_list *b_list_copy(const b_list *list);
BSTR_PUBLIC b_list *b_list_clone(const b_list *list);

BSTR_PUBLIC int64_t b_strstr(const bstring *const haystack, const bstring *needle, const unsigned pos);
BSTR_PUBLIC b_list *b_strsep(bstring *str, const char *const delim, const int refonly);

/*----------------------------------------------------------------------------*/

BSTR_PUBLIC int64_t b_strpbrk_pos(const bstring *bstr, unsigned pos, const bstring *delim);
BSTR_PUBLIC int64_t b_strrpbrk_pos(const bstring *bstr, unsigned pos, const bstring *delim);
#define b_strpbrk(BSTR_, DELIM_) b_strpbrk_pos((BSTR_), 0, (DELIM_))
#define b_strrpbrk(BSTR_, DELIM_) b_strrpbrk_pos((BSTR_), ((BSTR_)->slen), (DELIM_))

BSTR_PUBLIC bstring *b_dirname(const bstring *path);
BSTR_PUBLIC bstring *b_basename(const bstring *path);

BSTR_PUBLIC bstring *b_sprintf(const bstring *fmt, ...);
BSTR_PUBLIC bstring *b_vsprintf(const bstring *fmt, va_list args);
BSTR_PUBLIC int      b_fprintf(FILE *out_fp, const bstring *fmt, ...);
#define b_printf(...)  b_fprintf(stdout, __VA_ARGS__)
#define b_eprintf(...) b_fprintf(stderr, __VA_ARGS__)

/*----------------------------------------------------------------------------*/

INLINE int
b_conchar(bstring *bstr, const char ch)
{
        if (!bstr || !bstr->data || ((bstr->flags & BSTR_WRITE_ALLOWED) == 0))
                abort();

        if (bstr->mlen < (bstr->slen + 2))
                if (b_alloc(bstr, bstr->slen + 2) != BSTR_OK)
                        abort();

        bstr->data[bstr->slen++] = (uchar)ch;
        bstr->data[bstr->slen]   = (uchar)'\0';

        return BSTR_OK;
}


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
#ifdef _MSC_VER
#  undef __attribute__
#endif

#ifdef __cplusplus
}
#endif

#endif /* BSTRLIB_H */
