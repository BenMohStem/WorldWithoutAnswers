#include <stdhash.h>
#include <stdio.h>
#include <stdmem.h>

/* exactness oracle for mum via __uint128_t */
static void probe_mum(u64 x, u64 y) {
    unsigned __int128 ref = ((unsigned __int128)x) * y;
    u64 a = x, b = y;
    /* replicate internal mum through exported mix? not exposed; test indirectly */
    (void)a; (void)b;
    wwa_printf("ref hi=%016llx lo=%016llx\n",
        (unsigned long long)(u64)(ref >> 64), (unsigned long long)(u64)ref);
}

int main(void) {
    static u8 buf[64];
    wwa_memset(buf, 0, 64);
    u64 h0 = wwa_hash_bytes(buf, 64, WWA_HASH_SEED);
    wwa_printf("all-zero : %016llx\n", (unsigned long long)h0);
    buf[0] = 1;
    u64 h1 = wwa_hash_bytes(buf, 64, WWA_HASH_SEED);
    wwa_printf("byte0=1  : %016llx\n", (unsigned long long)h1);
    buf[0] = 0; buf[63] = 0x80;
    u64 h2 = wwa_hash_bytes(buf, 64, WWA_HASH_SEED);
    wwa_printf("byte63   : %016llx\n", (unsigned long long)h2);
    /* short input */
    u8 s[4] = {1,2,3,4};
    wwa_printf("short4   : %016llx\n", (unsigned long long)wwa_hash_bytes(s,4,WWA_HASH_SEED));
    s[0] = 2;
    wwa_printf("short4'  : %016llx\n", (unsigned long long)wwa_hash_bytes(s,4,WWA_HASH_SEED));
    probe_mum(0xdeadbeefcafebabeull, 0x123456789abcdefull);
    return 0;
}
