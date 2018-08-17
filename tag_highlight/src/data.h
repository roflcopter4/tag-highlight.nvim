#ifndef SRC_DATA_H
#define SRC_DATA_H

#include "linked_list.h"

#include "generic_list.h"
#include "mpack.h"
#include <pthread.h>

#define DATA_ARRSIZE 4096

enum filetype_id {
        FT_NONE, FT_C, FT_CPP, FT_CSHARP, FT_GO, FT_JAVA,
        FT_JAVASCRIPT, FT_LISP, FT_PERL, FT_PHP, FT_PYTHON,
        FT_RUBY, FT_RUST, FT_SHELL, FT_VIM, FT_ZSH,
};

enum event_types {
        EVENT_BUF_LINES,
        EVENT_BUF_CHANGED_TICK,
        EVENT_BUF_DETACH,
        EVENT_VIM_UPDATE,
};


struct settings_s {
        /* 6 bytes */
        uint16_t job_id;
        uint8_t  comp_level;
        bool enabled;
        bool use_compression;
        bool verbose;

        bstring      *settings_file;
        b_list       *ctags_args;
        b_list       *ignored_ftypes;
        b_list       *norecurse_dirs;
        mpack_dict_t *ignored_tags;
        mpack_dict_t *order;
        enum comp_type_e { COMP_NONE, COMP_GZIP, COMP_LZMA } comp_type;
};
typedef enum comp_type_e comp_type_t;

struct ftdata_s {
        b_list  *equiv;
        b_list  *ignored_tags;
        bstring *restore_cmds;
        bstring *order;
        const bstring vim_name;
        const bstring ctags_name;
        /* 3 bytes */
        const enum filetype_id id;
        bool initialized;
        bool restore_cmds_initialized;
};

struct top_dir {
        int16_t  tmpfd;
        uint16_t index;
        uint16_t refs;
        bool     recurse;
        bool     is_c;
        enum filetype_id ftid;

        bstring *gzfile;
        bstring *pathname;
        bstring *tmpfname;
        b_list  *tags;
};

struct bufdata {
        uint32_t ctick;
        uint32_t last_ctick; // 8 bytes
        uint16_t num;
        uint8_t  flags; // unused
        bool     initialized;
        int32_t  thread_pid; // 8 bytes

        bstring         *filename;
        /* b_list          *cmd_cache; */
        linked_list     *lines;
        ll_node         *lastref;
        struct ftdata_s *ft;
        struct top_dir  *topdir;
        struct atomic_call_array *calls;
};
        
struct buffer_list {
        struct bufdata *lst[DATA_ARRSIZE];
        struct bad_bufs_s {
                int lst[DATA_ARRSIZE];
                uint16_t qty;
                uint16_t mlen;
        } bad_bufs;

        uint16_t mkr;
        uint16_t mlen;
};

struct top_dir_list {
        struct top_dir *lst[DATA_ARRSIZE];
        uint16_t mkr;
        uint16_t mlen;
};

struct int_pdata {
        int       val;
        pthread_t parent_tid;
};


extern struct settings_s   settings;
extern struct buffer_list  buffers;
extern struct ftdata_s     ftdata[];
extern genlist *top_dirs;

extern int   sockfd;
extern const size_t ftdata_len;
extern const char *const m_type_names[];

#define DEFAULT_FD sockfd


/*===========================================================================*/
/*===========================================================================*/
/* Functions */

extern bool have_seen_file(const bstring *filename);

/*---------------------------------------------------------------------------*/
/* Buffers */

extern bool new_buffer(int fd, int bufnum);
extern int  find_buffer_ind(int bufnum);
extern bool is_bad_buffer(int bufnum);

extern struct bufdata *find_buffer(int bufnum);
extern struct bufdata *get_bufdata(int fd, int bufnum, struct ftdata_s *ft);
extern struct bufdata *null_find_bufdata(int bufnum, struct bufdata *bdata);
extern void destroy_bufdata(struct bufdata **bdata);


/*---------------------------------------------------------------------------*/
/* Events */
extern void handle_unexpected_notification(mpack_obj *note);
extern enum event_types handle_nvim_event(mpack_obj *event);

extern void *interrupt_call(void *vdata);


/*---------------------------------------------------------------------------*/
/* Archives */
extern b_list *get_archived_tags(struct bufdata *bdata);


#endif /* data.h */
