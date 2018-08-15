#include "util.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef DOSISH
#  include <direct.h>
#  define B_FILE_EQ(FILE1_, FILE2_) (b_iseq_caseless((FILE1_), (FILE2_)))
#  define SEPSTR "\\"
#else
#  define B_FILE_EQ(FILE1_, FILE2_) (b_iseq((FILE1_), (FILE2_)))
#  define SEPSTR "/"
#endif

#define IS_DOTDOT(FNAME_) \
        ((FNAME_)[0] == '.' && (!(FNAME_)[1] || ((FNAME_)[1] == '.' && !(FNAME_)[2])))
#define MAX_HEADER_SEARCH_LEVEL 8

#include "archive/archive_util.h"
#include "data.h"
#include "generic_list.h"
#include "highlight.h"
#include "mpack.h"

static inline void write_gzfile(struct top_dir *topdir);
static b_list *    find_header_files(struct bufdata *bdata);
static b_list *    find_header_paths(const b_list *src_dirs, const b_list *includes);
static b_list *    find_includes(struct bufdata *bdata);
static b_list *    find_src_dirs(struct bufdata *bdata, b_list *includes);
static bstring *   analyze_line(const bstring *line);
static void        recurse_headers(b_list **headers, b_list **searched, b_list *src_dirs,
                                   const bstring *cur_header, int level);
static void *      recurse_headers_thread(void *vdata);
static bstring *   find_file_in_dir_recurse(const bstring *dirpath, const bstring *find);
static int         exec_ctags(struct bufdata *bdata, b_list *headers);
static int         myfilter(const struct dirent *dp);

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

#define COMMANDFILE           "compile_commands.json"
#define PATH_STR              (PATH_MAX + 1)
#define LIT_STRLEN(STR_)      (sizeof(STR_) - 1llu)
#define CHAR_AT(UCHAR_, CTR_) ((char *)(&((UCHAR_)[CTR_])))

#ifdef __GNUC__
#  define VLA(VARIABLE, FIXED) (VARIABLE)
#else
#  define VLA(VARIABLE, FIXED) (FIXED)
#endif

/*======================================================================================*/


bool
run_ctags(struct bufdata *bdata)
{
        assert(bdata != NULL && bdata->topdir != NULL);
        if (!bdata->lines || !bdata->initialized) {
                echo("File is empty, cannot run ctags");
                return false;
        }

        /* Look for header files if we're processing C or C++ code. */
        b_list *headers = NULL;
        if (bdata->topdir->is_c)
                headers = find_header_files(bdata);

        /* Wipe any cached commands if they exist. */
        if (bdata->cmd_cache) {
                b_list_destroy(bdata->cmd_cache);
                bdata->cmd_cache = NULL;
        }

        int status = exec_ctags(bdata, headers);

        if (status != 0)
                warnx("ctags failed with status \"%d\"\n", status);
        else
                echo("Ctags finished successfully.");

        return (status == 0);
}


int
get_initial_taglist(struct bufdata *bdata)
{
        struct stat st;
        int         ret     = 0;
        bdata->topdir->tags = b_list_create();

        if (have_seen_file(bdata->filename)) {
                echo("Seen file before, running ctags in case there was just a "
                     "momentary disconnect on write...");
                goto force_ctags;
        }
        errno = 0;

        /* Read the compressed tags file if it exists, otherwise we run ctags
         * and read the file it creates. If there is a read error in the saved
         * file, run ctags as a backup. */
        if (stat(BS(bdata->topdir->gzfile), &st) == 0) {
                ret += getlines(bdata->topdir->tags, settings.comp_type,
                                bdata->topdir->gzfile);
                if (ret) {
                        for (unsigned i = 0; i < bdata->topdir->tags->qty; ++i)
                                b_write(bdata->topdir->tmpfd,
                                        bdata->topdir->tags->lst[i], B("\n"));
                } else {
                        warnx("Could not read file. Running ctags.");
                        if (!bdata->initialized)
                                return 0;
                        echo("linecount -> %d", bdata->lines->qty);
                        goto force_ctags;
                }
        } else {
                if (errno == ENOENT)
                        echo("File \"%s\" not found, running ctags.\n",
                             BS(bdata->topdir->gzfile));
                else
                        warn("Unexpected io error");
        force_ctags:
                run_ctags(bdata);
                write_gzfile(bdata->topdir);
                errno = 0;

                if (stat(BS(bdata->topdir->gzfile), &st) != 0)
                        err(1, "Failed to stat gzfile");

                ret += getlines(bdata->topdir->tags, COMP_NONE, bdata->topdir->tmpfname);
        }

        return ret;
}


int
update_taglist(struct bufdata *bdata)
{
        if (bdata->ctick == bdata->last_ctick) {
                echo("ctick unchanged");
                return false;
        }

        bdata->last_ctick = bdata->ctick;
        if (!run_ctags(bdata))
                errx(1, "Ctags failed, cannot continue.");

        if (!getlines(bdata->topdir->tags, COMP_NONE, bdata->topdir->tmpfname)) {
                echo("Failed to read file");
                return false;
        }

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
#ifdef LZMA_SUPPORT
        case COMP_LZMA: write_lzma(topdir);  break;
#endif
        default:        abort();
        }
        echo("Finished compressing tagfile!");
}


/*======================================================================================*/

#define SEARCH_WITH_THREADS

struct pdata {
        b_list **      searched;
        b_list *       src_dirs;
        const bstring *cur_header;
};


static b_list *
find_header_files(struct bufdata *bdata)
{
        b_list *includes = find_includes(bdata);
        b_list *src_dirs = find_src_dirs(bdata, includes);
        
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
                        pthread_create(&tid[i], NULL, &recurse_headers_thread, data[i]); 
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


static b_list *
find_includes(struct bufdata *bdata)
{
        if (!bdata->lines || bdata->lines->qty == 0)
                return NULL;
        b_list *includes = b_list_create();

        LL_FOREACH_F (bdata->lines, node) {
                bstring *file = analyze_line(node->data);
                if (file) {
                        b_list_append(&includes, file);
                        b_list_append(&includes, b_dirname(bdata->filename));
                }
        }
        if (includes->qty == 0) {
                b_list_destroy(includes);
                includes = NULL;
        }

        return includes;
}


static FILE *yetanotherlog;

static b_list *
find_src_dirs(struct bufdata *bdata, b_list *includes)
{
        yetanotherlog = safe_fopen_fmt("%s/alphasort.log", "wb", HOME);
        b_list  *src_dirs;
        bstring *json_file = find_file_in_dir_recurse(bdata->topdir->pathname, B(COMMANDFILE));

        if (json_file) {
                src_dirs = parse_json(json_file, bdata->filename, includes);
                if (!src_dirs) {
                        echo("Found nothing at all!!!");
                        src_dirs = b_list_create();
                }
        } else {
                echo("Couldn't find compile_commands.json");
                src_dirs = b_list_create();
        }

        b_list_append(&src_dirs, b_strcpy(bdata->topdir->pathname));
        bstring *file_dir = b_dirname(bdata->filename);
        echo("File dir is: %s", BS(file_dir));

        if (!b_iseq(bdata->topdir->pathname, file_dir)) {
                echo("Adding file dir to list");
                b_list_append(&src_dirs, file_dir);
        } else
                b_free(file_dir);

        b_free(json_file);
        fclose(yetanotherlog);
        return src_dirs;
}


/*======================================================================================*/


static void *
recurse_headers_thread(void *vdata)
{
        struct pdata *data = vdata;

        pthread_mutex_lock(&searched_mutex);
        b_list_append(data->searched, b_strcpy(data->cur_header)); 
        pthread_mutex_unlock(&searched_mutex);

        b_list  *includes = b_list_create();
        bstring  line[]   = {{0, 0, NULL, 0}};
        bstring *slurp    = b_quickread(BS(data->cur_header));
        uint8_t *bak      = slurp->data;

        while (b_memsep(line, slurp, '\n')) {
                bstring *file = analyze_line(line);
                if (file) {
                        b_list_append(&includes, file);
                        b_list_append(&includes, b_dirname(data->cur_header));
                }
                b_free(line);
        }
        free(slurp);
        free(bak);

        b_list *headers = find_header_paths(data->src_dirs, includes);
        if (headers) {
                unsigned i;
                b_list *copy = b_list_copy(headers);

                B_LIST_FOREACH (copy, file, i)
                        recurse_headers(&headers, data->searched,
                                        data->src_dirs, file, 2);

                b_list_destroy(copy);
                b_list_remove_dups(&headers);
        }

        b_list_destroy(includes);
        pthread_exit(headers);
}


static void
recurse_headers(b_list **headers, b_list **searched, b_list *src_dirs,
                const bstring *cur_header, const int level)
{
        if (level > MAX_HEADER_SEARCH_LEVEL || !cur_header)
                return;
        unsigned i;

        pthread_mutex_lock(&searched_mutex);
        B_LIST_FOREACH (*searched, file, i) {
                if (b_iseq(cur_header, file)) {
                        pthread_mutex_unlock(&searched_mutex);
                        return;
                }
        }
        b_list_append(searched, b_strcpy(cur_header)); 
        pthread_mutex_unlock(&searched_mutex);

        b_list  *includes = b_list_create();
        bstring  line[]   = {{0, 0, NULL, 0}};
        bstring *slurp    = b_quickread(BS(cur_header));
        uint8_t *bak      = slurp->data;

        while (b_memsep(line, slurp, '\n')) {
                bstring *file = analyze_line(line);
                if (file) {
                        b_list_append(&includes, file);
                        b_list_append(&includes, b_dirname(cur_header));
                }
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
                        const int ch = str[i];

                        if (ch == '"' || ch == '<') {
                                ++i;
                                uchar *end = memchr(&str[i], ch, len - i);
                                if (end)
                                        ret = b_fromblk(&str[i], PSUB(end, &str[i]));
                        }
                }
        }

        return ret;
}


static b_list *
find_header_paths(const b_list *src_dirs, const b_list *includes)
{
        if (includes->qty == 0 || src_dirs->qty == 0)
                return NULL;

        unsigned  i, num = 0;
        b_list   *headers = b_list_create_alloc(includes->qty);

        /* I could sprinkle ifdefs throughout to avoid duplication, but
         * I consider this way to be much easier to read. */
#ifdef DOSISH
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
                b_sprintfa(path, B("\\%s"), file)

                struct stat st;
                if (stat(BS(tmp), &st) == 0) {
                        b_list_append(&headers, tmp);
                        b_destroy(file);
                        includes_clone->lst[i] = NULL;
                        ++num;
                }

                b_destroy(path);
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
                                        b_destroy(file);
                                        includes_clone->lst[x] = NULL;
                                        ++num;
                                } else {
                                        b_free(tmp);
                                }
                        }
                }
        }
#else
        b_list *includes_clone = b_list_clone(includes);

        for (i = 0; i < includes_clone->qty; i += 2) {
                bstring *file = includes_clone->lst[i];
                bstring *path = includes_clone->lst[i+1];
                bstring *tmp  = b_concat_all(path, B("/"), file);

                struct stat st;
                if (stat(BS(tmp), &st) == 0) {
                        b_list_append(&headers, tmp);
                        b_destroy(file);
                        includes_clone->lst[i] = NULL;
                        ++num;
                } else
                        b_destroy(tmp);

                b_destroy(path);
                includes_clone->lst[i+1] = NULL;
        }

        B_LIST_FOREACH (src_dirs, dir, i) {
                if (!dir)
                        continue;
                unsigned x;
                int dirfd = open(BS(dir), O_RDONLY|O_DIRECTORY);
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
                                        b_destroy(file);
                                        includes_clone->lst[x] = NULL;
                                        ++num;
                                }
                        }
                }

                close(dirfd);
        }
#endif

        if (headers->qty == 0) {
                b_list_destroy(headers);
                headers = NULL;
        }
        b_list_destroy(includes_clone);

        return headers;
}


static bstring *
find_file_in_dir_recurse(const bstring *dirpath, const bstring *find)
{
        struct dirent **lst = NULL;
        bstring        *ret = NULL;
        const int       n   = scandir(BS(dirpath), &lst, myfilter, alphasort);

        if (n <= 0)
                return NULL;

        struct dirent find_ent[1];
        memset(find_ent, 0, sizeof(struct dirent));
        memcpy(find_ent->d_name, find->data, find->slen + 1);
        find_ent->d_type = DT_REG;

        struct dirent **dirp  = (struct dirent **)(&find_ent);
        struct dirent **match = bsearch(&dirp, lst, n, sizeof(struct dirent **),
                                        (int (*)(const void *, const void *))alphasort);

        if (match && *match) {
                bstring tmp = bt_fromblk((*match)->d_name, _D_EXACT_NAMLEN(*match));
                if (B_FILE_EQ(find, &tmp))
                        ret = b_concat_all(dirpath, B(SEPSTR), &tmp);
        }

        for (int i = 0; i < n; ++i) {
                if (!ret && lst[i]->d_type == DT_DIR)
                {
                        bstring  tmp     = bt_fromblk(lst[i]->d_name, _D_EXACT_NAMLEN(lst[i]));
                        bstring *newpath = b_concat_all(dirpath, B(SEPSTR), &tmp);
                        ret = find_file_in_dir_recurse(newpath, find);
                        b_free(newpath);
                }
                free(lst[i]);
        }

        free(lst);
        return ret;
}


/*======================================================================================*/


static int
myfilter(const struct dirent *dp)
{
        return !IS_DOTDOT(dp->d_name);
}


static int
exec_ctags(struct bufdata *bdata, b_list *headers)
{
#ifdef DOSISH
        bstring *cmd = b_fromcstr_alloc(2048, "ctags ");

        for (unsigned i = 0; i < settings.ctags_args->qty; ++i)
                b_sprintfa(cmd, B("\"%s\" "), settings.ctags_args->lst[i]);

        if (headers) {
                B_LIST_SORT(headers);
                bstring *tmp = b_join_quote(headers, B(" "), '"');
                b_sprintfa(cmd, B(" \"-f%s\" \"%s\" %s"),
                            bdata->topdir->tmpfname, bdata->filename, tmp);

                b_destroy(tmp);
                b_list_destroy(headers);

        } else if (bdata->topdir->recurse && !bdata->topdir->is_c) {
                b_sprintfa(cmd, B(" \"--languages=%s\" -R \"-f%s\" \"%s\""),
                            &bdata->ft->ctags_name, bdata->topdir->tmpfname,
                            bdata->topdir->pathname);
        } else {
                echo("Not recursing!!!");
                b_sprintfa(cmd, B(" \"-f%s\" \"%s\""),
                            bdata->topdir->tmpfname, bdata->filename);
        }

        echo("Running ctags command `%s`", BS(cmd));

        /* Yes, this directly uses unchecked user input in a call to system().
         * Frankly, if somehow someone takes over a user's vimrc then they're
         * already compromised, and if the user wants to attack their own
         * system for some reason then they can be my guest. */
        const int status = system(BS(cmd));
#else
        /* In a unix environment, let's avoid the headache of quoting arguments
         * by calling fork/exec, even at the cost of a bunch of extra
         * allocations and copying. */
        unsigned i;
        struct argument_vector *argv = argv_create(128);
        argv_append(argv, "ctags", true);
        B_LIST_FOREACH(settings.ctags_args, arg, i)
                if (arg)
                        argv_append(argv, b_bstr2cstr(arg, 0), false);

        argv_append(argv, argv_fmt("-f%s", BS(bdata->topdir->tmpfname)), false);

        if (!headers && bdata->topdir->recurse && !bdata->topdir->is_c) {
                argv_append(argv, argv_fmt("--languages=%s", BS(&bdata->ft->ctags_name)), false);
                argv_append(argv, "-R", true);
                argv_append(argv, b_bstr2cstr(bdata->topdir->pathname, 0), false);
        } else {
                argv_append(argv, b_bstr2cstr(bdata->filename, 0), false);
                if (headers) {
                        B_LIST_SORT(headers);
                        B_LIST_FOREACH(headers, bstr, i)
                                argv_append(argv, b_bstr2cstr(bstr, 0), false);
                }
        }

        argv_append(argv, (const char *)0, false);

#ifdef DEBUG
        {
                bstring *cmd = b_alloc_null(2048);
                for (char **tmp = argv->lst; *tmp; ++tmp)
                        b_sprintfa(cmd, B("\"%n\", "), *tmp);
                cmd->data[cmd->slen -= 2] = '\0';
                b_fprintf(stderr, B("Running command 'ctags' with args [%s]\n"), cmd);
                b_free(cmd);
        }
#endif

        int status = 0;
        int pid    = fork();

        if (pid == 0) {
                if (execvpe("ctags", argv->lst, environ) != 0)
                        err(1, "Exec failed");
        } else {
                waitpid(pid, &status, 0);
        }

        argv_destroy(argv);
        b_list_destroy(headers);

        echo("Status is %d", status);
#endif

        return status;
}
