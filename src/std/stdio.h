/* World Without Answers — stdio.h
   from-scratch console io: printf family, display, putchar, getchar.
   Line-buffered stdout. Also exports plain C names (printf, vprintf,
   putchar, puts, display, getchar) so user code and stderr.h work. */

#ifndef WWA_STDIO_H
#define WWA_STDIO_H

#include <stdtype.h>
#include <stdarg.h>

#define WWA_STDIN  0
#define WWA_STDOUT 1
#define WWA_STDERR 2

i32   wwa_printf(const char_t* fmt, ...);
i32   wwa_vprintf(const char_t* fmt, va_list ap);
i32   wwa_snprintf(char_t* buf, usize cap, const char_t* fmt, ...);
i32   wwa_vsnprintf(char_t* buf, usize cap, const char_t* fmt, va_list ap);
i32   wwa_putchar(i32 c);
i32   wwa_puts(const char_t* s);
i32   wwa_display(const char_t* s);
i32   wwa_getchar(void);
void  wwa_stdio_flush(void);
void  wwa_stdio_init(void);

#endif /* WWA_STDIO_H */