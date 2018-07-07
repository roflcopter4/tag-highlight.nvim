#include "util.h"

#include "data.h"
#include "mpack.h"
#include "mpack_code.h"

#define BI bt_init

const char *const m_type_names[] = {
    "MPACK_UNINITIALIZED", "MPACK_BOOL",   "MPACK_NIL",   "MPACK_NUM",
    "MPACK_EXT",           "MPACK_STRING", "MPACK_ARRAY", "MPACK_DICT",
};

const struct mpack_masks m_masks[] = {
    { NIL,    M_NIL,       false, 0xC0u,  0, "M_NIL"       },
    { BOOL,   M_TRUE,      false, 0xC3u,  0, "M_TRUE"      },
    { BOOL,   M_FALSE,     false, 0xC2u,  0, "M_FALSE"     },
    { STRING, M_STR_8,     false, 0xD9u,  0, "M_STR_8"     },
    { STRING, M_STR_16,    false, 0xDAu,  0, "M_STR_16"    },
    { STRING, M_STR_32,    false, 0xDBu,  0, "M_STR_32"    },
    { ARRAY,  M_ARRAY_16,  false, 0xDCu,  0, "M_ARRAY_16"  },
    { ARRAY,  M_ARRAY_32,  false, 0xDDu,  0, "M_ARRAY_32"  },
    { MAP,    M_MAP_16,    false, 0xDEu,  0, "M_MAP_16"    },
    { MAP,    M_MAP_32,    false, 0xDFu,  0, "M_MAP_32"    },
    { BIN,    M_BIN_8,     false, 0xC4u,  0, "M_BIN_8"     },
    { BIN,    M_BIN_16,    false, 0xC5u,  0, "M_BIN_16"    },
    { BIN,    M_BIN_32,    false, 0xC6u,  0, "M_BIN_32"    },
    { INT,    M_INT_8,     false, 0xD0u,  0, "M_INT_8"     },
    { INT,    M_INT_16,    false, 0xD1u,  0, "M_INT_16"    },
    { INT,    M_INT_32,    false, 0xD2u,  0, "M_INT_32"    },
    { INT,    M_INT_64,    false, 0xD3u,  0, "M_INT_64"    },
    { UINT,   M_UINT_8,    false, 0xCCu,  0, "M_UINT_8"    },
    { UINT,   M_UINT_16,   false, 0xCDu,  0, "M_UINT_16"   },
    { UINT,   M_UINT_32,   false, 0xCEu,  0, "M_UINT_32"   },
    { UINT,   M_UINT_64,   false, 0xCFu,  0, "M_UINT_64"   },
//  { EXT,    M_EXT_8,     false, 0xC7u,  0, "M_EXT_8"     },
//  { EXT,    M_EXT_16,    false, 0xC8u,  0, "M_EXT_16"    },
//  { EXT,    M_EXT_32,    false, 0xC9u,  0, "M_EXT_32"    },
    { EXT,    M_EXT_F1,    false, 0xD4u,  0, "M_EXT_F1"    },
    { EXT,    M_EXT_F2,    false, 0xD5u,  0, "M_EXT_F2"    },
    { EXT,    M_EXT_F4,    false, 0xD6u,  0, "M_EXT_F4"    },
    { STRING, M_FIXSTR_F,  true,  0xA0u,  5, "M_FIXSTR_F"  },
    { ARRAY,  M_ARRAY_F,   true,  0x90u,  4, "M_ARRAY_F"   },
    { MAP,    M_FIXMAP_F,  true,  0x80u,  4, "M_FIXMAP_F"  },
    { LINT,   M_POS_INT_F, true,  0x00u,  7, "M_POS_INT_F" },
    { LINT,   M_NEG_INT_F, true,  0xE0u,  5, "M_NEG_INT_F" },
};

struct ftdata_s ftdata[] = {
    { NULL, NULL, NULL, NULL, BI("NONE"),       BI("NONE"),       FT_NONE,       0, 0 },
    { NULL, NULL, NULL, NULL, BI("c"),          BI("c"),          FT_C,          0, 0 },
    { NULL, NULL, NULL, NULL, BI("cpp"),        BI("c++"),        FT_CPP,        0, 0 },
    { NULL, NULL, NULL, NULL, BI("cs"),         BI("csharp"),     FT_CSHARP,     0, 0 },
    { NULL, NULL, NULL, NULL, BI("go"),         BI("go"),         FT_GO,         0, 0 },
    { NULL, NULL, NULL, NULL, BI("java"),       BI("java"),       FT_JAVA,       0, 0 },
    { NULL, NULL, NULL, NULL, BI("javascript"), BI("javascript"), FT_JAVASCRIPT, 0, 0 },
    { NULL, NULL, NULL, NULL, BI("lisp"),       BI("lisp"),       FT_LISP,       0, 0 },
    { NULL, NULL, NULL, NULL, BI("perl"),       BI("perl"),       FT_PERL,       0, 0 },
    { NULL, NULL, NULL, NULL, BI("php"),        BI("php"),        FT_PHP,        0, 0 },
    { NULL, NULL, NULL, NULL, BI("python"),     BI("python"),     FT_PYTHON,     0, 0 },
    { NULL, NULL, NULL, NULL, BI("ruby"),       BI("ruby"),       FT_RUBY,       0, 0 },
    { NULL, NULL, NULL, NULL, BI("rust"),       BI("rust"),       FT_RUST,       0, 0 },
    { NULL, NULL, NULL, NULL, BI("sh"),         BI("sh"),         FT_SHELL,      0, 0 },
    { NULL, NULL, NULL, NULL, BI("vim"),        BI("vim"),        FT_VIM,        0, 0 },
    { NULL, NULL, NULL, NULL, BI("zsh"),        BI("zsh"),        FT_ZSH,        0, 0 },
};


const size_t m_masks_len = ARRSIZ(m_masks);
const size_t ftdata_len  = ARRSIZ(ftdata);

struct settings_s   settings = {0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0};
struct buffer_list  buffers  = {ZERO_512, {ZERO_512, 0, 512}, 0, 512};
struct top_dir_list top_dirs = {ZERO_512, 0, 512};

struct backups      backup_pointers = { NULL, 0, 0 };

int             sockfd;
FILE *          decodelog;
FILE *          mpack_log;
FILE *          vpipe;
const char *    program_name;
const char *    HOME;
pthread_mutex_t event_mutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ftdata_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mpack_main     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t printmutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t readlocksocket = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t readlockstdin  = PTHREAD_MUTEX_INITIALIZER;
