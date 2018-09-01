#include "util/util.h"
#include <dirent.h>
#include <sys/stat.h>

#include "api.h"
#include "util/find.h"

#define SKIP_SPACE(STR_, CTR_)                \
        do {                                  \
                while (isblank((STR_)[CTR_])) \
                        ++(CTR_);             \
        } while (0)

#define MAX_HEADER_SEARCH_LEVEL 8
#define PATH_STR                (PATH_MAX + 1u)
#define LIT_STRLEN(STR_)        (sizeof(STR_) - 1llu)
#define CHAR_AT(UCHAR_, CTR_)   ((char *)(&((UCHAR_)[CTR_])))
#define B_SYS_OK                (BSTR_MASK_USR1)
#define B_SYS_WITH_DIR          (BSTR_MASK_USR2)
#define SEARCH_WITH_THREADS

#include "data.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "util/archive.h"
#include "util/generic_list.h"

static pthread_mutex_t searched_mutex = PTHREAD_MUTEX_INITIALIZER;

static b_list *  find_includes(struct bufdata *bdata);
static b_list *  find_src_dirs(struct bufdata *bdata, b_list *includes);
static b_list *  find_header_paths(const b_list *src_dirs, const b_list *includes);
static bstring * analyze_line(const bstring *line);
static void *    recurse_headers_shim(void *vdata);
static void      recurse_headers(b_list **headers, b_list **searched, b_list *src_dirs,
                                 const bstring *cur_header, int level);

static void handle_file(b_list *includes, bstring *file, const bstring *cur_header);

struct pdata {
        b_list **      searched;
        b_list *       src_dirs;
        const bstring *cur_header;
};

/*======================================================================================*/

b_list *
find_header_files(struct bufdata *bdata)
{
        b_list *includes = find_includes(bdata);
        b_list *src_dirs = find_src_dirs(bdata, includes);
        
        bstring *asswipe = NULL;
        if (!includes || !src_dirs) {
                if (includes)
                        b_list_destroy(includes);
                if (src_dirs)
                        b_list_destroy(src_dirs);
                return NULL;
        }

        b_list *headers = find_header_paths(src_dirs, includes);
        b_list_destroy(includes);

        if (!headers)
                goto fail;

        if (headers->qty > 0) {
                unsigned i;
                B_LIST_SORT_FAST(headers);
                b_list *copy = b_list_copy(headers);
                b_list *searched = b_list_create();

#ifdef SEARCH_WITH_THREADS
                pthread_t     *tid  = nalloca(sizeof(pthread_t), copy->qty);
                b_list       **all  = nalloca(sizeof(b_list *), copy->qty);
                struct pdata **data = nalloca(sizeof(struct pdata *), copy->qty);

                for (i = 0; i < copy->qty; ++i) {
                        data[i]  = xmalloc(sizeof(struct pdata));
                        *data[i] = (struct pdata){&searched, src_dirs, copy->lst[i]};
                        pthread_create(&tid[i], NULL, &recurse_headers_shim, data[i]); 
                }

                for (i = 0; i < copy->qty; ++i) {
                        pthread_join(tid[i], (void **)(all + i));
                        free(data[i]);
                }

                for (i = 0; i < copy->qty; ++i)
                        if (all[i])
                                b_list_merge(&headers, all[i], BSTR_M_DEL_SRC);
#else
                B_LIST_FOREACH(copy, file, i)
                        recurse_headers(&headers, &searched, src_dirs, file, 1);
#endif

                b_list_remove_dups(&headers);
                b_list_destroy(copy);
                b_list_destroy(searched);
        } else {
                b_list_destroy(headers);
                headers = NULL;
        }

fail:
        b_list_destroy(src_dirs);
        return headers;
}

/*======================================================================================*/

static b_list *
find_includes(struct bufdata *bdata)
{
        if (!bdata->lines || bdata->lines->qty == 0)
                return NULL;
        b_list *includes = b_list_create();

        LL_FOREACH_F (bdata->lines, node) {
                bstring *file = analyze_line(node->data);
                if (file) {
                        handle_file(includes, file, bdata->filename);
                        /* b_list_append(&includes, file);
                        b_list_append(&includes, b_dirname(bdata->filename)); */
                }
        }
        if (includes->qty == 0) {
                b_list_destroy(includes);
                includes = NULL;
        }

        return includes;
}

static b_list *
find_src_dirs(struct bufdata *bdata, b_list *includes)
{
        b_list  *src_dirs;
        bstring *json_file = find_file(BS(bdata->topdir->pathname),
                                       "compile_commands.json", FIND_SHORTEST);

        if (json_file) {
                src_dirs = parse_json(json_file, bdata->filename, includes);
                if (!src_dirs) {
                        ECHO("Found nothing at all!!!");
                        src_dirs = b_list_create();
                }
        } else {
                ECHO("Couldn't find compile_commands.json");
                src_dirs = b_list_create();
        }

        b_list_append(&src_dirs, b_strcpy(bdata->topdir->pathname));
        bstring *file_dir = b_dirname(bdata->filename);
        ECHO("File dir is: %s", file_dir);

        if (!b_iseq(bdata->topdir->pathname, file_dir)) {
                ECHO("Adding file dir to list");
                b_list_append(&src_dirs, file_dir);
        } else
                b_free(file_dir);

        b_free(json_file);
        return src_dirs;
}

/*======================================================================================*/

#ifdef DOSISH

static b_list *
find_header_paths(const b_list *src_dirs, const b_list *includes)
{
        if (includes->qty == 0 || src_dirs->qty == 0)
                return NULL;

        unsigned  i, num = 0;
        b_list   *headers = b_list_create_alloc(includes->qty);

        b_list *includes_clone = b_list_copy(includes);
        B_LIST_FOREACH(includes_clone, file, i) {
                if (file)
                        for (unsigned x = 0; x < file->slen; ++x)
                                if (file->data[x] == '/')
                                        file->data[x] = '\\';
        }
        for (i = 0; i < includes_clone->qty; i += 2) {
                bstring *file = includes_clone->lst[i];
                bstring *path = includes_clone->lst[i+1];
                bstring *tmp  = b_concat_all(path, B("\\"), file);

                struct stat st;
                if (stat(BS(tmp), &st) == 0) {
                        b_list_append(&headers, tmp);
                        b_free(file);
                        includes_clone->lst[i] = NULL;
                        ++num;
                }

                b_free(path);
                includes_clone->lst[i+1] = NULL;
        }
        B_LIST_FOREACH (src_dirs, dir, i) {
                unsigned x;
                B_LIST_FOREACH (includes_clone, file, x) {
                        if (file) {
                                struct stat st;
                                bstring *tmp = b_concat_all(dir, B("\\"), file);
                                if (stat(BS(tmp), &st) == 0) {
                                        b_list_append(&headers, tmp);
                                        b_free(file);
                                        includes_clone->lst[x] = NULL;
                                        ++num;
                                } else {
                                        b_free(tmp);
                                }
                        }
                }
        }
        if (headers->qty == 0) {
                b_list_destroy(headers);
                headers = NULL;
        }
        b_list_destroy(includes_clone);

        return headers;
}

/*--------------------------------------------------------------------------------------*/
#else /* not DOSISH */
/*--------------------------------------------------------------------------------------*/

static bstring *find_header_paths_system(bstring *file, const bstring *sysdir);

static b_list *
find_header_paths(const b_list *src_dirs, const b_list *includes)
{
        if (includes->qty == 0 || src_dirs->qty == 0)
                return NULL;

        unsigned  i, num = 0;
        b_list *headers        = b_list_create_alloc(includes->qty);
        b_list *includes_clone = b_list_clone(includes);

        for (i = 0; i < includes_clone->qty; i += 2) {
                bstring   *file = includes_clone->lst[i];
                bstring   *path = includes_clone->lst[i+1];
                bstring   *tmp;
                struct stat st;
                if (file->data[0] == '/')
                        tmp = b_strcpy(file);
                else
                        tmp = b_concat_all(path, B("/"), file);

                if (stat(BS(tmp), &st) == 0) {
                add:    b_list_append(&headers, tmp);
                        b_free(file);
                        includes_clone->lst[i] = NULL;
                        ++num;
                } else {
                        b_free(tmp);
                        int pos = b_strchr(file, '/');
                        if (pos >= 0) {
                                char *base = (char *)(&file->data[pos + 1]);
                                char  newpath[PATH_STR];
                                snprintf(newpath, PATH_STR, "%s/%s", BS(path), base);
                                if (stat(newpath, &st) == 0) {
                                        tmp = b_fromcstr(newpath);
                                        goto add;
                                }
                        }
                }

                b_free(path);
                includes_clone->lst[i+1] = NULL;
        }

        B_LIST_FOREACH (src_dirs, dir, i) {
                if (!dir)
                        continue;
                unsigned x;

                int dirfd = open(BS(dir), O_PATH|O_DIRECTORY);
                if (dirfd == (-1)) {
                        warn("Failed to open directory '%s'", BS(dir));
                        continue;
                }

                B_LIST_FOREACH (includes_clone, file, x) {
                        if (file) {
                                struct stat st;
                                if (fstatat(dirfd, BS(file), &st, 0) == 0) {
                                        b_list_append(&headers,
                                                      b_concat_all(dir, B("/"), file));
                                        b_free(file);
                                        includes_clone->lst[x] = NULL;
                                        ++num;
                                }
                        }
                }

                close(dirfd);
        }

        if (headers->qty == 0) {
                b_list_destroy(headers);
                headers = NULL;
        }
        b_list_destroy(includes_clone);

        return headers;
}

static bstring *
find_header_paths_system(bstring *file, const bstring *sysdir)
{
        const char *paths[3] = {"/usr/include", "/usr/local/include", NULL};
        bstring    *ret     = NULL;
        if (sysdir)
                paths[2] = BS(sysdir);

        eprintf("Trying to find %s in the system directories\n", BS(file));

        if (b_strchr(file, '/') >= 0) {
                char     p[PATH_STR];
                bstring *d = b_dirname(file);
                bstring *f = b_basename(file);

                for (int i = 0; i < ARRSIZ(paths) && !ret; ++i) {
                        if (!paths[i] || !paths[i][0])
                                continue;
                        snprintf(p, PATH_STR, "%s/%s", paths[i], BS(d));
                        const int fd = open(p, O_DIRECTORY|O_PATH);
                        if (fd == (-1))
                                continue;
                        ret = find_file(p, BS(f), FIND_SHORTEST);
                        close(fd);
                }

                b_free(d);
                b_free(f);
        } else {
                for (int i = 0; i < ARRSIZ(paths) && !ret; ++i)
                        ret = find_file(paths[i], BS(file), FIND_SHORTEST);
        }

        return ret;
}

#endif /* DOSISH */

/*======================================================================================*/

static void *
recurse_headers_shim(void *vdata)
{
        struct pdata *data    = vdata;
        b_list       *headers = b_list_create();

        pthread_mutex_lock(&searched_mutex);
        b_list_append(data->searched, b_strcpy(data->cur_header)); 
        pthread_mutex_unlock(&searched_mutex);

        recurse_headers(&headers, data->searched,
                        data->src_dirs, data->cur_header, 1);

        pthread_exit(headers);
}

static void
recurse_headers(b_list **headers, b_list **searched, b_list *src_dirs,
                const bstring *cur_header, const int level)
{
        if (level > MAX_HEADER_SEARCH_LEVEL || !cur_header)
                return;
        unsigned i;

        if (level > 1) {
                pthread_mutex_lock(&searched_mutex);
                B_LIST_FOREACH (*searched, file, i) {
                        if (b_iseq(cur_header, file)) {
                                pthread_mutex_unlock(&searched_mutex);
                                return;
                        }
                }
                b_list_append(searched, b_strcpy(cur_header)); 
                pthread_mutex_unlock(&searched_mutex);
        }

#if 0
        bstring *d = b_dirname(cur_header);
        if (b_iseq(d, B("/usr/include")) && !(cur_header->flags & B_SYS_OK)) {
                b_destroy(d);
                return;
        }
        b_destroy(d);
#endif

        bstring  line[] = {{0, 0, NULL, 0}};
        bstring *slurp  = b_quickread(BS(cur_header));
        if (!slurp)
                return;

        b_list  *includes = b_list_create();
        uint8_t *bak      = slurp->data;

        while (b_memsep(line, slurp, '\n')) {
                bstring *file = analyze_line(line);
                if (file)
                        handle_file(includes, file, cur_header);
                b_free(line);
        }
        free(slurp);
        free(bak);

        b_list *new_headers = find_header_paths(src_dirs, includes);
        if (new_headers) {
                b_list *copy = b_list_copy(new_headers);
                b_list_merge(headers, new_headers, BSTR_M_DEL_SRC | BSTR_M_DEL_DUPS);
                B_LIST_FOREACH (copy, file, i)
                        recurse_headers(headers, searched, src_dirs, file, level + 1);
                b_list_destroy(copy);
        }

        b_list_destroy(includes);
}

static bstring *
analyze_line(const bstring *line)
{
        if (!line || !line->data)
                return NULL;
        const uchar *const str = line->data;
        const unsigned     len = line->slen;
        unsigned           i   = 0;
        bstring           *ret = NULL;
        SKIP_SPACE(str, i);

        if (str[i++] == '#') {
                SKIP_SPACE(str, i);

                if (strncmp(CHAR_AT(str, i), SLS("include")) == 0) {
                        i += LIT_STRLEN("include");
                        SKIP_SPACE(str, i);
                        int ch = str[i];

                        if (ch == '"' || ch == '<') {
                                ++i;
                                ch = (ch == '<') ? '>' : ch;
                                uchar *end = memchr(&str[i], ch, len - i);

                                if (end) {
                                        ret = b_fromblk(&str[i], PSUB(end, &str[i]));

                                        if (ch == '>') {
                                                int n = 0;
                                                if (b_strstr(line, B("/* TAG */"), i) > 0) {
                                                        bstring *tmp = find_header_paths_system(ret, NULL);
                                                        if (tmp) {
                                                                b_destroy(ret);
                                                                ret = tmp;
                                                        }
                                                        ret->flags |= B_SYS_OK;
                                                        b_fputs(stderr, ret, B("\n"));
                                                }
                                                else if ((n = b_strstr(line, B("/* TAG: "), i)) > 0) {
                                                        eprintf("Found %s!\n", &line->data[n]);
                                                        n += 8;
                                                        const int m  = b_strchrp(line, '*', n) - 1;
                                                        bstring ref  = bt_fromblk(line->data + n, m - n);
                                                        ref.data[ref.slen] = '\0';

                                                        bstring *tmp = find_header_paths_system(ret, &ref);
                                                        if (tmp) {
                                                                b_destroy(ret);
                                                                ret = tmp;
                                                        }
                                                        ret->flags |= B_SYS_OK | B_SYS_WITH_DIR;
                                                }
                                        }
                                }
                        }
                }
        }

        return ret;
}

static void
handle_file(b_list *includes, bstring *file, const bstring *cur_header)
{
#if 0
        if (file->flags & B_SYS_OK) {
                if (file->flags & B_SYS_WITH_DIR) {
                        size_t n      = strlen(BS(file));
                        char * sysdir = (char *)(file->data + n + 1);
                        file->slen    = n;
                        fprintf(stderr, "Found %s, with '%s' and %02X, %02X\n",
                                BS(file), sysdir, file->flags, BSTR_STANDARD);

                        b_list_append(&includes, file);
                        b_list_append(&includes, b_fromcstr(sysdir));
                } else {
                        fprintf(stderr, "Found %s, with no dir and %02X, %02X\n",
                                BS(file), file->flags, BSTR_STANDARD);
                        b_list_append(&includes, file);
                        b_list_append(&includes, (bstring *)NULL);
                }
        } else {
#endif
                b_list_append(&includes, file);
                if (!(file->flags & B_SYS_OK))
                        b_list_append(&includes, b_dirname(cur_header));
                else
                        b_list_append(&includes, b_fromcstr(""));
#if 0
        }
#endif
}
