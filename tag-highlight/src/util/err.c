#include "Common.h"
#include "util.h"
#include "mpack/mpack.h"
#include <sys/stat.h>

#include "highlight.h"

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

#define ERRSTACKSIZE (6384

extern void win32_print_stack(void);
static inline bstring * get_project_base(const char *fullpath);
extern FILE *echo_log;

#include "nvim_api/api.h"

#if 0
#define UNW_LOCAL_ONLY
#include <libunwind.h>

// Call this function to get a backtrace.
static void
do_stacktrace(void)
{
      unw_cursor_t  cursor;
      unw_context_t context;

      // Initialize cursor to current frame for local unwinding.
      unw_getcontext(&context);
      unw_init_local(&cursor, &context);

      // Unwind frames one by one, going up the frame stack.
      while (unw_step(&cursor) > 0) {
            unw_word_t offset, pc;
            unw_get_reg(&cursor, UNW_REG_IP, &pc);
            if (pc == 0) {
                  break;
            }
            fprintf(stderr, "0x%llX:", (long long)pc);

            char sym[256];
            if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
                  fprintf(stderr, " (%s+0x%llX)\n", sym, (long long)offset);
            } else {
                  fprintf(stderr, " -- error: unable to obtain symbol name for this frame\n");
            }
      }
}
#endif

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

        //do_stacktrace();

        /* nvim_err_write(btp_fromblk(tmp_buf, tmp_size)); */
        //fwrite(tmp_buf, 1, tmp_size, stderr);
        fflush(stderr);
        //fclose(tmp);

        /* if (settings.buffer_initialized) */

        exit(status);
        /* abort(); */
}

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

        error_t const e    = errno;
        bstring      *base = get_project_base(file);
        va_list       ap;
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
