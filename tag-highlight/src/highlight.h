#pragma once
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
typedef struct bufdata  Buffer;
typedef struct filetype Filetype;

struct settings_s { alignas(128)
      bstring    *cache_dir;
      bstring    *ctags_bin;
      bstring    *settings_file;
      bstring    *go_binary;
      b_list     *ctags_args;
      b_list     *ignored_ftypes;
      b_list     *norecurse_dirs;
      mpack_dict *ignored_tags;
      mpack_dict *order;

      void *talloc_ctx;

      uint16_t job_id;
      uint8_t  comp_type;
      uint8_t  comp_level;
      bool     enabled;
      bool     use_compression;
      bool     verbose;
      bool     buffer_initialized;
      bool     run_ctags;
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
      int16_t          tmpfd;
      uint16_t         index;
      uint16_t         refs;
      bool             recurse;
      nvim_filetype_id ftid;
      time_t           timestamp;

      bstring *gzfile;
      bstring *pathname;
      bstring *tmpfname;
      b_list  *tags;
};

struct bufdata {
      /* atomic_uint ctick; */
      p99_futex   ctick;
      atomic_uint last_ctick;
      uint32_t    num;
      uint16_t    num_failures;
      uint16_t    hl_id;
      atomic_bool initialized;
      bool        total_failure;

      struct {
            pthread_mutex_t total;
            pthread_mutex_t lang_mtx;
            p99_count       num_workers;
            p99_count       hl_waiters;
            pthread_t       pids[4];
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
            struct /*c_family*/ {
                  void   *clangdata;
                  b_list *headers;
            };
            struct /*golang*/ {
                  atomic_bool initialized;
                  void       *sock_info;
            } godata;
            struct /*generic*/ {
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

extern struct settings_s settings;
extern struct filetype **ftdata;
extern linked_list      *top_dirs;
extern size_t const      ftdata_len;

/*===========================================================================*/

enum destroy_buffer_flags {
      DES_BUF_DESTROY_NODE = 0x01,
      DES_BUF_SHOULD_CLEAR = 0x02,
      DES_BUF_TALLOC_FREE  = 0x04,
};

extern bool    have_seen_bufnum(unsigned bufnum);
extern void    destroy_buffer(Buffer *bdata, unsigned flags);
extern Buffer *new_buffer(unsigned bufnum);
extern Buffer *find_buffer(unsigned bufnum);
extern Buffer *get_bufdata(unsigned bufnum, struct filetype *ft);

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

extern int  update_taglist(Buffer *bdata, enum update_taglist_opts opts);
extern void update_highlight(Buffer *bdata, enum update_highlight_type type);
extern int  get_initial_taglist(Buffer *bdata);
extern void clear_highlight(Buffer *bdata, bool blocking);
extern void get_initial_lines(Buffer *bdata);
extern void launch_event_loop(void);
extern void b_list_dump_nvim(b_list const *list, char const *listname);

#include "macros.h"

/*===========================================================================*/
__END_DECLS
#endif /* highlight.h */
// vim: ft=c
