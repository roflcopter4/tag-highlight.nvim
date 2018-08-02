#ifndef B_UNUSED_H
#define B_UNUSED_H

#include <stddef.h>
#include <stdint.h>

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

typedef struct tagbstring bstring;
typedef unsigned char     uchar;


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


/*----------------------------------------------------------------------------*/


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
 * Compare beginning of bstring b0 with a block of memory of length len for equality.
 *
 * If the beginning of b0 differs from the memory block (or if b0 is too short),
 * 0 is returned, if the bstrings are the same, 1 is returned, if there is an
 * error, -1 is returned.
 */
BSTR_PUBLIC int b_is_stem_eq_blk(const bstring *b0, const void *blk, unsigned len);

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

/**
 * Replicate the starting bstring, b, end to end repeatedly until it
 * surpasses len characters, then chop the result to exactly len characters.
 *
 * This function operates in-place. This function will return with BSTR_ERR
 * if b is NULL or of length 0, otherwise BSTR_OK is returned.
 */
BSTR_PUBLIC int b_pattern(bstring *bstr, unsigned len);

/*----------------------------------------------------------------------------*/

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


/*============================================================================*/
/* Stream functions */

typedef int (*bNgetc)(void *parm);
typedef size_t (*bNread)(void *buff, size_t elsize, size_t nelem, void *parm);
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

typedef int (*b_cbfunc)(void *parm, unsigned ofs, unsigned len);


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
/* Accessor macros */

/**
 * Returns the length of the bstring.
 * If the bstring is NULL err is returned.
 */
#define b_lengthe(b, e) \
        (((b) == NULL) ? (unsigned)(e) : ((b)->slen))

/**
 * Returns the length of the bstring.
 * If the bstring is NULL, the length returned is 0.
 */
#define b_length(b) (b_lengthe((b), 0))

#define BLEN(b) b_length(b)

/**
 * Returns the char * data portion of the bstring b offset by off.
 * If b is NULL, err is returned.
 */
#define b_dataoffe(b, o, e)                 \
        (((b) == NULL || (b)->data == NULL) \
             ? (char *)(e)                  \
             : ((char *)(b)->data) + (o))

/**
 * Returns the char * data portion of the bstring b offset by off.
 * If b is NULL, NULL is returned.
 */
#define b_dataoff(b, o) (b_dataoffe((b), (o), NULL))

/**
 * Returns the char * data portion of the bstring b.
 * If b is NULL, err is returned.
 */
#define b_datae(b, e) (b_dataoffe(b, 0, e))

/**
 * Returns the char * data portion of the bstring b.
 * If b is NULL, NULL is returned.
 */
#define b_data(b) (b_dataoff(b, 0))

/**
 * Returns the p'th character of the bstring *b.
 * If the position p refers to a position that does not exist in the bstring or
 * the bstring is NULL, then c is returned.
 */
#define b_chare(b, p, e) \
        ((((unsigned)(p)) < (unsigned)b_length(b)) ? ((b)->data[(p)]) : (e))

/**
 * Returns the p'th character of the bstring *b.
 * If the position p refers to a position that does not exist in the bstring or
 * the bstring is NULL, then '\0' is returned.
 */
#define b_char(b, p) b_chare((b), (p), '\0')


/*============================================================================*/

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


#ifdef __cplusplus
}
#endif

#endif /* unused.h */
