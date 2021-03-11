#include "Common.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include <setjmp.h>

#define BI bt_init

static const struct filetype ftdata_static[] = {
    { NULL, NULL, NULL, NULL, BI("NONE"),       BI("NONE"),       FT_NONE,       0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("c"),          BI("c"),          FT_C,          0, 0, 1, 1 },
    { NULL, NULL, NULL, NULL, BI("cpp"),        BI("c++"),        FT_CXX,        0, 0, 1, 1 },
    { NULL, NULL, NULL, NULL, BI("cs"),         BI("c#"),         FT_CSHARP,     0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("go"),         BI("go"),         FT_GO,         0, 0, 0, 1 },
    { NULL, NULL, NULL, NULL, BI("java"),       BI("java"),       FT_JAVA,       0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("javascript"), BI("javascript"), FT_JAVASCRIPT, 0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("lisp"),       BI("lisp"),       FT_LISP,       0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("perl"),       BI("perl"),       FT_PERL,       0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("php"),        BI("php"),        FT_PHP,        0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("python"),     BI("python"),     FT_PYTHON,     0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("ruby"),       BI("ruby"),       FT_RUBY,       0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("rust"),       BI("rust"),       FT_RUST,       0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("sh"),         BI("sh"),         FT_SHELL,      0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("vim"),        BI("vim"),        FT_VIM,        0, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("zsh"),        BI("sh"),         FT_ZSH,        0, 0, 0, 0 },
};

size_t const ftdata_len = ARRSIZ(ftdata_static);
struct filetype **ftdata;
// struct filetype *ftdata[ARRSIZ(ftdata_static)];

__attribute__((constructor)) void init_ftdata(void)
{
        ftdata = talloc_array(NULL, struct filetype *, ftdata_len);
        /* ftdata = talloc_pool(NULL, ftdata_len * sizeof(struct filetype)); */

        for (unsigned i = 0; i < ftdata_len; ++i) {
                ftdata[i] = talloc(ftdata, struct filetype);
                memcpy(ftdata[i], &ftdata_static[i], sizeof(struct filetype));
        }
}

//__attribute__((constructor)) void free_ftdata(void)
//{
//        talloc_free(ftdata);
//        ftdata = NULL;
//}

extern bool             process_exiting;
extern jmp_buf          exit_buf;
extern FILE            *cmd_log, *echo_log, *main_log;
extern char const      *program_name;
extern pthread_mutex_t  update_mutex;

struct settings_s settings = {0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;
char const     *program_name;
genlist        *top_dirs;
char           *HOME;
FILE           *cmd_log;
FILE           *echo_log;
FILE           *main_log;
jmp_buf         exit_buf;
bool            process_exiting;
