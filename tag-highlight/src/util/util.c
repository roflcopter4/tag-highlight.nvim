#include "Common.h"
#include "util.h"
#include "mpack/mpack.h"
#include <sys/stat.h>

#include "highlight.h"

#define STARTSIZE 1024
#define GUESS 100
#define INC   10

#ifdef DOSISH
#  define restrict __restrict
   extern const char *program_invocation_short_name;
static void win32_print_stack(void);
#endif

#define SAFE_STAT(PATH, ST)                                     \
     do {                                                       \
             if ((stat((PATH), (ST)) != 0))                     \
                     err(1, "Failed to stat file '%s", (PATH)); \
     } while (0)

#define SHUTUPGCC __attribute__((__unused__)) ssize_t n =

#ifdef HAVE_EXECINFO_H
#  include <execinfo.h>
#  define SHOW_STACKTRACE()                                    \
        __extension__({                                        \
                void * arr[128];                               \
                size_t num = backtrace(arr, 128);              \
                fflush(stderr); fsync(2);                      \
                SHUTUPGCC write(2, SLS("<<< FATAL ERROR >>>\n" \
                                       "STACKTRACE:\n"));      \
                backtrace_symbols_fd(arr, num, 2);             \
                fsync(2);                                      \
        })
#  define FATAL_ERROR(...)                                             \
        __extension__({                                                \
                void * arr[128];                                       \
                char   buf[8192];                                      \
                size_t num = backtrace(arr, 128);                      \
                snprintf(buf, 8192, __VA_ARGS__);                      \
                fflush(stderr);                                        \
                SHUTUPGCC write(2, SLS("Fatal error\nSTACKTRACE:\n")); \
                backtrace_symbols_fd(arr, num, 2);                     \
                fsync(2);                                              \
                abort();                                               \
        })
#elif defined DOSISH
#  define FATAL_ERROR(...)                                             \
        do {                                                           \
                fflush(stderr);                                        \
                SHUTUPGCC write(2, SLS("Fatal error\nSTACKTRACE:\n")); \
                win32_print_stack();                                   \
                abort();                                               \
        } while (0)
#  define SHOW_STACKTRACE(...) (win32_print_stack())
#else
#  define FATAL_ERROR(...)                                             \
        do {                                                           \
                fflush(stderr);                                        \
                SHUTUPGCC write(2, SLS("Fatal error\nSTACKTRACE:\n")); \
                fsync(2);                                              \
                abort();                                               \
        } while (0)
#  define SHOW_STACKTRACE(...)
#endif

static bool file_is_reg(const char *filename);
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

__attribute__((__constructor__))
static void mutex_init(void)
{
        pthread_mutex_init(&mut);
}


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
safe_fopen_fmt(const char *const restrict mode,
               const char *const restrict fmt,
               ...)
{
        va_list va;
        va_start(va, fmt);
        char buf[SAFE_PATH_MAX + 1];
        vsnprintf(buf, SAFE_PATH_MAX + 1, fmt, va);
        va_end(va);

#ifdef DOSISH
        char newmode[8];
        {
              char const *ptr = mode;
              int         i = 0;
              char        ch;
              while((ch = *ptr++))
                    if (ch != 'e')
                          newmode[i++] = ch;
              newmode[i] = '\0';
        }
#else
        char const *const newmode = mode;
#endif

        FILE *fp = fopen(buf, newmode);
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
safe_open_fmt(const int flags, const int mode, const char *const restrict fmt, ...)
{
        va_list va;
        va_start(va, fmt);
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
        char         *endptr;
        const int64_t val = strtoll(str, &endptr, 10);

        if ((endptr == str) || (strict && *endptr != '\0'))
                errx(30, "Invalid integer \"%s\".\n", str);

        return val;
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

static inline bstring *
get_project_base(const char *fullpath)
{
        static const bstring searchfor = BT("/src/");
        bstring *path = b_fromcstr(fullpath);

        b_regularize_path_sep(path, '/');
        int64_t ind = b_strstr(path, &searchfor, 0);
        if (ind > 0) {
                path->slen -= (ind += 1); // Exclude the leading slash
                memmove(path->data, path->data + ind, path->slen + 1);
        }

        return path;
}

#define ERRSTACKSIZE (6384)
void
err_(int  const UNUSED    status,
     bool const           print_err,
     char const *restrict file,
     int  const           line,
     char const *restrict func,
     char const *restrict fmt,
     ...)
{
        error_t const e = errno;
        va_list       ap;
        va_start(ap, fmt);
        bstring *base = get_project_base(file);

        fprintf(stderr, "%s: (%s:%d - %s): ", program_invocation_short_name, BS(base), line, func);
        vfprintf(stderr, fmt, ap);
        if (print_err)
                fprintf(stderr, ": %s\n", strerror(e));
        else
                fputc('\n', stderr);
        va_end(ap);
        talloc_free(base);

        SHOW_STACKTRACE();
        fputc('\n', stderr);
        fflush(stderr);
        /* exit(status); */
        abort();
}

extern FILE *echo_log;
void
warn_(bool const           print_err,
      bool const           force,
      char const *restrict file,
      int  const           line,
      char const *restrict func,
      char const *restrict fmt,
      ...)
{
        static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

        if (!settings.verbose && !force)
                return;

        bstring      *base = get_project_base(file);
        va_list       ap;
        error_t const e = errno;
        pthread_mutex_lock(&mut);

        fprintf(stderr, "%s: (%s:%d - %s): ", program_invocation_short_name, BS(base), line, func);

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);

        if (print_err)
                fprintf(stderr, ": %s\n", strerror(e));
        else
                fputc('\n', stderr);

        talloc_free(base);
        fflush(stderr);
        pthread_mutex_unlock(&mut);
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

        do free(ptr);
        while ((ptr = va_arg(ap, void *)) != NULL);

        va_end(ap);
}

#define READ_FD  (0)
#define WRITE_FD (1)

#if defined HAVE_FORK
#  include <sys/wait.h>
#  define CLOSE(fd) (((close)(fd) == (-1)) ? err(1, "close()") : ((void)0))

void
fd_set_open_flag(int const fd, int const flag)
{
        int cur = fcntl(fd, F_GETFL);
        if (cur == (-1))
                err(1, "fcntl()");
        if (fcntl(fd, F_SETFL, cur | flag) == (-1))
                err(1, "fcntl()");
}

static void
open_pipe(int fds[2])
{
# ifdef HAVE_PIPE2
        if (pipe2(fds, O_CLOEXEC) == (-1))
                err(1, "pipe2()");
# else
        int flg;
        if (pipe(fds) == (-1))
                err(1, "pipe()");
        /* Surely the compiler will unroll this... */
        for (int i = 0; i < 2; ++i) {
                if ((flg = fcntl(fds[i], F_GETFL)) == (-1))
                        err(3+i, "fcntl(F_GETFL)");
                if (fcntl(fds[i], F_SETFL, flg | O_CLOEXEC) == (-1))
                        err(5+i, "fcntl(F_SETFL)");
        }
# endif
# ifdef __linux__  /* Can't do this on the BSDs. */
        if (fcntl(fds[0], F_SETPIPE_SZ, 16384) == (-1))
                err(2, "fcntl(F_SETPIPE_SZ)");
# endif
}

bstring *
get_command_output(const char *command, char *const *const argv, bstring *input, int *status)
{
        bstring *rd;
        int fds[2][2], pid, st = ~0;

        open_pipe(fds[0]);
        open_pipe(fds[1]);

        if ((pid = fork()) == 0) {
                if (dup2(fds[0][READ_FD], STDIN_FILENO) == (-1))
                        err(1, "dup2() failed\n");
                if (dup2(fds[1][WRITE_FD], STDOUT_FILENO) == (-1))
                        err(1, "dup2() failed\n");

                close(fds[0][0]);
                close(fds[0][1]);
                close(fds[1][0]);
                close(fds[1][1]);

                if (execvp(command, argv) == (-1))
                        err(1, "exec() failed\n");
        }

        close(fds[0][READ_FD]);
        close(fds[1][WRITE_FD]);
        if (input)                                  
                b_write(fds[0][WRITE_FD], input);
        close(fds[0][WRITE_FD]);

        rd = b_read_fd(fds[1][READ_FD]);
        close(fds[1][READ_FD]);

        if (waitpid(pid, &st, 0) != pid && errno != ECHILD)
                err(1, "waitpid()");
        if ((st >>= 8) != 0)
                echo("WARNING: Command failed with status %d", st);
        if (status)
                *status = st;

        return rd;
}

#if 0
bstring *
get_command_output(const char *command, char *const *const argv, bstring *input, int *status)
{
        bstring *tmpfname, *rd;
        int stdin_fds[2],  pid, st, tmpfd;

        if (pipe(stdin_fds) == (-1))
                err(1, "pipe()");

        tmpfname = nvim_call_function(B("tempname"), E_STRING).ptr;
        tmpfd    = safe_open(BS(tmpfname), O_CREAT|O_RDWR|O_TRUNC|O_BINARY, 0600);

        if ((pid = fork()) == 0) {
                if (dup2(stdin_fds[READ_FD], STDIN_FILENO) == (-1))
                        err(1, "dup2() failed\n");
                if (dup2(tmpfd, STDOUT_FILENO) == (-1))
                        err(1, "dup2() failed\n");

                close(stdin_fds[WRITE_FD]);
                close(stdin_fds[READ_FD]);
                close(tmpfd);

                if (execvp(command, argv) == (-1))
                        err(1, "exec() failed\n");
        }

        close(stdin_fds[READ_FD]);
        if (input)                                  
                b_write(stdin_fds[WRITE_FD], input);
        close(stdin_fds[WRITE_FD]);

        if (waitpid(pid, &st, 0) != pid && errno != ECHILD)
                err(1, "waitpid()");

        lseek(tmpfd, 0, SEEK_SET);
        rd = b_read_fd(tmpfd);
        close(tmpfd);
        remove(BS(tmpfname));
        b_free(tmpfname);

        if (WEXITSTATUS(st) != 0)
                warnx("Command failed with status %d (raw: 0x%X)", WEXITSTATUS(st), st);
        if (status)
                *status = st;

        return rd;
}
#endif

#  undef CLOSE

#elif defined DOSISH
#  define READ_BUFSIZE (8192LLU << 2)

static bstring *read_from_pipe(HANDLE han);

bstring *
get_command_output(const char *command, char *const *const argv, bstring *input, int *status)
{
        bstring *commandline = b_fromcstr("");

        for (char **s = (char **)argv; *s; ++s) {
                b_catchar(commandline, '"');
                b_catcstr(commandline, *s);
                b_catlit(commandline, "\" ");
        }

        bstring *ret = _win32_get_command_output(BS(commandline), input, status);
        b_destroy(commandline);
        return ret;
}

int
win32_start_process_with_pipe(char const *exe, char *argv, HANDLE pipehandles[2], PROCESS_INFORMATION *pi)
{
        HANDLE              handles[2][2];
        STARTUPINFOA        info;
        SECURITY_ATTRIBUTES attr = {.nLength = sizeof(SECURITY_ATTRIBUTES),
                                    .bInheritHandle = TRUE,
                                    .lpSecurityDescriptor = NULL};

        if (!CreatePipe(&handles[0][READ_FD], &handles[0][WRITE_FD], &attr, 0)) 
                win32_error_exit(1, "CreatePipe()", GetLastError());
        if (!CreatePipe(&handles[1][READ_FD], &handles[1][WRITE_FD], &attr, 0)) 
                win32_error_exit(1, "CreatePipe()", GetLastError());
        if (!SetHandleInformation(handles[0][WRITE_FD], HANDLE_FLAG_INHERIT, 0))
                win32_error_exit(1, "Stdin SetHandleInformation", GetLastError());
        if (!SetHandleInformation(handles[1][READ_FD], HANDLE_FLAG_INHERIT, 0))
                win32_error_exit(1, "Stdout SetHandleInformation", GetLastError());

        memset(&info, 0, sizeof(STARTUPINFOA));
        memset(pi, 0, sizeof(PROCESS_INFORMATION));

        info.cb         = sizeof(STARTUPINFOA);
        info.dwFlags    = STARTF_USESTDHANDLES;
        info.hStdInput  = handles[0][READ_FD];
        info.hStdOutput = handles[1][WRITE_FD];
        info.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

        if (!SetHandleInformation(info.hStdError, HANDLE_FLAG_INHERIT, 1))
                win32_error_exit(1, "Stdout SetHandleInformation", GetLastError());

        if (!CreateProcessA(exe, argv, NULL, NULL, TRUE, 0, NULL, NULL, &info, pi))
                win32_error_exit(1, "CreateProcess() failed", GetLastError());
        CloseHandle(handles[0][READ_FD]);
        CloseHandle(handles[1][WRITE_FD]);

        pipehandles[WRITE_FD] = handles[0][WRITE_FD];
        pipehandles[READ_FD]  = handles[1][READ_FD];
        return 0;
}

bstring *
_win32_get_command_output(char *argv, bstring *input, int *status)
{
        PROCESS_INFORMATION pi;
        HANDLE handles[2];
        DWORD  written, st;
        win32_start_process_with_pipe(NULL, argv, handles, &pi);

        if (!WriteFile(handles[WRITE_FD], input->data,
                       input->slen, &written, NULL) || written != input->slen)
                win32_error_exit(1, "WriteFile()", GetLastError());
        CloseHandle(handles[WRITE_FD]);
        
        bstring *ret = read_from_pipe(handles[READ_FD]);
        CloseHandle(handles[READ_FD]);

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &st);
        if (st != 0)
                SHOUT("Command failed with status %ld\n", st);
        if (status)
                *status = st;
        
        return ret;
}

// Read output from the child process's pipe for STDOUT
// and write to the parent process's pipe for STDOUT. 
// Stop when there is no more data. 
static bstring *
read_from_pipe(HANDLE han)
{
        DWORD    dwRead;
        CHAR     chBuf[READ_BUFSIZE];
        bstring *ret = b_alloc_null(READ_BUFSIZE);

        for (;;) {
                bool bSuccess = ReadFile(han, chBuf, READ_BUFSIZE, &dwRead, NULL);
                if (dwRead)
                        b_catblk(ret, chBuf, (unsigned)dwRead);
                if (!bSuccess || dwRead == 0)
                        break;
        }

        return ret;
} 

noreturn void
win32_error_exit(int const status, const char *msg, DWORD const dw)
{
        char *lpMsgBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
        eprintf("%s: Error: %s: %s\n", program_invocation_short_name, msg, lpMsgBuf);
        fflush(stderr);
        LocalFree(lpMsgBuf);
        SHOW_STACKTRACE();
        ExitProcess(status);
}
#else
#  error "Impossible operating system detected. VMS? OS/2? DOS? Maybe System/360? Yeesh."
#endif

#undef READ_FD
#undef WRITE_FD


#ifdef DOSISH

#include <dbghelp.h>

static void
win32_print_stack(void)
{
      unsigned int   i;
      void          *stack[100];
      unsigned short frames;
      SYMBOL_INFO   *symbol;
      HANDLE         process;

      process = GetCurrentProcess();

      SymInitialize(process, NULL, TRUE);

      frames = CaptureStackBackTrace(0, 100, stack, NULL);
      symbol = calloc(sizeof(SYMBOL_INFO) + (SIZE_C(256) * sizeof(char)), 1LLU);
      symbol->MaxNameLen   = 255;
      symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

      for (i = 0; i < frames; i++) {
            SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
            eprintf("%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address);
      }

      free(symbol);
}

#endif
