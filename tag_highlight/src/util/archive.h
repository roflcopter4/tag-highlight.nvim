#ifndef SRC_ARCHIVE_ARCHIVE_UTIL_H
#define SRC_ARCHIVE_ARCHIVE_UTIL_H


#include <stddef.h>
#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif

struct archive_size {
        size_t archive;
        size_t uncompressed;
};


extern int   gzip_size(struct archive_size *size, const char *name);
extern int   xz_size(struct archive_size *size, const char *filename);
extern char *lzma_message_strm(int code);

extern int getlines(b_list *tags, enum comp_type_e comptype, const bstring *filename);

extern void write_plain(struct top_dir *topdir);
extern void write_gzip(struct top_dir *topdir);
extern void write_lzma(struct top_dir *topdir);
extern void lazy_write_lzma(struct top_dir *topdir);


#ifdef __cplusplus
}
#endif
#endif /* archive_util.h */
