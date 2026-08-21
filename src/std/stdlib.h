/* World Without Answers — stdlib.h
   from-scratch heap allocator + numeric conversions + process exit.
   No libc. */

#ifndef WWA_STDLIB_H
#define WWA_STDLIB_H

#include <stdtype.h>
#include <stdos.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647

void_p wwa_malloc(usize size);
void_p wwa_calloc(usize count, usize size);
void_p wwa_realloc(void_p p, usize size);
void   wwa_free(void_p p);

void_p malloc(usize size);
void   free(void_p p);

i32    wwa_atoi(const char_t* s);
i64    wwa_atol(const char_t* s);
i64    wwa_strtol(const char_t* s, char_t** endptr, i32 base);
real64_t wwa_strtod(const char_t* s, char_t** endptr);
real32_t wwa_strtof(const char_t* s, char_t** endptr);
real64_t wwa_atof(const char_t* s);
usize  wwa_utoa(u64 v, char_t* buf, usize cap);
usize  wwa_utoa_base(u64 v, char_t* buf, usize cap, i32 base);
usize  wwa_itoa(i64 v, char_t* buf, usize cap);
usize  wwa_itoa_hex(u64 v, char_t* buf, usize cap);

char_t* wwa_strdup(const char_t* s);
char_t* wwa_strndup(const char_t* s, usize n);

void wwa_srand(u32 seed);
i32  wwa_rand(void);

i32  wwa_abs(i32 v);
i64  wwa_labs(i64 v);
i64  wwa_llabs(i64 v);

typedef struct { i32 quot, rem; } wwa_div_t;
typedef struct { i64 quot, rem; } wwa_ldiv_t;
typedef struct { i64 quot, rem; } wwa_lldiv_t;
wwa_div_t   wwa_div(i32 n, i32 d);
wwa_ldiv_t  wwa_ldiv(i64 n, i64 d);
wwa_lldiv_t wwa_lldiv(i64 n, i64 d);

void wwa_qsort(void_p base, usize count, usize size,
               i32 (*compare)(const void_p a, const void_p b));
void_p wwa_bsearch(const void_p key, const void_p base, usize count,
                   usize size, i32 (*compare)(const void_p a, const void_p b));

void wwa_abort(void) WWA_NORETURN;
void exit(i32 code) WWA_NORETURN;

#endif /* WWA_STDLIB_H */