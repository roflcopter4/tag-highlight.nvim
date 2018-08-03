set (archive_util_SOURCES
    readfile.c
    writefile.c
    archive_util_gzip.c
    archive_util.h
)

if (LZMA_SUPPORT)
    set (archive_util_SOURCES ${archive_util_SOURCES} archive_util_xz.c)
endif()

add_library(archive_util STATIC ${archive_util_SOURCES})