#include "util.h"
#include <sys/stat.h>

#include "data.h"
#include "mpack/mpack.h"

#define STARTSIZE 1024
#define GUESS 100
#define INC   10

#ifdef DOSISH
#  define restrict __restrict
   extern const char *program_invocation_short_name;
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
        char buf[SAFE_PATH_MAX + 1];
        vsnprintf(buf, SAFE_PATH_MAX + 1, fmt, va);
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
        char buf[SAFE_PATH_MAX + 1];
        vsnprintf(buf, SAFE_PATH_MAX + 1, fmt, va);
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
        struct stat st;
        SAFE_STAT(filename, &st);
        return S_ISREG(st.st_mode) || S_ISFIFO(st.st_mode);
}

int64_t
xatoi__(const char *const str, const bool strict)
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

#define ERRSTACKSIZE (6384)
void
err_(UNUSED const int status, const bool print_err, const char *const __restrict fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        char buf[ERRSTACKSIZE];

        if (print_err)
                snprintf(buf, ERRSTACKSIZE, "%s: %s: %s\n", program_invocation_short_name, fmt, strerror(errno));
        else
                snprintf(buf, ERRSTACKSIZE, "%s: %s\n", program_invocation_short_name, fmt);

        __mingw_vfprintf(stderr, buf, ap);
        va_end(ap);

        SHOW_STACKTRACE();

        abort();
        /* exit(status); */
}

extern FILE *echo_log;
void
warn_(const bool print_err, const char *const __restrict fmt, ...)
{
        va_list ap1, ap2;
        va_start(ap1, fmt);
        va_start(ap2, fmt);
        char buf[ERRSTACKSIZE];

        if (print_err)
                snprintf(buf, ERRSTACKSIZE, "%s: %s: %s\n", program_invocation_short_name, fmt, strerror(errno));
        else
                snprintf(buf, ERRSTACKSIZE, "%s: %s\n", program_invocation_short_name, fmt);

        vfprintf(stderr, buf, ap1);

/* #ifdef DEBUG
        vfprintf(echo_log, buf, ap2);
        fflush(echo_log);
#endif */

        va_end(ap1);
        va_end(ap2);
}

unsigned
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


#if defined(__GNUC__) && !defined(__clang__) && !defined(__cplusplus)
const char *
ret_func_name__(const char *const function, const size_t size)
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
free_all__(void *ptr, ...)
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
        if (!list || !list->lst || list->qty == 0)
                return;
        for (unsigned i = 0; i < list->qty; ++i)
                xfree(list->lst[i]);
        list->qty = 0;
}

#ifdef HAVE_FORK
#  define READ_FD  (0)
#  define WRITE_FD (1)
#  include <wait.h>

bstring *
(get_command_output)(const char *command, char *const *const argv, bstring *input)
{
        int fds[2][2], pid, status;
        if (pipe2(fds[0], O_CLOEXEC) == (-1) || pipe2(fds[1], O_CLOEXEC) == (-1)) 
                err(1, "pipe() failed\n");
        if ((pid = fork()) == 0) {
                if (dup2(fds[0][READ_FD], 0) == (-1) || dup2(fds[1][WRITE_FD], 1) == (-1))
                        err(1, "dup2() failed\n");
                if (execvp(command, argv) == (-1))
                        err(1, "exec() failed\n");
        }

        if (input)
                b_write(fds[0][WRITE_FD], input);
        close  (fds[0][READ_FD]);
        close  (fds[1][WRITE_FD]);
        close  (fds[0][WRITE_FD]);
        waitpid(pid, &status, 0);
        bstring *rd = b_read_fd(fds[1][READ_FD]);
        close(fds[1][READ_FD]);

        if ((status <<= 8) != 0)
                SHOUT("Command failed with status %d\n", status);
        return rd;
}
#endif
