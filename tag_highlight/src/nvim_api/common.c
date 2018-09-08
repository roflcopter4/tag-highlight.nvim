#include "util/util.h"
#include <dirent.h>

#include "nvim_api/api.h"
#include "intern.h"
#include "mpack/mpack.h"

#define PTHREAD_MUTEX_ACTION(MUT, ...)     \
        do {                               \
                pthread_mutex_lock(MUT);   \
                __VA_ARGS__                \
                pthread_mutex_unlock(MUT); \
        } while (0)

uint32_t        sok_count, io_count;
pthread_mutex_t mpack_main_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t api_mutex = PTHREAD_MUTEX_INITIALIZER;

genlist *wait_list = NULL;
pthread_mutex_t wait_list_mutex = PTHREAD_MUTEX_INITIALIZER;

static void free_wait_list(void);

/*======================================================================================*/

mpack_obj *
await_package(const int fd, const int count, const enum message_types mtype)
{
        pthread_cond_t    cond = PTHREAD_COND_INITIALIZER;
        struct nvim_wait *wt;

        {
                pthread_mutex_lock(&wait_list_mutex);
                if (!wait_list) {
                        wait_list = genlist_create_alloc(INIT_WAIT_LISTSZ);
                        atexit(free_wait_list);
                }

                wt  = xmalloc(sizeof(struct nvim_wait));
                *wt = (struct nvim_wait){mtype, (int16_t)fd, (int32_t)count, &cond, NULL};
                genlist_append(wait_list, wt);
                pthread_mutex_unlock(&wait_list_mutex);
        }

        pthread_mutex_lock(&api_mutex);
        pthread_cond_wait(&cond, &api_mutex);

        if (!wt->obj)
                errx(1, "Thread %u signalled with invalid object: aborting.",
                     (unsigned)pthread_self());

        const enum message_types mestype = m_expect(m_index(wt->obj, 0), E_NUM, false).num;

            if (mestype != MES_RESPONSE)
                errx(1, "Thread %u signalled object of incorrect type (%s vs %s): aborting.",
                     (unsigned)pthread_self(), m_message_type_repr[mestype],
                     m_message_type_repr[MES_RESPONSE]);

        mpack_obj *ret = wt->obj;

        pthread_mutex_lock(&wait_list_mutex);
        genlist_remove(wait_list, wt);
        pthread_mutex_unlock(&wait_list_mutex);

        pthread_mutex_unlock(&api_mutex);
        return ret;
}

mpack_obj *
generic_call(int *fd, const bstring *fn, const bstring *fmt, ...)
{
        pthread_mutex_lock(&mpack_main_mutex);
        CHECK_DEF_FD(*fd);
        mpack_obj *pack;
        const int  count = INC_COUNT(*fd);

        if (fmt) {
                va_list        ap;
                const unsigned size = fmt->slen + 16u;
                char *         buf  = alloca(size);
                snprintf(buf, size, "[d,d,s:[!%s]]", BS(fmt));

                va_start(ap, fmt);
                pack = encode_fmt(0, buf, MES_REQUEST, count, fn, &ap);
                va_end(ap);
        } else {
                pack = encode_fmt(0, "[d,d,s:[]]", MES_REQUEST, count, fn);
        }

        write_and_clean(*fd, pack, fn);

        /* mpack_obj *result = decode_stream(*fd, MES_RESPONSE); */
        mpack_obj *result = await_package(*fd, count, MES_RESPONSE);
        mpack_print_object(mpack_log, result);
        pthread_mutex_unlock(&mpack_main_mutex);
        return result;
}

/*======================================================================================*/

bstring *
get_notification(int fd)
{
        /* No mutex to lock here. */
        CHECK_DEF_FD(fd);
        /* mpack_obj *result = decode_stream(fd, MES_NOTIFICATION); */
        mpack_obj *result = await_package(fd, (-1), MES_NOTIFICATION);
        bstring   *ret    = b_strcpy(result->DAI[1]->data.str);
        PRINT_AND_DESTROY(result);
        return ret;
}

void
write_and_clean(const int fd, mpack_obj *pack, const bstring *func)
{
#ifdef DEBUG
#  ifdef LOG_RAW_MPACK
        char tmp[512]; snprintf(tmp, 512, "%s/rawmpack.log", HOME);
        const int rawlog = safe_open(tmp, O_CREAT|O_APPEND|O_WRONLY|O_DSYNC|O_BINARY, 0644);
        b_write(rawlog, B("\n"), *pack->packed, B("\n"));
        close(rawlog);
#  endif
        if (func && mpack_log)
                fprintf(mpack_log, "=================================\n"
                        "Writing request no %d to fd %d: \"%s\"\n",
                        COUNT(fd) - 1, fd, BS(func));

        mpack_print_object(mpack_log, pack);
#endif
        b_write(fd, *pack->packed);
        mpack_destroy(pack);
}

retval_t
m_expect_intern(mpack_obj *root, mpack_expect_t type)
{
        mpack_obj *errmsg = m_index(root, 2);
        mpack_obj *data   = m_index(root, 3);
        retval_t   ret    = { .ptr = NULL };

        if (mpack_type(errmsg) != MPACK_NIL) {
                bstring *err_str = m_expect(m_index(errmsg, 1), E_STRING, true).ptr;
                if (err_str) {
                        warnx("Neovim returned with an err_str: '%s'", BS(err_str));
                        b_destroy(err_str);
                        root->DAI[2] = NULL;
                }
        } else {
                ret = m_expect(data, type, true);
                root->DAI[3] = NULL;
        }

        mpack_destroy(root);
        return ret;
}

static void
free_wait_list(void)
{
        if (wait_list && wait_list->lst)
                genlist_destroy(wait_list);
}
