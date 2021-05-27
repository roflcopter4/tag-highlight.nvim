#ifndef THL_HIGHLIGHT_H_
#define THL_HIGHLIGHT_H_

#include "Common.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "util/list.h"

#include "contrib/p99/p99_count.h"
#include "contrib/p99/p99_defarg.h"
#include "contrib/p99/p99_futex.h"

__BEGIN_DECLS
/*===========================================================================*/

#define DATA_ARRSIZE 4096

typedef enum { COMP_NONE, COMP_GZIP, COMP_LZMA } comp_type_t;

P99_DECLARE_STRUCT(cmd_info);
struct cmd_info;
typedef struct bufdata Buffer;
typedef struct filetype Filetype;

struct settings_s {
        uint16_t    job_id;
        uint8_t     comp_level;
        bool        enabled;
        bool        use_compression;
        bool        verbose;
        comp_type_t comp_type;

        bstring    *cache_dir;
        bstring    *ctags_bin;
        bstring    *settings_file;
        bstring    *go_binary;
        b_list     *ctags_args;
        b_list     *ignored_ftypes;
        b_list     *norecurse_dirs;
        mpack_dict *ignored_tags;
        mpack_dict *order;

        void       *talloc_ctx;
};

struct filetype {
        b_list          *equiv;
        b_list          *ignored_tags;
        bstring         *restore_cmds;
        bstring         *order;
        cmd_info        *cmd_info;
        bstring          vim_name;
        bstring          ctags_name;
        nvim_filetype_id id;
        bool             initialized;
        bool             restore_cmds_initialized;
        bool             is_c;
        bool             has_parser;
};

struct top_dir {
        int16_t  tmpfd;
        uint16_t index;
        uint16_t refs;
        bool     recurse;
        nvim_filetype_id ftid;
        time_t  timestamp;

        bstring *gzfile;
        bstring *pathname;
        bstring *tmpfname;
        b_list  *tags;
};

struct bufdata {
        /* atomic_uint ctick; */
        p99_futex   ctick;
        p99_futex   highest_ctick;
        atomic_flag ctick_seen_2;
        atomic_flag ctick_seen_3;

        atomic_uint last_ctick;
        atomic_bool is_normal_mode;
        uint16_t    num;
        uint8_t     hl_id;
        atomic_bool initialized;

        struct {
                pthread_mutex_t total;
                pthread_mutex_t ctick;
                pthread_mutex_t lang_mtx;
                p99_count       num_workers;

                p99_count          hl_waiters;
                pthread_spinlock_t spinlock;
                pthread_cond_t     cond;
                pthread_mutex_t    cond_mtx;
        } lock;

        struct {
                bstring *full;
                bstring *base;
                bstring *path;
                char     suffix[8];
        } name;

        linked_list     *lines;
        struct filetype *ft;
        struct top_dir  *topdir;

        union {
                /* c_family */ 
                struct {
                        void   *clangdata;
                        b_list *headers;
                };
                /* golang */ 
                struct {
                        atomic_flag flg;
                        pid_t pid;
                        int   rd_fd;
                        void *sock_info;
                } godata;
                /* generic */
                struct {
                        mpack_arg_array *calls;
                        b_list          *cmd_cache;
                };
        };
};

struct cmd_info {
        unsigned num;
        int      kind;
        bstring *group;
};

extern struct settings_s   settings;
extern struct filetype   **ftdata;
extern linked_list        *top_dirs;
extern size_t const        ftdata_len;

/*===========================================================================*/

enum destroy_buffer_flags {
        DES_BUF_DESTROY_NODE = 0x01U,
        DES_BUF_SHOULD_CLEAR = 0x02U,
        DES_BUF_TALLOC_FREE  = 0x04U,
};

extern bool    have_seen_bufnum (int bufnum);
extern void    destroy_buffer   (Buffer *bdata, unsigned flags);
extern Buffer *new_buffer       (int bufnum);
extern Buffer *find_buffer      (int bufnum);
extern Buffer *get_bufdata      (int bufnum, struct filetype *ft);

/*===========================================================================*/
/* Old "highlight.h" */
/*===========================================================================*/

#define PKG "tag_highlight#"
#define nvim_get_var_pkg(FD__, VARNAME_, EXPECT_) \
        nvim_get_var((FD__), B(PKG VARNAME_), (EXPECT_))

enum update_taglist_opts {
        UPDATE_TAGLIST_NORMAL,
        UPDATE_TAGLIST_FORCE,
        UPDATE_TAGLIST_FORCE_LANGUAGE,
};

enum update_highlight_type {
        HIGHLIGHT_NORMAL,
        HIGHLIGHT_UPDATE,
        HIGHLIGHT_UPDATE_FORCE,
        HIGHLIGHT_REDO,
};

extern bool run_ctags          (Buffer *bdata, enum update_taglist_opts opts);
extern int  update_taglist     (Buffer *bdata, enum update_taglist_opts opts);
extern void update_highlight   (Buffer *bdata, enum update_highlight_type type);
extern int  get_initial_taglist(Buffer *bdata);
extern void clear_highlight    (Buffer *bdata, bool blocking);
extern void get_initial_lines  (Buffer *bdata);
extern void launch_event_loop  (void);
extern void b_list_dump_nvim  (const b_list *list, const char *listname);

#include "macros.h"

/*===========================================================================*/
__END_DECLS
#endif /* highlight.h */
