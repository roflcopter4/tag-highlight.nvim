#ifndef NVIM_API_INTERN_H_
#define NVIM_API_INTERN_H_

#include "Common.h"

#include "contrib/p99/p99_defarg.h"
#include "highlight.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"

__BEGIN_DECLS
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
#define COUNT()         _get_fd_count((1), false)
#define INC_COUNT()     _get_fd_count((1), true)

#define CHECK_DEF_FD(FD__)                  \
        __extension__ ({                    \
                int m_fd_ = (FD__);         \
                if (m_fd_ == 0)             \
                        m_fd_ = DEFAULT_FD; \
                (FD__) = m_fd_;             \
        })

/* ((FD__) = (((FD__) == 0) ? DEFAULT_FD : (FD__))) */

#define BS_FROMARR(ARRAY_) {(sizeof(ARRAY_) - 1), 0, (unsigned char *)(ARRAY_), 0}
#define INIT_WAIT_LISTSZ   (64)
#define INTERN __attribute__((__visibility__("hidden"))) extern

static inline int _get_fd_count(const int fd, const bool inc)
{
        /* if (inc)
                pthread_rwlock_wrlock(&nvim_connections->lock);
        else
                pthread_rwlock_rdlock(&nvim_connections->lock); */
        pthread_mutex_lock(&nvim_connections->mut);

        for (unsigned i = 0; i < nvim_connections->qty; ++i) {
                if (((struct nvim_connection *)(nvim_connections->lst[i]))->fd == fd) {
                        const int ret = ((struct nvim_connection *)(nvim_connections->lst[i]))->count;
                        if (inc)
                                ++((struct nvim_connection *)(nvim_connections->lst[i]))->count;
                        pthread_mutex_unlock(&nvim_connections->mut);
                        /* pthread_rwlock_unlock(&nvim_connections->lock); */
                        return ret;
                }
        }

        errx(1, "Couldn't find fd %d in nvim_connections.", fd);
}

INTERN mpack_obj *generic_call(const bstring *fn, const bstring *fmt, ...) __aWUR;
INTERN mpack_obj *await_package(_nvim_wait_node *node) __aWUR;

/* #define LOG_RAW_MPACK */
INTERN mpack_obj *write_and_clean(mpack_obj *pack, const int count, const bstring *func, FILE *logfp) __aWUR;
/* INTERN mpack_obj *write_and_clean(const int fd, mpack_obj *pack, const int count */
/* #ifdef LOG_RAW_MPACK */
                  /* ,const bstring *func, FILE *logfp */
/* #endif */
                /* ) __aWUR; */

INTERN retval_t   m_expect_intern(mpack_obj *root, mpack_expect_t type) __aWUR;

#define write_and_clean(...) P99_CALL_DEFARG(write_and_clean, 4, __VA_ARGS__)
#define write_and_clean_defarg_3() (NULL)
#ifdef DEBUG
#  define write_and_clean_defarg_4() (mpack_log)
#else
#  define write_and_clean_defarg_4() (NULL)
#endif

extern pthread_mutex_t mpack_main_mutex;
extern pthread_mutex_t api_mutex;

#undef INTERN

/*======================================================================================*/
__END_DECLS
#endif /* intern.h */

