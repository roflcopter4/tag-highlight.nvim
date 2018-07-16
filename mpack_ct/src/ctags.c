#include "util.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "archive/archive_util.h"
#include "data.h"
#include "highlight.h"
#include "mpack.h"

static inline void write_gzfile(struct top_dir *topdir);
static b_list *    find_header_files(struct bufdata *bdata, struct top_dir *topdir);
static b_list *    find_src_dirs(struct bufdata *bdata, struct top_dir *topdir);
static b_list *    find_includes(struct bufdata *bdata, struct top_dir *topdir);
static void        recurse_headers(b_list **headers, b_list **searched, b_list *src_dirs, const bstring *cur_header, int level);
static bstring *   analyze_line(const bstring *line);
static b_list *    find_header_paths(const b_list *src_dirs, const b_list *includes);


static void * recurse_headers_thread(void *vdata);


/*======================================================================================*/

struct pdata {
        b_list **      searched;
        b_list *       src_dirs;
        const bstring *cur_header;
};

static FILE *          header_log;
static pthread_mutex_t searched_mutex = PTHREAD_MUTEX_INITIALIZER;

/*======================================================================================*/

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

#define COMMANDFILE "compile_commands.json"
#define PATH_STR (PATH_MAX + 1)

#define CHECK_FILE(BUF_, ...)                            \
        __extension__({                                  \
                snprintf((BUF_), PATH_STR, __VA_ARGS__); \
                struct stat st;                          \
                errno = 0;                               \
                stat((BUF_), &st);                       \
                errno;                                   \
        })

#define LIT_STRLEN(STR_)       (sizeof(STR_) - 1llu)
#define CHAR_AT(UCHAR_, CTR_)  ((char *)(&((UCHAR_)[CTR_])))

#ifdef __GNUC__
#  define VLA(VARIABLE, FIXED) (VARIABLE)
#else
#  define VLA(VARIABLE, FIXED) (FIXED)
#endif

/*======================================================================================*/


bool
run_ctags(struct bufdata *bdata, struct top_dir *topdir)
{
        assert(topdir != NULL);

        b_list *headers = NULL;
        header_log = safe_fopen_fmt("%s/headerlist.log", "wb", HOME);

        if (topdir->is_c)
                headers = find_header_files(bdata, topdir);
        if (bdata->cmd_cache) {
                b_list_destroy(bdata->cmd_cache);
                bdata->cmd_cache = NULL;
        }
        bstring *cmd = b_fromcstr_alloc(2048, "ctags ");

        for (unsigned i = 0; i < settings.ctags_args->qty; ++i) {
                b_concat(cmd, settings.ctags_args->lst[i]);
                b_conchar(cmd, ' ');
        }

        if (headers) {
                B_LIST_SORT(headers);
                /* FILE *fp = safe_fopen_fmt("%s/headerlist.log", "wb", HOME); */
                b_dump_list(header_log, headers);

                bstring *tmp = b_join(headers, B(" "));
                b_formata(cmd, " -f%s '%s' %s", BS(topdir->tmpfname), BS(bdata->filename), BS(tmp));
                b_destroy(tmp);

                b_list_destroy(headers);
        } else if (topdir->recurse && !topdir->is_c) {
                b_formata(cmd, " --languages='%s' -R -f'%s' '%s'", BTS(bdata->ft->ctags_name), BS(topdir->tmpfname), BS(topdir->pathname));
        } else {
                echo("Not recursing!!!");
                b_formata(cmd, " -f%s '%s'", BS(topdir->tmpfname), BS(bdata->filename));
        }

        echo("Running ctags command \"%s\"\n", BS(cmd));

        /* Yes, this directly uses unchecked user input in a call to system().
         * Frankly, if somehow someone takes over a user's vimrc then they're
         * already compromised, and if the user wants to attack their own
         * system for some reason then they can be my guest. */
        int status = system(BS(cmd));

        if (status != 0)
                echo("ctags failed with status \"%d\"\n", status);
        else
                echo("Ctags finished successfully.");
        b_destroy(cmd);

        fclose(header_log);
        return (status == 0);
}


void
get_initial_taglist(struct bufdata *bdata, struct top_dir *topdir)
{
        struct stat st;
        errno = 0;
        topdir->tags = b_list_create();

        if (stat(BS(topdir->gzfile), &st) == 0) {
                getlines(topdir->tags, settings.comp_type, topdir->gzfile);
                for (unsigned i = 0; i < topdir->tags->qty; ++i)
                        b_write(topdir->tmpfd, topdir->tags->lst[i], B("\n"));
        } else {
                if (errno == ENOENT)
                        echo("File \"%s\" not found, running ctags.\n",
                             BS(topdir->gzfile));
                else
                        warn("Unexpected io error");

                run_ctags(bdata, topdir);
                write_gzfile(topdir);
                errno = 0;

                if (stat(BS(topdir->gzfile), &st) != 0)
                        err(1, "Failed to stat gzfile");

                getlines(topdir->tags, COMP_NONE, topdir->tmpfname);
        }
}


bool
update_taglist(struct bufdata *bdata)
{
        if (bdata->ctick == bdata->last_ctick)
                return false;

        bdata->last_ctick = bdata->ctick;
        assert(run_ctags(bdata, bdata->topdir));

        getlines(bdata->topdir->tags, COMP_NONE, bdata->topdir->tmpfname);
        write_gzfile(bdata->topdir);

        return true;
}


static inline void
write_gzfile(struct top_dir *topdir)
{
        echo("Compressing tagfile.");
        switch (settings.comp_type) {
        case COMP_NONE: write_plain(topdir); break;
        case COMP_GZIP: write_gzip(topdir);  break;
        case COMP_LZMA: write_lzma(topdir);  break;
        default:        abort();
        }
        echo("Finished compressing tagfile!");
}


/*======================================================================================*/

#define SEARCH_WITH_THREADS


static b_list *
find_header_files(struct bufdata *bdata, struct top_dir *topdir)
{
        b_list *includes = find_includes(bdata, topdir);
        b_list *src_dirs = find_src_dirs(bdata, topdir);
        if (!includes || !src_dirs)
                return NULL;
        b_list *headers  = find_header_paths(src_dirs, includes);

        b_list_destroy(includes);
        if (!headers)
                return NULL;

        if (headers->qty > 0) {
                unsigned i;
                B_LIST_SORT_FAST(headers);
                b_list *copy = b_list_copy(headers);
                b_list *searched = b_list_create();

#ifdef SEARCH_WITH_THREADS
                /* b_list **all = nmalloc(sizeof(b_list *), copy->qty); */
                /* pthread_t *tid = nmalloc(sizeof(pthread_t), copy->qty); */
                /* struct pdata **data = nmalloc(sizeof(struct pdata *), copy->qty); */
                pthread_t     tid[copy->qty];
                struct pdata *data[copy->qty];
                b_list       *all[copy->qty];

                B_LIST_FOREACH(copy, file, i) {
                /* for (i = 0; i < copy->qty; ++i) { */
                        data[i]  = xmalloc(sizeof(struct pdata));
                        *data[i] = (struct pdata){&searched, src_dirs, copy->lst[i]};
                        pthread_create(&tid[i], NULL, &recurse_headers_thread, data[i]); 
                        /* recurse_headers(&headers, &searched, src_dirs, file, 1); */
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
        } else {
                b_list_destroy(headers);
                headers = NULL;
        }

        return headers;
}


static void *
recurse_headers_thread(void *vdata)
{
        struct pdata *data = vdata;
        /* if (b_list_bsearch_fast(*searched, cur_header))
                return; */

        pthread_mutex_lock(&searched_mutex);
        b_add_to_list(data->searched, b_strcpy(data->cur_header)); 
        /* B_LIST_SORT_FAST(*data->searched); */
        pthread_mutex_unlock(&searched_mutex);

        b_list  *includes    = b_list_create();
        FILE    *fp          = safe_fopen(BS(data->cur_header), "rb");
        bstring *line        = NULL;

        while ((line = B_GETS(fp, '\n'))) {
                bstring *file = analyze_line(line);
                if (file)
                        b_add_to_list(&includes, file);
                b_free(line);
        }
        fclose(fp);

        b_list *headers = find_header_paths(data->src_dirs, includes);
        if (!headers) {
                fprintf(stderr, "Found nothing!\n");
                pthread_exit(NULL);
        }

        unsigned i;
        b_list *copy = b_list_copy(headers);

        B_LIST_FOREACH (copy, file, i)
                recurse_headers(&headers, data->searched, data->src_dirs, file, 2);

        b_list_destroy(copy);
        b_list_remove_dups(&headers);
        pthread_exit(headers);
}


static void
recurse_headers(b_list **headers, b_list **searched, b_list *src_dirs, const bstring *cur_header, const int level)
{
        if (level > 5)
                return;
        unsigned i;

        /* if (b_list_bsearch_fast(*searched, cur_header)) { */

        pthread_mutex_lock(&searched_mutex);
        B_LIST_FOREACH(*searched, file, i) {
                if (b_iseq(cur_header, file)) {
                        fprintf(stderr, "Already searched %s!\n", BS(cur_header));
                        pthread_mutex_unlock(&searched_mutex);
                        return;
                }
        }
        b_add_to_list(searched, b_strcpy(cur_header)); 
        /* B_LIST_SORT_FAST(*searched); */
        pthread_mutex_unlock(&searched_mutex);

        b_list  *includes    = b_list_create();
        FILE    *fp          = safe_fopen(BS(cur_header), "rb");
        bstring *line        = NULL;

        while ((line = B_GETS(fp, '\n'))) {
                bstring *file = analyze_line(line);
                if (file)
                        b_add_to_list(&includes, file);
                b_free(line);
        }
        fclose(fp);

        b_list *new_headers = find_header_paths(src_dirs, includes);
        if (!new_headers)
                return;

        b_list *copy = b_list_copy(new_headers);
        b_list_merge(headers, new_headers, BSTR_M_DEL_SRC | BSTR_M_DEL_DUPS);
        B_LIST_FOREACH (copy, file, i)
                recurse_headers(headers, searched, src_dirs, file, level + 1);
        b_list_destroy(copy);
}


/*======================================================================================*/


static b_list *
find_includes(struct bufdata *bdata, struct top_dir *topdir)
{
        b_list *includes = b_list_create();

        if (!bdata->lines || bdata->lines->qty == 0) {
                unsigned i;
                b_list *lines = nvim_buf_get_lines(0, bdata->num, 0, -1);
                B_LIST_FOREACH (lines, cur, i) {
                        bstring *file = analyze_line(cur);
                        if (file)
                                b_add_to_list(&includes, file);
                }
                b_list_destroy(lines);
        } else {
                LL_FOREACH_F (bdata->lines, node) {
                        bstring *file = analyze_line(node->data);
                        if (file)
                                b_add_to_list(&includes, file);
                }
        }
        
        if (includes->qty == 0) {
                b_list_destroy(includes);
                includes = NULL;
        }

        return includes;
}


static b_list *
find_src_dirs(struct bufdata *bdata, struct top_dir *topdir)
{
        b_list  *src_dirs;
        char     path[PATH_STR];
        char     cur_file[PATH_MAX + 64];
        size_t   size = snprintf(cur_file, (PATH_MAX + 64),
                                 ("  \"file\": \"%s\""), BS(bdata->filename));
        bstring *find = bt_fromblk(cur_file, size);

        if (CHECK_FILE(path, "./%s", COMMANDFILE) == 0) {
        } else if (CHECK_FILE(path, "../%s", COMMANDFILE) == 0) {
        } else if (CHECK_FILE(path, "%s/%s", BS(topdir->pathname), COMMANDFILE) == 0) {
        } else {
                echo("Couldn't find compile_commands.json");
                if (topdir->pathname) {
                        src_dirs = b_list_create();
                        b_add_to_list(&src_dirs, b_strcpy(topdir->pathname));
                        goto skip;
                } else
                        errx(1, "Cannot continue.");
        }

        echo("opening file \"%s\"", path);
        bstring *line = NULL, *cmd = NULL;
        b_list  *tmp  = b_list_create();
        FILE *compile = fopen(path, "r");

        if (!compile)
                err(1, "Failed to open file %s", path);

        while ((line = B_GETS(compile, '\n'))) {
                if ((line->slen >= 2) && (line->data[line->slen - 2] == ','))
                        line->data[(line->slen -= 2)] = '\0';
                else
                        line->data[(line->slen -= 1)] = '\0';

                b_add_to_list(&tmp, line);

                if (b_iseq(line, find)) {
                        if (strncmp(BS(tmp->lst[tmp->qty - 2]), ("  \"command\":"), 12) == 0) {
                                cmd = b_fromblk((tmp->lst[(tmp->qty)-2]->data + 14),
                                                (tmp->lst[(tmp->qty)-2]->slen - 15));
                                break;
                        }
                }
        }

        b_list_destroy(tmp);
        src_dirs = b_list_create();

        if (cmd) {
                char *tok = BS(cmd);
                while ((tok = strstr(tok, "-I"))) {
                        tok += 2;
                        SKIP_SPACE_PTR(tok);
                        int want;

                        switch (*tok) {
                        case '"':  want = '"';  break;
                        case '\'': want = '\''; break;
                        default:   want = ' ';  break;
                        }

                        char *end = strchrnul(tok, want);
                        bstring *filename = b_fromblk(tok, PSUB(end, tok));
                        b_add_to_list(&src_dirs, filename);

                        tok = end;
                }
        } else {
                b_add_to_list(&src_dirs, b_strcpy(topdir->pathname));
        }


skip:
        {
#if 0
                uchar   *ptr      = memrchr(bdata->filename->data, '/', bdata->filename->slen);
                size_t   len      = (ptrdiff_t)ptr - (ptrdiff_t)bdata->filename->data;
#endif
                int64_t len = b_strrchr(bdata->filename, '/');
                bstring *file_dir = b_fromblk(bdata->filename->data, len);

                b_fputs(stderr, B("File dir is "), file_dir, B("\n"));

                if (!b_iseq(topdir->pathname, file_dir))
                        b_add_to_list(&src_dirs, file_dir);
                else
                        b_free(file_dir);
        }
        /* b_add_to_list(&src_dirs, b_lit2bstr("/usr/include"));
        b_add_to_list(&src_dirs, b_lit2bstr("/usr/local/include")); */

        b_dump_list(header_log, src_dirs);

        return src_dirs;
}
/*======================================================================================*/


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

                        if (str[i] == '"') {
                                ++i;
                                uchar *end = memchr(&str[i], '"', len - i);
                                if (end) {
                                        /* const size_t slen = ((ptrdiff_t)(end) - */
                                                             /* (ptrdiff_t)(&str[i])); */

                                        const size_t slen = PSUB(end, str + i);
                                        ret = b_fromblk(&str[i], slen);
                                }
                        }
#if 0
                        else if (str[i] == '<') {
                                ++i;
                                uchar *end = memchr(&str[i], '>', len - i);
                                if (end) {
                                        const size_t slen = ((ptrdiff_t)(end) -
                                                             (ptrdiff_t)(&str[i]));
                                        ret = b_fromblk(&str[i], slen);
                                }
                        }
#endif
                }
        }

        return ret;
}


static b_list *
find_header_paths(const b_list *src_dirs, const b_list *includes)
{
        unsigned  i, num = 0;
        b_list   *headers        = b_list_create_alloc(includes->qty);
        b_list   *includes_clone = b_list_clone(includes);

        B_LIST_FOREACH (src_dirs, dir, i) {
                unsigned x;
                int      dirfd = open(BS(dir), O_RDONLY|O_DIRECTORY);
                assert(dirfd > 2);

                B_LIST_FOREACH (includes_clone, file, x) {
                        struct stat st;
                        /* if (file && fstatat(dirfd, BS(file), &st, 0) == 0) { */
                        if (file) {
                                errno = 0;
                                fstatat(dirfd, BS(file), &st, 0);
                                if (errno != ENOENT) {
                                        b_add_to_list(&headers, b_concat_all(dir, B("/"), file));
                                        b_destroy(file);
                                        includes_clone->lst[x] = NULL;
                                        ++num;
                                }
                        }
                }
                close(dirfd);
        }

        /* B_LIST_FOREACH (includes_clone, file, i)
                if (file)
                        echo("Failed to find file %s", BS(file)); */


        if (headers->qty == 0) {
                b_list_destroy(headers);
                headers = NULL;
        }
        b_list_destroy(includes_clone);

        return headers;
}
