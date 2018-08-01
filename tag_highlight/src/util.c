#include "util.h"
#include <dirent.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "mpack.h"

#define STARTSIZE 1024
#define GUESS 100
#define INC   10

#ifdef DOSISH
#  define restrict __restrict
#endif

#define SAFE_STAT(PATH, ST)                                     \
     do {                                                       \
             if ((stat((PATH), (ST)) != 0))                     \
                     err(1, "Failed to stat file '%s", (PATH)); \
     } while (0)

extern const char *program_name;

static bool file_is_reg(const char *filename);


FILE *
safe_fopen(const char *filename, const char *mode)
{
        FILE *fp = fopen(filename, mode);
        if (!fp)
                err(1, "Failed to open file \"%s\"", filename);
        if (!file_is_reg(filename))
                errx(1, "Invalid filetype \"%s\"\n", filename);
        return fp;
}

FILE *
safe_fopen_fmt(const char *const restrict fmt,
               const char *const restrict mode,
               ...)
{
        va_list va;
        va_start(va, mode);
        char buf[PATH_MAX + 1];
        vsnprintf(buf, PATH_MAX + 1, fmt, va);
        va_end(va);

        FILE *fp = fopen(buf, mode);
        if (!fp)
                err(1, "Failed to open file \"%s\"", buf);
        if (!file_is_reg(buf))
                errx(1, "Invalid filetype \"%s\"\n", buf);

        return fp;
}


int
safe_open(const char *const filename, const int flags, const int mode)
{
#ifdef DOSISH
        const int fd = open(filename, flags, _S_IREAD|_S_IWRITE);
#else
        const int fd = open(filename, flags, mode);
#endif
        //if (fd == (-1))
        //        err(1, "Failed to open file '%s'", filename);
        if (fd == (-1)) {
                fprintf(stderr, "Failed to open file \"%s\": %s\n", filename, strerror(errno));
                abort();
        }
        return fd;
}


int
safe_open_fmt(const char *const restrict fmt,
              const int flags, const int mode, ...)
{
        va_list va;
        va_start(va, mode);
        char buf[PATH_MAX + 1];
        vsnprintf(buf, PATH_MAX + 1, fmt, va);
        va_end(va);

        errno = 0;
#ifdef DOSISH
        const int fd = open(buf, flags, _S_IREAD|_S_IWRITE);
#else
        const int fd = open(buf, flags, mode);
#endif
        if (fd == (-1)) {
                fprintf(stderr, "Failed to open file \"%s\": %s\n", buf, strerror(errno));
                abort();
        }

        return fd;
}


bool
file_is_reg(const char *filename)
{
        //struct stat st = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        //                  {0}, {0}, {0}, {0, 0, 0}};
        struct stat st;
        SAFE_STAT(filename, &st);
        return S_ISREG(st.st_mode) || S_ISFIFO(st.st_mode);
}


#if 0
#ifdef USE_XMALLOC
void *
xmalloc(const size_t size)
{
        void *tmp = malloc(size);
        if (tmp == NULL)
                err(100, "Malloc call failed - attempted %zu bytes", size);
        return tmp;
}


void *
xcalloc(const int num, const size_t size)
{
        void *tmp = calloc(num, size);
        if (tmp == NULL)
                err(101, "Calloc call failed - attempted %zu bytes", size);
        return tmp;
}
#endif


void *
xrealloc(void *ptr, const size_t size)
{
        void *tmp = realloc(ptr, size);
        if (tmp == NULL)
                err(102, "Realloc call failed - attempted %zu bytes", size);
        return tmp;
}


#ifdef HAVE_REALLOCARRAY
void *
xreallocarray(void *ptr, size_t num, size_t size)
{
        void *tmp = reallocarray(ptr, num, size);
        if (tmp == NULL)
                err(103, "Realloc call failed - attempted %zu bytes", size);
        return tmp;
}
#endif
#endif

#ifdef USE_XMALLOC
void *
xmalloc_(const size_t size, const bstring *caller, const int lineno)
{
        void *tmp = malloc(size);
        if (tmp == NULL)
                err(100, "Malloc call failed - attempted %zu bytes"
                         "Called from %s on line %d.", size, BS(caller), lineno);
        return tmp;
}


void *
xcalloc_(const int num, const size_t size, const bstring *caller, const int lineno)
{
        void *tmp = calloc(num, size);
        if (tmp == NULL)
                err(101, "Calloc call failed - attempted %zu bytes"
                         "Called from %s on line %d.", size, BS(caller), lineno);
        return tmp;
}
#endif


void *
xrealloc_(void *ptr, const size_t size, const bstring *caller, const int lineno)
{
        void *tmp = realloc(ptr, size);
        if (tmp == NULL)
                err(102, "Realloc call failed - attempted %zu bytes.\n"
                         "Called from %s on line %d.", size, BS(caller), lineno);
        return tmp;
}


#ifdef HAVE_REALLOCARRAY
void *
xreallocarray_(void *ptr, size_t num, size_t size, const bstring *caller, const int lineno)
{
        void *tmp = reallocarray(ptr, num, size);
        if (tmp == NULL)
                err(103, "Realloc call failed - attempted %zu bytes"
                         "Called from %s on line %d.", size, BS(caller), lineno);
        return tmp;
}
#endif


int64_t
__xatoi(const char *const str, const bool strict)
{
        char *endptr;
        const long long int val = strtol(str, &endptr, 10);

        if ((endptr == str) || (strict && *endptr != '\0'))
                errx(30, "Invalid integer \"%s\".\n", str);

        return (int)val;
}


#ifdef DOSISH
char *
basename(char *path)
{
        assert(path != NULL && *path != '\0');
        const size_t len = strlen(path);
        char *ptr = path + len;
        while (*ptr != '/' && *ptr != '\\' && ptr != path)
                --ptr;
        
        return (*ptr == '/' || *ptr == '\\') ? ptr + 1 : ptr;
}
#endif


/* #ifndef HAVE_ERR */
#define ERRSTACKSIZE (6384)
void
__err(UNUSED const int status, const bool print_err, const char *const __restrict fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        char buf[ERRSTACKSIZE];

        if (print_err)
                snprintf(buf, ERRSTACKSIZE, "%s: %s: %s\n", program_name, fmt, strerror(errno));
        else
                snprintf(buf, ERRSTACKSIZE, "%s: %s\n", program_name, fmt);

        vfprintf(stderr, buf, ap);
        va_end(ap);

        abort();
        /* exit(status); */
}


extern FILE *echo_log;

void
__warn(const bool print_err, const char *const __restrict fmt, ...)
{
        va_list ap1, ap2;
        va_start(ap1, fmt);
        va_start(ap2, fmt);
        char buf[ERRSTACKSIZE];

        if (print_err)
                snprintf(buf, ERRSTACKSIZE, "%s: %s: %s\n", program_name, fmt, strerror(errno));
        else
                snprintf(buf, ERRSTACKSIZE, "%s: %s\n", program_name, fmt);

        nvim_vprintf(sockfd, buf, ap1);

#ifdef DEBUG
        vfprintf(echo_log, buf, ap2);
        fflush(echo_log);
#endif

        va_end(ap1);
        va_end(ap2);
}
/* #endif */


int
find_num_cpus(void)
{
#ifdef DOSISH
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
#elif MACOS
        int nm[2];
        size_t len = 4;
        uint32_t count;

        nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);

        if (count < 1) {
                nm[1] = HW_NCPU;
                sysctl(nm, 2, &count, &len, NULL, 0);
                if (count < 1) { count = 1; }
        }
        return count;
#else
        return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}


#if 0
#define MAX_INIT 8192
#define READ_BYTES 1

bstring *
read_stdin(void)
{
        bstring *buf = b_alloc_null(MAX_INIT);
        int nread    = read(0, &buf->data[buf->slen], READ_BYTES);

        if (nread == (-1))
                err(1, "read error");
        buf->slen += nread;
        fcntl(0, F_SETFL, oflags | O_NONBLOCK);

        do {
                if (buf->slen >= (buf->mlen - READ_BYTES - 50))
                        b_alloc(buf, buf->mlen * 2);
                nread = read(0, (buf->data + buf->slen), READ_BYTES);
                if (nread > 0)
                        buf->slen += nread;
        } while (nread == READ_BYTES);

        nvim_printf("Read %d bytes from stdin.\n", buf->slen);

        fcntl(0, F_SETFL, oflags);

        return buf;
}
#endif


#if 0
char *
num_to_str(const long long value)
{
        /* Generate the (reversed) string representation. */
        uint64_t inv = (value < 0) ? (-value) : (value);
         /* *ret = b_alloc_null(INT64_MAX_CHARS); */
        char ret[21], *rev, *fwd;
        rev = fwd = ret;

        do {
                *rev++ = (uchar)('0' + (inv % 10));
                inv    = (inv / 10);
        } while (inv);

        if (value < 0)
                *rev++ = (uchar)'-';

        /* Compute length and add null term. */
        *rev-- = '\0';

        /* Reverse the string. */
        while (fwd < rev) {
                char swap = *fwd;
                *fwd++    = *rev;
                *rev++    = swap;
        }

        return (char*[21]){ret};
}
#endif


#if defined(__GNUC__) && !(defined(__clang__) || defined(__cplusplus))
const char *
__ret_func_name(const char *const function, const size_t size)
{
        if (size + 2 > 256)
                return function;
        static _Thread_local char buf[256];
        memcpy(buf, function, size - 1);
        buf[size]   = '(';
        buf[size+1] = ')';
        buf[size+2] = '\0';
        return buf;
}
#endif


/*============================================================================*/
/* List operations */

void
__free_all(void *ptr, ...)
{
        va_list ap;
        va_start(ap, ptr);

        do xfree(ptr);
        while ((ptr = va_arg(ap, void *)) != NULL);

        va_end(ap);
}

void
free_stack_push(struct item_free_stack *list, void *item)
{
        if (list->qty == (list->max - 1))
                list->items = nrealloc(list->items, (list->max *= 2),
                                       sizeof(*list->items));
        list->items[list->qty++] = item;
}

void
add_backup(struct backups *list, void *item)
{
        if (!list->lst)
                list->lst = nmalloc(128, sizeof(char *));
        else if (list->qty >= (list->max - 1))
                list->lst = nrealloc(list->lst, (list->max *= 2),
                                     sizeof *list->lst);
        list->lst[list->qty++] = item;
}

void
free_backups(struct backups *list)
{
        for (unsigned i = 0; i < list->qty; ++i)
                free(list->lst[i]);
        list->qty = 0;
}

void
__b_dump_list_nvim(const b_list *list, const char *const listname)
{
        nvprintf("Dumping list \"%s\"\n", listname);
        for (unsigned i = 0; i < list->qty; ++i)
                nvprintf("%s\n", BS(list->lst[i]));
}
