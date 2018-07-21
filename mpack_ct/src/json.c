#include "util.h"
#include <sys/stat.h>

#include "contrib/jsmn/jsmn.h"
#include "mpack.h"

/* #define b_range(BSTR_, START_, END_) ({ \
        const unsigned start = (START_);\
        const unsigned end   = (END_);\
        const bstring *bstr  = (BSTR_); \
        assert(end <= bstr->slen); \
        (char *)(bstr->data */

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


static b_list * locate_file(const jsmntok_t *toks, const int ntoks, const bstring *js,
                            const bstring *filename, const bstring *base, b_list *includes);


b_list *
parse_json(const bstring *json_path, const bstring *filename, b_list *includes)
{
        struct stat st;
        if (stat(BS(json_path), &st) != 0)
                err(1, "Failed to stat file %s", BS(json_path));

        FILE *        fp    = safe_fopen(BS(json_path), "rb");
        const ssize_t size  = st.st_size;
        uint8_t *     buf   = xmalloc(size + 1);
        const ssize_t nread = fread(buf, 1, size + 1, fp);
        buf[size]           = '\0';
        fclose(fp);
        assert(nread == size);

        const int64_t pos  = b_strrchr(json_path, '/');
        bstring *     base = b_fromblk(json_path->data, pos);

        {
                char path_buf[PATH_MAX + 1];
                char *path = realpath(BS(base), path_buf);
                b_assign_cstr(base, path);
        }

        echo("Looking for '%s' in '%s'.", BS(filename), BS(base));

        jsmn_parser p;
        jsmn_init(&p);
        const int  ntoks = jsmn_parse(&p, (char *)buf, size, NULL, 0);
        jsmntok_t *toks  = nmalloc(sizeof(*toks), ntoks);

        jsmn_init(&p);
        const int nparsed = jsmn_parse(&p, (char *)buf, size, toks, ntoks);
        assert(nparsed == ntoks);

        echo("found %d tokens in the file that is %zd long", ntoks, size);

        const bstring *js_tmp = bt_fromblk(buf, size);
        
        b_list *ret = locate_file(toks, ntoks, js_tmp, filename, base, includes);
        b_destroy(base);
        free(toks);

        return ret;
}


static b_list *
locate_file(const jsmntok_t *toks,
            const int        ntoks,
            const bstring *  js,
            const bstring *  filename,
            const bstring *  base,
            b_list *         includes)
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
                                b_destroy(key);
                                continue;
                        }
                        b_destroy(key);

                        ASSERTX((unsigned)toks[i].end <= js->slen,
                                "offset %d is invalid (max %u)\n", toks[i].end, js->slen);
                        bstring *value = b_fromblk(b_at(js, toks[i].start),
                                                   toks[i].end - toks[i].start);
                        bstring *tmp;
                        char buf[PATH_MAX + 1];

                        if (value->data[0] == '.') {
                                memcpy(buf, base->data, base->slen);
                                memcpy(buf + base->slen, value->data + 1, value->slen - 1);
                                const unsigned size = base->slen + value->slen - 1;
                                buf[size]           = '\0';
                                tmp                 = bt_fromblk(buf, size);
                        } else if (value->data[0] != '/') {
                                memcpy(buf, base->data, base->slen);
                                buf[base->slen] = '/';
                                memcpy((buf + base->slen + 1), value->data, value->slen);
                                const unsigned size = base->slen + value->slen + 1;
                                buf[size]           = '\0';
                                tmp                 = bt_fromblk(buf, size);

                        } else {
                                tmp = value;
                        }
                        
                        /* echo("Comparing '%s' to '%s'", BS(filename), BS(tmp)); */

                        if (b_iseq(filename, tmp)) {
                                file_arr = arr_top;
                                b_destroy(value);
                                break;
                        }
                        b_destroy(value);
                        value = NULL;
                }
        }

        echo("file_arr is %d", file_arr);
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
                        echo("found command!");

                        while ((tok = strstr(tok, "-I"))) {
                                tok += 2;
                                SKIP_SPACE_PTR(tok);
                                int want;

                                switch (*tok) {
                                case '"':  want = '"';  break;
                                case '\'': want = '\''; break;
                                default:   want = ' ';  break;
                                }

                                char    *end     = strchrnul(tok, want);
                                size_t   toksize = PSUB(end, tok);
                                unsigned newsize;

                                if (*tok == '.') {
                                        char buf[PATH_MAX + 1];
                                        memcpy(buf, base->data, base->slen);
                                        memcpy((buf + base->slen), (tok + 1), (toksize - 1));

                                        newsize      = base->slen + toksize - 1u;
                                        buf[newsize] = '\0';
                                } else if (*tok != '/') {
                                        char buf[PATH_MAX + 1];
                                        memcpy(buf, base->data, base->slen);
                                        buf[base->slen] = '/';
                                        memcpy((buf + base->slen + 1), (tok + 1), toksize);

                                        newsize      = base->slen + toksize + 1u;
                                        buf[newsize] = '\0';
                                } else {
                                        newsize = toksize;
                                }

                                b_add_to_list(&ret, b_fromblk(tok, newsize));
                                tok = end;
                        }

                        b_destroy(cmd);
                        break;
                }
                if (b_iseq(key, B("arguments"))) {
                        do {
                                bstring *arg = b_fromblk(b_at(js, toks[i].start),
                                                          toks[i].end - toks[i].start);
                                if (arg->data[0] == '-' && arg->data[1] == 'I') {
                                        echo("Found '%s'", BS(arg));
                                        if (arg->data[2] != '/') {
                                                char buf[PATH_MAX + 1];
                                                unsigned size;

                                                if (arg->data[2] == '.') {
                                                        size = (base->slen + arg->slen - 3u);
                                                        memcpy(buf, base->data, base->slen);
                                                        memcpy((buf + base->slen), (arg->data + 3), arg->slen - 1u);
                                                        buf[size] = '\0';
                                                } else {
                                                        size = (base->slen + arg->slen - 2u);
                                                        memcpy(buf, base->data, base->slen);
                                                        buf[base->slen] = '/';
                                                        memcpy((buf + base->slen + 1), (arg->data + 2), arg->slen);
                                                        buf[++size] = '\0';
                                                }
                                                b_add_to_list(&ret, b_fromblk(buf, size));
                                        } else {
                                                b_add_to_list(
                                                    &ret, b_fromblk(arg->data + 2,
                                                                    arg->slen - 2u));
                                        }
                                } else if (arg->slen > 9u && strncmp(BS(arg), SLS("-include")) == 0) {
                                        bstring *tmp = b_fromblk(arg->data + 9, arg->slen - 9);
                                        echo("Adding header '%s'", BS(tmp));
                                        b_add_to_list(&includes, tmp);
                                }

                                b_destroy(arg);
                        } while (toks[++i].type == JSMN_STRING);

                        break;
                }

                b_destroy(key);
        }

        b_dump_list_nvim(ret);
        return ret;
}
