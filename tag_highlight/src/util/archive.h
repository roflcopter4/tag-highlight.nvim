#ifndef SRC_ARCHIVE_ARCHIVE_UTIL_H
#define SRC_ARCHIVE_ARCHIVE_UTIL_H

#include "Common.h"
#include "highlight.h"

__BEGIN_DECLS

struct archive_size {
        size_t archive;
        size_t uncompressed;
};

extern int   gzip_size(struct archive_size *size, const char *name);
extern int   xz_size(struct archive_size *size, const char *filename);
extern char *lzma_message_strm(unsigned code);

extern int getlines(b_list *tags, comp_type_t comptype, const bstring *filename);

extern void write_plain(struct top_dir *topdir);
extern void write_gzip(struct top_dir *topdir);
extern void write_lzma(struct top_dir *topdir);
extern void lazy_write_lzma(struct top_dir *topdir);

__END_DECLS
#endif /* archive_util.h */
