/* World Without Answers — stdmem.c
   word-at-a-time memory functions with runtime AVX2 dispatch. */

#include <stdmem.h>
#include <stdos.h>

#if defined(__GNUC__) || defined(__clang__)
typedef u64 wwa_mem_word_t __attribute__((may_alias));
#else
typedef u64 wwa_mem_word_t;
#endif

void_p wwa_memcpy_avx2(void_p dst, const void* src, usize n);
void_p wwa_memset_avx2(void_p dst, i32 c, usize n);
i32    wwa_memcmp_avx2(const void* a, const void* b, usize n);

static i32 g_wwa_mem_avx2 = -1;

static i32 wwa_mem_use_avx2(void) {
    if (g_wwa_mem_avx2 < 0) g_wwa_mem_avx2 = wwa_os_cpu_avx2();
    return g_wwa_mem_avx2;
}

void_p wwa_memcpy(void_p dst, const void* src, usize n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    if (n >= 1024 && wwa_mem_use_avx2()) return wwa_memcpy_avx2(dst, src, n);
    while (n >= 8) {
        wwa_mem_word_t v = *(const wwa_mem_word_t*)s;
        *(wwa_mem_word_t*)d = v;
        d += 8;
        s += 8;
        n -= 8;
    }
    while (n--) *d++ = *s++;
    return dst;
}

void_p wwa_memmove(void_p dst, const void* src, usize n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    if (d == s || n == 0) return dst;
    if (d < s || d >= s + n) return wwa_memcpy(dst, src, n);
    d += n;
    s += n;
    while (n >= 8) {
        d -= 8;
        s -= 8;
        *(wwa_mem_word_t*)d = *(const wwa_mem_word_t*)s;
        n -= 8;
    }
    while (n--) {
        d--;
        s--;
        *d = *s;
    }
    return dst;
}

void_p wwa_memset(void_p dst, i32 c, usize n) {
    u8* d = (u8*)dst;
    u64 v = (u8)c;
    if (n >= 1024 && wwa_mem_use_avx2()) return wwa_memset_avx2(dst, c, n);
    v |= v << 8;
    v |= v << 16;
    v |= v << 32;
    while (n >= 8) {
        *(wwa_mem_word_t*)d = v;
        d += 8;
        n -= 8;
    }
    while (n--) *d++ = (u8)c;
    return dst;
}

i32 wwa_memcmp(const void* a, const void* b, usize n) {
    const u8* x = (const u8*)a;
    const u8* y = (const u8*)b;
    if (n >= 1024 && wwa_mem_use_avx2()) return wwa_memcmp_avx2(a, b, n);
    while (n >= 8) {
        wwa_mem_word_t vx = *(const wwa_mem_word_t*)x;
        wwa_mem_word_t vy = *(const wwa_mem_word_t*)y;
        if (vx != vy) {
            usize k;
            for (k = 0; k < 8; k++) {
                if (x[k] != y[k]) return x[k] < y[k] ? -1 : 1;
            }
        }
        x += 8;
        y += 8;
        n -= 8;
    }
    while (n--) {
        if (*x != *y) return *x < *y ? -1 : 1;
        x++;
        y++;
    }
    return 0;
}

void_p wwa_memchr(const void* s, i32 c, usize n) {
    const u8* p = (const u8*)s;
    u64 rep = (u8)c;
    rep |= rep << 8;
    rep |= rep << 16;
    rep |= rep << 32;
    while (n >= 8) {
        u64 v = *(const wwa_mem_word_t*)p;
        u64 z = v ^ rep;
        u64 mask = (z - 0x0101010101010101ull) & ~z & 0x8080808080808080ull;
        if (mask) {
            usize k;
            for (k = 0; k < 8; k++) {
                if (p[k] == (u8)c) return (void_p)(p + k);
            }
        }
        p += 8;
        n -= 8;
    }
    while (n--) {
        if (*p == (u8)c) return (void_p)p;
        p++;
    }
    return NULL;
}

void_p wwa_memccpy(void_p dst, const void* src, i32 c, usize n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    while (n--) {
        *d = *s;
        if (*s == (u8)c) return d + 1;
        d++;
        s++;
    }
    return NULL;
}

void wwa_memset_explicit(void_p dst, i32 c, usize n) {
    volatile u8* p = (volatile u8*)dst;
    while (n--) *p++ = (u8)c;
}