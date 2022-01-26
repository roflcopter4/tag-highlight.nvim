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
    alignas(16)
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

#define INIT_WAIT_LISTSZ (64)
#define COUNT()     nvim_api_intern_get_fd_count((1), false)
#define INC_COUNT() nvim_api_intern_get_fd_count((1), true)

#define INTERN __attribute__((__visibility__("hidden"))) extern

INTERN int          nvim_api_intern_get_fd_count(int fd, bool inc);
INTERN mpack_obj   *nvim_api_intern_make_generic_call(bool blocking, const bstring *fn, const bstring *fmt, ...);
INTERN mpack_obj   *nvim_api_intern_make_special_call(bool blocking, const bstring *fn, mpack_obj *pack, int count);
INTERN mpack_retval nvim_api_intern_mpack_expect_wrapper(mpack_obj *root, mpack_expect_t type, uint64_t defval) __aWUR;

#undef INTERN
#define generic_call nvim_api_intern_make_generic_call
#define special_call nvim_api_intern_make_special_call
#define intern_mpack_expect nvim_api_intern_mpack_expect_wrapper


#define nvim_api_intern_mpack_expect_wrapper(...) P99_CALL_DEFARG(nvim_api_intern_mpack_expect_wrapper, 3, __VA_ARGS__)
#define nvim_api_intern_mpack_expect_wrapper_defarg_2() (UINT64_C(0))

extern pthread_mutex_t mpack_main_mutex;
extern pthread_mutex_t api_mutex;

/*======================================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* intern.h */
