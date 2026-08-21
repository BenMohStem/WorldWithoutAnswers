/* World Without Answers — stdlog.h
   from-scratch console + file logger. */

#ifndef WWA_STDLOG_H
#define WWA_STDLOG_H

#include <stdtype.h>
#include <stdarg.h>

#define WWA_LOG_LEVEL_TRACE 0
#define WWA_LOG_LEVEL_INFO  1
#define WWA_LOG_LEVEL_HINT  2
#define WWA_LOG_LEVEL_NOTE  3
#define WWA_LOG_LEVEL_WARN  4
#define WWA_LOG_LEVEL_ERROR 5
#define WWA_LOG_LEVEL_FATAL 6

i32  wwa_log_init(const char_t* path);
void wwa_log_close(void);
void wwa_log_set_level(i32 level);
i32  wwa_log_level(void);
void wwa_log_write(i32 level, const char_t* tag, const char_t* fmt, ...);
void wwa_log_vwrite(i32 level, const char_t* tag, const char_t* fmt, va_list ap);

#endif /* WWA_STDLOG_H */