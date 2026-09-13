#include <stdhash.h>
#include <stdthread.h>
#include <stdos.h>
#include <stdio.h>
#include <stdmem.h>

#define NTHREADS 8
#define ITERS 100000

static wwa_mutex_t g_mtx;
static i32 g_counter;
static u64 g_hashes[NTHREADS];

static void worker(void* arg) {
    i32 id = (i32)(usize)arg;
    u64 h = 0;
    for (i32 i = 0; i < ITERS; i++) {
        wwa_mutex_lock(&g_mtx);
        g_counter++;
        wwa_mutex_unlock(&g_mtx);
        h ^= wwa_hash_u64((u64)(id * ITERS + i));
    }
    g_hashes[id] = h;
}

int main(void) {
    /* hash sanity: known FNV-1a vector */
    u64 fnv = wwa_hash_str("a");
    wwa_printf("fnv64('a')      = %016llx (expect af63dc4c8601ec8c) %s\n",
           (unsigned long long)fnv, fnv==0xaf63dc4c8601ec8cull?"OK":"FAIL");
    /* avalanche: flip each bit of input, count flipped output bits */
    u8 buf[64]; wwa_memset(buf, 0, 64);
    u64 base = wwa_hash_bytes(buf, 64, WWA_HASH_SEED);
    u32 flips = 0, samples = 0;
    for (u32 b = 0; b < 512; b++) {
        buf[b/8] ^= (u8)(1u << (b%8));
        u64 h2 = wwa_hash_bytes(buf, 64, WWA_HASH_SEED);
        u64 d = base ^ h2;
        for (i32 k = 0; k < 64; k++) flips += (u32)((d>>k)&1);
        samples++;
        buf[b/8] ^= (u8)(1u << (b%8));
    }
    f32 pct = 100.0f*(f32)flips/(f32)((u64)samples*64);
    wwa_printf("avalanche       = %.1f%% (expect ~50%%) %s\n", pct, (pct>40&&pct<60)?"OK":"FAIL");
    /* streaming == one-shot */
    wwa_hash_state_t st; wwa_hash_init(&st, 7);
    for (i32 i = 0; i < 1000; i++) wwa_hash_update(&st, "0123456789abcdef", 16);
    u64 hs = wwa_hash_final(&st);
    u8 big[16000]; wwa_memset(big, 'x', 16); wwa_memset(big+16, 'y', 15984);
    for (i32 i = 0; i < 1000; i++) wwa_memmove(big + (usize)i*0, "0123456789abcdef", 16);
    /* build same byte stream: 1000 x "0123456789abcdef" then compare via chunks */
    static u8 stream[16000];
    for (i32 i = 0; i < 1000; i++) wwa_memmove(stream + (usize)i*16, "0123456789abcdef", 16);
    u64 ho = wwa_hash_bytes(stream, 16000, 7);
    wwa_printf("stream==oneshot = %s\n", hs==ho?"OK":"FAIL");
    /* cpu count */
    wwa_printf("cpu_count       = %u\n", wwa_os_cpu_count());
    /* threads: counter must hit NTHREADS*ITERS exactly */
    wwa_mutex_init(&g_mtx);
    wwa_thread_t th[NTHREADS];
    for (i32 i = 0; i < NTHREADS; i++) wwa_thread_spawn(&th[i], worker, (void*)(usize)i);
    for (i32 i = 0; i < NTHREADS; i++) wwa_thread_join(th[i]);
    wwa_printf("threaded counter= %d (expect %d) %s\n", g_counter, NTHREADS*ITERS,
           g_counter==NTHREADS*ITERS?"OK":"FAIL");
    wwa_mutex_destroy(&g_mtx);
    return 0;
}
