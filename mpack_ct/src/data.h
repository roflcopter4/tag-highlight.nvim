#ifndef SRC_DATA_H
#define SRC_DATA_H

#include "linked_list.h"
#include "mpack.h"

enum filetype_id {
        FT_NONE, FT_C, FT_CPP, FT_CSHARP, FT_GO, FT_JAVA,
        FT_JAVASCRIPT, FT_LISP, FT_PERL, FT_PHP, FT_PYTHON,
        FT_RUBY, FT_RUST, FT_SHELL, FT_VIM, FT_ZSH,
};

enum event_types {
        EVENT_BUF_LINES,
        EVENT_BUF_CHANGED_TICK,
        EVENT_BUF_DETACH,
};


struct settings_s {
        /* 6 bytes */
        uint16_t job_id;
        uint8_t  comp_level;
        bool enabled;
        bool use_compression;
        bool verbose;

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

        bstring *gzfile;
        bstring *pathname;
        bstring *tmpfname;
        b_list  *tags;
};

struct buffer_list {
        struct bufdata {
                uint32_t ctick;
                uint32_t last_ctick;
                uint16_t num;

                bstring         *filename;
                b_list          *cmd_cache;
                linked_list     *lines;
                ll_node         *current;
                struct ftdata_s *ft;
                struct top_dir  *topdir;
        } *lst[512];

        struct bad_bufs_s {
                int lst[512];
                uint16_t qty;
                uint16_t mlen;
        } bad_bufs;

        uint16_t mkr;
        uint16_t mlen;
};

struct top_dir_list {
        struct top_dir *lst[512];
        uint16_t mkr;
        uint16_t mlen;
};


extern struct settings_s   settings;
extern struct buffer_list  buffers;
extern struct ftdata_s     ftdata[];
extern struct top_dir_list top_dirs;

extern int   sockfd;
extern FILE *vpipe;
extern const size_t ftdata_len;
extern const char *const m_type_names[];

#define DEFAULT_FD sockfd


/*===========================================================================*/
/*===========================================================================*/
/* Functions */


/*---------------------------------------------------------------------------*/
/* Buffers */

extern bool new_buffer(int fd, int bufnum);
extern int  find_buffer_ind(int bufnum);
extern bool is_bad_buffer(int bufnum);

extern struct bufdata *find_buffer(int bufnum);
extern struct bufdata *get_bufdata(int fd, int bufnum);
extern struct bufdata *null_find_bufdata(int bufnum, struct bufdata *bdata);
extern void destroy_bufdata(struct bufdata **bdata);


/*---------------------------------------------------------------------------*/
/* Events */
extern void handle_unexpected_notification(mpack_obj *note);
/* extern void handle_nvim_event(mpack_obj *event); */
extern enum event_types handle_nvim_event(mpack_obj *event);



/*---------------------------------------------------------------------------*/
/* Archives */
extern b_list *get_archived_tags(struct bufdata *bdata);


/*===========================================================================*/
/*===========================================================================*/
/* Initializers */

#define ZERO_512                                                         \
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0 }


#endif /* data.h */
