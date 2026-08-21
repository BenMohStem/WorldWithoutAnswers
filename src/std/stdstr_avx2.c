/* World Without Answers — stdstr_avx2.c
   AVX2 string kernel. Compiled with -mavx2; dispatched at runtime. */

#include <stdstr.h>
#include <stdlib.h>
#include <immintrin.h>

usize wwa_strlen_avx2(const char_t* s) {
    const u8* p = (const u8*)s;
    for (;;) {
        u64 rem = 0x1000 - ((u64)(usize)p & 0xFFF);
        if (rem < 32) {
            while (rem > 0) {
                if (*p == 0) return (usize)(p - (const u8*)s);
                p++;
                rem--;
            }
            continue;
        }
        __m256i v = _mm256_loadu_si256((const __m256i*)p);
        __m256i z = _mm256_cmpeq_epi8(v, _mm256_setzero_si256());
        u32 mask = (u32)_mm256_movemask_epi8(z);
        if (mask) {
            return (usize)(p - (const u8*)s) + (usize)__builtin_ctz(mask);
        }
        p += 32;
    }
}