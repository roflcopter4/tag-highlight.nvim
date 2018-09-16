#include "util.h"
#include <dirent.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "mpack/mpack.h"

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

#ifdef HAVE_EXECINFO_H
#  include <execinfo.h>
#  define SHOW_STACKTRACE()                        \
        __extension__({                            \
                void * arr[128];                   \
                size_t num = backtrace(arr, 128);  \
                fflush(stderr);                    \
                dprintf(2, "STACKTRACE: \n");      \
                backtrace_symbols_fd(arr, num, 2); \
        })
#  define FATAL_ERROR(...)                                         \
        __extension__({                                            \
                void * arr[128];                                   \
                char   buf[8192];                                  \
                size_t num = backtrace(arr, 128);                  \
                snprintf(buf, 8192, __VA_ARGS__);                  \
                                                                   \
                warnx("Fatal error in func %s in %s, line "        \
                      "%d\n%sSTACKTRACE: ",                        \
                      FUNC_NAME, __FILE__, __LINE__, buf);         \
                fflush(stderr);                                    \
                backtrace_symbols_fd(arr, num, 2);                 \
                abort();                                           \
        })
#else
#  define FATAL_ERROR(...)                                    \
        do {                                                \
                warnx("Fatal error in func %s in %s, line " \
                      "%d\n%sSTACKTRACE: ",                 \
                      FUNC_NAME, __FILE__, __LINE__, buf);  \
                fflush(stderr);                             \
        } while (0)
#  define SHOW_STACKTRACE(...)
#endif

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


#ifdef USE_XMALLOC
void *
xmalloc(const size_t size)
{
        void *tmp = malloc(size);
        if (tmp == NULL)
                FATAL_ERROR("Malloc call failed - attempted %zu bytes", size);
                /* err(100, "Malloc call failed - attempted %zu bytes", size); */
        return tmp;
}


void *
xcalloc(const int num, const size_t size)
{
        void *tmp = calloc(num, size);
        if (tmp == NULL)
                FATAL_ERROR("Calloc call failed - attempted %zu bytes", size);
                /* err(101, "Calloc call failed - attempted %zu bytes", size); */
        return tmp;
}
#endif


void *
xrealloc(void *ptr, const size_t size)
{
        void *tmp = realloc(ptr, size);
        if (tmp == NULL)
                FATAL_ERROR("Realloc call failed - attempted %zu bytes", size);
                /* err(102, "Realloc call failed - attempted %zu bytes", size); */
        return tmp;
}


#ifdef HAVE_REALLOCARRAY
void *
xreallocarray(void *ptr, size_t num, size_t size)
{
        void *tmp = reallocarray(ptr, num, size);
        if (tmp == NULL)
                FATAL_ERROR("Realloc call failed - attempted %zu bytes", size);
                /* err(103, "Realloc call failed - attempted %zu bytes", size); */
        return tmp;
}
#endif

#if 0
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
                snprintf(buf, ERRSTACKSIZE, "%s: %s: %s\n", program_invocation_short_name, fmt, strerror(errno));
        else
                snprintf(buf, ERRSTACKSIZE, "%s: %s\n", program_invocation_short_name, fmt);

        vfprintf(stderr, buf, ap);
        va_end(ap);

        SHOW_STACKTRACE();

        abort();
        /* exit(status); */
}


extern FILE *echo_log;
extern int mainchan;

void
__warn(const bool print_err, const char *const __restrict fmt, ...)
{
        va_list ap1, ap2;
        va_start(ap1, fmt);
        va_start(ap2, fmt);
        char buf[ERRSTACKSIZE];

        if (print_err)
                snprintf(buf, ERRSTACKSIZE, "%s: %s: %s\n", program_invocation_short_name, fmt, strerror(errno));
        else
                snprintf(buf, ERRSTACKSIZE, "%s: %s\n", program_invocation_short_name, fmt);

        nvim_vprintf(mainchan, buf, ap1);

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
#if defined(DOSISH)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
#elif defined(MACOS)
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
#elif defined(__unix__) || defined(__linux__) || defined(BSD)
        return sysconf(_SC_NPROCESSORS_ONLN);
#else
#  error "Cannot determine operating system."
#endif
}


#if defined(__GNUC__) && !(defined(__clang__) || defined(__cplusplus))
const char *
__ret_func_name(const char *const function, const size_t size)
{
        if (size + 2 > 256)
                return function;
        static thread_local char buf[256];
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
                xfree(list->lst[i]);
        list->qty = 0;
}

void
__b_list_dump_nvim(const b_list *list, const char *const listname)
{
        echo("Dumping list \"%s\"\n", listname);
        for (unsigned i = 0; i < list->qty; ++i)
                echo("%s\n", BS(list->lst[i]));
}
