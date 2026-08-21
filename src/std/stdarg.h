/* World Without Answers — stdarg.h
   varargs through compiler builtins only. No libc. */

#ifndef WWA_STDARG_H
#define WWA_STDARG_H

#if defined(__GNUC__) || defined(__clang__)
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)
#elif defined(_MSC_VER)
#include <vadefs.h>
#else
#error "stdarg: no varargs support for this compiler"
#endif

#endif /* WWA_STDARG_H */