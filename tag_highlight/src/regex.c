#include "tag_highlight.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#define PATSIZ 512
#define _substr(INDEX, SUBJECT, OVECTOR) \
        ((char *)((SUBJECT) + (OVECTOR)[(INDEX)*2]))
#define _substrlen(INDEX, OVECTOR) \
        ((int)((OVECTOR)[(2 * (INDEX)) + 1] - (OVECTOR)[2 * (INDEX)]))
typedef unsigned char uchar;

extern b_list * get_pcre2_matches(const bstring *pattern, const bstring *subject, uint32_t flags);

#if 0
static const char *const arse[] = {
        "Some kinda subject or whatever",
        "You know, like, that thing",
        "12345",
        "Go, to, hell",
};
#endif

enum matches_e { ONLY = 1 };

#if 0
static b_list *
find_matches(bstring *pat, bstring *sub, const int matches)
{
        /* char pat[PATSIZ]; */
        pcre2_match_data *match_data;
        PCRE2_SIZE erroroffset;
        int errornumber;
        /* normalize_lang(norm_lang, lang, PATSIZ); */

        /* snprintf(pat, PATSIZ, "%s%s%s", PATTERN_PT1, norm_lang, PATTERN_PT2); */
        /* strlcpy(pat, "(0x[0-9a-f]{2,})", PATSIZ); */
        PCRE2_SPTR pattern = (PCRE2_SPTR)pat->data;

        pcre2_code *cre = pcre2_compile(pattern, pat->slen, PCRE2_CASELESS,
                                        &errornumber, &erroroffset, NULL);

        if (cre == NULL) {
                PCRE2_UCHAR buf[BUFSIZ];
                pcre2_get_error_message(errornumber, buf, BUFSIZ);
                fprintf(stderr, "PCRE2 compilation failed at offset %d: %s\n",
                       (int)erroroffset, buf);
                exit(1);
        }

        /* for (size_t iter = 0; iter < ARRSIZ(arse); ++iter) {
                const char *sub = arse[iter]; */
        /* bstring *line;
        int iter = 0;
        while ((line = B_GETS(stdin, '\n'))) { */
        {
                int iter = 0;
                bstring *line = sub;
                size_t offset = 0;

                /* line->data[--line->slen] = '\0'; */
                const char *sub = BS(line);
                ++iter;
again:;
                PCRE2_SPTR subject = (PCRE2_SPTR)sub;
                size_t subject_len = (size_t)strlen(sub);

                match_data = pcre2_match_data_create_from_pattern(cre, NULL);
                int rcnt   = pcre2_match(cre, subject, subject_len, 0,
                                         PCRE2_CASELESS, match_data, NULL);

#define substr(INDEX)    _substr(INDEX, subject, ovector)
#define substrlen(INDEX) _substrlen(INDEX, ovector)

                if (rcnt >= 0) {  /* match found */
                        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);

                        char tmp[2048];
                        size_t len = substrlen(ONLY);
                        offset += *ovector;
                        sub    += len + *ovector;

                        memcpy(tmp, substr(ONLY), len + 1);
                        printf("%d: => %zu match:  '%s'\n", iter, offset, tmp);
                        puts((char *)(line->data + offset));

                        offset += len;

                        puts((char *)(line->data + offset));
                        puts(sub);
                        putchar('\n');

                        pcre2_match_data_free(match_data);

                        if (*sub)
                                goto again;
                } else {
                        printf("%d: No matches found\n", iter);
                        pcre2_match_data_free(match_data);
                }
        }

        pcre2_code_free(cre);
        return 0;
}
#endif

#define HAX(CHAR_)                          \
        __extension__ ({                    \
                char *buf;                  \
                int   ch = (CHAR_);         \
                if (ch) {                   \
                        buf    = alloca(2); \
                        buf[0] = ch;        \
                        buf[1] = '\0';      \
                } else {                    \
                        buf    = alloca(3); \
                        buf[0] = '\\';      \
                        buf[1] = '0';       \
                        buf[2] = '\0';      \
                }                           \
                buf;                        \
        })


b_list *
get_pcre2_matches(const bstring *pattern, const bstring *subject, const uint32_t flags)
{
        int                 errornumber;
        size_t              erroroffset;
        pcre2_match_data_8 *match_data;
        PCRE2_SPTR8         pat_data = (PCRE2_SPTR8)(pattern->data);
        pcre2_code_8       *cre = pcre2_compile_8(pat_data, pattern->slen, flags,
                                                  &errornumber, &erroroffset, NULL);

        if (!cre) {
                PCRE2_UCHAR8 buf[8192];
                pcre2_get_error_message_8(errornumber, buf, 8192);
                errx(1, "PCRE2 compilation failed at offset %zu: %s\n",
                     erroroffset, buf);
        }

        /* bstring *sub = b_strcpy(subject); */
        /* char    *sub    = BS(subject); */
        size_t      offset = 0;
        size_t      slen   = subject->slen;
        PCRE2_SPTR8 subp   = (PCRE2_SPTR8)subject->data;

        for (;;) {
                match_data = pcre2_match_data_create_from_pattern_8(cre, NULL);
                int rcnt   = pcre2_match_8(cre, subp, slen, offset,
                                           flags, match_data, NULL);

                if (rcnt <= 0) {
                        pcre2_match_data_free(match_data);
                        break;
                }

                echo("Found %d matches!", rcnt);
                size_t *ovector = pcre2_get_ovector_pointer_8(match_data);
                /* offset              += *ovector; */

                for (int i = 0; i < rcnt; ++i) {
                        char         tmp[2048];
                        const size_t start = ovector[i*2];
                        const size_t end   = ovector[(i*2)+1];
                        const size_t len   = end - start;

                        memcpy(tmp, (subp + start), len);
                        tmp[len] = '\0';

                        echo("At %ld, found: \"%s\"", start, tmp);
                        echo("Also, ovector: %zu and %zu", ovector[i], ovector[i+1]);
                        echo("Furthermore: \"%s\" - '%s'", (const char *)(subp + end), HAX(((const char *)subp)[end]));
                }

                /* offset += init_len; */
                /* sub    += init_len + *ovector;
                slen   -= init_len + *ovector; */

                offset = ovector[1];
                pcre2_match_data_free_8(match_data);
        }

        pcre2_code_free_8(cre);
        return NULL;
}
