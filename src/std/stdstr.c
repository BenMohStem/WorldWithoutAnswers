/* World Without Answers — stdstr.c
   word-at-a-time string functions with runtime AVX2 dispatch. */

#include <stdstr.h>
#include <stdmem.h>
#include <stdos.h>
#include <stdlib.h>

#if defined(__GNUC__) || defined(__clang__)
typedef u64 wwa_str_word_t __attribute__((may_alias));
#else
typedef u64 wwa_str_word_t;
#endif

static u64 wwa_str_haszero(u64 x) {
    return (x - 0x0101010101010101ull) & ~x & 0x8080808080808080ull;
}

usize wwa_strlen_avx2(const char_t* s);

static i32 g_wwa_str_avx2 = -1;

static i32 wwa_str_use_avx2(void) {
    if (g_wwa_str_avx2 < 0) g_wwa_str_avx2 = wwa_os_cpu_avx2();
    return g_wwa_str_avx2;
}

usize wwa_strlen(const char_t* s) {
    const u8* p = (const u8*)s;
    if (wwa_str_use_avx2()) return wwa_strlen_avx2(s);
    {
        usize off = (usize)p & 7;
        while (off < 8) {
            if (*p == 0) return (usize)(p - (const u8*)s);
            p++;
            off++;
        }
        for (;;) {
            u64 v = *(const wwa_str_word_t*)p;
            u64 m = wwa_str_haszero(v);
            if (m) {
                u32 i = (u32)__builtin_ctzll(m) >> 3;
                return (usize)(p - (const u8*)s) + i;
            }
            p += 8;
        }
    }
}

usize wwa_strnlen(const char_t* s, usize maxlen) {
    usize n = 0;
    while (n < maxlen && s[n] != 0) n++;
    return n;
}

usize wwa_strlcpy(char_t* dst, const char_t* src, usize size) {
    usize slen = wwa_strlen(src);
    if (size > 0) {
        usize n = slen < size - 1 ? slen : size - 1;
        wwa_memcpy(dst, src, n);
        dst[n] = 0;
    }
    return slen;
}

usize wwa_strlcat(char_t* dst, const char_t* src, usize size) {
    usize dlen = wwa_strnlen(dst, size);
    usize slen = wwa_strlen(src);
    if (dlen < size) {
        usize n = slen < size - dlen - 1 ? slen : size - dlen - 1;
        wwa_memcpy(dst + dlen, src, n);
        dst[dlen + n] = 0;
    }
    return dlen + slen;
}

char_t* wwa_strcpy(char_t* dst, const char_t* src) {
    char_t* d = dst;
    for (;;) {
        *d = *src;
        if (*src == 0) break;
        d++;
        src++;
    }
    return dst;
}

char_t* wwa_strncpy(char_t* dst, const char_t* src, usize n) {
    usize i = 0;
    while (i < n && src[i] != 0) {
        dst[i] = src[i];
        i++;
    }
    while (i < n) dst[i++] = 0;
    return dst;
}

char_t* wwa_strcat(char_t* dst, const char_t* src) {
    char_t* d = dst + wwa_strlen(dst);
    for (;;) {
        *d = *src;
        if (*src == 0) break;
        d++;
        src++;
    }
    return dst;
}

char_t* wwa_strncat(char_t* dst, const char_t* src, usize n) {
    char_t* d = dst + wwa_strlen(dst);
    usize i = 0;
    while (i < n && src[i] != 0) {
        d[i] = src[i];
        i++;
    }
    d[i] = 0;
    return dst;
}

i32 wwa_strcasecmp(const char_t* a, const char_t* b) {
    for (;;) {
        u8 ca = (u8)wwa_tolower((i32)(u8)*a);
        u8 cb = (u8)wwa_tolower((i32)(u8)*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
        a++;
        b++;
    }
}

i32 wwa_strncasecmp(const char_t* a, const char_t* b, usize n) {
    usize i = 0;
    while (i < n) {
        u8 ca = (u8)wwa_tolower((i32)(u8)a[i]);
        u8 cb = (u8)wwa_tolower((i32)(u8)b[i]);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
        i++;
    }
    return 0;
}

i32 wwa_strcmp(const char_t* a, const char_t* b) {
    const u8* x = (const u8*)a;
    const u8* y = (const u8*)b;
    for (;;) {
        u64 vx = *(const wwa_str_word_t*)x;
        u64 vy = *(const wwa_str_word_t*)y;
        u64 diff = vx ^ vy;
        u64 zero = wwa_str_haszero(vx) | wwa_str_haszero(vy);
        if (diff | zero) {
            u64 bit = diff | zero;
            u32 i = (u32)__builtin_ctzll(bit) >> 3;
            u8 cx = x[i];
            u8 cy = y[i];
            if (cx != cy) return cx < cy ? -1 : 1;
            return 0;
        }
        x += 8;
        y += 8;
    }
}

i32 wwa_strncmp(const char_t* a, const char_t* b, usize n) {
    usize i = 0;
    while (i < n) {
        u8 ca = (u8)a[i];
        u8 cb = (u8)b[i];
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
        i++;
    }
    return 0;
}

char_t* wwa_strchr(const char_t* s, i32 c) {
    const u8* p = (const u8*)s;
    u8 target = (u8)c;
    u64 rep = target;
    rep |= rep << 8;
    rep |= rep << 16;
    rep |= rep << 32;
    if (target == 0) return (char_t*)(s + wwa_strlen(s));
    for (;;) {
        u64 v = *(const wwa_str_word_t*)p;
        u64 z = v ^ rep;
        u64 mask = wwa_str_haszero(z);
        u64 zero = wwa_str_haszero(v);
        if (mask | zero) {
            u64 bit = mask | zero;
            u32 i = (u32)__builtin_ctzll(bit) >> 3;
            return p[i] == target ? (char_t*)(p + i) : NULL;
        }
        p += 8;
    }
}

char_t* wwa_strrchr(const char_t* s, i32 c) {
    const char_t* p = s + wwa_strlen(s);
    u8 target = (u8)c;
    if (target == 0) return (char_t*)p;
    while (p >= s) {
        if ((u8)*p == target) return (char_t*)p;
        p--;
    }
    return NULL;
}

char_t* wwa_strstr(const char_t* hay, const char_t* needle) {
    if (*needle == 0) return (char_t*)hay;
    for (;;) {
        hay = wwa_strchr(hay, *needle);
        if (hay == NULL) return NULL;
        {
            usize i = 1;
            while (needle[i] != 0 && hay[i] == needle[i]) i++;
            if (needle[i] == 0) return (char_t*)hay;
        }
        hay++;
    }
}

char_t* wwa_strtok_r(char_t* s, const char_t* delim, char_t** save) {
    char_t* p;
    if (s == NULL) s = *save;
    if (s == NULL) return NULL;
    s += wwa_strspn(s, delim);
    if (*s == 0) {
        *save = NULL;
        return NULL;
    }
    p = s;
    while (*p != 0 && wwa_strchr(delim, *p) == NULL) p++;
    if (*p == 0) {
        *save = NULL;
    } else {
        *p = 0;
        *save = p + 1;
    }
    return s;
}

char_t* wwa_strtok(char_t* s, const char_t* delim) {
    static char_t* save;
    return wwa_strtok_r(s, delim, &save);
}

usize wwa_strspn(const char_t* s, const char_t* accept) {
    u8 table[256];
    usize n = 0;
    wwa_memset(table, 0, sizeof(table));
    while (*accept) table[(u8)*accept++] = 1;
    while (s[n] != 0 && table[(u8)s[n]]) n++;
    return n;
}

usize wwa_strcspn(const char_t* s, const char_t* reject) {
    u8 table[256];
    usize n = 0;
    wwa_memset(table, 0, sizeof(table));
    while (*reject) table[(u8)*reject++] = 1;
    while (s[n] != 0 && !table[(u8)s[n]]) n++;
    return n;
}

char_t* wwa_strpbrk(const char_t* s, const char_t* accept) {
    const char_t* a;
    while (*s) {
        for (a = accept; *a; a++) {
            if (*a == *s) return (char_t*)s;
        }
        s++;
    }
    return NULL;
}

char_t* wwa_strerror(i32 err) {
    static char_t buf[40];
    switch (err) {
    case 0:          return "No error";
    case WWA_EIO:    return "Input/output error";
    case WWA_ENOENT: return "No such file or directory";
    case WWA_EBADF:  return "Bad file descriptor";
    case WWA_ENOMEM: return "Out of memory";
    case WWA_EACCES: return "Permission denied";
    case WWA_EFAULT: return "Bad address";
    case WWA_EINVAL: return "Invalid argument";
    case WWA_EDOM:   return "Domain error";
    case WWA_ERANGE: return "Range error";
    default: break;
    }
    wwa_memcpy(buf, "Unknown error ", 14);
    wwa_itoa(err, buf + 14, sizeof(buf) - 14);
    return buf;
}

i32 wwa_tolower(i32 c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

i32 wwa_toupper(i32 c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

i32 wwa_isdigit(i32 c) {
    return (c >= '0' && c <= '9') ? 1 : 0;
}

i32 wwa_isxdigit(i32 c) {
    return wwa_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ? 1 : 0;
}

i32 wwa_isalpha(i32 c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ? 1 : 0;
}

i32 wwa_isalnum(i32 c) {
    return wwa_isalpha(c) || wwa_isdigit(c);
}

i32 wwa_isspace(i32 c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') ? 1 : 0;
}

i32 wwa_isupper(i32 c) {
    return (c >= 'A' && c <= 'Z') ? 1 : 0;
}

i32 wwa_islower(i32 c) {
    return (c >= 'a' && c <= 'z') ? 1 : 0;
}

i32 wwa_isprint(i32 c) {
    return (c >= 32 && c < 127) ? 1 : 0;
}

i32 wwa_ispunct(i32 c) {
    return wwa_isprint(c) && !wwa_isalnum(c) && !wwa_isspace(c) ? 1 : 0;
}

i32 wwa_iscntrl(i32 c) {
    return (c < 32 || c == 127) ? 1 : 0;
}

static i32 wwa_errno_value = 0;
i32 *wwa_errno_loc(void) {
    return &wwa_errno_value;
}