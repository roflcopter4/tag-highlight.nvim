#ifndef THL_HIGHLIGHT_H_
#define THL_HIGHLIGHT_H_

#include "Common.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"
#include "util/list.h"

#include "contrib/p99/p99_count.h"
#include "contrib/p99/p99_defarg.h"
#include "contrib/p99/p99_futex.h"

#ifdef __cplusplus
extern "C" {
#endif
/*===========================================================================*/
/* Old "data.h" */
/*===========================================================================*/

/* typedef volatile p99_futex vfutex_t; */

#define DATA_ARRSIZE 4096

typedef enum { COMP_NONE, COMP_GZIP, COMP_LZMA } comp_type_t;

typedef struct bufdata Buffer;

struct settings_s {
        uint16_t    job_id;
        uint8_t     comp_level;
        bool        enabled;
        bool        use_compression;
        bool        verbose;
        comp_type_t comp_type;

        bstring      *cache_dir;
        bstring      *ctags_bin;
        bstring      *settings_file;
        b_list       *ctags_args;
        b_list       *ignored_ftypes;
        b_list       *norecurse_dirs;
        mpack_dict *ignored_tags;
        mpack_dict *order;
};

struct filetype {
        b_list          *equiv;
        b_list          *ignored_tags;
        bstring         *restore_cmds;
        bstring         *order;
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
        atomic_uint ctick;
        atomic_uint last_ctick;
        atomic_bool is_normal_mode;
        uint16_t    num;
        uint8_t     hl_id;
        bool        initialized;

        struct {
                pthread_mutex_t total;
                pthread_mutex_t ctick;
                p99_count       num_workers;
                p99_futex       fut;
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
                struct /* C, C++ */ {
                        void   *clangdata;
                        b_list *headers;
                };
                struct /* Everything else */ {
                        mpack_arg_array *calls;
                        b_list          *cmd_cache;
                };
        };
};

#if 0
struct buffer_list {
        Buffer *lst[DATA_ARRSIZE];
#if 0
        struct bad_bufs_s {
                int      lst[DATA_ARRSIZE];
                uint16_t qty;
                uint16_t mlen;
        } bad_bufs;
#endif

        uint16_t        mkr;
        uint16_t        mlen;
        pthread_mutex_t lock;
};
extern struct buffer_list  buffers;
#endif

#if 0
struct top_dir_list {
        struct top_dir *lst[DATA_ARRSIZE];
        uint16_t        mkr;
        uint16_t        mlen;
};
#endif


extern struct settings_s   settings;
extern struct filetype     ftdata[];
extern genlist            *top_dirs;
extern size_t const        ftdata_len;

/*===========================================================================*/

extern bool    have_seen_bufnum (int bufnum);
/* extern int     find_buffer_ind  (int bufnum); */
/* extern bool    is_bad_buffer    (int bufnum); */
/* extern void    destroy_bufdata  (Buffer **bdata_p); */
extern void    destroy_buffer   (Buffer *bdata);
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
extern void _b_list_dump_nvim  (const b_list *list, const char *listname);

ALWAYS_INLINE Buffer *
find_current_buffer(void)
{
        return find_buffer(nvim_get_current_buf());
}

ALWAYS_INLINE void __attribute__((__nonnull__))
_nvim_buf_attach_bdata_wrap(const Buffer *const bdata)
{
        nvim_buf_attach(bdata->num);
}


#define update_highlight(...)       P99_CALL_DEFARG(update_highlight, 2, __VA_ARGS__)
#define update_highlight_defarg_0() (find_current_buffer())
#define update_highlight_defarg_1() (HIGHLIGHT_NORMAL)
#define clear_highlight(...)        P99_CALL_DEFARG(clear_highlight, 2, __VA_ARGS__)
#define clear_highlight_defarg_0()  (find_current_buffer())
#define clear_highlight_defarg_1()  (false)
/* #define new_buffer(...)             P99_CALL_DEFARG(new_buffer, 2, __VA_ARGS__) */
/* #define new_buffer_defarg_1()       (false) */

#define b_list_dump_nvim(LST) _b_list_dump_nvim((LST), #LST)

#define nvim_buf_attach(buf_)                                \
        (                                                    \
            (_Generic((buf_),                                \
                      int:      nvim_buf_attach,             \
                      unsigned: nvim_buf_attach,             \
                      uint16_t: nvim_buf_attach,             \
                      Buffer *: _nvim_buf_attach_bdata_wrap) \
            )(buf_)                                          \
        )

#define echo(...)                                                                                                 \
        do {                                                                                                      \
                if (P99_UNLIKELY(settings.verbose)) {                                                             \
                        P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                                                        \
                          (nvim_out_write(B("tag_highlight: " __VA_ARGS__ "\n")))                                 \
                          (nvim_printf("tag_highlight: " P99_CHS(0, __VA_ARGS__) "\n", P99_SKP(1, __VA_ARGS__))); \
                }                                                                                                 \
        } while (0)

#define ECHO(...)                                                                                                      \
        do {                                                                                                           \
                if (P99_UNLIKELY(settings.verbose)) {                                                                  \
                        P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                                                             \
                          (nvim_out_write(B("tag_highlight: " __VA_ARGS__ "\n")))                                      \
                          (nvim_b_printf(B("tag_highlight: " P99_CHS(0, __VA_ARGS__) "\n"), P99_SKP(1, __VA_ARGS__))); \
                }                                                                                                      \
        } while (0)

#define SHOUT(...)                                                                                \
        P99_IF_EQ_1(P99_NARG(__VA_ARGS__))                                                        \
          (nvim_out_write(B("tag_highlight: " __VA_ARGS__ "\n")))                                 \
          (nvim_printf("tag_highlight: " P99_CHS(0, __VA_ARGS__) "\n", P99_SKP(1, __VA_ARGS__))); \


/*===========================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* highlight.h */
