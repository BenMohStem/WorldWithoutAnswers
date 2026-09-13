/* World Without Answers — stdhash.c
   From-scratch 64-bit hash (wyhash-family design, self-authored).
   Core round: exact 128-bit multiply, output = hi ^ lo.
   Every operand XORed with nonzero constants / running state, so no
   degenerate fixed points. One-shot IS streaming -> identical results.
   Layout: 32-byte lane blocks (two 16-byte halves), strict in-bounds
   reads on every path. */
#include <stdhash.h>
#include <stdmem.h>

#define WWA_H_S0 0xA0761D6478BD642Full
#define WWA_H_S1 0xE7037ED1A0B428DBull
#define WWA_H_S2 0x8EBC6AF09C88C6E3ull
#define WWA_H_S3 0x589965CC75374CC3ull

static inline u64 wwa_ry(u64 x) { return (x << 33) | (x >> 31); }

/* exact 128-bit product via 32-bit halves with carry chain */
static inline void wwa_mum(u64* a, u64* b) {
    u64 r = *a, s = *b;
    u64 rl = (u32)r, rh = r >> 32, sl = (u32)s, sh = s >> 32;
    u64 ll = rl * sl, lh = rl * sh, hl = rh * sl, hh = rh * sh;
    u64 mid = lh + hl;
    u64 c1  = (mid < lh) ? 1 : 0;
    u64 lo  = ll + (mid << 32);
    u64 c2  = (lo < ll) ? 1 : 0;
    u64 hi  = hh + (mid >> 32) + (c1 << 32) + c2;
    *a = hi;
    *b = lo;
}

static inline u64 wwa_mix(u64 a, u64 b) {
    wwa_mum(&a, &b);
    return a ^ b;
}

u64 wwa_hash_mix(u64 a, u64 b) { return wwa_mix(a, b); }

static inline u64 wwa_read64(const u8* p) {
    u64 v; wwa_memmove(&v, p, 8); return v;
}
static inline u64 wwa_readn(const u8* p, usize n) {
    u64 v = 0;
    for (usize i = 0; i < n; i++) v |= (u64)p[i] << (8*i);
    return v;
}

/* consume 32 bytes: two chained mixes, both operands keyed */
static inline void wwa_lane(u64* sa, u64* sb, const u8* p) {
    u64 t = wwa_mix(wwa_read64(p)      ^ WWA_H_S1, wwa_read64(p + 8)  ^ *sb);
    *sb  = wwa_mix(wwa_read64(p + 16)  ^ WWA_H_S2, wwa_read64(p + 24) ^ t);
    *sa ^= wwa_ry(t);
}

void wwa_hash_init(wwa_hash_state_t* h, u64 seed) {
    h->a = seed ^ WWA_H_S0;
    h->b = seed ^ WWA_H_S3;
    h->seed = seed;
    h->tail_len = 0;
    h->total_len = 0;
}

void wwa_hash_update(wwa_hash_state_t* h, const void* data, usize len) {
    const u8* p = (const u8*)data;
    h->total_len += len;
    if (h->tail_len) {
        usize take = 32 - h->tail_len;
        if (take > len) take = len;
        wwa_memmove(h->tail + h->tail_len, p, take);
        h->tail_len += take; p += take; len -= take;
        if (h->tail_len < 32) return;
        wwa_lane(&h->a, &h->b, h->tail);
        h->tail_len = 0;
    }
    while (len >= 32) {
        wwa_lane(&h->a, &h->b, p);
        p += 32; len -= 32;
    }
    if (len) {
        wwa_memmove(h->tail, p, len);
        h->tail_len = len;
    }
}

u64 wwa_hash_final(wwa_hash_state_t* h) {
    usize len = h->tail_len;
    const u8* p = h->tail;
    u64 x, y;
    if (len == 0) {
        /* nothing buffered: fold running state directly */
        return wwa_mix(wwa_mix(h->a, h->b) ^ h->seed,
                       (u64)h->total_len ^ WWA_H_S3);
    }
    if (len <= 16) {
        if (len <= 8) {
            x = wwa_readn(p, len >> 1);
            y = wwa_readn(p + (len >> 1), len - (len >> 1));
        } else {
            x = wwa_read64(p);
            y = wwa_read64(p + len - 8);
        }
        return wwa_mix(h->seed ^ (u64)h->total_len,
                       wwa_mix(x ^ WWA_H_S1, y ^ h->a));
    }
    /* 16 < len < 32: three strictly in-bounds reads */
    x = wwa_mix(wwa_read64(p) ^ WWA_H_S1, wwa_read64(p + ((len >> 4) << 2)) ^ h->a);
    y = wwa_mix(wwa_read64(p + len - 16) ^ WWA_H_S2, wwa_read64(p + len - 8) ^ h->b);
    x = wwa_mix(x, wwa_readn(p + (len >> 1), 8) ^ WWA_H_S3);
    return wwa_mix(x ^ y ^ h->seed, (u64)h->total_len);
}

u64 wwa_hash_bytes(const void* data, usize len, u64 seed) {
    wwa_hash_state_t h;
    wwa_hash_init(&h, seed);
    wwa_hash_update(&h, data, len);
    return wwa_hash_final(&h);
}

u64 wwa_hash_str(const char* s) {
    u64 h = 0xCBF29CE484222325ull;
    while (*s) {
        h ^= (u64)(u8)*s++;
        h *= 0x100000001B3ull;
    }
    return h;
}

u64 wwa_hash_u64(u64 x) {
    x += WWA_HASH_SEED;
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
}
