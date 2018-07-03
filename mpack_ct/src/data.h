#ifndef SRC_DATA_H
#define SRC_DATA_H

#include "linked_list.h"
#include "mpack.h"

enum filetype_id {
        FT_NONE, FT_C, FT_CPP, FT_CSHARP, FT_GO, FT_JAVA,
        FT_JAVASCRIPT, FT_LISP, FT_PERL, FT_PHP, FT_PYTHON,
        FT_RUBY, FT_RUST, FT_SHELL, FT_VIM, FT_ZSH,
};

struct settings_s {
        /* 6 bytes */
        uint16_t job_id;
        uint8_t  comp_level;
        bool enabled;
        bool use_compression;
        bool verbose;

        b_list     *ctags_args;
        b_list     *norecurse_dirs;
        bstring    *compression_type;
        dictionary *ignored_tags;
        dictionary *order;
};

struct ftdata_s {
        b_list  *equiv;
        b_list  *ignored_tags;
        bstring *order;
        const bstring vim_name;
        const bstring ctags_name;
        /* 3 bytes */
        const enum filetype_id id;
        bool initialized;
};

struct buffer_list {
        struct bufdata {
                /* 8 bytes */
                uint16_t num;
                int16_t  tmpfd;
                uint32_t ctick;
                /* 4 bytes */
                uint32_t last_ctick;

                bstring     *filename;
                bstring     *gzfile;
                bstring     *tmpfname;
                bstring     *topdir;
                b_list      *tags;
                linked_list *lines;
                ll_node     *current;

                struct ftdata_s *ft;
        } *lst[512];
        /* } **lst; */

        struct {
                int lst[512];
                unsigned short qty;
                unsigned short mlen;
        } bad_bufs;

        unsigned qty;
        unsigned mlen;
};


extern FILE *vpipe;
extern struct settings_s  settings;
extern struct buffer_list buffers;
extern struct ftdata_s    ftdata[];
extern const size_t ftdata_len;
extern const char *const m_type_names[];


/*===========================================================================*/
/* Functions */


/*---------------------------------------------------------------------------*/
extern bool new_buffer(int fd, int bufnum);
extern int  find_buffer_ind(const int bufnum);
extern struct bufdata *find_buffer(int bufnum);
extern struct bufdata *get_bufdata(int fd, int bufnum);
extern void destroy_bufdata(struct bufdata **bdata);


/*---------------------------------------------------------------------------*/
extern void handle_unexpected_notification(mpack_obj *note);
extern void handle_nvim_event(mpack_obj *event);


/*---------------------------------------------------------------------------*/
extern b_list *get_archived_tags(struct bufdata *bdata);


/* Initializers */

#define ZERO_512                                                                                          \
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, }


#endif /* data.h */
