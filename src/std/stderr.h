/* World Without Answers — stderr.h
   colored LOG_ macros on top of printf/putchar/exit.
   Standalone header: no stdtype.h dependency, works with plain C types.
   Has its own tiny timestamp source (build date by default;
   enable STERR_USE_WWA_TIME for the from-scratch wwa clock). */

#ifndef STDERR_H
#define STDERR_H

#include <stdarg.h>

/* TODO: add time and date and other things to the log so it will be better */

extern int printf(const char* format, ...);
extern int vprintf(const char* format, va_list ap);
extern int putchar(int c);
extern void exit(int status);

#if defined(__GNUC__) || defined(__clang__)
#define STERR_UNUSED __attribute__((unused))
#else
#define STERR_UNUSED
#endif

#ifdef _DEBUG
#define DEBIF(cond) if (cond)
#define DRIF(cond)  if (!(cond))
#else
#define DEBIF(cond) if (0)
#define DRIF(cond)  if (1)
#endif

#define STERR_SET_COLOR(color) printf("%s", (color))

#define STERR_RESET    "\033[0m"
#define STERR_BOLD     "\033[1m"
#define STERR_DIM      "\033[2m"
#define STERR_RED      "\033[31m"
#define STERR_GREEN    "\033[32m"
#define STERR_YELLOW   "\033[33m"
#define STERR_BLUE     "\033[34m"
#define STERR_MAGENTA  "\033[35m"
#define STERR_CYAN     "\033[36m"
#define STERR_WHITE    "\033[37m"

#if defined(STERR_USE_WWA_TIME)
#include <stdtime.h>
#define STERR_TIME_BUF 24
static STERR_UNUSED void _stderr_now(char* buf, int cap) {
    wwa_time_str(buf, (usize)cap);
}
#elif defined(HAVE_TIME_H)
#include <time.h>
#define STERR_TIME_BUF 32
static STERR_UNUSED void _stderr_now(char* buf, int cap) {
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    strftime(buf, (size_t)cap, "%Y-%m-%d %H:%M:%S", tm);
}
#else
#define STERR_TIME_BUF 32
static STERR_UNUSED void _stderr_now(char* buf, int cap) {
    const char* build = __DATE__ " " __TIME__;
    int i;
    for (i = 0; i < cap - 1 && build[i] != 0; i++) buf[i] = build[i];
    buf[i] = 0;
}
#endif

static STERR_UNUSED void _stderr_log(const char* prefix, const char* color,
                                     const char* file, const char* func, int line,
                                     const char* format, ...) {
    va_list args;
    char timebuf[STERR_TIME_BUF];
    va_start(args, format);
    _stderr_now(timebuf, STERR_TIME_BUF);
    STERR_SET_COLOR(color);
    printf("[%s] %s:%d %s(): %s", timebuf, file, line, func, prefix);
    vprintf(format, args);
    printf("\n");
    STERR_SET_COLOR(STERR_RESET);
    va_end(args);
}

#define LOG_TRACE(...) _stderr_log("[TRACE] ", STERR_CYAN,    __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  _stderr_log("[INFO]  ", STERR_WHITE,   __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_HINT(...)  _stderr_log("[HINT]  ", STERR_GREEN,   __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_NOTE(...)  _stderr_log("[NOTE]  ", STERR_MAGENTA, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  _stderr_log("[WARN]  ", STERR_YELLOW,  __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) _stderr_log("[ERROR] ", STERR_RED,     __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) _stderr_log("[FATAL] ", STERR_RED,     __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__); exit(1)

#endif /* STDERR_H */