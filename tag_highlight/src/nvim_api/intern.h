#ifndef NVIM_API_INTERN_H_
#define NVIM_API_INTERN_H_

#include "Common.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"

#ifdef __cplusplus
extern "C" {
#endif
/*======================================================================================*/

struct nvim_connection {
        int      fd;
        unsigned count;
        enum nvim_connection_type type;
};
extern genlist *nvim_connections;

#define BS_FROMARR(ARRAY_)                    \
        {.data = ((unsigned char *)(ARRAY_)), \
         .slen = sizeof(ARRAY_) - 1,          \
         .mlen = 0,                           \
         .flags = 0}

#define INIT_WAIT_LISTSZ   (64)
#define COUNT()            _get_fd_count((1), false)
#define INC_COUNT()        _get_fd_count((1), true)

extern int _get_fd_count(const int fd, const bool inc);

#define INTERN __attribute__((__visibility__("hidden"))) extern

INTERN mpack_obj    *_nvim_api_generic_call(bool blocking, const bstring *fn, const bstring *fmt, ...);
INTERN mpack_obj    *_nvim_api_special_call(bool blocking, const bstring *fn, mpack_obj *pack, int count);
INTERN mpack_retval  m_expect_intern(mpack_obj *root, mpack_expect_t type) __aWUR;

#undef INTERN
#define generic_call _nvim_api_generic_call
#define special_call _nvim_api_special_call

extern pthread_mutex_t mpack_main_mutex;
extern pthread_mutex_t api_mutex;

/*======================================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* intern.h */
