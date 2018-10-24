#include "tag_highlight.h"
#include <sys/stat.h>

#include "contrib/jsmn/jsmn.h"
#include "data.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"

#ifdef DOSISH
//#  include <direct.h>
#  define B_FILE_EQ(FILE1_, FILE2_) (b_iseq_caseless((FILE1_), (FILE2_)))
#else
#  define B_FILE_EQ(FILE1_, FILE2_) (b_iseq((FILE1_), (FILE2_)))
#endif

#ifdef _MSC_VER
#  define restrict __restrict
#endif

#define b_at(BSTR_, OFFSET_)                                         \
        (((unsigned)(OFFSET_) > (BSTR_)->slen)                       \
             ? (errx(1, "Bstring offset is out of bounds.\n"), NULL) \
             : ((BSTR_)->data + (OFFSET_)))

#define SKIP_SPACE(STR_, CTR_)                \
        do {                                  \
                while (isblank((STR_)[CTR_])) \
                        ++(CTR_);             \
        } while (0)

#define SKIP_SPACE_PTR(PTR_)                  \
        do {                                  \
                while (isblank(*(PTR_)))      \
                        ++(PTR_);             \
        } while (0)

#ifdef DOSISH
#define ISROOT(STR_) \
        (isalpha((STR_)[0]) && (STR_)[1] == ':' && (STR_)[2] == '\\' && (STR_)[3] == '\\')
#else
#  define ISROOT(STR_) ((STR_)[0] == '/')
#endif


static b_list  *extract_info(jsmntok_t const *toks,
                             int              ntoks,
                             bstring const   *js,
                             bstring const   *filename,
                             bstring const   *base,
                             b_list          *includes);
static unsigned cannon_path(char          *restrict buf,
                            bstring const *restrict path,
                            bstring const *restrict file,
                            unsigned                file_offset);
static unsigned cannon_dotpath(char          *restrict buf,
                               bstring const *restrict path,
                               bstring const *restrict file,
                               unsigned                file_offset);


b_list *
parse_json(const bstring *json_path, const bstring *filename, b_list *includes)
{
        struct stat st;
        if (stat(BS(json_path), &st) != 0)
                err(1, "Failed to stat file %s", BS(json_path));

        FILE          *fp    = safe_fopen(BS(json_path), "rb");
        const ssize_t  size  = st.st_size;
        uint8_t       *buf   = xmalloc(size + 1);
        const ssize_t  nread = fread(buf, 1, size + 1, fp);
        buf[size]            = '\0';
        fclose(fp);
        assert(nread == size);

        int64_t pos = b_strrchr(json_path, '/');
        if (pos == (-1)) {
#ifdef DOSISH
                pos = b_strrchr(json_path, '\\');
                if (pos == (-1))
#endif
                        pos = json_path->slen;
        }
        bstring *base = b_fromblk(json_path->data, pos);

        {
                char path_buf[SAFE_PATH_MAX + 1];
                char *path = realpath(BS(base), path_buf);
                b_assign_cstr(base, path);
        }

        ECHO("Base is %s", base);
        ECHO("Determining number of json tokens.");
        jsmn_parser p;
        jsmn_init(&p);
        const int  ntoks = jsmn_parse(&p, (char *)buf, size, NULL, 0);
        jsmntok_t *toks  = nmalloc(ntoks, sizeof(*toks));

        ECHO("Parsing json file (%d toks).", ntoks);
        jsmn_init(&p);
        const int nparsed = jsmn_parse(&p, (char *)buf, size, toks, ntoks);
        assert(nparsed == ntoks);

        const bstring js_tmp = bt_fromblk(buf, size);
        b_list        *ret   = extract_info(toks, ntoks, &js_tmp,
                                            filename, base, includes);

        b_free(base);
        xfree(toks);
        xfree(buf);

        return ret;
}


static b_list *
extract_info(const jsmntok_t *toks,
             const int        ntoks,
             const bstring   *js,
             const bstring   *filename,
             const bstring   *base,
             b_list          *includes)
{
        int arr_top = 0;
        int file_arr = (-1);

        for (int i = 0; i < ntoks; ++i) {
                if (toks[i].type == JSMN_OBJECT) {
                        arr_top = i;
                } else if (toks[i].type == JSMN_STRING) {
                        assert((unsigned)toks[i].end <= js->slen);
                        bstring *key = b_fromblk(b_at(js, toks[i].start),
                                                 toks[i].end - toks[i].start);
                        ++i;

                        if (!b_iseq(key, B("file"))) {
                                b_free(key);
                                continue;
                        }
                        b_free(key);

                        ASSERTX((unsigned)toks[i].end <= js->slen,
                                "offset %d is invalid (max %u)\n", toks[i].end, js->slen);
                        bstring *value = b_fromblk(b_at(js, toks[i].start),
                                                   toks[i].end - toks[i].start);
                        bstring *tmp;
                        char     buf[SAFE_PATH_MAX + 1];
                        /* ECHO("Value is %s", BS(value)); */

                        if (value->data[0] == '.') {
                                /* ECHO("is dotpath"); */
                                const unsigned size = cannon_dotpath(buf, base, value, 0);
                                tmp                 = btp_fromblk(buf, size);
                        } else if (ISROOT(value->data)) {
                                /* ECHO("is root"); */
                                tmp = value;
                        } else {
                                const unsigned size = cannon_path(buf, base, value, 0);
                                tmp                 = btp_fromblk(buf, size);
                        }

                        /* ECHO("tmp is %s", BS(tmp)); */
#ifdef DOSISH
                        for (unsigned x = 0; x < tmp->slen; ++x)
                                if (tmp->data[x] == '/')
                                        tmp->data[x] = '\\';
#endif

                        if (B_FILE_EQ(filename, tmp)) {
                                file_arr = arr_top;
                                b_free(value);
                                break;
                        }
                        b_free(value);
                        value = NULL;
                }
        }

        /* ECHO("file_arr is %d", file_arr); */
        if (file_arr == (-1))
                return NULL;
        b_list *ret = b_list_create();

        for (int i = (file_arr + 1); i < ntoks; ++i) {
                if (toks[i].type != JSMN_STRING)
                        continue;
                assert((unsigned)toks[i].end <= js->slen);

                bstring *key = b_fromblk(b_at(js, toks[i].start),
                                         toks[i].end - toks[i].start);
                ++i;
                assert((unsigned)toks[i].end <= js->slen);

                if (b_iseq(key, B("command"))) {
                        bstring *cmd = b_fromblk(b_at(js, toks[i].start),
                                                 toks[i].end - toks[i].start);
                        char    *tok = BS(cmd);

                        while ((tok = strstr(tok, "-I"))) {
                                /* ECHO("Found '%s'", tok); */
                                tok += 2;
                                SKIP_SPACE_PTR(tok);
                                int want;

                                switch (*tok) {
                                case '"':  want = '"';  break;
                                case '\'': want = '\''; break;
                                default:   want = ' ';  break;
                                }

                                char *end = strchrnul(tok, want);
                                //const bstring btok[1] = {bt_fromblk(tok, PSUB(end, tok))};
                                //const bstring btok[] = {b_static_fromblk(tok, PSUB(end, tok))};
                                bstring btok[1];
                                *btok = bt_fromblk(tok, (unsigned)PSUB(end, tok));

                                //bstring *btok = b_refblk(tok, PSUB(end, tok));
                                char           buf[SAFE_PATH_MAX + 1], *newtok;
                                unsigned       newsize;

                                if (*tok == '.') {
                                        //ECHO("A dotfile...");
                                        newsize = cannon_dotpath(buf, base, btok, 0);
                                        newtok  = buf;
                                } else if (ISROOT(tok)) {
                                        //ECHO("An absolute path...");
                                        newsize = btok->slen;
                                        newtok  = tok;
                                } else {
                                        //ECHO("A bare file...");
                                        newsize = cannon_path(buf, base, btok, 0);
                                        newtok  = buf;
                                }

                                b_list_append(&ret, b_fromblk(newtok, newsize));
                                /* ECHO("Which is now '%s'", BS(ret->lst[ret->qty - 1])); */
                                tok = end;
                                //b_free(btok);
                        }

                        b_free(cmd);
                        b_free(key);
                        break;
                }
                if (b_iseq(key, B("arguments"))) {
                        do {
                                bstring *arg = b_fromblk(b_at(js, toks[i].start),
                                                         toks[i].end - toks[i].start);

                                if (arg->data[0] == '-' && arg->data[1] == 'I') {
                                        /* ECHO("Found '%s'", BS(arg)); */
                                        char     buf[SAFE_PATH_MAX + 1], *newtok;
                                        unsigned size;

                                        if (arg->data[2] == '.') {
                                                /* ECHO("A dotfile..."); */
                                                size   = cannon_dotpath(buf, base, arg, 2);
                                                newtok = buf;
                                        } else if (ISROOT(arg->data)) {
                                                /* ECHO("An absolute path..."); */
                                                size   = arg->slen - 2u;
                                                newtok = (char *)(arg->data + 2);
                                        } else {
                                                /* ECHO("A bare file..."); */
                                                size   = cannon_path(buf, base, arg, 2);
                                                newtok = buf;
                                        }

                                        b_list_append(&ret, b_fromblk(newtok, size));
                                        /* ECHO("Which is now '%s'", BS(ret->lst[ret->qty - 1])); */
                                } else if (arg->slen > 9u && strncmp(BS(arg), SLS("-include")) == 0) {
                                        bstring *tmp = b_fromblk(arg->data + 9,
                                                                 arg->slen - 9u);
                                        //ECHO("Adding header '%s'", BS(tmp));
                                        b_list_append(&includes, tmp);
                                }

                                b_free(arg);
                        } while (toks[++i].type == JSMN_STRING);

                        b_free(key);
                        break;
                }

                b_free(key);
        }

        /* b_list_dump_nvim(ret); */
        return ret;
}


static unsigned
cannon_path(char *const restrict          buf,
            const bstring *const restrict path,
            const bstring *const restrict file,
            const unsigned                file_offset)
{
        const unsigned maxsize = (file->slen + path->slen - file_offset + 1u);

        memcpy(buf, path->data, path->slen);
        buf[path->slen] = '/';
        memcpy(buf + path->slen + 1u,
               file->data + file_offset,
               file->slen - file_offset);

        buf[maxsize] = '\0';
        return maxsize;
}


static unsigned
cannon_dotpath(char *const restrict          buf,
               const bstring *const restrict path,
               const bstring *const restrict file,
               const unsigned                file_offset)
{
        //const unsigned maxsize = (file->slen + path->slen - file_offset - 1u);

        //memcpy(buf, path->data, path->slen);
        //memcpy(buf + path->slen,
        //       file->data + file_offset + 1u,
        //       file->slen - file_offset - 1u);

        //buf[maxsize] = '\0';

        ECHO("Trying to fix path '%s' with base '%s'", path, file);
        errno = 0;
        if (chdir(BS(path)) == (-1))
                err(1, "Failed to chdir to path \"%s\"", BS(path));

        char newbuf[SAFE_PATH_MAX + 1];
        const char *ptr = BS(file) + file_offset;
        unsigned i = 0;

        while (!isblank(*ptr)) {
                if (i > 0 && *ptr == '\\' && *(ptr - 1) == '\\') {
                        ++ptr;
                        continue;
                }
                newbuf[i++] = *ptr++;
        }
        newbuf[i] = '\0';

        //ECHO("path: %s, file: %s, offset: %u, so -> %s", BS(path), newbuf, file_offset, BS(file) + file_offset);
        if (errno)
                err(1, "Failed to chdir to %s", BS(path));
        char *ret = realpath(newbuf, buf);

        return (unsigned)strlen(ret);
}
