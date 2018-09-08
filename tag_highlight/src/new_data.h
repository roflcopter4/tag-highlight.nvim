#ifndef NEW_SRC_DATA_H
#define NEW_SRC_DATA_H

#include "util/util.h"

#include "nvim_api/api.h"
#include "mpack/mpack.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif
/*======================================================================================*/


/* Filetype identification */
enum cnvim_filetype_id_e {
        FT_NONE, FT_C, FT_CPP, FT_CSHARP, FT_GO, FT_JAVA,
        FT_JAVASCRIPT, FT_LISP, FT_PERL, FT_PHP, FT_PYTHON,
        FT_RUBY, FT_RUST, FT_SHELL, FT_VIM, FT_ZSH,
};

#if 0
enum cnvim_event_type_e {
        EVENT_BUF_LINES,
        EVENT_BUF_CHANGED_TICK,
        EVENT_BUF_DETACH,
        EVENT_VIM_UPDATE,
};

enum cnvim_comp_type_e {
        COMP_NONE, COMP_GZIP, COMP_LZMA
};
#endif

typedef enum cnvim_filetype_id_e cnvim_filetype_id;
/* typedef enum cnvim_event_type_e  cnvim_event_type; */
/* typedef enum cnvim_comp_type_e   cnvim_comp_type; */
typedef struct cnvim_filetype    cnvim_filetype;
typedef struct cnvim_buffer      cnvim_buffer;

/* General hardcoded neovim filetype information. */
struct cnvim_filetype {
        const bstring     name;
        const filetype_id id;
};

/* Represents an opened neovim buffer. */
struct cnvim_buffer {
        bstring *full_name;
        bstring *base_name;
        bstring *path_name;

        cnvim_filetype *ft;
};


/*======================================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* new_data.h */
