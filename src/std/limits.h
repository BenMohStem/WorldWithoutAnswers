/* World Without Answers — limits.h
   Integer limits. Portable across 32/64-bit long. */

#ifndef WWA_LIMITS_H
#define WWA_LIMITS_H

#include "stdtype.h"

#define CHAR_BIT    8
#define SCHAR_MIN   ((signed char)(-128))
#define SCHAR_MAX   ((signed char)(127))
#define UCHAR_MAX   ((unsigned char)(255))
#define CHAR_MIN    SCHAR_MIN
#define CHAR_MAX    SCHAR_MAX

#define SHRT_MIN    ((short)(-32768))
#define SHRT_MAX    ((short)(32767))
#define USHRT_MAX   ((unsigned short)(65535))

#define INT_MIN     ((int)(-2147483647 - 1))
#define INT_MAX     ((int)(2147483647))
#define UINT_MAX    ((unsigned int)(0xFFFFFFFFu))

#if defined(__GNUC__) || defined(__clang__)
#if __SIZEOF_LONG__ == 4
#define LONG_MIN    ((long)(-2147483647L - 1))
#define LONG_MAX    ((long)(2147483647L))
#define ULONG_MAX   ((unsigned long)(0xFFFFFFFFUL))
#else
#define LONG_MIN    ((long)(-9223372036854775807L - 1))
#define LONG_MAX    ((long)(9223372036854775807L))
#define ULONG_MAX   ((unsigned long)(0xFFFFFFFFFFFFFFFFUL))
#endif
#elif defined(_MSC_VER)
#define LONG_MIN    ((long)(-2147483647L - 1))
#define LONG_MAX    ((long)(2147483647L))
#define ULONG_MAX   ((unsigned long)(0xFFFFFFFFUL))
#endif

#define LLONG_MIN   INT64_MIN
#define LLONG_MAX   INT64_MAX
#define ULLONG_MAX  UINT64_MAX

#define SIZE_MAX    UINT64_MAX
#define PTRDIFF_MIN INT64_MIN
#define PTRDIFF_MAX INT64_MAX

#define WCHAR_MIN   0
#define WCHAR_MAX   65535
#define WINT_MIN    0
#define WINT_MAX    65535

#define MB_LEN_MAX  2

#endif /* WWA_LIMITS_H */
