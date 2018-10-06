#ifndef SRC_DATA_H
#define SRC_DATA_H

#include "nvim_api/api.h"
#include "mpack/mpack.h"
#include "util/list.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_ARRSIZE 4096

enum event_types {
        EVENT_BUF_LINES,
        EVENT_BUF_CHANGED_TICK,
        EVENT_BUF_DETACH,
        EVENT_VIM_UPDATE,
};

typedef enum { COMP_NONE, COMP_GZIP, COMP_LZMA } comp_type_t;

struct settings_s {
        uint16_t    job_id;
        uint8_t     comp_level;
        bool        enabled;
        bool        use_compression;
        bool        verbose;
        comp_type_t comp_type;

        bstring      *ctags_bin;
        bstring      *settings_file;
        b_list       *ctags_args;
        b_list       *ignored_ftypes;
        b_list       *norecurse_dirs;
        mpack_dict_t *ignored_tags;
        mpack_dict_t *order;
};

struct filetype {
        b_list       *equiv;
        b_list       *ignored_tags;
        bstring      *restore_cmds;
        bstring      *order;
        const bstring vim_name;
        const bstring ctags_name;
        const nvim_filetype_id id;
        bool initialized;
        bool restore_cmds_initialized;
        bool is_c;
};

struct top_dir {
        int16_t  tmpfd;
        uint16_t index;
        uint16_t refs;
        bool     recurse;
        nvim_filetype_id ftid;

        bstring *gzfile;
        bstring *pathname;
        bstring *tmpfname;
        b_list  *tags;
};

struct bufdata {
        uint32_t ctick;
        uint32_t last_ctick;
        uint16_t num;
        uint8_t  hl_id;
        bool     initialized;

        pthread_mutex_t  mut;
        pthread_rwlock_t lock;

        struct {
                bstring *full;
                bstring *base;
                bstring *path;
        } name;

        linked_list     *lines;
        struct filetype *ft;
        struct top_dir  *topdir;

        /* This feels so hacky. */
        union {
                /* C, C++ */
                struct {
                        void   *clangdata;
                        b_list *headers;
                };
                /* Everything else */
                struct {
                        nvim_call_array *calls;
                        b_list          *cmd_cache;
                };
        };
};
        
struct buffer_list {
        struct bufdata *lst[DATA_ARRSIZE];
        struct bad_bufs_s {
                int      lst[DATA_ARRSIZE];
                uint16_t qty;
                uint16_t mlen;
        } bad_bufs;

        uint16_t mkr;
        uint16_t mlen;
};

struct top_dir_list {
        struct top_dir *lst[DATA_ARRSIZE];
        uint16_t        mkr;
        uint16_t        mlen;
};

struct int_pdata {
        int       val;
        pthread_t parent_tid;
};


extern struct settings_s   settings;
extern struct buffer_list  buffers;
extern struct filetype     ftdata[];
extern genlist            *top_dirs;

extern const size_t ftdata_len;


/*===========================================================================*/
/*===========================================================================*/
/* Functions */

extern bool            have_seen_file   (const bstring *filename);
extern bool            new_buffer       (int fd, int bufnum);
extern int             find_buffer_ind  (int bufnum);
extern bool            is_bad_buffer    (int bufnum);
extern void            destroy_bufdata  (struct bufdata **bdata);
extern struct bufdata *find_buffer      (int bufnum);
extern struct bufdata *get_bufdata      (int fd, int bufnum, struct filetype *ft);
extern struct bufdata *null_find_bufdata(int bufnum, struct bufdata *bdata);

#define new_buffer(...) P99_CALL_DEFARG(new_buffer, 2, __VA_ARGS__)
#define new_buffer_defarg_0() (0)

/*---------------------------------------------------------------------------*/
/* Events */
extern void handle_unexpected_notification(mpack_obj *note);

/*---------------------------------------------------------------------------*/
/* Archives */
extern b_list *get_archived_tags(struct bufdata *bdata);

/*===========================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* data.h */
