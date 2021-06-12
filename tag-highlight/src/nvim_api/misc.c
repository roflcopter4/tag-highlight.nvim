#include "Common.h"

#ifdef DOSISH
//#  include <direct.h>
#  undef mkdir
#  define mkdir(PATH, MODE) _mkdir(PATH)
#else
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/un.h>
#endif

#include "intern.h"
#include "mpack/mpack.h"
#include "nvim_api/api.h"

extern genlist *_nvim_wait_list;
genlist        *_nvim_wait_list   = NULL;

#if 0
genlist *nvim_connections  = NULL;
int      _nvim_api_read_fd = 0;

static void add_nvim_connection(const int fd, const enum nvim_connection_type type);
static void clean_nvim_connections(void);
static void free_wait_list(void);

/*======================================================================================*/

 void
(_nvim_init)(const enum nvim_connection_type init_type, const int init_fd)
{
        if (nvim_connections)
                errx(1, "Neovim connection already initialized.");
        nvim_connections = genlist_create_alloc(8);
        add_nvim_connection(init_fd, init_type);
        atexit(clean_nvim_connections);

        _nvim_wait_list = genlist_create_alloc(INIT_WAIT_LISTSZ);
        atexit(free_wait_list);
}
void
_nvim_init(void)
{
        if (nvim_connections)
                errx(1, "Neovim connection already initialized.");
        nvim_connections = genlist_create_alloc(8);
        add_nvim_connection(1, NVIM_SOCKET);
        atexit(clean_nvim_connections);

        _nvim_wait_list = genlist_create_alloc(INIT_WAIT_LISTSZ);
        atexit(free_wait_list);
}


/*
 * Request for Neovim to create an additional server socket, then connect to it.
 * In Windows we must instead use a named pipe, opened like any other file.
 * Windows does support Unix sockets nowadays, but Neovim doesn't use them.
 */
int
_nvim_create_socket(void)
{
        bstring *name = nvim_call_function(B("serverstart"), E_STRING).ptr;

#ifdef DOSISH
        const int fd = safe_open(BS(name), O_RDWR|O_BINARY, 0);
        add_nvim_connection(fd, NVIM_FILE);
#else
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        memcpy(addr.sun_path, name->data, name->slen + 1);
        addr.sun_family = AF_UNIX;
        const int fd    = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == (-1))
                err(1, "Failed to create socket instance.");
        if (connect(fd, (struct sockaddr *)(&addr), sizeof(addr)) == (-1))
                err(2, "Failed to connect to socket.");

        add_nvim_connection(fd, NVIM_SOCKET);
#endif

        b_destroy(name);
        return fd;
}

static void
add_nvim_connection(const int fd, const enum nvim_connection_type type)
{
        struct nvim_connection *con = malloc(sizeof(struct nvim_connection));
        *con = (struct nvim_connection){fd, 0, type};
        genlist_append(nvim_connections, con);
}

static void
clean_nvim_connections(void)
{
        genlist_destroy(nvim_connections);
}

static void
free_wait_list(void)
{
        if (_nvim_wait_list && _nvim_wait_list->lst)
                genlist_destroy(_nvim_wait_list);
}
#endif

int
(_nvim_get_tmpfile)(bstring *restrict*restrict name, const bstring *restrict suffix)
{
        bstring *tmp = nvim_call_function(B("tempname"), E_STRING).ptr;
        if (suffix)
                b_concat(tmp, suffix);
        int ret = safe_open(BS(tmp), O_BINARY|O_CREAT|O_WRONLY, 0600);
        if (name)
                *name = tmp;
        else
                b_destroy(tmp);
        return ret;
}

int
nvim_api_intern_get_fd_count(UNUSED const int fd, const bool inc)
{
        static atomic_int count = ATOMIC_VAR_INIT(0);
        int ret;

        if (inc) {
                ret = atomic_fetch_add(&count, 1);
        } else {
                ret = atomic_load(&count);
        }

        return ret;

#if 0
        pthread_mutex_lock(&nvim_connections->mut);

        for (unsigned i = 0; i < nvim_connections->qty; ++i) {
                struct nvim_connection *cur = nvim_connections->lst[i];

                if (cur->fd == fd) {
                        const int ret = cur->count;
                        if (inc)
                                ++cur->count;
                        pthread_mutex_unlock(&nvim_connections->mut);
                        return ret;
                }
        }

        errx(1, "Couldn't find fd %d in nvim_connections.", fd);
#endif
}
