/* World Without Answers — stdint.h
   Exact-width and limit macros. Thin wrapper over stdtype.h. */

#ifndef WWA_STDINT_H
#define WWA_STDINT_H

#include "stdtype.h"

typedef wwa_i8    int8_t;
typedef wwa_u8    uint8_t;
typedef wwa_i16   int16_t;
typedef wwa_u16   uint16_t;
typedef wwa_i32   int32_t;
typedef wwa_u32   uint32_t;
typedef wwa_i64   int64_t;
typedef wwa_u64   uint64_t;
typedef wwa_imax  intmax_t;
typedef wwa_umax  uintmax_t;
typedef wwa_usize size_t;
typedef wwa_isize intptr_t;
typedef wwa_usize uintptr_t;

#define INT8_MIN   ((int8_t)(-128))
#define INT8_MAX   ((int8_t)(127))
#define UINT8_MAX  ((uint8_t)(255))
#define INT16_MIN  ((int16_t)(-32768))
#define INT16_MAX  ((int16_t)(32767))
#define UINT16_MAX ((uint16_t)(65535))
#define INT32_MIN  ((int32_t)(-2147483647 - 1))
#define INT32_MAX  ((int32_t)(2147483647))
#define UINT32_MAX ((uint32_t)(0xFFFFFFFFu))
#define INT64_MIN  ((int64_t)(-9223372036854775807ll - 1))
#define INT64_MAX  ((int64_t)(9223372036854775807ll))
#define UINT64_MAX ((uint64_t)(0xFFFFFFFFFFFFFFFFull))

#define INTMAX_MIN  INT64_MIN
#define INTMAX_MAX  INT64_MAX
#define UINTMAX_MAX UINT64_MAX

#define SIZE_MAX    UINT64_MAX
#define PTRDIFF_MIN INT64_MIN
#define PTRDIFF_MAX INT64_MAX
#define INTPTR_MIN  INT64_MIN
#define INTPTR_MAX  INT64_MAX
#define UINTPTR_MAX UINT64_MAX

#define INT8_C(x)   ((int8_t)(x))
#define INT16_C(x)  ((int16_t)(x))
#define INT32_C(x)  ((int32_t)(x))
#define INT64_C(x)  ((int64_t)(x))
#define UINT8_C(x)  ((uint8_t)(x))
#define UINT16_C(x) ((uint16_t)(x))
#define UINT32_C(x) ((uint32_t)(x))
#define UINT64_C(x) ((uint64_t)(x))
#define INTMAX_C    INT64_C
#define UINTMAX_C   UINT64_C

#endif /* WWA_STDINT_H */
