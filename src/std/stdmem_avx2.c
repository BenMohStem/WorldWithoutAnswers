/* World Without Answers — stdmem_avx2.c
   AVX2 memory kernels. Compiled with -mavx2; dispatched at runtime. */

#include <stdmem.h>
#include <stdlib.h>
#include <immintrin.h>

void_p wwa_memcpy_avx2(void_p dst, const void* src, usize n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    usize rem = n;
    usize h = (32 - ((usize)d & 31)) & 31;
    usize i;
    if (h > rem) h = rem;
    for (i = 0; i < h; i++) d[i] = s[i];
    d += h;
    s += h;
    rem -= h;
    while (rem >= 32) {
        __m256i v = _mm256_loadu_si256((const __m256i*)s);
        _mm256_store_si256((__m256i*)d, v);
        d += 32;
        s += 32;
        rem -= 32;
    }
    while (rem--) *d++ = *s++;
    return dst;
}

void_p wwa_memset_avx2(void_p dst, i32 c, usize n) {
    u8* d = (u8*)dst;
    usize rem = n;
    usize h = (32 - ((usize)d & 31)) & 31;
    usize i;
    __m256i v;
    if (h > rem) h = rem;
    for (i = 0; i < h; i++) d[i] = (u8)c;
    d += h;
    rem -= h;
    v = _mm256_set1_epi8((char)c);
    while (rem >= 32) {
        _mm256_store_si256((__m256i*)d, v);
        d += 32;
        rem -= 32;
    }
    while (rem--) *d++ = (u8)c;
    return dst;
}

i32 wwa_memcmp_avx2(const void* a, const void* b, usize n) {
    const u8* x = (const u8*)a;
    const u8* y = (const u8*)b;
    usize rem = n;
    usize h = (32 - ((usize)x & 31)) & 31;
    usize i;
    if (h > rem) h = rem;
    for (i = 0; i < h; i++) {
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    }
    x += h;
    y += h;
    rem -= h;
    while (rem >= 32) {
        __m256i vx = _mm256_loadu_si256((const __m256i*)x);
        __m256i vy = _mm256_loadu_si256((const __m256i*)y);
        __m256i eq = _mm256_cmpeq_epi8(vx, vy);
        i32 mask = _mm256_movemask_epi8(eq);
        if (mask != -1) {
            u32 m = (u32)(~mask);
            u32 k = (u32)__builtin_ctz(m);
            return x[k] < y[k] ? -1 : 1;
        }
        x += 32;
        y += 32;
        rem -= 32;
    }
    while (rem--) {
        if (*x != *y) return *x < *y ? -1 : 1;
        x++;
        y++;
    }
    return 0;
}