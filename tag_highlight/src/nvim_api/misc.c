#include "util/util.h"

#ifdef DOSISH
#  include <direct.h>
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

genlist *nvim_connections = NULL;
genlist *wait_list        = NULL;

static void add_nvim_connection(const int fd, const enum nvim_connection_type type);
static void clean_nvim_connections(void);
static void free_wait_list(void);

/*======================================================================================*/

#if 0
 void
(_nvim_init)(const enum nvim_connection_type init_type, const int init_fd)
{
        if (nvim_connections)
                errx(1, "Neovim connection already initialized.");
        nvim_connections = genlist_create_alloc(8);
        add_nvim_connection(init_fd, init_type);
        atexit(clean_nvim_connections);

        wait_list = genlist_create_alloc(INIT_WAIT_LISTSZ);
        atexit(free_wait_list);
}
#endif
void
_nvim_init(void)
{
        if (nvim_connections)
                errx(1, "Neovim connection already initialized.");
        nvim_connections = genlist_create_alloc(8);
        add_nvim_connection(1, NVIM_SOCKET);
        atexit(clean_nvim_connections);

        wait_list = genlist_create_alloc(INIT_WAIT_LISTSZ);
        atexit(free_wait_list);
}


/*
 * Request for Neovim to create an additional server socket, then connect to it.
 * In Windows we must instead use a named pipe, opened like any other file.
 * Windows does support Unix sockets nowadays, but Neovim doesn't use them.
 */
int
_nvim_create_socket(const int mes_fd)
{
        bstring *name = nvim_call_function(mes_fd, B("serverstart"), E_STRING).ptr;

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

        b_free(name);
        return fd;
}

static void
add_nvim_connection(const int fd, const enum nvim_connection_type type)
{
        struct nvim_connection *con = xmalloc(sizeof(struct nvim_connection));
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
        if (wait_list && wait_list->lst)
                genlist_destroy(wait_list);
}
