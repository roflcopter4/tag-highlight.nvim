#include "Common.h"
#include "highlight.h"
#include "mpack/mpack.h"

#define BI bt_init

static const struct filetype ftdata_static[] = {
    { 0, 0, 0, 0, 0, BI("NONE"),       BI("NONE"),       FT_NONE,       0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("c"),          BI("c"),          FT_C,          0, 0, 1, 1 },
    { 0, 0, 0, 0, 0, BI("cpp"),        BI("c++"),        FT_CXX,        0, 0, 1, 1 },
    { 0, 0, 0, 0, 0, BI("cs"),         BI("c#"),         FT_CSHARP,     0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("go"),         BI("go"),         FT_GO,         0, 0, 0, 1 },
    { 0, 0, 0, 0, 0, BI("java"),       BI("java"),       FT_JAVA,       0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("javascript"), BI("javascript"), FT_JAVASCRIPT, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("lisp"),       BI("lisp"),       FT_LISP,       0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("lua"),        BI("lua"),        FT_LUA,        0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("perl"),       BI("perl"),       FT_PERL,       0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("php"),        BI("php"),        FT_PHP,        0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("python"),     BI("python"),     FT_PYTHON,     0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("ruby"),       BI("ruby"),       FT_RUBY,       0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("rust"),       BI("rust"),       FT_RUST,       0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("sh"),         BI("sh"),         FT_SHELL,      0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("vim"),        BI("vim"),        FT_VIM,        0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, BI("zsh"),        BI("sh"),         FT_ZSH,        0, 0, 0, 0 },
};

extern void talloc_emergency_library_init(void);

size_t const ftdata_len = ARRSIZ(ftdata_static);
struct filetype **ftdata;

__attribute__((__constructor__(105)))
static void init_ftdata(void)
{
      talloc_emergency_library_init();
        ftdata = talloc_array(NULL, struct filetype *, ftdata_len);

        for (unsigned i = 0; i < ftdata_len; ++i) {
                ftdata[i] = talloc(ftdata, struct filetype);
                memcpy(ftdata[i], &ftdata_static[i], sizeof(struct filetype));
        }
}

#ifndef HAVE_PROGRAM_INVOCATION_SHORT_NAME
const char *program_invocation_short_name;
#endif
#ifndef HAVE_PROGRAM_INVOCATION_NAME
const char *program_invocation_name;
#endif

extern bool             process_exiting;
extern jmp_buf          exit_buf;
extern FILE            *cmd_log, *echo_log, *main_log;
extern char const      *program_name;
extern pthread_mutex_t  update_mutex;

pthread_mutex_t update_mutex    = PTHREAD_MUTEX_INITIALIZER;
bool            process_exiting = false;
char const     *program_name;
linked_list    *top_dirs;
char           *HOME;
FILE           *cmd_log;
FILE           *echo_log;
FILE           *main_log;
jmp_buf         exit_buf;

struct settings_s settings = {0};

