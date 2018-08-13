#ifndef TOP_BSTRLIB_H
#  error Never include this file manually. Include "bstrlib.h".
#endif
#ifndef BSTRLIB_ADDITIONS_H
#define BSTRLIB_ADDITIONS_H

#include "bstrlib.h"
#include "defines.h"
/*======================================================================================*/
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
        ((bstring){ .slen = (LEN), .mlen = 0, .data = ((uchar *)(BLK)), .flags = 0x00u })

#define bt_fromcstr(CSTR) \
        ((bstring){ .slen = strlen(CSTR), .mlen = 0, .data = ((uchar *)(CSTR)), .flags = 0x00u })

#define bt_fromarray(CSTR) \
        ((bstring){ .slen = (sizeof(CSTR) - 1), .mlen = 0, .data = (uchar *)(CSTR), .flags = 0x00u })


#define btp_fromblk(BLK, LEN) \
        ((bstring[]){{.slen = (LEN), .mlen = 0, .data = ((uchar *)(BLK)), .flags = 0x00u}})

#define btp_fromcstr(STR_) \
        ((bstring[]){{.slen = strlen(STR_), .mlen = 0, .data = ((uchar *)(STR_)), .flags = 0x00u}})

#define btp_fromarray(CARRAY_) \
        ((bstring[]){{.slen  = (sizeof(CARRAY_) - 1), .mlen  = 0, .data  = ((unsigned char *)(CARRAY_)), .flags = 0}})


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


/*--------------------------------------------------------------------------------------*/
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

BSTR_PUBLIC bstring *b_quickread(const char *__restrict fmt, ...);


/*--------------------------------------------------------------------------------------*/
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
#define b_join_all(JOIN, END, ...) \
        __b_concat_all((JOIN), (END), __VA_ARGS__, B_LIST_END_MARK)
#define b_join_append_all(BDEST, JOIN, END, ...) \
        __b_append_all((BDEST), (JOIN), (END), __VA_ARGS__, B_LIST_END_MARK)

/*--------------------------------------------------------------------------------------*/

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
BSTR_PUBLIC void __b_list_dump(FILE *fp, const b_list *list, const char *listname);

#define b_free_all(...)        __b_free_all(__VA_ARGS__, B_LIST_END_MARK)
#define b_puts(...)            __b_fputs(stdout, __VA_ARGS__, B_LIST_END_MARK)
#define b_warn(...)            __b_fputs(stderr, __VA_ARGS__, B_LIST_END_MARK)
#define b_fputs(__FP, ...)     __b_fputs(__FP, __VA_ARGS__,   B_LIST_END_MARK)
#define b_write(__FD, ...)     __b_write(__FD, __VA_ARGS__,   B_LIST_END_MARK)
#define b_list_dump(FP_, LST_) __b_list_dump((FP_), (LST_), #LST_)


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

/*--------------------------------------------------------------------------------------*/

#define BSTR_M_DEL_SRC   0x01
#define BSTR_M_SORT      0x02
#define BSTR_M_SORT_FAST 0x04
#define BSTR_M_DEL_DUPS  0x08

BSTR_PUBLIC int     b_list_append(b_list **list, bstring *bstr);
BSTR_PUBLIC int     b_list_merge(b_list **dest, b_list *src, int flags);
BSTR_PUBLIC int     b_list_remove_dups(b_list **listp);
BSTR_PUBLIC b_list *b_list_copy(const b_list *list);
BSTR_PUBLIC b_list *b_list_clone(const b_list *list);

BSTR_PUBLIC bstring * b_join_quote(const b_list *bl, const bstring *sep, int ch);

BSTR_PUBLIC int64_t b_strstr(const bstring *const haystack, const bstring *needle, const unsigned pos);
BSTR_PUBLIC int     b_memsep(bstring *dest, bstring *stringp, const char delim);
BSTR_PUBLIC b_list *b_strsep(bstring *ostr, const char *const delim, const int refonly);

/*--------------------------------------------------------------------------------------*/

BSTR_PUBLIC int64_t b_strpbrk_pos(const bstring *bstr, unsigned pos, const bstring *delim);
BSTR_PUBLIC int64_t b_strrpbrk_pos(const bstring *bstr, unsigned pos, const bstring *delim);
#define b_strpbrk(BSTR_, DELIM_) b_strpbrk_pos((BSTR_), 0, (DELIM_))
#define b_strrpbrk(BSTR_, DELIM_) b_strrpbrk_pos((BSTR_), ((BSTR_)->slen), (DELIM_))

BSTR_PUBLIC bstring *  b_dirname(const bstring *path);
BSTR_PUBLIC bstring *  b_basename(const bstring *path);

BSTR_PUBLIC int        b_chomp(bstring *bstr);
BSTR_PUBLIC int        b_replace_ch(bstring *bstr, int find, int replacement);

BSTR_PUBLIC bstring *  b_sprintf(const bstring *fmt, ...);
BSTR_PUBLIC bstring *  b_vsprintf(const bstring *fmt, va_list args);
BSTR_PUBLIC int        b_fprintf(FILE *out_fp, const bstring *fmt, ...);
BSTR_PUBLIC int        b_vfprintf(FILE *out_fp, const bstring *fmt, va_list args);
BSTR_PUBLIC int        b_dprintf(const int out_fd, const bstring *fmt, ...);
BSTR_PUBLIC int        b_vdprintf(const int out_fd, const bstring *fmt, va_list args);
BSTR_PUBLIC int        b_sprintf_a(bstring *dest, const bstring *fmt, ...);
BSTR_PUBLIC int        b_vsprintf_a(bstring *dest, const bstring *fmt, va_list args);

#define b_printf(...)  b_fprintf(stdout, __VA_ARGS__)
#define b_eprintf(...) b_fprintf(stderr, __VA_ARGS__)

/*--------------------------------------------------------------------------------------*/

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

#endif /* additions.h */
