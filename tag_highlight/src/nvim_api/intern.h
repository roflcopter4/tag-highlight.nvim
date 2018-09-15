#ifndef SRC_NVIM_API_INTERN_H
#define SRC_NVIM_API_INTERN_H

#include "nvim_api/api.h"
#include "bstring/bstring.h"
#include "mpack/mpack.h"

#include "p99/p99_defarg.h"

#ifdef __cplusplus
extern "C" {
#endif
/*======================================================================================*/

#define COUNT(FD_)         (((FD_) == 1) ? io_count : sok_count)
#define INC_COUNT(FD_)     (((FD_) == 1) ? io_count++ : sok_count++)
#define CHECK_DEF_FD(FD__) ((FD__) = (((FD__) == 0) ? DEFAULT_FD : (FD__)))
#define BS_FROMARR(ARRAY_) {(sizeof(ARRAY_) - 1), 0, (unsigned char *)(ARRAY_), 0}
#define INIT_WAIT_LISTSZ   (64)

__attribute__((visibility("hidden"))) 
extern mpack_obj *generic_call(int *fd, const bstring *fn, const bstring *fmt, ...);

__attribute__((visibility("hidden"))) 
extern mpack_obj *await_package(int fd, int count, enum message_types mtype);

__attribute__((visibility("hidden"))) 
extern void write_and_clean(int fd, mpack_obj *pack, const bstring *func, FILE *logfp);

__attribute__((visibility("hidden"))) 
extern retval_t m_expect_intern(mpack_obj *root, mpack_expect_t type);


#define write_and_clean(...) P99_CALL_DEFARG(write_and_clean, 4, __VA_ARGS__)
#define write_and_clean_defarg_0() (0)
#ifdef DEBUG
#  define write_and_clean_defarg_3() (mpack_log)
#else
#  define write_and_clean_defarg_3() (NULL)
#endif

extern uint32_t        sok_count, io_count;
extern pthread_mutex_t mpack_main_mutex;
extern pthread_mutex_t api_mutex;


/*======================================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* intern.h */
