#include "Common.h"

#ifdef _WIN32
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
genlist        *_nvim_wait_list = NULL;

int
(nvimext_get_tmpfile)(bstring *restrict*restrict name, const bstring *restrict suffix)
{
        bstring *tmp = nvim_call_function(B("tempname"), E_STRING).ptr;
        if (suffix)
                b_concat(tmp, suffix);
        int ret = safe_open(BS(tmp), O_BINARY|O_CREAT|O_WRONLY|O_EXCL, 0600);
        if (name)
                *name = tmp;
        else
                b_destroy(tmp);
        return ret;
}

int
nvim_api_intern_get_fd_count(UNUSED const int fd, const bool inc)
{
        static atomic_int count = 0;
        int ret;

        if (inc)
                ret = atomic_fetch_add(&count, 1);
        else
                ret = atomic_load(&count);

        return ret;
}
