/* World Without Answers — stdhash.h
   From-scratch 64-bit hashing. No libc, no 3rd-party.

   wyhash64 (Wang Yi, public domain) — final mixing:
     three 64x64->64 multiply-xor rounds; avalanche within 3 muls.
     Passes SMHasher worst-case keys; ~10 GB/s on modern x86.
   FNV-1a 64 — stable, simple, for short strings/ids.

   Collision math: for k distinct keys into 2^64 buckets,
   P(any collision) <= k*(k-1)/2^65 (uniform birthday bound).
   For k = 10^9 files: P < 2.7e-2 ... acceptable for build caching;
   manifest stores size+mtime as tiebreakers, making effective
   collision probability negligible (independent witnesses). */

#ifndef WWA_STDHASH_H
#define WWA_STDHASH_H

#include <stdtype.h>

#define WWA_HASH_SEED 0x9E3779B97F4A7C15ull

/* one-shot wyhash64 of arbitrary bytes */
u64 wwa_hash_bytes(const void* data, usize len, u64 seed);

/* streaming variant: init -> update* -> final (identical to one-shot) */
typedef struct {
    u64 a, b, seed;
    u8  tail[32];
    usize tail_len;
    u64 total_len;
} wwa_hash_state_t;

void wwa_hash_init(wwa_hash_state_t* h, u64 seed);
void wwa_hash_update(wwa_hash_state_t* h, const void* data, usize len);
u64  wwa_hash_final(wwa_hash_state_t* h);

/* FNV-1a 64-bit — for short stable strings */
u64 wwa_hash_str(const char* s);

/* integer mix (splitmix64 finalizer) — for ids/pointers */
u64 wwa_hash_u64(u64 x);

/* public 128-multiply mixing round (hi^lo) — for combining hashes */
u64 wwa_hash_mix(u64 a, u64 b);

#endif /* WWA_STDHASH_H */
