/* World Without Answers — stdlib.c
   from-scratch heap allocator:
   4096 size-class buckets (16..65536 bytes, 16-byte steps),
   blocks carved from 64 KB superblocks, 16-byte payload alignment,
   O(1) alloc/free, free memory returns to buckets (never to the OS).
   Larger allocations are direct OS regions. */

#include <stdlib.h>
#include <stdmem.h>
#include <stdstr.h>

#define WWA_ALLOC_SUPER      65536
#define WWA_ALLOC_BUCKETS    4096
#define WWA_ALLOC_MIN_SIZE   16
#define WWA_ALLOC_STEP       16
#define WWA_ALLOC_HEADER     16
#define WWA_ALLOC_MAGIC_BUCKET 0x574141424B54524DULL
#define WWA_ALLOC_MAGIC_LARGE  0x5741414C47524D45ULL

typedef struct wwa_free_node {
    struct wwa_free_node* next;
} wwa_free_node_t;

static wwa_free_node_t* g_buckets[WWA_ALLOC_BUCKETS];

static i32 wwa_alloc_bucket(usize size) {
    if (size < WWA_ALLOC_MIN_SIZE) size = WWA_ALLOC_MIN_SIZE;
    return (i32)((size - WWA_ALLOC_MIN_SIZE + WWA_ALLOC_STEP - 1) / WWA_ALLOC_STEP);
}

static void wwa_alloc_grow(i32 b) {
    usize bsize = WWA_ALLOC_MIN_SIZE + (usize)b * WWA_ALLOC_STEP;
    void_p mem = wwa_os_alloc(WWA_ALLOC_SUPER);
    u8* p;
    usize count, i;
    if (mem == NULL) return;
    p = (u8*)mem;
    count = WWA_ALLOC_SUPER / (bsize + WWA_ALLOC_HEADER);
    for (i = 0; i < count; i++) {
        u8* blk = p + i * (bsize + WWA_ALLOC_HEADER);
        wwa_free_node_t* node;
        *(u64*)blk = bsize;
        *(u64*)(blk + 8) = WWA_ALLOC_MAGIC_BUCKET;
        node = (wwa_free_node_t*)(blk + WWA_ALLOC_HEADER);
        node->next = g_buckets[b];
        g_buckets[b] = node;
    }
}

void_p wwa_malloc(usize size) {
    if (size == 0) size = WWA_ALLOC_MIN_SIZE;
    if (size > WWA_ALLOC_SUPER - WWA_ALLOC_HEADER) {
        u64* hdr = (u64*)wwa_os_alloc(size + WWA_ALLOC_HEADER);
        if (hdr == NULL) return NULL;
        hdr[0] = (u64)(size + WWA_ALLOC_HEADER);
        hdr[1] = WWA_ALLOC_MAGIC_LARGE;
        return hdr + 2;
    }
    {
        i32 b = wwa_alloc_bucket(size);
        wwa_free_node_t* node = g_buckets[b];
        if (node == NULL) {
            wwa_alloc_grow(b);
            node = g_buckets[b];
        }
        if (node == NULL) return NULL;
        g_buckets[b] = node->next;
        return (void_p)node;
    }
}

void wwa_free(void_p p) {
    u64* hdr;
    u64 size, magic;
    if (p == NULL) return;
    hdr = (u64*)p - 2;
    size = hdr[0];
    magic = hdr[1];
    if (magic == WWA_ALLOC_MAGIC_LARGE) {
        wwa_os_free(hdr, (usize)size);
        return;
    }
    if (magic == WWA_ALLOC_MAGIC_BUCKET) {
        i32 b = wwa_alloc_bucket((usize)size);
        wwa_free_node_t* node = (wwa_free_node_t*)p;
        node->next = g_buckets[b];
        g_buckets[b] = node;
        return;
    }
}

void_p wwa_calloc(usize count, usize size) {
    void_p p;
    if (size != 0 && count > (usize)-1 / size) return NULL;
    p = wwa_malloc(count * size);
    if (p != NULL) wwa_memset(p, 0, count * size);
    return p;
}

void_p wwa_realloc(void_p p, usize size) {
    void_p np;
    u64* hdr;
    u64 old_size;
    if (p == NULL) return wwa_malloc(size);
    if (size == 0) {
        wwa_free(p);
        return NULL;
    }
    hdr = (u64*)p - 2;
    if (hdr[1] == WWA_ALLOC_MAGIC_LARGE) {
        old_size = hdr[0] - WWA_ALLOC_HEADER;
    } else {
        old_size = hdr[0];
    }
    if (size <= old_size) return p;
    np = wwa_malloc(size);
    if (np == NULL) return NULL;
    wwa_memcpy(np, p, old_size);
    wwa_free(p);
    return np;
}

i32 wwa_atoi(const char_t* s) {
    i64 v = wwa_atol(s);
    return (i32)v;
}

i64 wwa_atol(const char_t* s) {
    i64 v = 0;
    i32 neg = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return neg ? -v : v;
}

i64 wwa_strtol(const char_t* s, char_t** endptr, i32 base) {
    i64 v = 0;
    i32 neg = 0, ovf = 0, have_digit = 0;
    const char_t* nptr_orig = s;
    if (base != 0 && (base < 2 || base > 36)) {
        if (endptr != NULL) *endptr = (char_t*)nptr_orig;
        return 0;
    }
    while (wwa_isspace((i32)(u8)*s)) s++;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    if (base == 0) {
        base = 10;
        if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (*s == '0') {
            base = 8;
        }
    } else if (base == 16 && *s == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    for (;;) {
        i32 d = -1;
        u8 c = (u8)*s;
        if (c >= '0' && c <= '9') {
            d = (i32)c - '0';
        } else if (c >= 'a' && c <= 'z') {
            d = (i32)c - 'a' + 10;
        } else if (c >= 'A' && c <= 'Z') {
            d = (i32)c - 'A' + 10;
        }
        if (d < 0 || d >= base) break;
        have_digit = 1;
        if (!ovf && v > ((i64)0x7FFFFFFFFFFFFFFFull - (i64)d) / base) ovf = 1;
        if (!ovf) v = v * base + (i64)d;
        s++;
    }
    if (endptr != NULL) *endptr = (char_t*)(have_digit ? s : nptr_orig);
    if (ovf) return neg ? (i64)(-((i64)0x7FFFFFFFFFFFFFFFull) - 1) : (i64)0x7FFFFFFFFFFFFFFFull;
    return neg ? -v : v;
}

typedef union {
    real64_t f;
    u64 u;
} wwa_dbl_pow_t;

static real64_t wwa_dbl_pow2(i32 n) {
    wwa_dbl_pow_t x;
    if (n > 1023) {
        x.u = 0x7FF0000000000000ull;
        return x.f;
    }
    if (n < -1074) return 0.0;
    if (n < -1022) {
        x.u = 1ull << (n + 1074);
        return x.f;
    }
    x.u = (u64)(n + 1023) << 52;
    return x.f;
}

static real64_t wwa_dbl_pow5(i32 n) {
    real64_t v = 1.0;
    while (n-- > 0) v *= 5.0;
    return v;
}

static real64_t wwa_dbl_pow10(i32 k) {
    real64_t v = 1.0;
    while (k > 22) {
        v *= 1.0e22;
        k -= 22;
    }
    while (k < -22) {
        v *= 1.0e-22;
        k += 22;
    }
    if (k > 0) v *= wwa_dbl_pow2(k) * wwa_dbl_pow5(k);
    else if (k < 0) v /= wwa_dbl_pow2(-k) * wwa_dbl_pow5(-k);
    return v;
}

static i32 wwa_digit_val(i32 c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

real64_t wwa_strtod(const char_t* s, char_t** endptr) {
    const char_t* nptr_orig = s;
    real64_t v = 0.0;
    i32 neg = 0, have = 0;
    i32 hex = 0;
    i32 exp10 = 0;
    i32 exp2 = 0;
    u64 mant = 0;
    i32 ndig = 0;
    wwa_dbl_pow_t inf;
    wwa_dbl_pow_t nan;
    if (s == NULL) s = "";
    while (wwa_isspace((i32)(u8)*s)) s++;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    if (wwa_strncasecmp(s, "infinity", 8) == 0) {
        s += 8;
        if (endptr != NULL) *endptr = (char_t*)s;
        inf.u = 0x7FF0000000000000ull;
        return neg ? -inf.f : inf.f;
    }
    if (wwa_strncasecmp(s, "inf", 3) == 0) {
        s += 3;
        if (endptr != NULL) *endptr = (char_t*)s;
        inf.u = 0x7FF0000000000000ull;
        return neg ? -inf.f : inf.f;
    }
    if (wwa_strncasecmp(s, "nan", 3) == 0) {
        s += 3;
        if (endptr != NULL) *endptr = (char_t*)s;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        hex = 1;
        s += 2;
    }
    if (hex) {
        while (wwa_isxdigit((i32)(u8)*s)) {
            if (mant < 0x1000000000000000ull)
                mant = mant * 16 + (u64)wwa_digit_val((i32)(u8)*s);
            have = 1;
            s++;
        }
        if (*s == '.') {
            s++;
            while (wwa_isxdigit((i32)(u8)*s)) {
                if (mant < 0x1000000000000000ull)
                    mant = mant * 16 + (u64)wwa_digit_val((i32)(u8)*s);
                exp2 -= 4;
                have = 1;
                s++;
            }
        }
    } else {
        while (*s >= '0' && *s <= '9') {
            if (mant <= 1844674407370955160ull)
                mant = mant * 10 + (u64)(*s - '0');
            have = 1;
            s++;
        }
        if (*s == '.') {
            s++;
            while (*s >= '0' && *s <= '9') {
                if (mant <= 1844674407370955160ull)
                    mant = mant * 10 + (u64)(*s - '0');
                exp10--;
                have = 1;
                s++;
            }
        }
    }
    if (!have) {
        if (endptr != NULL) *endptr = (char_t*)nptr_orig;
        return 0.0;
    }
    if ((hex && (*s == 'p' || *s == 'P')) || (!hex && (*s == 'e' || *s == 'E'))) {
        const char_t* ep = s + 1;
        i32 eneg = 0, ev = 0, edig = 0;
        if (*ep == '-') {
            eneg = 1;
            ep++;
        } else if (*ep == '+') {
            ep++;
        }
        while (*ep >= '0' && *ep <= '9') {
            ev = ev * 10 + (*ep - '0');
            if (ev > 1000000) ev = 1000000;
            edig = 1;
            ep++;
        }
        if (edig) {
            if (hex) exp2 += eneg ? -ev : ev;
            else exp10 += eneg ? -ev : ev;
            s = ep;
        }
    }
    if (endptr != NULL) *endptr = (char_t*)s;
    if (hex) {
        v = (real64_t)mant;
        if (exp2 != 0) v *= wwa_dbl_pow2(exp2);
    } else {
        v = (real64_t)mant;
        if (exp10 != 0) v *= wwa_dbl_pow10(exp10);
    }
    return neg ? -v : v;
}

real32_t wwa_strtof(const char_t* s, char_t** endptr) {
    return (real32_t)wwa_strtod(s, endptr);
}

real64_t wwa_atof(const char_t* s) {
    return wwa_strtod(s, NULL);
}

usize wwa_utoa(u64 v, char_t* buf, usize cap) {
    char_t tmp[24];
    i32 n = 0, i, full;
    do {
        tmp[n++] = (char_t)('0' + (i32)(v % 10));
        v /= 10;
    } while (v);
    full = n;
    if (cap == 0) return (usize)full;
    if ((usize)full >= cap) n = (i32)cap - 1;
    for (i = 0; i < n; i++) buf[i] = tmp[full - 1 - i];
    buf[n] = 0;
    return (usize)n;
}

usize wwa_itoa(i64 v, char_t* buf, usize cap) {
    u64 uv;
    char_t tmp[24];
    i32 n = 0, i;
    if (v < 0) {
        uv = (u64)(-(v + 1)) + 1;
        tmp[n++] = '-';
    } else {
        uv = (u64)v;
    }
    {
        char_t d[24];
        i32 m = 0;
        do {
            d[m++] = (char_t)('0' + (i32)(uv % 10));
            uv /= 10;
        } while (uv);
        while (m--) tmp[n++] = d[m];
    }
    if (cap == 0) return (usize)n;
    if ((usize)n >= cap) n = (i32)cap - 1;
    for (i = 0; i < n; i++) buf[i] = tmp[i];
    buf[n] = 0;
    return (usize)n;
}

usize wwa_utoa_base(u64 v, char_t* buf, usize cap, i32 base) {
    char_t tmp[66];
    i32 n = 0, i, full;
    if (base < 2 || base > 36) base = 10;
    do {
        u32 d = (u32)(v % (u64)base);
        tmp[n++] = (char_t)(d < 10 ? '0' + d : 'a' + (d - 10));
        v /= (u64)base;
    } while (v);
    full = n;
    if (cap == 0) return (usize)full;
    if ((usize)full >= cap) n = (i32)cap - 1;
    for (i = 0; i < n; i++) buf[i] = tmp[full - 1 - i];
    buf[n] = 0;
    return (usize)n;
}

usize wwa_itoa_hex(u64 v, char_t* buf, usize cap) {
    return wwa_utoa_base(v, buf, cap, 16);
}

char_t* wwa_strdup(const char_t* s) {
    usize n = wwa_strlen(s);
    char_t* p = (char_t*)wwa_malloc(n + 1);
    if (p != NULL) wwa_memcpy(p, s, n + 1);
    return p;
}

char_t* wwa_strndup(const char_t* s, usize n) {
    usize m = 0;
    char_t* p;
    while (m < n && s[m] != 0) m++;
    p = (char_t*)wwa_malloc(m + 1);
    if (p != NULL) {
        wwa_memcpy(p, s, m);
        p[m] = 0;
    }
    return p;
}

static u64 g_wwa_rand_state = 0x9E3779B97F4A7C15ull;

void wwa_srand(u32 seed) {
    g_wwa_rand_state = (u64)seed | ((u64)seed << 32) | 0x9E3779B97F4A7C15ull;
}

i32 wwa_rand(void) {
    u64 z = (g_wwa_rand_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (i32)((z ^ (z >> 31)) >> 33);
}

i32 wwa_abs(i32 v) {
    return v < 0 ? -v : v;
}

i64 wwa_labs(i64 v) {
    return v < 0 ? -v : v;
}

i64 wwa_llabs(i64 v) {
    return v < 0 ? -v : v;
}

wwa_div_t wwa_div(i32 n, i32 d) {
    wwa_div_t r;
    r.quot = n / d;
    r.rem = n % d;
    return r;
}

wwa_ldiv_t wwa_ldiv(i64 n, i64 d) {
    wwa_ldiv_t r;
    r.quot = n / d;
    r.rem = n % d;
    return r;
}

wwa_lldiv_t wwa_lldiv(i64 n, i64 d) {
    wwa_lldiv_t r;
    r.quot = n / d;
    r.rem = n % d;
    return r;
}

static void wwa_sort_swap_bytes(u8* a, u8* b, usize size) {
    usize i;
    for (i = 0; i < size; i++) {
        u8 t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

static void wwa_sort_swap(u8* a, u8* b, usize size) {
    if (size <= 64) {
        static u8 tmp[64];
        wwa_memcpy(tmp, a, size);
        wwa_memcpy(a, b, size);
        wwa_memcpy(b, tmp, size);
        return;
    }
    wwa_sort_swap_bytes(a, b, size);
}

static void wwa_qsort_insert(u8* p, usize n, usize size,
                             i32 (*compare)(const void_p, const void_p)) {
    usize j;
    if (size <= 64) {
        static u8 tmp[64];
        for (j = 1; j < n; j++) {
            usize k = j;
            wwa_memcpy(tmp, p + j * size, size);
            while (k > 0 && compare(tmp, p + (k - 1) * size) < 0) {
                wwa_memcpy(p + k * size, p + (k - 1) * size, size);
                k--;
            }
            wwa_memcpy(p + k * size, tmp, size);
        }
        return;
    }
    for (j = 1; j < n; j++) {
        usize k = j;
        usize a, b;
        while (k > 0 && compare(p + j * size, p + (k - 1) * size) < 0) k--;
        if (k >= j) continue;
        a = k * size;
        b = j * size + size;
        while (a < b) {
            u8 t = p[a];
            p[a] = p[b - 1];
            p[b - 1] = t;
            a++;
            b--;
        }
        a = (k + 1) * size;
        b = j * size + size;
        while (a < b) {
            u8 t = p[a];
            p[a] = p[b - 1];
            p[b - 1] = t;
            a++;
            b--;
        }
    }
}

static void wwa_qsort_rec(u8* p, usize n, usize size,
                          i32 (*compare)(const void_p, const void_p)) {
    u8* pivot;
    usize i, j;
    if (n <= 1) return;
    if (n <= 16) {
        wwa_qsort_insert(p, n, size, compare);
        return;
    }
    pivot = p + (n - 1) * size;
    if (compare(p, pivot) > 0) wwa_sort_swap(p, pivot, size);
    if (compare(p + (n / 2) * size, p) < 0) wwa_sort_swap(p + (n / 2) * size, p, size);
    if (compare(pivot, p + (n / 2) * size) < 0) wwa_sort_swap(pivot, p + (n / 2) * size, size);
    wwa_sort_swap(p + (n / 2) * size, pivot, size);
    i = 0;
    for (j = 0; j + 1 < n; j++) {
        if (compare(p + j * size, pivot) < 0) {
            wwa_sort_swap(p + i * size, p + j * size, size);
            i++;
        }
    }
    wwa_sort_swap(p + i * size, pivot, size);
    wwa_qsort_rec(p, i, size, compare);
    wwa_qsort_rec(p + (i + 1) * size, n - i - 1, size, compare);
}

void wwa_qsort(void_p base, usize count, usize size,
               i32 (*compare)(const void_p, const void_p)) {
    if (base == NULL || count < 2 || size == 0) return;
    wwa_qsort_rec((u8*)base, count, size, compare);
}

void_p wwa_bsearch(const void_p key, const void_p base, usize count,
                   usize size, i32 (*compare)(const void_p, const void_p)) {
    usize lo = 0, hi = count;
    while (lo < hi) {
        usize mid = lo + (hi - lo) / 2;
        i32 c = compare(key, (void_p)((const u8*)base + mid * size));
        if (c == 0) return (void_p)((const u8*)base + mid * size);
        if (c < 0) hi = mid;
        else lo = mid + 1;
    }
    return NULL;
}

WWA_NORETURN void wwa_abort(void) {
    wwa_os_exit(EXIT_FAILURE);
}

WWA_NORETURN void exit(i32 code) {
    wwa_os_exit(code);
}