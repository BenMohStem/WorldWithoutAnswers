/* World Without Answers — stdlog.c
   logger writing "[YYYY-MM-DD HH:MM:SS.mmm LEVEL tag] message" lines
   to stderr and to a file (default res/logs/wwa.log). */

#include <stdlog.h>
#include <stdos.h>
#include <stdtime.h>
#include <stdstr.h>
#include <stdio.h>

#define WWA_LOG_DEFAULT_PATH "res/logs/wwa.log"
#define WWA_LOG_BUF_SIZE 2048

static i32 g_log_fd = -1;
static i32 g_log_level = WWA_LOG_LEVEL_TRACE;
static i32 g_log_init = 0;

static const char_t* wwa_log_level_name(i32 level) {
    switch (level) {
    case WWA_LOG_LEVEL_TRACE: return "TRACE";
    case WWA_LOG_LEVEL_INFO:  return "INFO";
    case WWA_LOG_LEVEL_HINT:  return "HINT";
    case WWA_LOG_LEVEL_NOTE:  return "NOTE";
    case WWA_LOG_LEVEL_WARN:  return "WARN";
    case WWA_LOG_LEVEL_ERROR: return "ERROR";
    case WWA_LOG_LEVEL_FATAL: return "FATAL";
    default: return "????";
    }
}

i32 wwa_log_init(const char_t* path) {
    if (path == NULL) path = WWA_LOG_DEFAULT_PATH;
    wwa_os_dir_create("res/logs");
    if (g_log_fd >= 0) wwa_os_file_close(g_log_fd);
    g_log_fd = wwa_os_file_open(path, WWA_OS_FILE_WRITE | WWA_OS_FILE_CREATE | WWA_OS_FILE_APPEND);
    g_log_init = 1;
    return g_log_fd >= 0 ? 0 : -1;
}

void wwa_log_close(void) {
    if (g_log_fd >= 0) {
        wwa_os_file_close(g_log_fd);
        g_log_fd = -1;
    }
}

void wwa_log_set_level(i32 level) {
    g_log_level = level;
}

i32 wwa_log_level(void) {
    return g_log_level;
}

void wwa_log_vwrite(i32 level, const char_t* tag, const char_t* fmt, va_list ap) {
    char_t buf[WWA_LOG_BUF_SIZE];
    usize pos = 0;
    usize i;
    if (!g_log_init) wwa_log_init(NULL);
    if (level < g_log_level) return;
    buf[pos++] = '[';
    {
        char_t t[24];
        i32 n = wwa_time_str(t, sizeof(t));
        if (n > 0) {
            for (i = 0; i < (usize)n && pos < sizeof(buf) - 1; i++) buf[pos++] = t[i];
        }
        buf[pos++] = '.';
        {
            u32 ms = (u32)(wwa_time_ms() % 1000);
            char_t m[4];
            m[0] = (char_t)('0' + ms / 100);
            m[1] = (char_t)('0' + (ms / 10) % 10);
            m[2] = (char_t)('0' + ms % 10);
            m[3] = 0;
            for (i = 0; i < 3 && pos < sizeof(buf) - 1; i++) buf[pos++] = m[i];
        }
    }
    buf[pos++] = ' ';
    {
        const char_t* name = wwa_log_level_name(level);
        usize nl = wwa_strlen(name);
        for (i = 0; i < nl && pos < sizeof(buf) - 1; i++) buf[pos++] = name[i];
    }
    if (tag != NULL && tag[0] != 0) {
        buf[pos++] = ' ';
        {
            usize tl = wwa_strlen(tag);
            for (i = 0; i < tl && pos < sizeof(buf) - 1; i++) buf[pos++] = tag[i];
        }
    }
    buf[pos++] = ']';
    buf[pos++] = ' ';
    {
        char_t* p = buf + pos;
        usize cap = sizeof(buf) - pos - 2;
        i32 n = wwa_vsnprintf(p, cap, fmt, ap);
        if (n < 0) n = 0;
        pos += (usize)n < cap ? (usize)n : cap;
    }
    if (pos + 1 < sizeof(buf)) {
        buf[pos++] = '\n';
        buf[pos] = 0;
    }
    wwa_os_write(WWA_OS_FD_STDERR, buf, pos);
    if (g_log_fd >= 0) wwa_os_write(g_log_fd, buf, pos);
    if (level == WWA_LOG_LEVEL_FATAL) wwa_log_close();
}

void wwa_log_write(i32 level, const char_t* tag, const char_t* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    wwa_log_vwrite(level, tag, fmt, ap);
    va_end(ap);
}