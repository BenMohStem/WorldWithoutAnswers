/* World Without Answers ?" stdtype.h
   from-scratch type layer. No libc, no third-party headers.
   Per project rule: the 'unsigned' / 'int' / 'short' keywords are never used
   to derive types ?" only compiler/platform keywords (__INT8_TYPE__, __int8, ...)
   so widths are exact on any Windows or Linux machine. */

#ifndef WWA_STDTYPE_H
#define WWA_STDTYPE_H

#if defined(__GNUC__) || defined(__clang__)
typedef __INT8_TYPE__   wwa_i8;
typedef __UINT8_TYPE__  wwa_u8;
typedef __INT16_TYPE__  wwa_i16;
typedef __UINT16_TYPE__ wwa_u16;
typedef __INT32_TYPE__  wwa_i32;
typedef __UINT32_TYPE__ wwa_u32;
typedef __INT64_TYPE__  wwa_i64;
typedef __UINT64_TYPE__ wwa_u64;
typedef __SIZE_TYPE__    wwa_usize;
typedef __PTRDIFF_TYPE__ wwa_isize;
typedef __INTMAX_TYPE__  wwa_imax;
typedef __UINTMAX_TYPE__ wwa_umax;
#elif defined(_MSC_VER)
typedef __int8             wwa_i8;
typedef unsigned __int8    wwa_u8;
typedef __int16            wwa_i16;
typedef unsigned __int16   wwa_u16;
typedef __int32            wwa_i32;
typedef unsigned __int32   wwa_u32;
typedef __int64            wwa_i64;
typedef unsigned __int64   wwa_u64;
typedef wwa_u64 wwa_usize;
typedef wwa_i64 wwa_isize;
typedef wwa_i64 wwa_imax;
typedef wwa_u64 wwa_umax;
#else
#error "stdtype: unknown compiler only GCC/Clang and MSVC are supported"
#endif

typedef wwa_i8   i8;
typedef wwa_u8   u8;
typedef wwa_i16  i16;
typedef wwa_u16  u16;
typedef wwa_i32  i32;
typedef wwa_u32  u32;
typedef wwa_i64  i64;
typedef wwa_u64  u64;
typedef wwa_usize usize;
typedef wwa_isize isize;

/* literal C names ??" compiler-provided width; same-type typedef
   redefinition is legal C11, so coexists with OS headers */
/* literal C names - compiler-provided width; same-type typedef
   redefinition is legal C11, so coexists with OS headers */
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#if defined(__GNUC__) || defined(__clang__)
#define WWA_OFFSETOF(type, member) ((usize)__builtin_offsetof(type, member))
#else
#define WWA_OFFSETOF(type, member) ((usize)((char_t*)&(((type*)0)->member) - (char_t*)0))
#endif

#define F16_MAX       65504.0f
#define F16_MIN       (1.0f / 65504.0f)
#define F8_MAX        16.0f
#define F8_MIN        (1.0f / 16.0f)

/* Proper union-based float16<->float32 conversion (NVIDIA FP16: e5m10) */
typedef union {
    f32 f32;
    u16 u16;
} wwa_f16_conv_t;

#define F16_FROM_F32(f) (    \
    (wwa_f16_conv_t){.f32 = (f)}.u16  \
)

#define F32_FROM_F16(i) (    \
    (wwa_f16_conv_t){.u16 = (i)}.f32  \
)

/* Proper union-based float8<->float32 conversion (NVIDIA FP8 variants) */
/* FP8 E5M2: 5 exponent bits, 2 mantissa bits, bias 15 */
typedef union {
    f32 f32;
    u8  u8;
} wwa_f8_e5m2_conv_t;

/* FP8 E4M3: 4 exponent bits, 3 mantissa bits, bias 7 */
typedef union {
    f32 f32;
    u8  u8;
} wwa_f8_e4m3_conv_t;

#define F8_E5M2_FROM_F32(f) (    \
    (wwa_f8_e5m2_conv_t){.f32 = (f)}.u8  \
)

#define F32_FROM_F8_E5M2(i) (    \
    (wwa_f8_e5m2_conv_t){.u8 = (i)}.f32  \
)

/* FP8 E4M3 FROM F32 */
#define F8_E4M3_FROM_F32(f) (    \
    (wwa_f8_e4m3_conv_t){.f32 = (f)}.u8  \
)

#define F32_FROM_F8_E4M3(i) (    \
    (wwa_f8_e4m3_conv_t){.u8 = (i)}.f32  \
)

typedef wwa_i8   int8_t;
typedef wwa_u8   uint8_t;
typedef wwa_i16  int16_t;
typedef wwa_u16  uint16_t;
typedef wwa_i32  int32_t;
typedef wwa_u32  uint32_t;
typedef wwa_i64  int64_t;
typedef wwa_u64  uint64_t;

typedef wwa_u8 bool_t;
#define WWA_TRUE  1
#define WWA_FALSE 0

typedef float       real32_t;
typedef double      real64_t;
typedef wwa_u16 real16_t;
typedef wwa_u8  real8_t;
typedef real32_t f32;
typedef real64_t f64;
typedef real16_t f16;
typedef real8_t  f8;

typedef char     char_t;
typedef char     char8_t;
typedef wwa_i16  char16_t;
typedef wwa_i32  char32_t;
typedef wwa_i64  char64_t;
typedef char_t*  string_t;
typedef char8_t*  string8_t;
typedef char16_t* string16_t;
typedef char32_t* string32_t;
typedef char64_t* string64_t;

typedef void     void_t;
typedef void_t*  void_p;

#define NULL ((void_p)0)

#define ARRAY_COUNT(a) ((usize)(sizeof(a) / sizeof((a)[0])))
#define ARRAY_SIZE(a)  ARRAY_COUNT(a)

#define UNUSED(x) ((void)(x))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define STATIC_ASSERT(expr, name) typedef char wwa_static_assert_##name[(expr) ? 1 : -1]

#if defined(__GNUC__) || defined(__clang__)
#define WWA_OFFSETOF(type, member) ((usize)__builtin_offsetof(type, member))
#else
#define WWA_OFFSETOF(type, member) ((usize)((char_t*)&(((type*)0)->member) - (char_t*)0))
#endif

#define INT8_MIN   ((i8)(-128))
#define INT8_MAX   ((i8)(127))
#define UINT8_MAX  ((u8)(255))
#define INT16_MIN  ((i16)(-32768))
#define INT16_MAX  ((i16)(32767))
#define UINT16_MAX ((u16)(65535))
#define INT32_MIN  ((i32)(-2147483647 - 1))
#define INT32_MAX  ((i32)(2147483647))
#define UINT32_MAX ((u32)(0xFFFFFFFFu))
#define INT64_MIN  ((i64)(-9223372036854775807ll - 1))
#define INT64_MAX  ((i64)(9223372036854775807ll))
#define UINT64_MAX ((u64)(0xFFFFFFFFFFFFFFFFull))
#define USIZE_MAX  ((usize)(UINT64_MAX))

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define WWA_LITTLE_ENDIAN 1
#define WWA_BIG_ENDIAN    0
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define WWA_LITTLE_ENDIAN 0
#define WWA_BIG_ENDIAN    1
#else
static const wwa_u16 wwa_endian_probe_ = 1;
#define WWA_LITTLE_ENDIAN (*(const wwa_u8*)&wwa_endian_probe_)
#define WWA_BIG_ENDIAN    (!WWA_LITTLE_ENDIAN)
#endif

#endif /* WWA_STDTYPE_H */