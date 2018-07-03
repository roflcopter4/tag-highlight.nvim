#ifndef ARCHIVE_UTIL_H
#define ARCHIVE_UTIL_H


#include <stddef.h>

struct archive_size {
        size_t archive;
        size_t uncompressed;
};


void gzip_size(struct archive_size *size, const char *name);
void xz_size(struct archive_size *size, const char *filename);
char * lzma_message_strm(int code);


#endif /* archive_util.h */
