#ifndef ARCHIVE_UTIL_H
#define ARCHIVE_UTIL_H


#include <stddef.h>
#include "data.h"

struct archive_size {
        size_t archive;
        size_t uncompressed;
};


void gzip_size(struct archive_size *size, const char *name);
void xz_size(struct archive_size *size, const char *filename);
char * lzma_message_strm(int code);


void write_plain(struct top_dir *topdir);
void write_gzip(struct top_dir *topdir);
void write_lzma(struct top_dir *topdir);
void lazy_write_lzma(struct top_dir *topdir);


#endif /* archive_util.h */
