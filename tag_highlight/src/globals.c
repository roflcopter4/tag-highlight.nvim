#include "tag_highlight.h"
#include <setjmp.h>

#include "data.h"
#include "mpack/mpack.h"

#define BI bt_init

/* Initializers */
#define ZERO_8    0, 0, 0, 0, 0, 0, 0, 0
#define ZERO_64   ZERO_8,   ZERO_8,   ZERO_8,   ZERO_8,   ZERO_8,   ZERO_8,   ZERO_8,   ZERO_8
#define ZERO_512  ZERO_64,  ZERO_64,  ZERO_64,  ZERO_64,  ZERO_64,  ZERO_64,  ZERO_64,  ZERO_64
#define ZERO_4096 ZERO_512, ZERO_512, ZERO_512, ZERO_512, ZERO_512, ZERO_512, ZERO_512, ZERO_512

#define ZERO_DATA ZERO_4096

struct filetype ftdata[] = {
    { NULL, NULL, NULL, NULL, BI("NONE"),       BI("NONE"),       FT_NONE,       0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("c"),          BI("c"),          FT_C,          0, 0, 1 },
    { NULL, NULL, NULL, NULL, BI("cpp"),        BI("c++"),        FT_CPP,        0, 0, 1 },
    { NULL, NULL, NULL, NULL, BI("cs"),         BI("csharp"),     FT_CSHARP,     0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("go"),         BI("go"),         FT_GO,         0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("java"),       BI("java"),       FT_JAVA,       0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("javascript"), BI("javascript"), FT_JAVASCRIPT, 0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("lisp"),       BI("lisp"),       FT_LISP,       0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("perl"),       BI("perl"),       FT_PERL,       0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("php"),        BI("php"),        FT_PHP,        0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("python"),     BI("python"),     FT_PYTHON,     0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("ruby"),       BI("ruby"),       FT_RUBY,       0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("rust"),       BI("rust"),       FT_RUST,       0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("sh"),         BI("sh"),         FT_SHELL,      0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("vim"),        BI("vim"),        FT_VIM,        0, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("zsh"),        BI("sh"),         FT_ZSH,        0, 0, 0 },
};


extern bool             process_exiting;
extern b_list          *seen_files;
extern jmp_buf          exit_buf;
extern struct backups   backup_pointers;
extern FILE            *cmd_log, *echo_log, *main_log;
extern const char      *program_name;
extern pthread_mutex_t  update_mutex;

genlist *top_dirs = NULL;

struct settings_s   settings        = {0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
struct buffer_list  buffers         = {{ZERO_DATA}, {{ZERO_DATA}, 0, DATA_ARRSIZE}, 0, DATA_ARRSIZE};
struct backups      backup_pointers = { NULL, 0, 0 };

const size_t    ftdata_len  = ARRSIZ(ftdata);
/* int             mainchan    = (-1);
int             bufchan     = (-1); */
const char *    program_name;
char *          HOME;
FILE *          cmd_log;
FILE *          echo_log;
FILE *          main_log;
jmp_buf         exit_buf;
pthread_mutex_t update_mutex    = PTHREAD_MUTEX_INITIALIZER;
b_list *        seen_files      = NULL;
bool            process_exiting = false;

#if 0
/* Some constants. */
const long double USEC2SECOND = 1000000.0L;
const long double NSEC2SECOND = 1000000000.0L;
#ifdef DOSISH
const long double SLEEP_CONV = 1000.0L;
#else
const long double SLEEP_CONV = NSEC2SECOND;
#endif
#endif
