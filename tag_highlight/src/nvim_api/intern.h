#ifndef SRC_NVIM_API_INTERN_H
#define SRC_NVIM_API_INTERN_H

#include "bstring/bstring.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"

#include "p99/p99_defarg.h"

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

#ifndef DEFAULT_FD
#  define DEFAULT_FD (1)
#endif
#define COUNT(FD_)         _get_fd_count((FD_), false)
#define INC_COUNT(FD_)     _get_fd_count((FD_), true)

#define CHECK_DEF_FD(FD__)                  \
        ({                                  \
                int m_fd_ = (FD__);         \
                if (m_fd_ == 0)             \
                        m_fd_ = DEFAULT_FD; \
                (FD__) = m_fd_;             \
        })

/* ((FD__) = (((FD__) == 0) ? DEFAULT_FD : (FD__))) */

#define BS_FROMARR(ARRAY_) {(sizeof(ARRAY_) - 1), 0, (unsigned char *)(ARRAY_), 0}
#define INIT_WAIT_LISTSZ   (64)
#define INTERN __attribute__((visibility("hidden"))) extern

static inline int _get_fd_count(const int fd, const bool inc)
{
        for (unsigned i = 0; i < nvim_connections->qty; ++i) {
                if (((struct nvim_connection *)(nvim_connections->lst[i]))->fd == fd) {
                        const int ret = ((struct nvim_connection *)(nvim_connections->lst[i]))->count;
                        if (inc)
                                ++((struct nvim_connection *)(nvim_connections->lst[i]))->count;
                        return ret;
                }
        }

        errx(1, "Couldn't find fd %d in nvim_connections.", fd);
}

INTERN mpack_obj *generic_call(int *fd, const bstring *fn, const bstring *fmt, ...);
INTERN mpack_obj *await_package(int fd, int count, enum message_types mtype);
INTERN void write_and_clean(int fd, mpack_obj *pack, const bstring *func, FILE *logfp);
INTERN retval_t m_expect_intern(mpack_obj *root, mpack_expect_t type);

#define write_and_clean(...) P99_CALL_DEFARG(write_and_clean, 4, __VA_ARGS__)
#define write_and_clean_defarg_0() (0)
#ifdef DEBUG
#  define write_and_clean_defarg_3() (mpack_log)
#else
#  define write_and_clean_defarg_3() (NULL)
#endif

extern pthread_mutex_t mpack_main_mutex;
extern pthread_mutex_t api_mutex;

#undef INTERN

/*======================================================================================*/
#ifdef __cplusplus
}
#endif
#endif /* intern.h */
