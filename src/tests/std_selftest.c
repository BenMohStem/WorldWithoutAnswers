/* World Without Answers — std_selftest.c
   full edge-case coverage of the from-scratch std library.
   Exits 0 only when every check passes; prints STD_SELFTEST_OK.
   Child mode: "std_selftest --child" returns 7 (spawn self-test). */

#include <stdtype.h>
#include <stdos.h>
#include <stdtime.h>
#include <stdmem.h>
#include <stdstr.h>
#include <stdmath.h>
#include <stdfenv.h>
#include <stdfloat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdlog.h>
#include <stderr.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

static i32 g_tests = 0;
static i32 g_fails = 0;

#define CHECK(cond) do { \
    g_tests++; \
    if (!(cond)) { \
        g_fails++; \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_STR(a, b) do { \
    g_tests++; \
    if (wwa_strcmp((a), (b)) != 0) { \
        g_fails++; \
        printf("FAIL %s:%d: [%s] != [%s]\n", __FILE__, __LINE__, (a), (b)); \
    } \
} while (0)

#define CHECK_F32(a, b, eps) do { \
    g_tests++; \
    { \
        real32_t va = (a), vb = (b); \
        real32_t diff = wwa_fabsf(va - vb); \
        real32_t scale = wwa_fabsf(vb) > 1.0f ? wwa_fabsf(vb) : 1.0f; \
        if (diff > (eps) * scale) { \
            g_fails++; \
            printf("FAIL %s:%d: %s = %f != %s = %f\n", __FILE__, __LINE__, #a, (double)va, #b, (double)vb); \
        } \
    } \
} while (0)

#define CHECK_F64(a, b, eps) do { \
    g_tests++; \
    { \
        real64_t va = (a), vb = (b); \
        real64_t diff = wwa_fabs(va - vb); \
        real64_t scale = wwa_fabs(vb) > 1.0 ? wwa_fabs(vb) : 1.0; \
        if (diff > (eps) * scale) { \
            g_fails++; \
            printf("FAIL %s:%d: %s = %e != %s = %e\n", __FILE__, __LINE__, #a, va, #b, vb); \
        } \
    } \
} while (0)

static u8 g_big_bytes[2 * 65536 + 256];

static i32 fmt_vsnprintf(char_t* out, usize cap, const char_t* fmt, ...) {
    va_list ap;
    i32 n;
    va_start(ap, fmt);
    n = wwa_vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    return n;
}

static i32 fmt_vprintf(const char_t* fmt, ...) {
    va_list ap;
    i32 n;
    va_start(ap, fmt);
    n = wwa_vprintf(fmt, ap);
    va_end(ap);
    return n;
}

static void log_vwrite_helper(i32 level, const char_t* tag, const char_t* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    wwa_log_vwrite(level, tag, fmt, ap);
    va_end(ap);
}

static void test_mem(void) {
    static const usize sizes[] = {0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 32, 33,
                                  63, 64, 65, 127, 128, 129, 255, 256, 257,
                                  511, 1024, 4096, 65536};
    usize si, off;
    for (si = 0; si < ARRAY_COUNT(sizes); si++) {
        usize n = sizes[si];
        u8* src = g_big_bytes + 16;
        u8* dst = g_big_bytes + 16 + n + 32;
        usize i;
        for (i = 0; i < n; i++) src[i] = (u8)(i * 7 + 3);
        wwa_memcpy(dst, src, n);
        for (i = 0; i < n; i++) CHECK(dst[i] == src[i]);
        wwa_memset(dst, 0, n);
        CHECK(dst[0] == 0 && (n == 0 || dst[n - 1] == 0));
        wwa_memset(dst, 0xAB, n);
        for (i = 0; i < n; i++) CHECK(dst[i] == 0xAB);
    }
    for (off = 0; off < 8; off++) {
        usize n = 5000;
        u8* src = g_big_bytes + 32 + off;
        u8* dst = g_big_bytes + 5072;
        usize i;
        for (i = 0; i < n; i++) src[i] = (u8)(i * 13 + off);
        wwa_memcpy(dst, src, n);
        for (i = 0; i < n; i++) CHECK(dst[i] == src[i]);
    }
    {
        usize n = 4000;
        u8* buf = g_big_bytes + 128;
        usize i;
        for (i = 0; i < n; i++) buf[i] = (u8)(i & 0xFF);
        wwa_memmove(buf + 100, buf, n - 100);
        for (i = 0; i < n - 100; i++) CHECK(buf[100 + i] == (u8)(i & 0xFF));
        for (i = 0; i < n; i++) buf[i] = (u8)(i & 0xFF);
        wwa_memmove(buf, buf + 100, n - 100);
        for (i = 0; i < n - 100; i++) CHECK(buf[i] == (u8)((i + 100) & 0xFF));
    }
    {
        u8 a[100], b[100];
        usize i;
        for (i = 0; i < 100; i++) a[i] = (u8)i;
        for (i = 0; i < 100; i++) b[i] = (u8)i;
        CHECK(wwa_memcmp(a, b, 100) == 0);
        b[50] = 0xFF;
        CHECK(wwa_memcmp(a, b, 100) < 0);
        b[50] = 0x00;
        CHECK(wwa_memcmp(a, b, 100) > 0);
        CHECK(wwa_memcmp(a, b, 0) == 0);
    }
    {
        u8 a2[2048], b2[2048];
        usize i;
        for (i = 0; i < 2048; i++) a2[i] = (u8)(i * 5);
        for (i = 0; i < 2048; i++) b2[i] = a2[i];
        CHECK(wwa_memcmp(a2, b2, 2048) == 0);
        b2[1000] = 0xFE;
        CHECK(wwa_memcmp(a2, b2, 2048) < 0);
        b2[1000] = 0x00;
        CHECK(wwa_memcmp(a2, b2, 2048) > 0);
        b2[1000] = a2[1000];
        CHECK(wwa_memcmp(a2, b2, 2048) == 0);
    }
    {
        u8 a[64];
        usize i;
        void_p hit;
        for (i = 0; i < 64; i++) a[i] = (u8)(i * 3);
        hit = wwa_memchr(a, 42, 64);
        CHECK(hit == a + 14);
        CHECK(wwa_memchr(a, 43, 64) == NULL);
        CHECK(wwa_memchr(a, 0, 0) == NULL);
    }
    {
        u8 src[32], dst[32];
        usize i;
        void_p end;
        for (i = 0; i < 32; i++) src[i] = (u8)(i + 1);
        end = wwa_memccpy(dst, src, 5, 32);
        CHECK(end == dst + 5);
        CHECK(dst[4] == 5);
        CHECK(wwa_memccpy(dst, src, 99, 32) == NULL);
    }
    {
        u8 a[64];
        wwa_memset(a, 0x5A, 64);
        wwa_memset_explicit(a, 0x00, 64);
        CHECK(a[0] == 0 && a[63] == 0);
    }
}

static void test_str(void) {
    CHECK(wwa_strlen("") == 0);
    CHECK(wwa_strlen("hello") == 5);
    {
        char_t s[512];
        usize i;
        for (i = 0; i < 500; i++) s[i] = 'a';
        s[500] = 0;
        CHECK(wwa_strlen(s) == 500);
    }
    CHECK(wwa_strcmp("abc", "abc") == 0);
    CHECK(wwa_strcmp("abc", "abd") < 0);
    CHECK(wwa_strcmp("abd", "abc") > 0);
    CHECK(wwa_strcmp("abc", "abcd") < 0);
    CHECK(wwa_strcmp("", "") == 0);
    CHECK(wwa_strncmp("abc", "abcx", 3) == 0);
    CHECK(wwa_strncmp("abc", "abx", 3) != 0);
    {
        char_t d[16];
        wwa_strcpy(d, "hello");
        CHECK_STR(d, "hello");
        wwa_strcat(d, " world");
        CHECK_STR(d, "hello world");
        wwa_strncpy(d, "abcd", 3);
        CHECK(d[0] == 'a' && d[1] == 'b' && d[2] == 'c');
    }
CHECK(wwa_strchr("hello", 'l') - "hello" == 2);
    CHECK(wwa_strchr("hello", 'e') - "hello" == 1);
    CHECK(wwa_strchr("hello", 'z') == NULL);
    CHECK(wwa_strchr("hello", 0) - "hello" == 5);
    CHECK(wwa_strrchr("hello", 'l') - "hello" == 3);
    CHECK(wwa_strrchr("hello", 'z') == NULL);
    CHECK(wwa_strstr("hello world", "world") - "hello world" == 6);
    CHECK(wwa_strstr("hello", "zz") == NULL);
    {
        const char_t* s = "abc";
        CHECK(wwa_strstr(s, "") == s);
    }
    CHECK(wwa_strspn("abc123", "abc") == 3);
    CHECK(wwa_strcspn("abc123", "123") == 3);
    CHECK(wwa_tolower('A') == 'a');
    CHECK(wwa_toupper('a') == 'A');
    CHECK(wwa_isdigit('5') == 1);
    CHECK(wwa_isdigit('x') == 0);
    CHECK(wwa_isalpha('x') == 1);
    CHECK(wwa_isalnum('5') == 1);
    CHECK(wwa_isspace(' ') == 1);
    CHECK(wwa_isxdigit('f') == 1);
    CHECK(wwa_islower('a') == 1);
    CHECK(wwa_islower('A') == 0);
    CHECK(wwa_isupper('A') == 1);
    CHECK(wwa_isupper('a') == 0);
    CHECK(wwa_iscntrl(0) == 1);
    CHECK(wwa_iscntrl('\n') == 1);
    CHECK(wwa_iscntrl('\t') == 1);
    CHECK(wwa_iscntrl(127) == 1);
    CHECK(wwa_iscntrl(' ') == 0);
    CHECK(wwa_isprint(' ') == 1);
    CHECK(wwa_isprint('~') == 1);
    CHECK(wwa_isprint('\n') == 0);
    CHECK(wwa_isprint(127) == 0);
    CHECK(wwa_ispunct('!') == 1);
    CHECK(wwa_ispunct(',') == 1);
    CHECK(wwa_ispunct('a') == 0);
    CHECK(wwa_ispunct('1') == 0);
    CHECK(wwa_ispunct(' ') == 0);
    {
        char_t d[16];
        wwa_strcpy(d, "hello");
        wwa_strncat(d, " world!!", 6);
        CHECK_STR(d, "hello world");
        wwa_strncat(d, "xyz", 0);
        CHECK_STR(d, "hello world");
    }
    CHECK(wwa_strcasecmp("HeLLo", "hello") == 0);
    CHECK(wwa_strcasecmp("abc", "abd") < 0);
    CHECK(wwa_strcasecmp("ABC", "abb") > 0);
    CHECK(wwa_strcasecmp("", "") == 0);
    CHECK(wwa_strncasecmp("AbC", "aBcX", 3) == 0);
    CHECK(wwa_strncasecmp("AbC", "aBd", 3) < 0);
    CHECK(wwa_strncasecmp("AbC", "aBcX", 2) == 0);
    {
        char_t s[] = "a,b,,c";
        char_t* save = NULL;
        char_t* t;
        t = wwa_strtok_r(s, ",", &save);
        CHECK(t != NULL && wwa_strcmp(t, "a") == 0);
        t = wwa_strtok_r(NULL, ",", &save);
        CHECK(t != NULL && wwa_strcmp(t, "b") == 0);
        t = wwa_strtok_r(NULL, ",", &save);
        CHECK(t != NULL && wwa_strcmp(t, "c") == 0);
        CHECK(wwa_strtok_r(NULL, ",", &save) == NULL);
    }
    {
        char_t s[] = "  hello  world  ";
        char_t* save = NULL;
        char_t* t = wwa_strtok_r(s, " ", &save);
        CHECK(t != NULL && wwa_strcmp(t, "hello") == 0);
        t = wwa_strtok_r(NULL, " ", &save);
        CHECK(t != NULL && wwa_strcmp(t, "world") == 0);
        CHECK(wwa_strtok_r(NULL, " ", &save) == NULL);
    }
    {
        char_t a[] = "x;y";
        char_t b[] = "p:q";
        char_t* sa = NULL;
        char_t* sb = NULL;
        CHECK(wwa_strcmp(wwa_strtok_r(a, ";", &sa), "x") == 0);
        CHECK(wwa_strcmp(wwa_strtok_r(b, ":", &sb), "p") == 0);
        CHECK(wwa_strcmp(wwa_strtok_r(NULL, ";", &sa), "y") == 0);
        CHECK(wwa_strcmp(wwa_strtok_r(NULL, ":", &sb), "q") == 0);
        CHECK(wwa_strtok_r(NULL, ";", &sa) == NULL);
        CHECK(wwa_strtok_r(NULL, ":", &sb) == NULL);
    }
    {
        char_t s[] = "one";
        char_t* t = wwa_strtok(s, ",");
        CHECK(t != NULL && wwa_strcmp(t, "one") == 0);
        CHECK(wwa_strtok(NULL, ",") == NULL);
    }
    {
        char_t s[] = ",";
        char_t* save = NULL;
        CHECK(wwa_strtok_r(s, ",", &save) == NULL);
    }
    {
        char_t d[16];
        usize n;
        CHECK(wwa_strnlen("hello", 3) == 3);
        CHECK(wwa_strnlen("hello", 10) == 5);
        CHECK(wwa_strnlen("", 10) == 0);
        n = wwa_strlcpy(d, "hello", sizeof(d));
        CHECK(n == 5 && wwa_strcmp(d, "hello") == 0);
        n = wwa_strlcpy(d, "hello world", 6);
        CHECK(n == 11 && wwa_strcmp(d, "hello") == 0);
        n = wwa_strlcpy(d, "hi", sizeof(d));
        CHECK(n == 2 && wwa_strcmp(d, "hi") == 0);
        wwa_strcpy(d, "hello");
        n = wwa_strlcat(d, " world", sizeof(d));
        CHECK(n == 11 && wwa_strcmp(d, "hello world") == 0);
        wwa_strcpy(d, "hello");
        n = wwa_strlcat(d, " world!!!", 11);
        CHECK(n == 14 && wwa_strcmp(d, "hello worl") == 0);
        wwa_strcpy(d, "hi");
        n = wwa_strlcat(d, "there", 2);
        CHECK(n == 7 && wwa_strcmp(d, "hi") == 0);
    }
    CHECK(wwa_strpbrk("hello world", "aeiou") - "hello world" == 1);
    CHECK(wwa_strpbrk("hello", "xyz") == NULL);
    CHECK(wwa_strpbrk("", "abc") == NULL);
    CHECK(wwa_strpbrk("abc", "") == NULL);
    CHECK(wwa_strcmp(wwa_strerror(0), "No error") == 0);
    CHECK(wwa_strcmp(wwa_strerror(WWA_EDOM), "Domain error") == 0);
    CHECK(wwa_strcmp(wwa_strerror(WWA_ERANGE), "Range error") == 0);
    CHECK(wwa_strcmp(wwa_strerror(WWA_ENOMEM), "Out of memory") == 0);
    CHECK(wwa_strcmp(wwa_strerror(WWA_EINVAL), "Invalid argument") == 0);
    CHECK_STR(wwa_strerror(12345), "Unknown error 12345");
    CHECK_STR(wwa_strerror(-7), "Unknown error -7");
    {
        static char_t b2[40];
        wwa_itoa(12345, b2, sizeof(b2));
        CHECK_STR(b2, "12345");
        wwa_itoa(-7, b2, sizeof(b2));
        CHECK_STR(b2, "-7");
    }
}

static void test_num(void) {
    char_t buf[32];
    CHECK(wwa_atoi("0") == 0);
    CHECK(wwa_atoi("-1") == -1);
    CHECK(wwa_atoi(" 42") == 42);
    CHECK(wwa_atoi("42abc") == 42);
    CHECK(wwa_atoi("2147483647") == 2147483647);
    CHECK(wwa_atoi("-2147483648") == -2147483647 - 1);
    CHECK(wwa_atol("9223372036854775807") == 9223372036854775807ll);
    wwa_itoa(-9223372036854775807ll - 1, buf, sizeof(buf));
    CHECK_STR(buf, "-9223372036854775808");
    wwa_utoa(18446744073709551615ull, buf, sizeof(buf));
    CHECK_STR(buf, "18446744073709551615");
    wwa_itoa_hex(0xDEADBEEF, buf, sizeof(buf));
    CHECK_STR(buf, "deadbeef");
    wwa_itoa(0, buf, sizeof(buf));
    CHECK_STR(buf, "0");
    CHECK(wwa_utoa(12345, buf, 0) == 5);
    {
        const i64 sv[] = { 0, 1, -1, 9, -9, 10, -10, 99, -99, 100, -100, 1024, -1024,
                           12345, -12345, 123456789, -123456789, 2147483647,
                           -2147483648, 123456789012345678ll, -123456789012345678ll,
                           9223372036854775807ll, -9223372036854775807ll - 1 };
        usize k;
        char_t buf2[32];
        for (k = 0; k < sizeof(sv) / sizeof(sv[0]); k++) {
            wwa_itoa(sv[k], buf, sizeof(buf));
            CHECK(wwa_atol(buf) == sv[k]);
            wwa_itoa(wwa_atol(buf), buf2, sizeof(buf2));
            CHECK_STR(buf2, buf);
        }
    }
    {
        const u64 uv[] = { 0, 1, 9, 10, 99, 100, 1024, 12345, 4294967295ull,
                           4294967296ull, 12345678901234567890ull, 0xFFFFFFFFFFFFFFFFull };
        const i32 bases[] = { 2, 8, 16, 36 };
        usize k, b;
        for (k = 0; k < sizeof(uv) / sizeof(uv[0]); k++) {
            for (b = 0; b < sizeof(bases) / sizeof(bases[0]); b++) {
                usize need = wwa_utoa_base(uv[k], buf, 0, bases[b]);
                if (need < sizeof(buf) && uv[k] <= 9223372036854775807ull) {
                    CHECK(wwa_utoa_base(uv[k], buf, sizeof(buf), bases[b]) == need);
                    CHECK(wwa_strtol(buf, NULL, bases[b]) == (i64)uv[k]);
                }
            }
        }
        wwa_utoa(18446744073709551615ull, buf, 10);
        CHECK_STR(buf, "184467440");
        CHECK(wwa_utoa(18446744073709551615ull, buf, 10) == 9);
        wwa_utoa_base(4294967296ull, buf, 32, 2);
        CHECK_STR(buf, "1000000000000000000000000000000");
        CHECK(wwa_utoa_base(4294967296ull, buf, 32, 2) == 31);
        wwa_utoa(12345, buf, 4);
        CHECK_STR(buf, "123");
        wwa_itoa(-123456789, buf, 5);
        CHECK_STR(buf, "-123");
        CHECK(wwa_itoa(-123456789, buf, 5) == 4);
    }
    {
        const i64 tsv[] = { 0, 1, -1, 123456789, -123456789,
                            9223372036854775807ll, -9223372036854775807ll - 1 };
        const u64 tsu[] = { 0, 1, 123456789, 18446744073709551615ull };
        char_t full[64], small[16];
        usize k, c, L;
        for (k = 0; k < sizeof(tsv) / sizeof(tsv[0]); k++) {
            wwa_itoa(tsv[k], full, sizeof(full));
            L = wwa_strlen(full);
            for (c = 1; c <= L + 2; c++) {
                usize w = wwa_itoa(tsv[k], small, c);
                CHECK(w == (c <= L ? c - 1 : L));
                if (c > L)
                    CHECK_STR(small, full);
                else
                    CHECK(wwa_memcmp(small, full, c - 1) == 0 && small[c - 1] == 0);
            }
        }
        for (k = 0; k < sizeof(tsu) / sizeof(tsu[0]); k++) {
            wwa_utoa(tsu[k], full, sizeof(full));
            L = wwa_strlen(full);
            for (c = 1; c <= L + 2; c++) {
                usize w = wwa_utoa(tsu[k], small, c);
                CHECK(w == (c <= L ? c - 1 : L));
                if (c > L)
                    CHECK_STR(small, full);
                else
                    CHECK(wwa_memcmp(small, full, c - 1) == 0 && small[c - 1] == 0);
            }
        }
    }
    {
        wwa_utoa_base(255, buf, sizeof(buf), 16);
        CHECK_STR(buf, "ff");
        wwa_utoa_base(255, buf, sizeof(buf), 8);
        CHECK_STR(buf, "377");
        wwa_utoa_base(255, buf, sizeof(buf), 2);
        CHECK_STR(buf, "11111111");
        wwa_utoa_base(255, buf, sizeof(buf), 36);
        CHECK_STR(buf, "73");
        wwa_utoa_base(18446744073709551615ull, buf, sizeof(buf), 16);
        CHECK_STR(buf, "ffffffffffffffff");
        wwa_utoa_base(0, buf, sizeof(buf), 2);
        CHECK_STR(buf, "0");
        wwa_itoa_hex(0xFFFFFFFFFFFFFFFFull, buf, sizeof(buf));
        CHECK_STR(buf, "ffffffffffffffff");
        wwa_itoa_hex(0x123456789ABCDEF0ull, buf, sizeof(buf));
        CHECK_STR(buf, "123456789abcdef0");
    }
    {
        char_t* end;
        CHECK(wwa_strtol("42", &end, 10) == 42 && *end == 0);
        CHECK(wwa_strtol("-42", &end, 10) == -42 && *end == 0);
        CHECK(wwa_strtol("  123abc", &end, 10) == 123 && wwa_strcmp(end, "abc") == 0);
        CHECK(wwa_strtol("0xff", &end, 0) == 255 && *end == 0);
        CHECK(wwa_strtol("0X1F", &end, 16) == 31 && *end == 0);
        CHECK(wwa_strtol("017", &end, 0) == 15 && *end == 0);
        CHECK(wwa_strtol("1010", &end, 2) == 10 && *end == 0);
        CHECK(wwa_strtol("z", &end, 36) == 35 && *end == 0);
        CHECK(wwa_strtol("10", &end, 36) == 36 && *end == 0);
        CHECK(wwa_strtol("-0", &end, 10) == 0 && *end == 0);
        CHECK(wwa_strtol("abc", &end, 10) == 0 && end == (char_t*)"abc");
        CHECK(wwa_strtol("9223372036854775807", &end, 10) == 9223372036854775807ll && *end == 0);
        CHECK(wwa_strtol("9223372036854775808", &end, 10) == 9223372036854775807ll);
        CHECK(wwa_strtol("-9223372036854775809", &end, 10) == -9223372036854775807ll - 1);
        CHECK(wwa_strtol("0x", &end, 0) == 0 && wwa_strcmp(end, "0x") == 0);
        CHECK(wwa_strtol("10", &end, 1) == 0 && end == (char_t*)"10");
        CHECK(wwa_strtol("10", NULL, 10) == 10);
        CHECK(wwa_strtol("0x10", &end, 10) == 0 && wwa_strcmp(end, "x10") == 0);
    }
    CHECK(wwa_abs(-5) == 5);
    CHECK(wwa_abs(5) == 5);
    CHECK(wwa_labs(-9223372036854775807ll - 1) == (i64)0x8000000000000000ull);
    CHECK(wwa_labs(0) == 0);
    {
        char_t* d = wwa_strdup("hello");
        CHECK(d != NULL);
        CHECK_STR(d, "hello");
        wwa_free(d);
        d = wwa_strndup("hello world", 5);
        CHECK(d != NULL);
        CHECK_STR(d, "hello");
        wwa_free(d);
        d = wwa_strndup("hi", 100);
        CHECK(d != NULL);
        CHECK_STR(d, "hi");
        wwa_free(d);
    }
    {
        char_t* end;
        CHECK(wwa_strtof("0.5", NULL) == 0.5f);
        CHECK(wwa_strtof("0.1", NULL) == 0.1f);
        CHECK(wwa_strtof("3.14159", NULL) == 3.14159f);
        CHECK(wwa_strtof("-1.25e3", NULL) == -1250.0f);
        CHECK(wwa_strtof("1e10", NULL) == 1.0e10f);
        CHECK(wwa_strtof("1e-5", NULL) == 1.0e-5f);
        CHECK(wwa_strtof(" 42abc", &end) == 42.0f && wwa_strcmp(end, "abc") == 0);
        CHECK(wwa_strtof("abc", &end) == 0.0f && end == (char_t*)"abc");
        CHECK(wwa_strtof("1e400", NULL) == wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_strtof("-1e400", NULL) == -wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_strtof("1e-400", NULL) == 0.0f);
        CHECK(wwa_strtof("inf", NULL) == wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_strtof("-INF", NULL) == -wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_strtof("nan", NULL) != wwa_strtof("nan", NULL));
        CHECK(wwa_strtof(".5", NULL) == 0.5f);
        CHECK(wwa_strtof("5.", NULL) == 5.0f);
        CHECK(wwa_strtof("+7", NULL) == 7.0f);
        CHECK(wwa_strtof("123456789", NULL) == 123456789.0f);
        CHECK(wwa_strtof("1.5e2", &end) == 150.0f && *end == 0);
    }
    {
        char_t* end;
        CHECK(wwa_strtod("0.5", NULL) == 0.5);
        CHECK(wwa_strtod("0.1", NULL) == 0.1);
        CHECK(wwa_strtod("-1.25e3", NULL) == -1250.0);
        CHECK(wwa_strtod("1e-5", NULL) == 1e-5);
        CHECK(wwa_strtod("1e400", NULL) > 1.0e308);
        CHECK(wwa_strtod("-1e400", NULL) < -1.0e308);
        CHECK(wwa_strtod("1e-400", NULL) == 0.0);
        CHECK(wwa_strtod("0x1.8p1", NULL) == 3.0);
        CHECK(wwa_strtod("0x1p-1", NULL) == 0.5);
        CHECK(wwa_strtod("0x0.8p1", NULL) == 1.0);
        CHECK(wwa_strtod("0X1P4", NULL) == 16.0);
        CHECK(wwa_strtod("0x10", NULL) == 16.0);
        CHECK(wwa_strtod("0x1.8p+2", NULL) == 6.0);
        CHECK(wwa_strtod("0x1.8p-2", NULL) == 0.375);
        CHECK_F32(wwa_strtod("0x1.99999ap-4", NULL), 0.1, 1e-6f);
        CHECK(wwa_strtod("0x", &end) == 0.0 && end == (char_t*)"0x");
        CHECK(wwa_strtod("0x1p", &end) == 1.0 && wwa_strcmp(end, "p") == 0);
        CHECK(wwa_strtod("abc", &end) == 0.0 && end == (char_t*)"abc");
        CHECK(wwa_strtod(" 3.5xyz", &end) == 3.5 && wwa_strcmp(end, "xyz") == 0);
        CHECK(wwa_strtod("inf", NULL) > 1.0e308);
        CHECK(wwa_strtod("-inf", NULL) < -1.0e308);
        CHECK(wwa_strtod("nan", NULL) != wwa_strtod("nan", NULL));
        CHECK(wwa_atof("3.14") == 3.14);
        CHECK(wwa_atof("-0.5e2") == -50.0);
        CHECK(wwa_strtof("0x1.8p1", NULL) == 3.0f);
    }
    {
        wwa_utoa_base(255, buf, sizeof(buf), 16);
        CHECK_STR(buf, "ff");
        wwa_utoa_base(10, buf, sizeof(buf), 2);
        CHECK_STR(buf, "1010");
        wwa_utoa_base(35, buf, sizeof(buf), 36);
        CHECK_STR(buf, "z");
        wwa_utoa_base(36, buf, sizeof(buf), 36);
        CHECK_STR(buf, "10");
        wwa_utoa_base(8, buf, sizeof(buf), 8);
        CHECK_STR(buf, "10");
        wwa_utoa_base(0, buf, sizeof(buf), 16);
        CHECK_STR(buf, "0");
        wwa_utoa_base(123, buf, sizeof(buf), 1);
        CHECK_STR(buf, "123");
        wwa_utoa_base(0xDEADBEEF, buf, sizeof(buf), 16);
        CHECK_STR(buf, "deadbeef");
        wwa_utoa_base(18446744073709551615ull, buf, sizeof(buf), 36);
        CHECK_STR(buf, "3w5e11264sgsf");
    }
    CHECK(wwa_llabs(-1234567890123ll) == 1234567890123ll);
    CHECK(wwa_llabs(42ll) == 42ll);
    CHECK(wwa_llabs(0ll) == 0ll);
    {
        wwa_div_t d = wwa_div(17, 5);
        CHECK(d.quot == 3 && d.rem == 2);
        d = wwa_div(-17, 5);
        CHECK(d.quot == -3 && d.rem == -2);
        d = wwa_div(17, -5);
        CHECK(d.quot == -3 && d.rem == 2);
        d = wwa_div(-17, -5);
        CHECK(d.quot == 3 && d.rem == -2);
        d = wwa_div(0, 5);
        CHECK(d.quot == 0 && d.rem == 0);
    }
    {
        wwa_ldiv_t d = wwa_ldiv(-17, 5);
        CHECK(d.quot == -3 && d.rem == -2);
        d = wwa_ldiv(17, -5);
        CHECK(d.quot == -3 && d.rem == 2);
        wwa_lldiv_t e = wwa_lldiv(1000000000000ll, 7);
        CHECK(e.quot == 142857142857ll && e.rem == 1);
        e = wwa_lldiv(-1000000000000ll, 7);
        CHECK(e.quot == -142857142857ll && e.rem == -1);
    }
}

static void test_math(void) {
    CHECK_F32(wwa_sinf(0.0f), 0.0f, 1e-7f);
    CHECK_F32(wwa_cosf(0.0f), 1.0f, 1e-7f);
    CHECK_F32(wwa_sinf((real32_t)WWA_PI_2), 1.0f, 1e-6f);
    CHECK_F32(wwa_cosf((real32_t)WWA_PI), -1.0f, 1e-6f);
    CHECK_F32(wwa_sinf((real32_t)WWA_PI), 0.0f, 1e-6f);
    CHECK_F32(wwa_tanf((real32_t)(WWA_PI / 4)), 1.0f, 1e-5f);
    CHECK_F32(wwa_expf(0.0f), 1.0f, 1e-7f);
    CHECK_F32(wwa_expf(1.0f), (real32_t)WWA_E, 1e-6f);
    CHECK_F32(wwa_logf(1.0f), 0.0f, 1e-7f);
    CHECK_F32(wwa_logf((real32_t)WWA_E), 1.0f, 1e-6f);
    CHECK_F32(wwa_logf(2.0f), 0.6931472f, 1e-6f);
    CHECK_F32(wwa_powf(2.0f, 10.0f), 1024.0f, 1e-5f);
    CHECK_F32(wwa_powf(4.0f, 0.5f), 2.0f, 1e-5f);
    CHECK_F32(wwa_powf(2.0f, 0.0f), 1.0f, 0.0f);
    CHECK_F32(wwa_atanf(1.0f), (real32_t)WWA_PI_4, 1e-6f);
    CHECK_F32(wwa_atanf(0.0f), 0.0f, 1e-7f);
    CHECK_F32(wwa_atanf(-1.0f), (real32_t)(-WWA_PI_4), 1e-6f);
    CHECK_F32(wwa_atan2f(1.0f, 1.0f), (real32_t)WWA_PI_4, 1e-6f);
    CHECK_F32(wwa_atan2f(-1.0f, -1.0f), (real32_t)(-3.0 * WWA_PI_4), 1e-6f);
    CHECK_F32(wwa_atan2f(1.0f, 0.0f), (real32_t)WWA_PI_2, 1e-6f);
    CHECK_F32(wwa_atan2f(0.0f, 1.0f), 0.0f, 1e-7f);
    CHECK_F32(wwa_sqrtf(2.0f) * wwa_sqrtf(2.0f), 2.0f, 1e-6f);
    CHECK_F32(wwa_sqrtf(0.0f), 0.0f, 0.0f);
    CHECK_F32(wwa_fmodf(5.5f, 2.0f), 1.5f, 1e-6f);
    CHECK_F32(wwa_fmodf(-5.5f, 2.0f), -1.5f, 1e-6f);
    CHECK_F32(wwa_floorf(2.7f), 2.0f, 0.0f);
    CHECK_F32(wwa_floorf(-2.7f), -3.0f, 0.0f);
    CHECK_F32(wwa_ceilf(2.1f), 3.0f, 0.0f);
    CHECK_F32(wwa_ceilf(-2.1f), -2.0f, 0.0f);
    CHECK_F32(wwa_roundf(2.5f), 3.0f, 0.0f);
    CHECK_F32(wwa_roundf(-2.5f), -3.0f, 0.0f);
    CHECK_F32(wwa_truncf(2.9f), 2.0f, 0.0f);
    CHECK_F32(wwa_truncf(-2.9f), -2.0f, 0.0f);
    CHECK_F32(wwa_fabsf(-3.5f), 3.5f, 0.0f);
    {
        u32 i;
        for (i = 0; i < 20000; i++) {
            real32_t x = (real32_t)((i % 6283) - 3141) * 0.001f;
            real32_t s = wwa_sinf(x);
            real32_t c = wwa_cosf(x);
            CHECK_F32(s * s + c * c, 1.0f, 2e-5f);
            CHECK_F32(wwa_sinf(-x), -s, 1e-6f);
        }
    }
    {
        i32 e;
        CHECK_F32(wwa_ldexpf(1.0f, 0), 1.0f, 0.0f);
        CHECK_F32(wwa_ldexpf(1.5f, 10), 1536.0f, 0.0f);
        CHECK_F32(wwa_ldexpf(1.0f, -10), 0.0009765625f, 0.0f);
        CHECK_F32(wwa_ldexpf(1.0f, -149), 1.401298464e-45f, 0.0f);
        CHECK(wwa_ldexpf(3.0f, 128) == wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_ldexpf(1.0f, 2000) == wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_ldexpf(1.0f, -2000) == 0.0f);
        CHECK_F32(wwa_frexpf(6.0f, &e), 0.75f, 0.0f);
        CHECK(e == 3);
        CHECK_F32(wwa_frexpf(1.0f, &e), 0.5f, 0.0f);
        CHECK(e == 1);
        CHECK_F32(wwa_frexpf(1.401298464e-45f, &e), 0.5f, 0.0f);
        CHECK(e == -148);
        CHECK_F32(wwa_frexpf(-6.0f, &e), -0.75f, 0.0f);
        {
            real32_t m2 = wwa_frexpf(12345.678f, &e);
            CHECK_F32(wwa_ldexpf(m2, e), 12345.678f, 1e-4f);
        }
        CHECK_F32(wwa_exp2f(0.0f), 1.0f, 0.0f);
        CHECK_F32(wwa_exp2f(10.0f), 1024.0f, 0.0f);
        CHECK_F32(wwa_exp2f(-10.0f), 0.0009765625f, 0.0f);
        CHECK_F32(wwa_exp2f(1.5f), 2.828427f, 1e-5f);
        CHECK_F32(wwa_exp2f(127.0f), 1.7014118e38f, 1e-4f);
        CHECK_F32(wwa_log2f(1.0f), 0.0f, 1e-7f);
        CHECK_F32(wwa_log2f(8.0f), 3.0f, 1e-6f);
        CHECK_F32(wwa_log2f(1024.0f), 10.0f, 1e-6f);
        CHECK_F32(wwa_log2f(0.5f), -1.0f, 1e-6f);
        CHECK_F32(wwa_log2f(3.0f), 1.5849625f, 1e-6f);
        CHECK_F32(wwa_log10f(1.0f), 0.0f, 1e-7f);
        CHECK_F32(wwa_log10f(1000.0f), 3.0f, 1e-5f);
        CHECK_F32(wwa_log10f(0.001f), -3.0f, 1e-5f);
        CHECK_F32(wwa_log10f(2.0f), 0.30103f, 1e-6f);
        CHECK_F32(wwa_sinhf(0.0f), 0.0f, 1e-7f);
        CHECK_F32(wwa_sinhf(1.0f), 1.1752012f, 1e-5f);
        CHECK_F32(wwa_sinhf(-1.0f), -1.1752012f, 1e-5f);
        CHECK_F32(wwa_coshf(0.0f), 1.0f, 1e-7f);
        CHECK_F32(wwa_coshf(1.0f), 1.5430806f, 1e-5f);
        CHECK_F32(wwa_coshf(-1.0f), 1.5430806f, 1e-5f);
        CHECK_F32(wwa_sinhf(1.0f) * wwa_sinhf(1.0f) + 1.0f, wwa_coshf(1.0f) * wwa_coshf(1.0f), 1e-4f);
        CHECK_F32(wwa_tanhf(0.0f), 0.0f, 1e-7f);
        CHECK_F32(wwa_tanhf(1.0f), 0.76159416f, 1e-5f);
        CHECK_F32(wwa_tanhf(100.0f), 1.0f, 1e-6f);
        CHECK_F32(wwa_tanhf(-100.0f), -1.0f, 1e-6f);
        CHECK_F32(wwa_asinf(0.0f), 0.0f, 1e-7f);
        CHECK_F32(wwa_asinf(0.5f), (real32_t)(WWA_PI / 6), 1e-5f);
        CHECK_F32(wwa_asinf(1.0f), (real32_t)WWA_PI_2, 1e-5f);
        CHECK_F32(wwa_asinf(-1.0f), (real32_t)(-WWA_PI_2), 1e-5f);
        CHECK_F32(wwa_acosf(0.5f), (real32_t)(WWA_PI / 3), 1e-5f);
        CHECK_F32(wwa_acosf(1.0f), 0.0f, 1e-6f);
        CHECK_F32(wwa_acosf(-1.0f), (real32_t)WWA_PI, 1e-5f);
        CHECK_F32(wwa_sinf(wwa_asinf(0.3f)), 0.3f, 1e-5f);
        CHECK_F32(wwa_cosf(wwa_acosf(0.3f)), 0.3f, 1e-5f);
        CHECK(wwa_asinf(2.0f) != wwa_asinf(2.0f));
        CHECK(wwa_acosf(2.0f) != wwa_acosf(2.0f));
        CHECK(wwa_copysignf(3.0f, -1.0f) == -3.0f);
        CHECK(wwa_copysignf(-3.0f, 1.0f) == 3.0f);
        CHECK(wwa_copysignf(3.0f, 1.0f) == 3.0f);
        CHECK(wwa_signbitf(-0.0f) == 1);
        CHECK(wwa_signbitf(0.0f) == 0);
        CHECK(wwa_signbitf(-5.0f) == 1);
        CHECK(wwa_signbitf(5.0f) == 0);
        CHECK(wwa_fminf(3.0f, 5.0f) == 3.0f);
        CHECK(wwa_fmaxf(3.0f, 5.0f) == 5.0f);
        CHECK(wwa_fminf(5.0f, 3.0f) == 3.0f);
        CHECK(wwa_fmaxf(5.0f, 3.0f) == 5.0f);
        CHECK(wwa_fminf(1.0f, 0.0f / 0.0f) == 1.0f);
        CHECK(wwa_fmaxf(1.0f, 0.0f / 0.0f) == 1.0f);
        CHECK(wwa_fminf(-0.0f, 0.0f) == -0.0f);
        CHECK(wwa_fmaxf(-0.0f, 0.0f) == 0.0f);
        {
            real32_t ip;
            CHECK_F32(wwa_modff(3.75f, &ip), 0.75f, 0.0f);
            CHECK(ip == 3.0f);
            CHECK_F32(wwa_modff(-3.75f, &ip), -0.75f, 0.0f);
            CHECK(ip == -3.0f);
        }
        {
            u32 i;
            for (i = 0; i < 600; i++) {
                real32_t x = (real32_t)((i % 600) - 300) * 0.01f;
                CHECK_F32(wwa_sinhf(x) * wwa_sinhf(x) - wwa_coshf(x) * wwa_coshf(x), -1.0f, 1e-3f);
            }
            for (i = 0; i < 2000; i++) {
                real32_t x = (real32_t)((i % 2000) - 1000) * 0.01f;
                CHECK_F32(wwa_sinhf(x) / wwa_coshf(x), wwa_tanhf(x), 1e-5f);
            }
        }
        CHECK_F32(wwa_erff(0.0f), 0.0f, 0.0f);
        CHECK_F32(wwa_erff(1.0e-6f), 1.128379167e-6f, 1e-6f);
        CHECK_F32(wwa_erff(0.5f), 0.5204999f, 1e-6f);
        CHECK_F32(wwa_erff(1.0f), 0.8427008f, 1e-6f);
        CHECK_F32(wwa_erff(2.0f), 0.9953223f, 1e-6f);
        CHECK_F32(wwa_erff(3.0f), 0.9999779f, 1e-6f);
        CHECK_F32(wwa_erff(-1.0f), -0.8427008f, 1e-6f);
        CHECK_F32(wwa_erff(5.0f), 1.0f, 0.0f);
        CHECK_F32(wwa_erff(-5.0f), -1.0f, 0.0f);
        CHECK_F32(wwa_asinhf(0.0f), 0.0f, 1e-7f);
        CHECK_F32(wwa_asinhf(1.0f), 0.8813736f, 1e-6f);
        CHECK_F32(wwa_asinhf(-1.0f), -0.8813736f, 1e-6f);
        CHECK_F32(wwa_asinhf(1.0e10f), 23.71899f, 1e-5f);
        CHECK(wwa_asinhf(0.0f / 0.0f) != wwa_asinhf(0.0f / 0.0f));
        CHECK_F32(wwa_acoshf(1.0f), 0.0f, 1e-7f);
        CHECK_F32(wwa_acoshf(2.0f), 1.3169579f, 1e-6f);
        CHECK_F32(wwa_acoshf(1.0e10f), 23.71899f, 1e-5f);
        CHECK(wwa_acoshf(0.5f) != wwa_acoshf(0.5f));
        CHECK(wwa_acoshf(wwa_ldexpf(1.0f, 2000)) == wwa_ldexpf(1.0f, 2000));
        CHECK_F32(wwa_atanhf(0.0f), 0.0f, 1e-7f);
        CHECK_F32(wwa_atanhf(0.5f), 0.54930615f, 1e-6f);
        CHECK_F32(wwa_atanhf(-0.5f), -0.54930615f, 1e-6f);
        CHECK_F32(wwa_atanhf(0.999f), 3.8002012f, 1e-5f);
        CHECK(wwa_atanhf(1.0f) == wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_atanhf(-1.0f) == -wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_atanhf(2.0f) != wwa_atanhf(2.0f));
        CHECK_F32(wwa_hypotf(3.0f, 4.0f), 5.0f, 1e-6f);
        CHECK_F32(wwa_hypotf(1.0e38f, 1.0e38f), 1.4142135e38f, 1e-4f);
        CHECK_F32(wwa_hypotf(1.0e-40f, 1.0e-40f), 1.4142136e-40f, 1e-4f);
        CHECK(wwa_hypotf(0.0f, 0.0f) == 0.0f);
        CHECK(wwa_hypotf(0.0f / 0.0f, 1.0f) != wwa_hypotf(0.0f / 0.0f, 1.0f));
        CHECK_F32(wwa_remainderf(5.5f, 2.0f), -0.5f, 1e-6f);
        CHECK_F32(wwa_remainderf(-5.5f, 2.0f), 0.5f, 1e-6f);
        CHECK_F32(wwa_remainderf(7.0f, 2.0f), -1.0f, 1e-6f);
        CHECK(wwa_remainderf(5.0f, 2.5f) == 0.0f);
        CHECK(wwa_remainderf(5.0f, 0.0f) != wwa_remainderf(5.0f, 0.0f));
        CHECK_F32(wwa_scalbnf(1.5f, 10), 1536.0f, 0.0f);
        CHECK_F32(wwa_scalbnf(1.0f, -149), 1.401298464e-45f, 0.0f);
        CHECK(wwa_scalblnf(2.0f, 2000000000ll) == wwa_ldexpf(1.0f, 2000));
        {
            u32 i;
            for (i = 0; i < 2000; i++) {
                real32_t x = (real32_t)((i % 2000) - 1000) * 0.01f;
                CHECK_F32(wwa_sinhf(wwa_asinhf(x)), x, 1e-5f);
                if (x > -0.9f && x < 0.9f) {
                    CHECK_F32(wwa_tanhf(wwa_atanhf(x)), x, 1e-5f);
                }
            }
        }
        {
            u32 i;
            for (i = 0; i < 1000; i++) {
                real32_t x = (real32_t)((i % 1000) - 500) * 0.01f;
                CHECK_F32(wwa_erff(x), -wwa_erff(-x), 1e-6f);
            }
        }
        CHECK_F32(wwa_expm1f(0.0f), 0.0f, 0.0f);
        CHECK_F32(wwa_expm1f(1.0f), 1.7182818f, 1e-6f);
        CHECK_F32(wwa_expm1f(-1.0f), -0.63212055f, 1e-6f);
        CHECK_F32(wwa_expm1f(0.5f), 0.64872128f, 1e-6f);
        CHECK_F32(wwa_expm1f(10.0f), 22025.465f, 1e-2f);
        CHECK(wwa_expm1f(-100.0f) == -1.0f);
        CHECK(wwa_expm1f(100.0f) == wwa_ldexpf(1.0f, 2000));
        CHECK_F32(wwa_log1pf(0.0f), 0.0f, 0.0f);
        CHECK_F32(wwa_log1pf(1.0f), 0.6931472f, 1e-6f);
        CHECK_F32(wwa_log1pf(-0.5f), -0.6931472f, 1e-6f);
        CHECK_F32(wwa_log1pf(1.7182818f), 1.0f, 1e-6f);
        CHECK_F32(wwa_log1pf(1.0e-7f), 9.9999995e-8f, 1e-5f);
        CHECK_F32(wwa_log1pf(1.0e10f), 23.025851f, 1e-4f);
        CHECK(wwa_log1pf(-1.0f) == -wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_log1pf(-2.0f) != wwa_log1pf(-2.0f));
        CHECK(wwa_cbrtf(0.0f) == 0.0f);
        CHECK_F32(wwa_cbrtf(8.0f), 2.0f, 0.0f);
        CHECK_F32(wwa_cbrtf(-8.0f), -2.0f, 0.0f);
        CHECK_F32(wwa_cbrtf(27.0f), 3.0f, 1e-6f);
        CHECK_F32(wwa_cbrtf(2.0f), 1.2599211f, 1e-6f);
        CHECK_F32(wwa_cbrtf(-2.0f), -1.2599211f, 1e-6f);
        CHECK_F32(wwa_cbrtf(1.0e30f), 1.0e10f, 1e-3f);
        CHECK(wwa_fmaf(1.5f, 2.5f, 0.25f) == 4.0f);
        CHECK(wwa_fmaf(2.5f, 4.5f, 1.25f) == 12.5f);
        CHECK(wwa_fmaf(0.25f, 0.5f, 0.125f) == 0.25f);
        CHECK(wwa_fmaf(3.0f, 3.0f, 1.0f) == 10.0f);
        CHECK(wwa_fmaf(-3.0f, 3.0f, 1.0f) == -8.0f);
        CHECK(wwa_fmaf(2.0f, 2.0f, -4.0f) == 0.0f);
        CHECK(wwa_fmaf(0.0f, 0.0f, 1.5f) == 1.5f);
        CHECK(wwa_fmaf(1.0e30f, 1.0e30f, -1.0e30f) > 3.4028235e38f);
        CHECK(wwa_fmaf(0.0f, wwa_ldexpf(1.0f, 2000), 1.0f) != wwa_fmaf(0.0f, wwa_ldexpf(1.0f, 2000), 1.0f));
        CHECK(wwa_fmaf(wwa_ldexpf(1.0f, 2000), 1.0f, -wwa_ldexpf(1.0f, 2000)) !=
              wwa_fmaf(wwa_ldexpf(1.0f, 2000), 1.0f, -wwa_ldexpf(1.0f, 2000)));
        CHECK(wwa_fmaf(wwa_ldexpf(1.0f, 2000), 1.0f, 2.0f) == wwa_ldexpf(1.0f, 2000));
        CHECK_F32(wwa_erfcf(0.0f), 1.0f, 0.0f);
        CHECK_F32(wwa_erfcf(0.5f), 0.47950012f, 1e-6f);
        CHECK_F32(wwa_erfcf(1.0f), 0.15729921f, 1e-6f);
        CHECK_F32(wwa_erfcf(-1.0f), 1.8427008f, 1e-6f);
        CHECK_F32(wwa_erfcf(2.0f), 0.00467773f, 1e-6f);
        CHECK_F32(wwa_erfcf(5.0f), 0.0f, 1e-6f);
        CHECK(wwa_nextafterf(1.0f, 2.0f) == 1.0000001f);
        CHECK(wwa_nextafterf(1.0f, 0.0f) == 0.99999994f);
        CHECK(wwa_nextafterf(0.0f, 1.0f) == 1.4012985e-45f);
        CHECK(wwa_nextafterf(0.0f, -1.0f) == -1.4012985e-45f);
        CHECK(wwa_nextafterf(-0.0f, 1.0f) == 1.4012985e-45f);
        CHECK(wwa_nextafterf(3.4028235e38f, 0.0f) == 3.4028233e38f);
        CHECK(wwa_nextafterf(3.4028235e38f, wwa_ldexpf(1.0f, 2000)) > 3.4028235e38f);
        CHECK(wwa_nextafterf(1.0f, 1.0f) == 1.0f);
        {
            u32 i;
            for (i = 0; i < 2000; i++) {
                real32_t x = (real32_t)((i % 2000) - 1000) * 0.001f;
                CHECK_F32(wwa_expm1f(x) + 1.0f, wwa_expf(x), 1e-6f);
                if (x > -0.99f && x < 0.99f) {
                    CHECK_F32(wwa_log1pf(x), wwa_logf(1.0f + x), 1e-5f);
                }
            }
        }
        CHECK(wwa_rintf(2.5f) == 2.0f);
        CHECK(wwa_rintf(3.5f) == 4.0f);
        CHECK(wwa_rintf(-2.5f) == -2.0f);
        CHECK(wwa_rintf(-3.5f) == -4.0f);
        CHECK(wwa_rintf(0.5f) == 0.0f);
        CHECK(wwa_rintf(1.5f) == 2.0f);
        CHECK(wwa_rintf(0.49999997f) == 0.0f);
        CHECK(wwa_rintf(16777216.0f) == 16777216.0f);
        CHECK(wwa_rintf(1.0e30f) == 1.0e30f);
        CHECK(wwa_rintf(wwa_ldexpf(1.0f, 2000)) == wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_rintf(0.0f / 0.0f) != wwa_rintf(0.0f / 0.0f));
        CHECK(wwa_rintf(-0.5f) == -0.0f);
        CHECK(wwa_signbitf(wwa_rintf(-0.5f)) == 1);
        CHECK(wwa_nearbyintf(2.5f) == 2.0f);
        CHECK(wwa_nearbyintf(-3.5f) == -4.0f);
        CHECK(wwa_lrintf(2.5f) == 2);
        CHECK(wwa_lrintf(-2.5f) == -2);
        CHECK(wwa_lrintf(3.5f) == 4);
        CHECK(wwa_lrintf(1.9f) == 2);
        CHECK(wwa_lrintf(2147483648.0f) == -2147483647 - 1);
        CHECK(wwa_lrintf(0.0f / 0.0f) == -2147483647 - 1);
        CHECK(wwa_llrintf(2.5f) == 2);
        CHECK(wwa_llrintf(-3.5f) == -4);
        CHECK(wwa_llrintf(9007199254740992.0f) == 9007199254740992ll);
        CHECK(wwa_llrintf(1.0e30f) == -9223372036854775807ll - 1);
        CHECK(wwa_lroundf(2.5f) == 3);
        CHECK(wwa_lroundf(-2.5f) == -3);
        CHECK(wwa_lroundf(2.4f) == 2);
        CHECK(wwa_lroundf(2147483648.0f) == -2147483647 - 1);
        CHECK(wwa_llroundf(2.5f) == 3);
        CHECK(wwa_llroundf(-2.5f) == -3);
        CHECK(wwa_llroundf(1.0e30f) == -9223372036854775807ll - 1);
        CHECK(wwa_fdimf(3.0f, 2.0f) == 1.0f);
        CHECK(wwa_fdimf(2.0f, 3.0f) == 0.0f);
        CHECK(wwa_fdimf(5.0f, 5.0f) == 0.0f);
        CHECK(wwa_fdimf(0.5f, 0.25f) == 0.25f);
        CHECK(wwa_fdimf(0.0f / 0.0f, 1.0f) != wwa_fdimf(0.0f / 0.0f, 1.0f));
        CHECK(wwa_fdimf(1.0f, 0.0f / 0.0f) != wwa_fdimf(1.0f, 0.0f / 0.0f));
        CHECK(wwa_fdimf(3.4028235e38f, -3.4028235e38f) == wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_fdimf(-1.0e30f, 1.0e30f) == 0.0f);
        CHECK(wwa_ilogbf(1.0f) == 0);
        CHECK(wwa_ilogbf(2.0f) == 1);
        CHECK(wwa_ilogbf(0.5f) == -1);
        CHECK(wwa_ilogbf(1024.0f) == 10);
        CHECK(wwa_ilogbf(3.4028235e38f) == 127);
        CHECK(wwa_ilogbf(1.4012985e-45f) == -149);
        CHECK(wwa_ilogbf(0.0f) == -2147483647 - 1);
        CHECK(wwa_ilogbf(-0.0f) == -2147483647 - 1);
        CHECK(wwa_ilogbf(wwa_ldexpf(1.0f, 2000)) == 2147483647);
        CHECK(wwa_ilogbf(0.0f / 0.0f) == 2147483647);
        CHECK(wwa_logbf(1.0f) == 0.0f);
        CHECK(wwa_logbf(2.0f) == 1.0f);
        CHECK(wwa_logbf(1024.0f) == 10.0f);
        CHECK(wwa_logbf(0.0f) == -wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_logbf(wwa_ldexpf(1.0f, 2000)) == wwa_ldexpf(1.0f, 2000));
        CHECK(wwa_logbf(0.0f / 0.0f) != wwa_logbf(0.0f / 0.0f));
        {
            u32 i;
            for (i = 0; i < 2000; i++) {
                real32_t x = (real32_t)((i % 2000) - 1000) * 0.5f;
                CHECK(wwa_rintf(x) == -wwa_rintf(-x));
                CHECK(wwa_fdimf(x, x * 0.25f) == (x > x * 0.25f ? x - x * 0.25f : 0.0f));
            }
        }
        CHECK(wwa_fpclassifyf(0.0f) == 2);
        CHECK(wwa_fpclassifyf(-0.0f) == 2);
        CHECK(wwa_fpclassifyf(1.0f) == 4);
        CHECK(wwa_fpclassifyf(-1.0f) == 4);
        CHECK(wwa_fpclassifyf(1.4012985e-45f) == 3);
        CHECK(wwa_fpclassifyf(1.1754944e-38f) == 4);
        CHECK(wwa_fpclassifyf(3.4028235e38f) == 4);
        CHECK(wwa_fpclassifyf(wwa_ldexpf(1.0f, 2000)) == 1);
        CHECK(wwa_fpclassifyf(0.0f / 0.0f) == 0);
        CHECK(wwa_isnanf(0.0f / 0.0f) == 1);
        CHECK(wwa_isnanf(1.0f) == 0);
        CHECK(wwa_isnanf(wwa_ldexpf(1.0f, 2000)) == 0);
        CHECK(wwa_isinff(wwa_ldexpf(1.0f, 2000)) == 1);
        CHECK(wwa_isinff(-wwa_ldexpf(1.0f, 2000)) == 1);
        CHECK(wwa_isinff(1.0f) == 0);
        CHECK(wwa_isfinitef(0.0f) == 1);
        CHECK(wwa_isfinitef(3.4028235e38f) == 1);
        CHECK(wwa_isfinitef(wwa_ldexpf(1.0f, 2000)) == 0);
        CHECK(wwa_isfinitef(0.0f / 0.0f) == 0);
        CHECK(wwa_isnormalf(1.0f) == 1);
        CHECK(wwa_isnormalf(1.4012985e-45f) == 0);
        CHECK(wwa_isnormalf(0.0f) == 0);
        CHECK(wwa_isnormalf(wwa_ldexpf(1.0f, 2000)) == 0);
        CHECK(wwa_nanf(NULL) != wwa_nanf(NULL));
        CHECK(wwa_nanf("") != wwa_nanf(""));
        CHECK(wwa_isgreaterf(2.0f, 1.0f) == 1);
        CHECK(wwa_isgreaterf(1.0f, 2.0f) == 0);
        CHECK(wwa_isgreaterf(0.0f / 0.0f, 1.0f) == 0);
        CHECK(wwa_isgreaterf(1.0f, 0.0f / 0.0f) == 0);
        CHECK(wwa_isgreaterequalf(1.0f, 1.0f) == 1);
        CHECK(wwa_isgreaterequalf(0.0f / 0.0f, 1.0f) == 0);
        CHECK(wwa_islessf(1.0f, 2.0f) == 1);
        CHECK(wwa_islessf(2.0f, 1.0f) == 0);
        CHECK(wwa_islessf(0.0f / 0.0f, 2.0f) == 0);
        CHECK(wwa_islessequalf(1.0f, 1.0f) == 1);
        CHECK(wwa_islessequalf(0.0f / 0.0f, 2.0f) == 0);
        CHECK(wwa_islessgreaterf(1.0f, 2.0f) == 1);
        CHECK(wwa_islessgreaterf(1.0f, 1.0f) == 0);
        CHECK(wwa_islessgreaterf(0.0f / 0.0f, 1.0f) == 0);
        CHECK(wwa_isunorderedf(0.0f / 0.0f, 1.0f) == 1);
        CHECK(wwa_isunorderedf(1.0f, 0.0f / 0.0f) == 1);
        CHECK(wwa_isunorderedf(1.0f, 2.0f) == 0);
        CHECK_F32(wwa_lgammaf(1.0f), 0.0f, 1e-6f);
        CHECK_F32(wwa_lgammaf(3.0f), 0.6931472f, 1e-6f);
        CHECK_F32(wwa_lgammaf(0.5f), 0.5723649f, 1e-6f);
        CHECK(wwa_isinff(wwa_lgammaf(0.0f)) == 1);
        CHECK(wwa_isinff(wwa_lgammaf(-1.0f)) == 1);
        CHECK_F32(wwa_tgammaf(5.0f), 24.0f, 1e-5f);
        CHECK_F32(wwa_tgammaf(0.5f), 1.7724538f, 1e-6f);
        CHECK(wwa_tgammaf(-0.5f) < 0.0f);
        CHECK(wwa_tgammaf(-1.5f) > 0.0f);
        CHECK(wwa_isinff(wwa_tgammaf(0.0f)) == 1);
        CHECK(wwa_isinff(wwa_hypotf(wwa_ldexpf(1.0f, 2000), 0.0f / 0.0f)) == 1);
        CHECK_F32(wwa_cabsf((wwa_complexf_t){3.0f, 4.0f}), 5.0f, 1e-6f);
        CHECK_F32(wwa_cabsf((wwa_complexf_t){-3.0f, -4.0f}), 5.0f, 1e-6f);
        CHECK_F32(wwa_cabsf((wwa_complexf_t){1.0e30f, 1.0e30f}), 1.4142135e30f, 1e-6f);
        CHECK_F32(wwa_cargf((wwa_complexf_t){1.0f, 1.0f}), 0.78539816f, 1e-6f);
        CHECK_F32(wwa_cargf((wwa_complexf_t){0.0f, -1.0f}), -1.5707963f, 1e-6f);
        CHECK(wwa_crealf((wwa_complexf_t){2.5f, -3.5f}) == 2.5f);
        CHECK(wwa_cimagf((wwa_complexf_t){2.5f, -3.5f}) == -3.5f);
        CHECK(wwa_cimagf(wwa_conjf((wwa_complexf_t){2.5f, -3.5f})) == 3.5f);
        CHECK(wwa_signbitf(wwa_conjf((wwa_complexf_t){1.0f, 0.0f}).im) == 1);
        CHECK(wwa_isinff(wwa_cprojf((wwa_complexf_t){-wwa_ldexpf(1.0f, 2000), 2.0f}).re) == 1);
        CHECK(wwa_cprojf((wwa_complexf_t){2.0f, 3.0f}).re == 2.0f);
        CHECK(wwa_cprojf((wwa_complexf_t){2.0f, 3.0f}).im == 3.0f);
        CHECK(wwa_signbitf(wwa_cprojf((wwa_complexf_t){1.0f, -1.0e30f}).im) == 1);
    }
}

static void test_math_double(void) {
    real64_t ip;
    i32 e;
    CHECK(wwa_fabs(-3.5) == 3.5);
    CHECK(wwa_fabs(3.5) == 3.5);
    CHECK(wwa_trunc(2.9) == 2.0);
    CHECK(wwa_trunc(-2.9) == -2.0);
    CHECK(wwa_trunc(0.0) == 0.0);
    CHECK(wwa_floor(2.7) == 2.0);
    CHECK(wwa_floor(-2.7) == -3.0);
    CHECK(wwa_ceil(2.1) == 3.0);
    CHECK(wwa_ceil(-2.1) == -2.0);
    CHECK(wwa_round(2.5) == 3.0);
    CHECK(wwa_round(-2.5) == -3.0);
    CHECK(wwa_copysign(3.0, -1.0) == -3.0);
    CHECK(wwa_copysign(-3.0, 1.0) == 3.0);
    CHECK(wwa_signbit(-0.0) == 1);
    CHECK(wwa_signbit(0.0) == 0);
    CHECK(wwa_fmin(3.0, 5.0) == 3.0);
    CHECK(wwa_fmax(3.0, 5.0) == 5.0);
    CHECK(wwa_fmin(1.0, 0.0 / 0.0) == 1.0);
    CHECK(wwa_fmax(1.0, 0.0 / 0.0) == 1.0);
    CHECK(wwa_signbit(wwa_fmin(-0.0, 0.0)) == 1);
    CHECK(wwa_signbit(wwa_fmax(-0.0, 0.0)) == 0);
    CHECK_F64(wwa_modf(3.75, &ip), 0.75, 0.0);
    CHECK(ip == 3.0);
    CHECK_F64(wwa_modf(-3.75, &ip), -0.75, 0.0);
    CHECK(ip == -3.0);
    CHECK_F64(wwa_frexp(6.0, &e), 0.75, 0.0);
    CHECK(e == 3);
    CHECK_F64(wwa_frexp(1.0, &e), 0.5, 0.0);
    CHECK(e == 1);
    CHECK_F64(wwa_frexp(4.9406564584124654e-324, &e), 0.5, 0.0);
    CHECK(e == -1073);
    {
        real64_t m = wwa_frexp(12345.6789012345, &e);
        CHECK_F64(wwa_ldexp(m, e), 12345.6789012345, 1e-12);
    }
    CHECK(wwa_ldexp(1.0, 0) == 1.0);
    CHECK(wwa_ldexp(1.5, 10) == 1536.0);
    CHECK(wwa_ldexp(1.0, -10) == 0.0009765625);
    CHECK(wwa_ldexp(1.0, -1074) == 4.9406564584124654e-324);
    CHECK(wwa_ldexp(1.0, -1075) == 0.0);
    CHECK(wwa_ldexp(3.0, 2000) > 1.7976931348623157e308);
    CHECK(wwa_ldexp(-3.0, 2000) < -1.7976931348623157e308);
    CHECK(wwa_ldexp(1.0, -2000) == 0.0);
    CHECK(wwa_scalbn(1.5, 10) == 1536.0);
    CHECK(wwa_scalbln(2.0, 2000000000ll) > 1.7976931348623157e308);
    CHECK_F64(wwa_sqrt(4.0), 2.0, 1e-13);
    CHECK_F64(wwa_sqrt(2.0) * wwa_sqrt(2.0), 2.0, 1e-13);
    CHECK_F64(wwa_sqrt(1.0e300), 1.0e150, 1e-13);
    CHECK_F64(wwa_sqrt(1.0e-300), 1.0e-150, 1e-13);
    CHECK_F64(wwa_sqrt(4.9406564584124654e-324), 2.2227587494850775e-162, 1e-13);
    CHECK(wwa_sqrt(0.0) == 0.0);
    CHECK(wwa_sqrt(-1.0) != wwa_sqrt(-1.0));
    CHECK(wwa_exp(0.0) == 1.0);
    CHECK_F64(wwa_exp(1.0), WWA_E, 1e-13);
    CHECK_F64(wwa_exp(-1.0), 0.36787944117144233, 1e-13);
    CHECK_F64(wwa_exp(0.5), 1.6487212707001282, 1e-13);
    CHECK_F64(wwa_exp(10.0), 22026.465794806718, 1e-12);
    CHECK(wwa_exp(710.0) > 1.7976931348623157e308);
    CHECK(wwa_exp(-1000.0) == 0.0);
    CHECK(wwa_log(1.0) == 0.0);
    CHECK(wwa_log(0.0) < -1.7976931348623157e308);
    CHECK(wwa_log(-1.0) != wwa_log(-1.0));
    CHECK_F64(wwa_log(2.0), 0.6931471805599453, 1e-14);
    CHECK_F64(wwa_log(WWA_E), 1.0, 1e-14);
    CHECK_F64(wwa_log(1.0e300), 690.7755278982137, 1e-12);
    CHECK_F64(wwa_log(1.0e-300), -690.7755278982137, 1e-12);
    CHECK_F64(wwa_pow(2.0, 10.0), 1024.0, 1e-13);
    CHECK_F64(wwa_pow(4.0, 0.5), 2.0, 1e-13);
    CHECK(wwa_pow(2.0, 0.0) == 1.0);
    CHECK(wwa_pow(0.0, 0.0) == 1.0);
    CHECK(wwa_pow(0.0, -1.0) > 1.7976931348623157e308);
    CHECK(wwa_pow(-2.0, 3.0) != wwa_pow(-2.0, 3.0));
    CHECK(wwa_exp2(0.0) == 1.0);
    CHECK(wwa_exp2(10.0) == 1024.0);
    CHECK(wwa_exp2(-10.0) == 0.0009765625);
    CHECK_F64(wwa_exp2(0.5), 1.4142135623730951, 1e-14);
    CHECK_F64(wwa_exp2(-0.5), 0.7071067811865476, 1e-14);
    CHECK_F64(wwa_exp2(1023.0), 8.98846567431158e307, 1e-14);
    CHECK(wwa_exp2(1024.0) > 1.7976931348623157e308);
    CHECK(wwa_exp2(-1074.0) == 4.9406564584124654e-324);
    CHECK(wwa_log2(1.0) == 0.0);
    CHECK_F64(wwa_log2(8.0), 3.0, 1e-14);
    CHECK_F64(wwa_log2(1024.0), 10.0, 1e-14);
    CHECK_F64(wwa_log2(0.5), -1.0, 1e-14);
    CHECK_F64(wwa_log2(3.0), 1.584962500721156, 1e-13);
    CHECK(wwa_log10(1.0) == 0.0);
    CHECK_F64(wwa_log10(1000.0), 3.0, 1e-13);
    CHECK_F64(wwa_log10(0.001), -3.0, 1e-13);
    CHECK_F64(wwa_log10(2.0), 0.3010299956639812, 1e-14);
    CHECK_F64(wwa_fmod(5.5, 2.0), 1.5, 1e-13);
    CHECK_F64(wwa_fmod(-5.5, 2.0), -1.5, 1e-13);
    CHECK(wwa_fmod(5.5, 0.0) != wwa_fmod(5.5, 0.0));
    CHECK(wwa_sin(0.0) == 0.0);
    CHECK_F64(wwa_sin(1.0), 0.8414709848078965, 1e-14);
    CHECK_F64(wwa_sin(WWA_PI_2), 1.0, 1e-13);
    CHECK_F64(wwa_sin(-1.0), -0.8414709848078965, 1e-14);
    CHECK_F64(wwa_cos(0.0), 1.0, 0.0);
    CHECK_F64(wwa_cos(1.0), 0.5403023058681398, 1e-14);
    CHECK_F64(wwa_cos(WWA_PI), -1.0, 1e-13);
    CHECK_F64(wwa_tan(WWA_PI_4), 1.0, 1e-13);
    CHECK_F64(wwa_tan(1.0), 1.5574077246549023, 1e-13);
    CHECK_F64(wwa_atan(0.0), 0.0, 0.0);
    CHECK_F64(wwa_atan(1.0), WWA_PI_4, 1e-13);
    CHECK_F64(wwa_atan(-1.0), -WWA_PI_4, 1e-13);
    CHECK_F64(wwa_atan(0.5), 0.4636476090008061, 1e-14);
    CHECK_F64(wwa_atan(2.0), 1.1071487177940904, 1e-13);
    CHECK_F64(wwa_atan(1000.0), 1.569796327128230, 1e-13);
    CHECK_F64(wwa_atan2(1.0, 1.0), WWA_PI_4, 1e-13);
    CHECK_F64(wwa_atan2(-1.0, -1.0), -2.356194490192345, 1e-13);
    CHECK_F64(wwa_atan2(1.0, 0.0), WWA_PI_2, 1e-13);
    CHECK_F64(wwa_atan2(-1.0, 0.0), -WWA_PI_2, 1e-13);
    CHECK(wwa_atan2(0.0, 1.0) == 0.0);
    CHECK_F64(wwa_atan2(0.0, -1.0), WWA_PI, 1e-13);
    CHECK(wwa_sinh(0.0) == 0.0);
    CHECK_F64(wwa_sinh(1.0), 1.1752011936438014, 1e-14);
    CHECK_F64(wwa_sinh(-1.0), -1.1752011936438014, 1e-14);
    CHECK_F64(wwa_sinh(0.5), 0.5210953054937474, 1e-14);
    CHECK_F64(wwa_sinh(2.0), 3.626860407847019, 1e-13);
    CHECK(wwa_sinh(710.0) > 1.7976931348623157e308);
    CHECK_F64(wwa_cosh(0.0), 1.0, 0.0);
    CHECK_F64(wwa_cosh(1.0), 1.5430806348152437, 1e-14);
    CHECK_F64(wwa_cosh(-1.0), 1.5430806348152437, 1e-14);
    CHECK_F64(wwa_cosh(0.5), 1.1276259652063807, 1e-14);
    CHECK_F64(wwa_cosh(2.0), 3.7621956910836314, 1e-13);
    CHECK(wwa_cosh(710.0) > 1.7976931348623157e308);
    CHECK(wwa_tanh(0.0) == 0.0);
    CHECK_F64(wwa_tanh(1.0), 0.7615941559557649, 1e-14);
    CHECK_F64(wwa_tanh(-1.0), -0.7615941559557649, 1e-14);
    CHECK_F64(wwa_tanh(0.5), 0.4621171572600098, 1e-14);
    CHECK_F64(wwa_tanh(2.0), 0.9640275800758169, 1e-13);
    CHECK(wwa_tanh(1000.0) == 1.0);
    CHECK(wwa_tanh(-1000.0) == -1.0);
    CHECK(wwa_asin(0.0) == 0.0);
    CHECK_F64(wwa_asin(0.5), WWA_PI / 6.0, 1e-14);
    CHECK_F64(wwa_asin(1.0), WWA_PI_2, 1e-13);
    CHECK_F64(wwa_asin(-1.0), -WWA_PI_2, 1e-13);
    CHECK_F64(wwa_asin(-0.5), -WWA_PI / 6.0, 1e-14);
    CHECK(wwa_asin(2.0) != wwa_asin(2.0));
    CHECK_F64(wwa_acos(0.5), WWA_PI / 3.0, 1e-14);
    CHECK_F64(wwa_acos(-0.5), 2.0 * WWA_PI / 3.0, 1e-14);
    CHECK_F64(wwa_acos(0.0), WWA_PI_2, 1e-14);
    CHECK_F64(wwa_acos(1.0), 0.0, 0.0);
    CHECK_F64(wwa_acos(-1.0), WWA_PI, 1e-13);
    CHECK(wwa_acos(2.0) != wwa_acos(2.0));
    CHECK_F64(wwa_erf(0.5), 0.5204998778130465, 1e-14);
    CHECK_F64(wwa_erf(1.0), 0.8427007929497149, 1e-14);
    CHECK_F64(wwa_erf(-1.0), -0.8427007929497149, 1e-14);
    CHECK_F64(wwa_erf(1.5), 0.9661051464753107, 1e-13);
    CHECK_F64(wwa_erf(2.0), 0.9953222650189527, 1e-13);
    CHECK_F64(wwa_erf(3.0), 0.9999779095030014, 1e-12);
    CHECK_F64(wwa_erf(4.0), 0.9999999845827421, 1e-12);
    CHECK(wwa_erf(8.0) == 1.0);
    CHECK_F64(wwa_asinh(1.0), 0.8813735870195430, 1e-14);
    CHECK_F64(wwa_asinh(-1.0), -0.8813735870195430, 1e-14);
    CHECK_F64(wwa_asinh(2.0), 1.4436354751788103, 1e-14);
    CHECK_F64(wwa_asinh(1.0e10), 23.718998110500529, 1e-12);
    CHECK_F64(wwa_acosh(1.0), 0.0, 0.0);
    CHECK_F64(wwa_acosh(2.0), 1.3169578969248166, 1e-14);
    CHECK_F64(wwa_acosh(10.0), 2.993222846126381, 1e-13);
    CHECK(wwa_acosh(0.5) != wwa_acosh(0.5));
    CHECK(wwa_atanh(0.0) == 0.0);
    CHECK_F64(wwa_atanh(0.5), 0.5493061443340548, 1e-14);
    CHECK_F64(wwa_atanh(-0.5), -0.5493061443340548, 1e-14);
    CHECK_F64(wwa_atanh(0.9), 1.4722194895832204, 1e-13);
    CHECK(wwa_atanh(1.0) > 1.7976931348623157e308);
    CHECK(wwa_atanh(-1.0) < -1.7976931348623157e308);
    CHECK(wwa_atanh(2.0) != wwa_atanh(2.0));
    CHECK(wwa_hypot(3.0, 4.0) == 5.0);
    CHECK_F64(wwa_hypot(1.0e308, 1.0e308), 1.4142135623730951e308, 1e-12);
    CHECK_F64(wwa_hypot(1.0e-300, 1.0e-300), 1.4142135623730951e-300, 1e-12);
    CHECK_F64(wwa_hypot(1.0e308, 0.0), 1.0e308, 1e-12);
    CHECK_F64(wwa_remainder(5.0, 2.0), 1.0, 0.0);
    CHECK_F64(wwa_remainder(7.0, 2.0), -1.0, 0.0);
    CHECK_F64(wwa_remainder(5.5, 2.0), -0.5, 0.0);
    CHECK_F64(wwa_remainder(-5.5, 2.0), 0.5, 0.0);
    CHECK(wwa_remainder(4.0, 2.0) == 0.0);
    CHECK(wwa_signbit(wwa_remainder(-4.0, 2.0)) == 1);
    CHECK_F64(wwa_remainder(1.5, 0.5), 0.0, 0.0);
    CHECK(wwa_remainder(5.0, 0.0) != wwa_remainder(5.0, 0.0));
    {
        i32 i;
        for (i = 0; i < 4000; i++) {
            real64_t x = ((real64_t)(i % 4000) - 2000.0) * 0.0007;
            CHECK_F64(wwa_cosh(x) * wwa_cosh(x) - wwa_sinh(x) * wwa_sinh(x), 1.0, 1e-11);
            CHECK_F64(wwa_tanh(x) - wwa_sinh(x) / wwa_cosh(x), 0.0, 1e-12);
            CHECK_F64(wwa_asin(wwa_sin(x)), x, 1e-12);
            CHECK_F64(wwa_acos(wwa_cos(x)), wwa_fabs(x), 1e-12);
            CHECK_F64(wwa_atanh(x * 0.1), 0.5 * wwa_log((1.0 + x * 0.1) / (1.0 - x * 0.1)), 1e-13);
            CHECK_F64(wwa_asinh(x), wwa_log(x + wwa_sqrt(x * x + 1.0)), 1e-13);
        }
    }
    {
        i32 i;
        for (i = 0; i < 4000; i++) {
            real64_t x = ((real64_t)(i % 4000) - 2000.0) * 0.05;
            CHECK_F64(wwa_sin(x) * wwa_sin(x) + wwa_cos(x) * wwa_cos(x), 1.0, 1e-12);
            CHECK_F64(wwa_sin(-x), -wwa_sin(x), 1e-13);
        }
        for (i = 0; i < 4000; i++) {
            real64_t x = ((real64_t)(i % 4000) - 2000.0) * 0.0007;
            CHECK_F64(wwa_exp(x) * wwa_exp(-x), 1.0, 1e-12);
            CHECK_F64(wwa_atan(wwa_tan(x)), x, 1e-12);
        }
    }
    CHECK(wwa_expm1(0.0) == 0.0);
    CHECK_F64(wwa_expm1(1.0), 1.7182818284590453, 1e-14);
    CHECK_F64(wwa_expm1(-1.0), -0.6321205588285577, 1e-14);
    CHECK_F64(wwa_expm1(0.5), 0.6487212707001282, 1e-14);
    CHECK_F64(wwa_expm1(1.0e-8), 1.0000000050000000e-8, 1e-17);
    CHECK_F64(wwa_expm1(10.0), 22025.465794806718, 1e-11);
    CHECK(wwa_expm1(-1000.0) == -1.0);
    CHECK(wwa_expm1(710.0) > 1.7976931348623157e308);
    CHECK(wwa_log1p(0.0) == 0.0);
    CHECK_F64(wwa_log1p(1.0), 0.6931471805599453, 1e-14);
    CHECK_F64(wwa_log1p(1.7182818284590453), 1.0, 1e-14);
    CHECK_F64(wwa_log1p(1.0e-10), 9.9999999995000000e-11, 1e-21);
    CHECK_F64(wwa_log1p(-0.5), -0.6931471805599453, 1e-14);
    CHECK_F64(wwa_log1p(1.0e300), 690.7755278982137, 1e-12);
    CHECK(wwa_log1p(-1.0) < -1.7976931348623157e308);
    CHECK(wwa_log1p(-2.0) != wwa_log1p(-2.0));
    CHECK(wwa_cbrt(0.0) == 0.0);
    CHECK(wwa_cbrt(8.0) == 2.0);
    CHECK(wwa_cbrt(-8.0) == -2.0);
    CHECK(wwa_cbrt(27.0) == 3.0);
    CHECK_F64(wwa_cbrt(2.0), 1.2599210498948732, 1e-14);
    CHECK_F64(wwa_cbrt(-2.0), -1.2599210498948732, 1e-14);
    CHECK_F64(wwa_cbrt(1.0e300), 1.0e100, 1e-11);
    CHECK_F64(wwa_cbrt(1.0e-300), 1.0e-100, 1e-11);
    CHECK_F64(wwa_cbrt(4.9406564584124654e-324) * wwa_cbrt(4.9406564584124654e-324) *
              wwa_cbrt(4.9406564584124654e-324), 4.9406564584124654e-324, 0.0);
    CHECK(wwa_fma(1.5, 2.5, 0.25) == 4.0);
    CHECK(wwa_fma(2.5, 4.5, 1.25) == 12.5);
    CHECK(wwa_fma(0.25, 0.5, 0.125) == 0.25);
    CHECK(wwa_fma(3.0, 3.0, 1.0) == 10.0);
    CHECK(wwa_fma(-3.0, 3.0, 1.0) == -8.0);
    CHECK(wwa_fma(2.0, 2.0, -4.0) == 0.0);
    CHECK(wwa_fma(0.0, 0.0, 1.5) == 1.5);
    CHECK_F64(wwa_fma(1.0e200, 1.0e-100, 5.0), 1.0e100, 1e-12);
    CHECK(wwa_fma(1.0e308, 1.0e308, -1.0e308) > 1.7976931348623157e308);
    CHECK(wwa_fma(0.0, 1.0e308 * 1.0e308, 1.0) != wwa_fma(0.0, 1.0e308 * 1.0e308, 1.0));
    CHECK(wwa_fma(1.0e308 * 1.0e308, 1.0, -1.0e308 * 1.0e308) !=
          wwa_fma(1.0e308 * 1.0e308, 1.0, -1.0e308 * 1.0e308));
    CHECK(wwa_fma(1.0e308 * 1.0e308, 1.0, 2.0) == 1.0e308 * 1.0e308);
    CHECK(wwa_erfc(0.0) == 1.0);
    CHECK_F64(wwa_erfc(1.0), 0.15729920705028513, 1e-14);
    CHECK_F64(wwa_erfc(-1.0), 1.8427007929497149, 1e-14);
    CHECK_F64(wwa_erfc(2.0), 0.004677734981047266, 1e-15);
    CHECK_F64(wwa_erfc(3.0), 2.2090496998585448e-5, 1e-16);
    CHECK_F64(wwa_erfc(4.0), 1.5417257900280019e-8, 1e-18);
    CHECK_F64(wwa_erfc(8.0), 1.1224297172982926e-29, 1e-38);
    CHECK(wwa_erfc(30.0) == 0.0);
    CHECK(wwa_erfc(-30.0) == 2.0);
    CHECK(wwa_nextafter(1.0, 2.0) == 1.0000000000000002);
    CHECK(wwa_nextafter(1.0, 0.0) == 0.9999999999999999);
    CHECK(wwa_nextafter(0.0, 1.0) == 4.9406564584124654e-324);
    CHECK(wwa_nextafter(0.0, -1.0) == -4.9406564584124654e-324);
    CHECK(wwa_nextafter(-0.0, 1.0) == 4.9406564584124654e-324);
    CHECK(wwa_nextafter(-0.0, -1.0) == -4.9406564584124654e-324);
    CHECK(wwa_nextafter(1.7976931348623157e308, 0.0) == 1.7976931348623155e308);
    CHECK(wwa_nextafter(1.7976931348623157e308, wwa_ldexp(1.0, 2000)) > 1.7976931348623157e308);
    CHECK(wwa_nextafter(-1.7976931348623157e308, 0.0) == -1.7976931348623155e308);
    CHECK(wwa_nextafter(-1.7976931348623157e308, -wwa_ldexp(1.0, 2000)) < -1.7976931348623157e308);
    CHECK(wwa_nextafter(1.0, 1.0) == 1.0);
    {
        i32 i;
        for (i = 0; i < 4000; i++) {
            real64_t x = ((real64_t)(i % 4000) - 2000.0) * 0.0007;
            CHECK_F64(wwa_expm1(x) + 1.0, wwa_exp(x), 1e-13);
            if (x > -1.3 && x < 1.3) {
                CHECK_F64(wwa_log1p(x), wwa_log(1.0 + x), 1e-13);
            }
            CHECK_F64(wwa_cbrt(x) * wwa_cbrt(x) * wwa_cbrt(x), x, 1e-12);
            CHECK_F64(wwa_erfc(x) + wwa_erfc(-x), 2.0, 1e-13);
        }
    }
    CHECK(wwa_rint(2.5) == 2.0);
    CHECK(wwa_rint(3.5) == 4.0);
    CHECK(wwa_rint(-2.5) == -2.0);
    CHECK(wwa_rint(-3.5) == -4.0);
    CHECK(wwa_rint(0.5) == 0.0);
    CHECK(wwa_rint(1.5) == 2.0);
    CHECK(wwa_rint(0.49999999999999994) == 0.0);
    CHECK(wwa_rint(2.4999999999999996) == 2.0);
    CHECK(wwa_rint(9007199254740992.0) == 9007199254740992.0);
    CHECK(wwa_rint(1.0e300) == 1.0e300);
    CHECK(wwa_rint(1.7976931348623157e308) == 1.7976931348623157e308);
    CHECK(wwa_rint(wwa_ldexp(1.0, 2000)) == wwa_ldexp(1.0, 2000));
    CHECK(wwa_rint(0.0 / 0.0) != wwa_rint(0.0 / 0.0));
    CHECK(wwa_rint(-0.5) == -0.0);
    CHECK(wwa_signbit(wwa_rint(-0.5)) == 1);
    CHECK(wwa_nearbyint(2.5) == 2.0);
    CHECK(wwa_nearbyint(-3.5) == -4.0);
    CHECK(wwa_lrint(2.5) == 2);
    CHECK(wwa_lrint(-2.5) == -2);
    CHECK(wwa_lrint(3.5) == 4);
    CHECK(wwa_lrint(1.9) == 2);
    CHECK(wwa_lrint(-1.9) == -2);
    CHECK(wwa_lrint(2147483647.0) == 2147483647);
    CHECK(wwa_lrint(2147483648.0) == -2147483647 - 1);
    CHECK(wwa_lrint(0.0 / 0.0) == -2147483647 - 1);
    CHECK(wwa_llrint(2.5) == 2);
    CHECK(wwa_llrint(-3.5) == -4);
    CHECK(wwa_llrint(9007199254740992.0) == 9007199254740992ll);
    CHECK(wwa_llrint(-9007199254740992.0) == -9007199254740992ll);
    CHECK(wwa_llrint(9.2233720368547758e18) == -9223372036854775807ll - 1);
    CHECK(wwa_llrint(1.0e300) == -9223372036854775807ll - 1);
    CHECK(wwa_lround(2.5) == 3);
    CHECK(wwa_lround(-2.5) == -3);
    CHECK(wwa_lround(2.4) == 2);
    CHECK(wwa_lround(3.5) == 4);
    CHECK(wwa_lround(2147483648.0) == -2147483647 - 1);
    CHECK(wwa_llround(2.5) == 3);
    CHECK(wwa_llround(-2.5) == -3);
    CHECK(wwa_llround(1.0e300) == -9223372036854775807ll - 1);
    CHECK(wwa_fdim(3.0, 2.0) == 1.0);
    CHECK(wwa_fdim(2.0, 3.0) == 0.0);
    CHECK(wwa_fdim(5.0, 5.0) == 0.0);
    CHECK(wwa_fdim(0.5, 0.25) == 0.25);
    CHECK(wwa_fdim(0.0 / 0.0, 1.0) != wwa_fdim(0.0 / 0.0, 1.0));
    CHECK(wwa_fdim(1.0, 0.0 / 0.0) != wwa_fdim(1.0, 0.0 / 0.0));
    CHECK(wwa_fdim(1.7976931348623157e308, -1.7976931348623157e308) >
          1.7976931348623157e308);
    CHECK(wwa_fdim(-1.7976931348623157e308, 1.7976931348623157e308) == 0.0);
    CHECK(wwa_signbit(wwa_fdim(-0.0, 0.0)) == 0);
    CHECK(wwa_ilogb(1.0) == 0);
    CHECK(wwa_ilogb(2.0) == 1);
    CHECK(wwa_ilogb(0.5) == -1);
    CHECK(wwa_ilogb(0.25) == -2);
    CHECK(wwa_ilogb(1024.0) == 10);
    CHECK(wwa_ilogb(1.7976931348623157e308) == 1023);
    CHECK(wwa_ilogb(2.2250738585072014e-308) == -1022);
    CHECK(wwa_ilogb(4.9406564584124654e-324) == -1074);
    CHECK(wwa_ilogb(0.0) == -2147483647 - 1);
    CHECK(wwa_ilogb(-0.0) == -2147483647 - 1);
    CHECK(wwa_ilogb(wwa_ldexp(1.0, 2000)) == 2147483647);
    CHECK(wwa_ilogb(0.0 / 0.0) == 2147483647);
    CHECK(wwa_logb(1.0) == 0.0);
    CHECK(wwa_logb(2.0) == 1.0);
    CHECK(wwa_logb(1024.0) == 10.0);
    CHECK(wwa_logb(0.0) < -1.7976931348623157e308);
    CHECK(wwa_logb(wwa_ldexp(1.0, 2000)) > 1.7976931348623157e308);
    CHECK(wwa_logb(0.0 / 0.0) != wwa_logb(0.0 / 0.0));
    CHECK(wwa_scalbn(1.5, 10) == 1536.0);
    CHECK(wwa_scalbn(1.0, -1074) == 4.9406564584124654e-324);
    CHECK(wwa_scalbn(1.0, 1024) > 1.7976931348623157e308);
    CHECK(wwa_scalbn(0.0, 1000) == 0.0);
    CHECK(wwa_scalbn(wwa_ldexp(1.0, 2000), -1000) == wwa_ldexp(1.0, 2000));
    CHECK(wwa_scalbln(2.0, 2000000000ll) > 1.7976931348623157e308);
    CHECK(wwa_scalbln(1.0, -2000000000ll) == 0.0);
    CHECK(wwa_scalbln(1.5, 3) == 12.0);
    {
        i32 i;
        for (i = 0; i < 4000; i++) {
            real64_t x = ((real64_t)(i % 4000) - 2000.0) * 0.5;
            CHECK(wwa_rint(x) == -wwa_rint(-x));
            CHECK(wwa_fdim(x, x * 0.25) == (x > x * 0.25 ? x - x * 0.25 : 0.0));
            if (x != 0.0) CHECK(wwa_logb(x) == (real64_t)wwa_ilogb(x));
        }
    }
    CHECK(wwa_fpclassify(0.0) == 2);
    CHECK(wwa_fpclassify(-0.0) == 2);
    CHECK(wwa_fpclassify(1.0) == 4);
    CHECK(wwa_fpclassify(-1.0) == 4);
    CHECK(wwa_fpclassify(4.9406564584124654e-324) == 3);
    CHECK(wwa_fpclassify(2.2250738585072014e-308) == 4);
    CHECK(wwa_fpclassify(1.7976931348623157e308) == 4);
    CHECK(wwa_fpclassify(wwa_ldexp(1.0, 2000)) == 1);
    CHECK(wwa_fpclassify(0.0 / 0.0) == 0);
    CHECK(wwa_isnan(0.0 / 0.0) == 1);
    CHECK(wwa_isnan(1.0) == 0);
    CHECK(wwa_isnan(wwa_ldexp(1.0, 2000)) == 0);
    CHECK(wwa_isinf(wwa_ldexp(1.0, 2000)) == 1);
    CHECK(wwa_isinf(-wwa_ldexp(1.0, 2000)) == 1);
    CHECK(wwa_isinf(1.0) == 0);
    CHECK(wwa_isfinite(0.0) == 1);
    CHECK(wwa_isfinite(1.7976931348623157e308) == 1);
    CHECK(wwa_isfinite(wwa_ldexp(1.0, 2000)) == 0);
    CHECK(wwa_isfinite(0.0 / 0.0) == 0);
    CHECK(wwa_isnormal(1.0) == 1);
    CHECK(wwa_isnormal(4.9406564584124654e-324) == 0);
    CHECK(wwa_isnormal(0.0) == 0);
    CHECK(wwa_isnormal(wwa_ldexp(1.0, 2000)) == 0);
    CHECK(wwa_nan(NULL) != wwa_nan(NULL));
    CHECK(wwa_nan("") != wwa_nan(""));
    CHECK(wwa_isgreater(2.0, 1.0) == 1);
    CHECK(wwa_isgreater(1.0, 2.0) == 0);
    CHECK(wwa_isgreater(0.0 / 0.0, 1.0) == 0);
    CHECK(wwa_isgreater(1.0, 0.0 / 0.0) == 0);
    CHECK(wwa_isgreaterequal(1.0, 1.0) == 1);
    CHECK(wwa_isgreaterequal(0.0 / 0.0, 1.0) == 0);
    CHECK(wwa_isless(1.0, 2.0) == 1);
    CHECK(wwa_isless(2.0, 1.0) == 0);
    CHECK(wwa_isless(0.0 / 0.0, 2.0) == 0);
    CHECK(wwa_islessequal(1.0, 1.0) == 1);
    CHECK(wwa_islessequal(0.0 / 0.0, 2.0) == 0);
    CHECK(wwa_islessgreater(1.0, 2.0) == 1);
    CHECK(wwa_islessgreater(1.0, 1.0) == 0);
    CHECK(wwa_islessgreater(0.0 / 0.0, 1.0) == 0);
    CHECK(wwa_isunordered(0.0 / 0.0, 1.0) == 1);
    CHECK(wwa_isunordered(1.0, 0.0 / 0.0) == 1);
    CHECK(wwa_isunordered(1.0, 2.0) == 0);
    CHECK_F64(wwa_lgamma(1.0), 0.0, 1e-12);
    CHECK_F64(wwa_lgamma(2.0), 0.0, 1e-12);
    CHECK_F64(wwa_lgamma(3.0), 0.6931471805599453, 1e-12);
    CHECK_F64(wwa_lgamma(5.0), 3.1780538303479458, 1e-12);
    CHECK_F64(wwa_lgamma(7.0), 6.579251212010101, 1e-12);
    CHECK_F64(wwa_lgamma(8.0), 8.525161361065415, 1e-12);
    CHECK_F64(wwa_lgamma(10.0), 12.80182748008147, 1e-11);
    CHECK_F64(wwa_lgamma(100.0), 359.1342053695754, 1e-11);
    CHECK_F64(wwa_lgamma(0.5), 0.5723649429247001, 1e-12);
    CHECK_F64(wwa_lgamma(1.5), -0.1207822376352452, 1e-12);
    CHECK_F64(wwa_lgamma(0.25), 1.2880225246980775, 1e-11);
    CHECK_F64(wwa_lgamma(0.75), 0.2032809514312954, 1e-11);
    CHECK_F64(wwa_lgamma(-0.5), 1.2655121234846454, 1e-12);
    CHECK_F64(wwa_lgamma(-1.5), 0.8600470153764814, 1e-12);
    CHECK_F64(wwa_lgamma(-2.5), -0.0562437164976743, 1e-12);
    CHECK_F64(wwa_lgamma(-0.25), 1.589575312551186, 1e-11);
    CHECK(wwa_isinf(wwa_lgamma(0.0)) == 1);
    CHECK(wwa_isinf(wwa_lgamma(-0.0)) == 1);
    CHECK(wwa_isinf(wwa_lgamma(-1.0)) == 1);
    CHECK(wwa_isinf(wwa_lgamma(-2.0)) == 1);
    CHECK(wwa_isinf(wwa_lgamma(wwa_ldexp(1.0, 2000))) == 1);
    CHECK_F64(wwa_tgamma(1.0), 1.0, 1e-12);
    CHECK_F64(wwa_tgamma(2.0), 1.0, 1e-12);
    CHECK_F64(wwa_tgamma(3.0), 2.0, 1e-12);
    CHECK_F64(wwa_tgamma(4.0), 6.0, 1e-12);
    CHECK_F64(wwa_tgamma(5.0), 24.0, 1e-12);
    CHECK_F64(wwa_tgamma(10.0), 362880.0, 1e-11);
    CHECK_F64(wwa_tgamma(0.5), 1.772453850905516, 1e-12);
    CHECK_F64(wwa_tgamma(-0.5), -3.544907701811032, 1e-12);
    CHECK_F64(wwa_tgamma(-1.5), 2.3632718012073547, 1e-12);
    CHECK_F64(wwa_tgamma(-2.5), -0.9453087204829419, 1e-12);
    CHECK_F64(wwa_tgamma(-0.25), -4.9016668098607104, 1e-12);
    CHECK_F64(wwa_tgamma(100.0), 9.33262154439441e155, 1e-11);
    CHECK(wwa_isinf(wwa_tgamma(172.0)) == 1);
    CHECK(wwa_tgamma(0.0) == wwa_ldexp(1.0, 2000));
    CHECK(wwa_tgamma(-1.0) == wwa_ldexp(1.0, 2000));
    CHECK(wwa_tgamma(-3.0) == wwa_ldexp(1.0, 2000));
    CHECK(wwa_tgamma(0.0 / 0.0) != wwa_tgamma(0.0 / 0.0));
    {
        i32 i;
        for (i = 0; i < 400; i++) {
            real64_t x = 0.25 + (real64_t)(i % 400) * 0.1;
            CHECK_F64(wwa_lgamma(x + 1.0), wwa_lgamma(x) + wwa_log(x), 1e-11);
            CHECK_F64(wwa_tgamma(x + 1.0), x * wwa_tgamma(x), 1e-10);
        }
for (i = 0; i < 100; i++) {
            real64_t x = 0.01 + (real64_t)(i % 100) * 0.0048;
            CHECK_F64(wwa_lgamma(x),
                      1.1447298858494002 - wwa_log(wwa_fabs(wwa_sin(WWA_PI * x)))
                          - wwa_lgamma(1.0 - x),
                      1e-11);
        }
    }
    CHECK_F64(wwa_cabs((wwa_complex_t){3.0, 4.0}), 5.0, 1e-12);
    CHECK_F64(wwa_cabs((wwa_complex_t){-3.0, -4.0}), 5.0, 1e-12);
    CHECK_F64(wwa_cabs((wwa_complex_t){1.0e200, 1.0e200}), 1.4142135623730951e200, 1e-12);
    CHECK(wwa_cabs((wwa_complex_t){1.7976931348623157e308, 1.0}) ==
          1.7976931348623157e308);
    CHECK(wwa_isinf(wwa_cabs((wwa_complex_t){wwa_ldexp(1.0, 2000), 1.0})) == 1);
    CHECK(wwa_isnan(wwa_cabs((wwa_complex_t){1.0, 0.0 / 0.0})) == 1);
    CHECK(wwa_isinf(wwa_cabs((wwa_complex_t){wwa_ldexp(1.0, 2000), 0.0 / 0.0})) == 1);
    CHECK_F64(wwa_carg((wwa_complex_t){1.0, 0.0}), 0.0, 1e-12);
    CHECK_F64(wwa_carg((wwa_complex_t){0.0, 1.0}), 1.5707963267948966, 1e-12);
    CHECK_F64(wwa_carg((wwa_complex_t){-1.0, 0.0}), 3.141592653589793, 1e-12);
    CHECK_F64(wwa_carg((wwa_complex_t){0.0, -1.0}), -1.5707963267948966, 1e-12);
    CHECK_F64(wwa_carg((wwa_complex_t){1.0, 1.0}), 0.7853981633974483, 1e-12);
    CHECK_F64(wwa_carg((wwa_complex_t){-1.0, -1.0}), -2.356194490192345, 1e-12);
    CHECK_F64(wwa_carg((wwa_complex_t){1.0e200, 1.0e200}), 0.7853981633974483, 1e-12);
    CHECK(wwa_creal((wwa_complex_t){2.5, -3.5}) == 2.5);
    CHECK(wwa_cimag((wwa_complex_t){2.5, -3.5}) == -3.5);
    CHECK(wwa_creal(wwa_conj((wwa_complex_t){2.5, -3.5})) == 2.5);
    CHECK(wwa_cimag(wwa_conj((wwa_complex_t){2.5, -3.5})) == 3.5);
    CHECK(wwa_signbit(wwa_conj((wwa_complex_t){1.0, 0.0}).im) == 1);
    CHECK(wwa_signbit(wwa_conj((wwa_complex_t){1.0, -0.0}).im) == 0);
    CHECK(wwa_isinf(wwa_cproj((wwa_complex_t){wwa_ldexp(1.0, 2000), 3.0}).re) == 1);
    CHECK(wwa_cproj((wwa_complex_t){wwa_ldexp(1.0, 2000), 3.0}).im == 0.0);
    CHECK(wwa_signbit(wwa_cproj((wwa_complex_t){5.0, -wwa_ldexp(1.0, 2000)}).im) == 1);
    CHECK(wwa_cproj((wwa_complex_t){2.0, 3.0}).re == 2.0);
    CHECK(wwa_cproj((wwa_complex_t){2.0, 3.0}).im == 3.0);
    CHECK(wwa_cproj((wwa_complex_t){0.0 / 0.0, wwa_ldexp(1.0, 2000)}).re ==
          wwa_ldexp(1.0, 2000));
    {
        i32 i;
        for (i = 0; i < 1000; i++) {
            real64_t x = ((real64_t)(i % 1000) - 500.0) * 0.37;
            real64_t y = ((real64_t)((i * 7) % 1000) - 500.0) * 0.31;
            CHECK_F64(wwa_cabs((wwa_complex_t){x, y}),
                      wwa_sqrt(x * x + y * y), 1e-13);
        }
    }
    {
        i32 i;
        for (i = 0; i < 4000; i++) {
            real64_t x = ((real64_t)(i % 4000) - 2000.0) * 0.0005;
            CHECK(wwa_isfinite(x) == 1);
            if (x != 0.0) {
                CHECK(wwa_isnormal(x) == 1);
                CHECK(wwa_fpclassify(x) == 4);
            }
        }
    }
}

static void test_float(void) {
    CHECK(wwa_f16_from_f32(1.0f) == 0x3C00);
    CHECK(wwa_f16_from_f32(-1.0f) == 0xBC00);
    CHECK(wwa_f16_from_f32(0.0f) == 0);
    CHECK(wwa_f16_to_f32(0x3C00) == 1.0f);
    CHECK(wwa_f16_to_f32(0x7BFF) == 65504.0f);
    CHECK(wwa_f16_from_f32(65504.0f) == 0x7BFF);
    CHECK(wwa_bf16_from_f32(1.0f) == 0x3F80);
    CHECK(wwa_bf16_to_f32(0x3F80) == 1.0f);
    {
        real32_t vals[] = {0.0f, 1.0f, -1.0f, 0.5f, 65504.0f, 1e-3f, -123.456f};
        u32 i;
        for (i = 0; i < ARRAY_COUNT(vals); i++) {
            u16 h = wwa_f16_from_f32(vals[i]);
            CHECK_F32(wwa_f16_to_f32(h), vals[i], 1e-3f);
        }
    }
    CHECK(wwa_fp8_e4m3_from_f32(448.0f) == 0x7E);
    CHECK(wwa_fp8_e4m3_from_f32(-448.0f) == 0xFE);
    CHECK(wwa_fp8_e4m3_from_f32(448.5f) == 0x7E);
    CHECK(wwa_fp8_e4m3_from_f32(1.0f) == 0x38);
    CHECK(wwa_fp8_e4m3_from_f32(-1.0f) == 0xB8);
    CHECK(wwa_fp8_e4m3_from_f32(0.0f) == 0);
    CHECK(wwa_fp8_e4m3_to_f32(0x7E) == 448.0f);
    CHECK(wwa_fp8_e4m3_to_f32(0x38) == 1.0f);
    CHECK(wwa_fp8_e4m3_to_f32(0x7F) != wwa_fp8_e4m3_to_f32(0x7F));
    CHECK(wwa_fp8_e4m3_from_f32(1.0e30f) == 0x7F);
    CHECK(wwa_fp8_e4m3_from_f32(-1.0e30f) == 0xFF);
    CHECK(wwa_fp8_e5m2_from_f32(1.0f) == 0x3C);
    CHECK(wwa_fp8_e5m2_from_f32(0.5f) == 0x38);
    CHECK(wwa_fp8_e5m2_from_f32(-1.0f) == 0xBC);
    CHECK(wwa_fp8_e5m2_from_f32(57344.0f) == 0x7B);
    CHECK(wwa_fp8_e5m2_from_f32(1.0e30f) == 0x7C);
    CHECK(wwa_fp8_e5m2_to_f32(0x7C) == wwa_fp8_e5m2_to_f32(0x7C) && wwa_fp8_e5m2_to_f32(0x7C) > 1.0e30f);
    CHECK(wwa_fp6_e3m2_from_f32(28.0f) == 0x1F);
    CHECK(wwa_fp6_e3m2_from_f32(-28.0f) == 0x3F);
    CHECK(wwa_fp6_e3m2_from_f32(1.0f) == 0x0C);
    CHECK(wwa_fp6_e3m2_from_f32(0.25f) == 0x04);
    CHECK(wwa_fp6_e3m2_to_f32(0x1F) == 28.0f);
    CHECK(wwa_fp6_e3m2_from_f32(1.0e30f) == 0x1F);
    CHECK(wwa_fp6_e2m3_from_f32(7.5f) == 0x1F);
    CHECK(wwa_fp6_e2m3_from_f32(-7.5f) == 0x3F);
    CHECK(wwa_fp6_e2m3_from_f32(0.125f) == 0x01);
    CHECK(wwa_fp6_e2m3_to_f32(0x1F) == 7.5f);
    CHECK(wwa_fp6_e2m3_to_f32(0x3F) == -7.5f);
    CHECK(wwa_fp6_e2m3_from_f32(1.0e30f) == 0x1F);
    CHECK(wwa_fp4_e2m1_from_f32(0.0f) == 0);
    CHECK(wwa_fp4_e2m1_from_f32(0.5f) == 1);
    CHECK(wwa_fp4_e2m1_from_f32(1.0f) == 2);
    CHECK(wwa_fp4_e2m1_from_f32(1.5f) == 3);
    CHECK(wwa_fp4_e2m1_from_f32(2.0f) == 4);
    CHECK(wwa_fp4_e2m1_from_f32(3.0f) == 5);
    CHECK(wwa_fp4_e2m1_from_f32(4.0f) == 6);
    CHECK(wwa_fp4_e2m1_from_f32(6.0f) == 7);
    CHECK(wwa_fp4_e2m1_from_f32(-3.0f) == 13);
    CHECK(wwa_fp4_e2m1_from_f32(1.0e30f) == 7);
    CHECK(wwa_fp4_e2m1_to_f32(7) == 6.0f);
    CHECK(wwa_fp4_e2m1_to_f32(13) == -3.0f);
    {
        u32 p = wwa_fp6_pack_4(1.0f, -2.0f, 28.0f, 0.5f);
        real32_t a, b, c, d;
        wwa_fp6_unpack_4(p, &a, &b, &c, &d);
        CHECK(a == 1.0f);
        CHECK(b == -2.0f);
        CHECK(c == 28.0f);
        CHECK(d == 0.5f);
    }
    {
        u8 p = wwa_fp4_pack_2(3.0f, -1.5f);
        real32_t a, b;
        wwa_fp4_unpack_2(p, &a, &b);
        CHECK(a == 3.0f);
        CHECK(b == -1.5f);
    }
    CHECK(wwa_mx_scale_from_f32(1.0f) == 127);
    CHECK(wwa_mx_scale_to_f32(127) == 1.0f);
    CHECK(wwa_mx_scale_to_f32(0) == 0.0f);
    CHECK(wwa_mx_scale_to_f32(128) == 2.0f);
    CHECK(wwa_mx_scale_to_f32(126) == 0.5f);
    {
        static real32_t data[64];
        u8 q[64];
        u8 sc[4];
        real32_t back[64];
        u32 i;
        for (i = 0; i < 64; i++) {
            data[i] = i < 32 ? (real32_t)((i & 1) ? -(448.0f - (real32_t)i) : (448.0f - (real32_t)i)) : (real32_t)((i & 1) ? -((real32_t)i * 0.01f) : ((real32_t)i * 0.01f));
        }
        wwa_mxfp8_e4m3_quantize(data, 64, q, sc, 32);
        CHECK(sc[0] == 127 || sc[0] == 126);
        CHECK(sc[1] != 0);
        wwa_mxfp8_e4m3_dequantize(q, sc, 64, back, 32);
        CHECK_F32(back[0], 448.0f, 1e-4f);
        CHECK_F32(back[31], -417.0f, 0.01f);
        CHECK_F32(back[32], 0.32f, 0.03f);
        CHECK_F32(back[63], -0.63f, 0.05f);
        for (i = 0; i < 64; i++) {
            if (data[i] < 0.0f && back[i] > 0.0f) {
                g_fails++;
                printf("FAIL %s:%d: MX sign lost at %d\n", __FILE__, __LINE__, (i32)i);
            }
        }
    }
    {
        static real32_t data[64];
        u8 q[64];
        u8 sc[4];
        real32_t back[64];
        u32 i;
        for (i = 0; i < 64; i++) {
            data[i] = (real32_t)((i & 1) ? -(real32_t)(i + 1) * 0.01f : (real32_t)(i + 1) * 0.01f);
        }
        wwa_mxfp8_e5m2_quantize(data, 64, q, sc, 32);
        wwa_mxfp8_e5m2_dequantize(q, sc, 64, back, 32);
        for (i = 0; i < 64; i++) {
            if (data[i] < 0.0f && back[i] > 0.0f) {
                g_fails++;
                printf("FAIL %s:%d: E5M2 MX sign lost at %d\n", __FILE__, __LINE__, (i32)i);
            }
        }
        wwa_mxfp6_e3m2_quantize(data, 64, q, sc, 32);
        wwa_mxfp6_e3m2_dequantize(q, sc, 64, back, 32);
        for (i = 0; i < 64; i++) {
            if (data[i] < 0.0f && back[i] > 0.0f) {
                g_fails++;
                printf("FAIL %s:%d: FP6 MX sign lost at %d\n", __FILE__, __LINE__, (i32)i);
            }
        }
        wwa_mxfp4_e2m1_quantize(data, 64, q, sc, 32);
        wwa_mxfp4_e2m1_dequantize(q, sc, 64, back, 32);
        for (i = 0; i < 64; i++) {
            if (data[i] < 0.0f && back[i] > 0.0f) {
                g_fails++;
                printf("FAIL %s:%d: FP4 MX sign lost at %d\n", __FILE__, __LINE__, (i32)i);
            }
        }
        wwa_nvfp4_quantize(data, 64, q, sc);
        wwa_nvfp4_dequantize(q, sc, 64, back);
        for (i = 0; i < 64; i++) {
            if (data[i] < 0.0f && back[i] > 0.0f) {
                g_fails++;
                printf("FAIL %s:%d: NVFP4 sign lost at %d\n", __FILE__, __LINE__, (i32)i);
            }
        }
    }
}

static void test_alloc(void) {
    static const usize sizes[] = {16, 17, 100, 1024, 4096, 65536, 100000, 1048576};
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sizes); i++) {
        void_p p = wwa_malloc(sizes[i]);
        CHECK(p != NULL);
        if (p != NULL) {
            CHECK(((usize)p & 15) == 0);
            wwa_memset(p, 0x5A, sizes[i]);
            CHECK(((u8*)p)[0] == 0x5A && ((u8*)p)[sizes[i] - 1] == 0x5A);
            wwa_free(p);
        }
    }
    for (i = 0; i < 5000; i++) {
        void_p p = wwa_malloc((usize)((i * 2654435761u) % 5000) + 16);
        CHECK(p != NULL);
        if (p != NULL) {
            wwa_memset(p, (i32)(i & 0xFF), 16);
            wwa_free(p);
        }
    }
    {
        void_p p = wwa_calloc(100, 16);
        CHECK(p != NULL);
        if (p != NULL) {
            CHECK(((u8*)p)[0] == 0 && ((u8*)p)[1599] == 0);
            wwa_free(p);
        }
    }
    {
        void_p p = wwa_malloc(32);
        usize i;
        p = wwa_realloc(p, 100000);
        CHECK(p != NULL);
        if (p != NULL) {
            for (i = 0; i < 100000; i++) ((u8*)p)[i] = (u8)(i & 0xFF);
            p = wwa_realloc(p, 16);
            CHECK(p != NULL);
            if (p != NULL) {
                CHECK(((u8*)p)[0] == 0);
                wwa_free(p);
            }
        }
    }
    {
        void_p p = wwa_malloc(17);
        u8* q;
        CHECK(p != NULL);
        if (p != NULL) {
            wwa_memset(p, 0x33, 17);
            q = (u8*)wwa_realloc(p, 24);
            CHECK(q != NULL);
            if (q != NULL) {
                CHECK(q[0] == 0x33 && q[16] == 0x33);
                q = (u8*)wwa_realloc(q, 8);
                CHECK(q != NULL);
                if (q != NULL) {
                    CHECK(q[0] == 0x33 && q[16] == 0x33);
                    wwa_free(q);
                }
            }
        }
    }
    CHECK(wwa_malloc(0) != NULL);
    {
        void_p p = wwa_os_alloc(12345);
        CHECK(p != NULL);
        if (p != NULL) {
            wwa_memset(p, 0x77, 12345);
            CHECK(((u8*)p)[0] == 0x77 && ((u8*)p)[12344] == 0x77);
            wwa_os_free(p, 12345);
        }
        CHECK(wwa_os_alloc(0) != NULL);
    }
}

static i32 test_sort_cmp(const void_p a, const void_p b) {
    i32 x = *(const i32*)a;
    i32 y = *(const i32*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static i32 test_sort_cmp_i64(const void_p a, const void_p b) {
    i64 x = *(const i64*)a;
    i64 y = *(const i64*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static i32 test_sort_cmp_str(const void_p a, const void_p b) {
    const char_t* x = *(const char_t* const*)a;
    const char_t* y = *(const char_t* const*)b;
    return wwa_strcmp(x, y);
}

static void test_sort(void) {
    {
        i32 a[1000];
        u32 i;
        for (i = 0; i < 1000; i++) a[i] = (i32)((i * 2654435761u) % 100000) - 50000;
        wwa_qsort(a, 1000, sizeof(i32), test_sort_cmp);
        for (i = 1; i < 1000; i++) CHECK(a[i - 1] <= a[i]);
        for (i = 1; i < 1000; i++) CHECK(a[i] >= -50000 && a[i] < 50000);
    }
    {
        i32 a[100];
        u32 i;
        for (i = 0; i < 100; i++) a[i] = (i32)(i % 5);
        wwa_qsort(a, 100, sizeof(i32), test_sort_cmp);
        for (i = 1; i < 100; i++) CHECK(a[i - 1] <= a[i]);
    }
    {
        i32 a[2] = {2, 1};
        i32 b[1] = {7};
        i32 c[1];
        wwa_qsort(a, 2, sizeof(i32), test_sort_cmp);
        CHECK(a[0] == 1 && a[1] == 2);
        wwa_qsort(b, 1, sizeof(i32), test_sort_cmp);
        CHECK(b[0] == 7);
        wwa_qsort(c, 0, sizeof(i32), test_sort_cmp);
        wwa_qsort(NULL, 0, sizeof(i32), test_sort_cmp);
    }
    {
        i32 a[64];
        u32 i;
        i32 key;
        void_p hit;
        for (i = 0; i < 64; i++) a[i] = (i32)(i * 3);
        hit = wwa_bsearch(a + 20, a, 64, sizeof(i32), test_sort_cmp);
        CHECK(hit == a + 20);
        key = 61;
        hit = wwa_bsearch(&key, a, 64, sizeof(i32), test_sort_cmp);
        CHECK(hit == NULL);
        CHECK(wwa_bsearch(a, a, 0, sizeof(i32), test_sort_cmp) == NULL);
    }
    {
        i64 a[4096];
        u32 i;
        for (i = 0; i < 4096; i++) a[i] = (i64)(i * 2654435761u) % 4096 - 2048;
        wwa_qsort(a, 4096, sizeof(i64), test_sort_cmp_i64);
        for (i = 1; i < 4096; i++) CHECK(a[i - 1] <= a[i]);
    }
    {
        char_t* s1 = wwa_strdup("first");
        char_t* s2 = wwa_strdup("second");
        char_t* s3 = wwa_strdup("third");
        char_t* arr[3];
        arr[0] = s3;
        arr[1] = s1;
        arr[2] = s2;
        wwa_qsort(arr, 3, sizeof(char_t*), test_sort_cmp_str);
        CHECK(wwa_strcmp(arr[0], "first") == 0);
        CHECK(wwa_strcmp(arr[1], "second") == 0);
        CHECK(wwa_strcmp(arr[2], "third") == 0);
        wwa_free(s1);
        wwa_free(s2);
        wwa_free(s3);
    }
    wwa_srand(12345);
    {
        i32 r1 = wwa_rand();
        i32 r2 = wwa_rand();
        CHECK(r1 >= 0 && r1 <= RAND_MAX);
        CHECK(r2 >= 0 && r2 <= RAND_MAX);
        CHECK(r1 != r2);
        wwa_srand(12345);
        CHECK(wwa_rand() == r1);
        CHECK(wwa_rand() == r2);
        wwa_srand(54321);
        CHECK(wwa_rand() != r1);
    }
    {
        u32 i;
        i64 sum = 0;
        wwa_srand(999);
        for (i = 0; i < 100000; i++) sum += (i64)wwa_rand();
        CHECK(wwa_abs((i64)(sum - (i64)100000 * RAND_MAX / 2)) < (i64)RAND_MAX * 4000);
    }
}

static void test_stdio(void) {
    char_t buf[64];
    i32 n;
    wwa_snprintf(buf, sizeof(buf), "%d", 42);
    CHECK_STR(buf, "42");
    wwa_snprintf(buf, sizeof(buf), "%05d", 42);
    CHECK_STR(buf, "00042");
    wwa_snprintf(buf, sizeof(buf), "%-5d|", 42);
    CHECK_STR(buf, "42   |");
    wwa_snprintf(buf, sizeof(buf), "%+d", 42);
    CHECK_STR(buf, "+42");
    wwa_snprintf(buf, sizeof(buf), "%x", 255);
    CHECK_STR(buf, "ff");
    wwa_snprintf(buf, sizeof(buf), "%X", 255);
    CHECK_STR(buf, "FF");
    wwa_snprintf(buf, sizeof(buf), "%#x", 255);
    CHECK_STR(buf, "0xff");
    wwa_snprintf(buf, sizeof(buf), "%o", 8);
    CHECK_STR(buf, "10");
    wwa_snprintf(buf, sizeof(buf), "%#o", 8);
    CHECK_STR(buf, "010");
    wwa_snprintf(buf, sizeof(buf), "%lld", -9223372036854775807ll - 1);
    CHECK_STR(buf, "-9223372036854775808");
    wwa_snprintf(buf, sizeof(buf), "%u", (u32)-1);
    CHECK_STR(buf, "4294967295");
    wwa_snprintf(buf, sizeof(buf), "%f", 3.5);
    CHECK_STR(buf, "3.500000");
    wwa_snprintf(buf, sizeof(buf), "%.2f", 3.14159);
    CHECK_STR(buf, "3.14");
    wwa_snprintf(buf, sizeof(buf), "%.0f", 3.9);
    CHECK_STR(buf, "4");
    wwa_snprintf(buf, sizeof(buf), "%e", 12345.678);
    CHECK_STR(buf, "1.234568e+04");
    wwa_snprintf(buf, sizeof(buf), "%.2e", 0.0012345);
    CHECK_STR(buf, "1.23e-03");
    wwa_snprintf(buf, sizeof(buf), "%g", 123456.0);
    CHECK_STR(buf, "123456");
    wwa_snprintf(buf, sizeof(buf), "%g", 0.0001);
    CHECK_STR(buf, "0.0001");
    wwa_snprintf(buf, sizeof(buf), "%g", 0.00001);
    CHECK_STR(buf, "1e-05");
    wwa_snprintf(buf, sizeof(buf), "%s", "hello");
    CHECK_STR(buf, "hello");
    wwa_snprintf(buf, sizeof(buf), "%8s|", "hello");
    CHECK_STR(buf, "   hello|");
    wwa_snprintf(buf, sizeof(buf), "%-8s|", "hello");
    CHECK_STR(buf, "hello   |");
    wwa_snprintf(buf, sizeof(buf), "%.3s", "hello");
    CHECK_STR(buf, "hel");
    wwa_snprintf(buf, sizeof(buf), "%c", 'A');
    CHECK_STR(buf, "A");
    wwa_snprintf(buf, sizeof(buf), "%%");
    CHECK_STR(buf, "%");
    wwa_snprintf(buf, sizeof(buf), "%zu", (usize)123);
    CHECK_STR(buf, "123");
    wwa_snprintf(buf, sizeof(buf), "%p", (void_p)(usize)0x1234);
    CHECK(buf[0] == '0' && buf[1] == 'x');
    n = wwa_snprintf(buf, 3, "%d", 12345);
    CHECK(n == 5);
    CHECK_STR(buf, "12");
    {
        char_t big[320];
        wwa_snprintf(big, sizeof(big), "%f", 1.0e300);
        CHECK_STR(big, "1000000000000000052504760255204420248704468581108159154915854115511802457988908195786371375080447864043704443832883878176942523235360430575644792184786706982848387200926575803737830233794788090059368953234970799945081119038967640880074652742780142494579258788820056842838115669472196386865459400540160.000000");
    }
    wwa_snprintf(buf, sizeof(buf), "%d %d %d", 1, 2, 3);
    CHECK_STR(buf, "1 2 3");
    wwa_snprintf(buf, sizeof(buf), "%.4f", 0.5);
    CHECK_STR(buf, "0.5000");
    wwa_snprintf(buf, sizeof(buf), "%g", 1.0e10);
    CHECK_STR(buf, "1e+10");
    wwa_snprintf(buf, sizeof(buf), "%a", 1.5);
    CHECK_STR(buf, "0x1.800000p+0");
    wwa_snprintf(buf, sizeof(buf), "%a", 0.5);
    CHECK_STR(buf, "0x1.000000p-1");
    wwa_snprintf(buf, sizeof(buf), "%a", 0.0);
    CHECK_STR(buf, "0x0.000000p+0");
    wwa_snprintf(buf, sizeof(buf), "%a", -1.5);
    CHECK_STR(buf, "-0x1.800000p+0");
    wwa_snprintf(buf, sizeof(buf), "%A", 1.5);
    CHECK_STR(buf, "0X1.800000P+0");
    wwa_snprintf(buf, sizeof(buf), "%.3a", 3.5);
    CHECK_STR(buf, "0x1.c00p+1");
    wwa_snprintf(buf, sizeof(buf), "%a", 0.1);
    CHECK_STR(buf, "0x1.99999ap-4");
    wwa_snprintf(buf, sizeof(buf), "%a", 2.0);
    CHECK_STR(buf, "0x1.000000p+1");
    wwa_snprintf(buf, sizeof(buf), "%a", 0.25);
    CHECK_STR(buf, "0x1.000000p-2");
    wwa_snprintf(buf, sizeof(buf), "%.13a", 1.0);
    CHECK_STR(buf, "0x1.0000000000000p+0");
    {
        char_t big[320];
        wwa_snprintf(big, sizeof(big), "%a", 1.0e300);
        CHECK(wwa_strncmp(big, "0x1.", 4) == 0);
        CHECK(wwa_strstr(big, "p+") != NULL);
    }
    wwa_snprintf(buf, sizeof(buf), "%*d|", 6, 42);
    CHECK_STR(buf, "    42|");
    wwa_snprintf(buf, sizeof(buf), "%-*d|", 6, 42);
    CHECK_STR(buf, "42    |");
    wwa_snprintf(buf, sizeof(buf), "%0*d", 5, 42);
    CHECK_STR(buf, "00042");
    wwa_snprintf(buf, sizeof(buf), "%*.*f", 10, 2, 3.14159);
    CHECK_STR(buf, "      3.14");
    wwa_snprintf(buf, sizeof(buf), "%.*s", 3, "hello");
    CHECK_STR(buf, "hel");
    wwa_snprintf(buf, sizeof(buf), "%.*s", 0, "hello");
    CHECK_STR(buf, "");
    wwa_snprintf(buf, sizeof(buf), "%*s|", 8, "hi");
    CHECK_STR(buf, "      hi|");
    wwa_snprintf(buf, sizeof(buf), "%hd", 70000);
    CHECK_STR(buf, "4464");
    wwa_snprintf(buf, sizeof(buf), "%hd", -12345);
    CHECK_STR(buf, "-12345");
    wwa_snprintf(buf, sizeof(buf), "%hhd", 70000);
    CHECK_STR(buf, "4464");
    wwa_snprintf(buf, sizeof(buf), "%hx", 0x1FFFF);
    CHECK_STR(buf, "ffff");
    wwa_snprintf(buf, sizeof(buf), "%ld", 9223372036854775807ll);
    CHECK_STR(buf, "9223372036854775807");
    wwa_snprintf(buf, sizeof(buf), "%lx", 0x123456789ABCDEFull);
    CHECK_STR(buf, "123456789abcdef");
    wwa_snprintf(buf, sizeof(buf), "%lo", 0x1FFull);
    CHECK_STR(buf, "777");
    wwa_snprintf(buf, sizeof(buf), "%zu", (usize)-1);
    CHECK_STR(buf, "18446744073709551615");
    wwa_snprintf(buf, sizeof(buf), "%i", -7);
    CHECK_STR(buf, "-7");
    wwa_snprintf(buf, sizeof(buf), "% d|", 42);
    CHECK_STR(buf, " 42|");
    wwa_snprintf(buf, sizeof(buf), "% d", -42);
    CHECK_STR(buf, "-42");
    wwa_snprintf(buf, sizeof(buf), "%.0e", 12345.678);
    CHECK_STR(buf, "1e+04");
    wwa_snprintf(buf, sizeof(buf), "%+e", 12345.678);
    CHECK_STR(buf, "+1.234568e+04");
    wwa_snprintf(buf, sizeof(buf), "%G", 0.00001);
    CHECK_STR(buf, "1E-05");
    wwa_snprintf(buf, sizeof(buf), "%.4g", 123456.0);
    CHECK_STR(buf, "1.235e+05");
    wwa_snprintf(buf, sizeof(buf), "%.4g", 0.0000123456);
    CHECK_STR(buf, "1.235e-05");
    wwa_snprintf(buf, sizeof(buf), "%.0f", 2.5);
    CHECK_STR(buf, "3");
    wwa_snprintf(buf, sizeof(buf), "%.0f", -2.5);
    CHECK_STR(buf, "-3");
    wwa_snprintf(buf, sizeof(buf), "%.2f", 1.999);
    CHECK_STR(buf, "2.00");
    wwa_snprintf(buf, sizeof(buf), "%.0f", 999.5);
    CHECK_STR(buf, "1000");
    wwa_snprintf(buf, sizeof(buf), "%3c|", 'A');
    CHECK_STR(buf, "  A|");
    wwa_snprintf(buf, sizeof(buf), "%s", NULL);
    CHECK_STR(buf, "(null)");
    {
        const double av[] = { 0.0, 1.0, -1.0, 0.5, -0.5, 0.1, 3.141592653589793,
                              1e10, 1e-10, 1e300, -1e300, 2.2250738585072014e-308,
                              9007199254740992.0, 1.7976931348623157e308 };
        usize k;
        char_t hx[80];
        for (k = 0; k < sizeof(av) / sizeof(av[0]); k++) {
            wwa_snprintf(hx, sizeof(hx), "%.13a", av[k]);
            CHECK(wwa_strtod(hx, NULL) == av[k]);
        }
    }
    {
        const double gv[] = { 0.0, 1.0, -1.0, 0.5, -0.5, 0.1, 3.141592653589793,
                              1e10, 9007199254740992.0 };
        const double ev[] = { 0.5, -0.5, 0.1, 3.141592653589793, 1e10,
                              123.456, -789.012, 1e-3, 5e-3 };
        usize k;
        char_t gs[400];
        for (k = 0; k < sizeof(gv) / sizeof(gv[0]); k++) {
            wwa_snprintf(gs, sizeof(gs), "%.17g", gv[k]);
            CHECK(wwa_strtod(gs, NULL) == gv[k]);
        }
        for (k = 0; k < sizeof(ev) / sizeof(ev[0]); k++) {
            wwa_snprintf(gs, sizeof(gs), "%.16e", ev[k]);
            CHECK(wwa_strtod(gs, NULL) == ev[k]);
        }
    }
    {
        i32 cnt = -1;
        i64 lcnt = -1;
        i16 hcnt = -1;
        wwa_snprintf(buf, sizeof(buf), "ab%ncd", &cnt);
        CHECK(cnt == 2);
        wwa_snprintf(buf, sizeof(buf), "%d %d%ln", 1, 2, &lcnt);
        CHECK(lcnt == 3);
        wwa_snprintf(buf, sizeof(buf), "x%hny", &hcnt);
        CHECK(hcnt == 1);
    }
    {
        char_t vb[64];
        i32 vn = fmt_vsnprintf(vb, sizeof(vb), "%d/%s/%c", 7, "seven", 'X');
        CHECK(vn == 9);
        CHECK_STR(vb, "7/seven/X");
        vn = fmt_vsnprintf(vb, 4, "%d/%s/%c", 7, "seven", 'X');
        CHECK(vn == 9);
        CHECK_STR(vb, "7/s");
    }
    CHECK(fmt_vprintf("%d", 42) == 2);
    CHECK(wwa_putchar('A') == 'A');
    CHECK(wwa_puts("stdio-check") == 0);
    CHECK(wwa_display("display-check") == 0);
    wwa_stdio_flush();
    wwa_stdio_init();
}

static void test_log(void) {
    char_t path[64];
    i32 fd;
    char_t buf[512];
    isize n;
    CHECK(wwa_log_level() == WWA_LOG_LEVEL_TRACE);
    wwa_log_set_level(WWA_LOG_LEVEL_ERROR);
    CHECK(wwa_log_level() == WWA_LOG_LEVEL_ERROR);
    wwa_log_set_level(WWA_LOG_LEVEL_TRACE);
    CHECK(wwa_log_level() == WWA_LOG_LEVEL_TRACE);
    wwa_snprintf(path, sizeof(path), "res/logs/selftest_%d.log", (i32)wwa_time_pid());
    CHECK(wwa_log_init(path) == 0);
    wwa_log_write(WWA_LOG_LEVEL_INFO, "selftest", "hello from log %d", 42);
    wwa_log_write(WWA_LOG_LEVEL_WARN, "selftest", "warning %s", "warn");
    log_vwrite_helper(WWA_LOG_LEVEL_NOTE, "selftest", "vwrite %d", 7);
    wwa_log_close();
    fd = wwa_os_file_open(path, WWA_OS_FILE_READ);
    CHECK(fd >= 0);
    if (fd >= 0) {
        n = wwa_os_read(fd, buf, sizeof(buf) - 1);
        wwa_os_file_close(fd);
        CHECK(n > 0);
        if (n > 0) {
            buf[n] = 0;
            CHECK(wwa_strstr(buf, "hello from log 42") != NULL);
            CHECK(wwa_strstr(buf, "warning warn") != NULL);
            CHECK(wwa_strstr(buf, "vwrite 7") != NULL);
            CHECK(wwa_strstr(buf, "selftest") != NULL);
        }
    }
    wwa_os_file_delete(path);
}

static void test_std_headers(void) {
    /* stddef.h */
    CHECK(offsetof(struct { char a; int b; }, b) == 4 || offsetof(struct { char a; int b; }, b) == sizeof(char) + sizeof(int) - sizeof(char));
    { size_t sz = sizeof(size_t); CHECK(sz == 8); }
    { ptrdiff_t pd = 42 - 10; CHECK(pd == 32); }
    { max_align_t ma; CHECK(sizeof(ma) >= 8); }

    /* stdint.h */
    CHECK(INT8_MIN == -128);
    CHECK(INT8_MAX == 127);
    CHECK(UINT8_MAX == 255);
    CHECK(INT16_MIN == -32768);
    CHECK(INT16_MAX == 32767);
    CHECK(UINT16_MAX == 65535);
    CHECK(INT32_MIN == (-2147483647 - 1));
    CHECK(INT32_MAX == 2147483647);
    CHECK(UINT32_MAX == 0xFFFFFFFFu);
    CHECK(INT64_MIN == (-9223372036854775807ll - 1));
    CHECK(INT64_MAX == 9223372036854775807ll);
    CHECK(UINT64_MAX == 0xFFFFFFFFFFFFFFFFull);
    CHECK(SIZE_MAX == UINT64_MAX);
    CHECK(INTMAX_MAX == INT64_MAX);
    CHECK(UINTMAX_MAX == UINT64_MAX);
    CHECK(sizeof(intmax_t) == 8);
    CHECK(sizeof(uintmax_t) == 8);
    CHECK(INT8_C(42) == 42);
    CHECK(UINT8_C(200) == 200);
    CHECK(INT64_C(123456789) == 123456789ll);

    /* stdbool.h */
    { bool_t b = true; CHECK(b == 1); }
    { bool_t b = false; CHECK(b == 0); }
    CHECK(true == 1);
    CHECK(false == 0);
    CHECK(__bool_true_false_are_defined == 1);

    /* float.h */
    CHECK(FLT_RADIX == 2);
    CHECK(FLT_MANT_DIG == 24);
    CHECK(FLT_DIG == 6);
    CHECK(FLT_MAX > 1.0f);
    CHECK(FLT_MIN > 0.0f);
    CHECK(FLT_EPSILON > 0.0f);
    CHECK(DBL_MANT_DIG == 53);
    CHECK(DBL_DIG == 15);
    CHECK(DBL_MAX > 1.0);
    CHECK(DBL_MIN > 0.0);
    CHECK(DBL_EPSILON > 0.0);
    CHECK(DBL_MAX == 1.7976931348623157e+308);
    CHECK(DBL_MIN == 2.2250738585072014e-308);
    CHECK(DBL_EPSILON == 2.2204460492503131e-16);

    /* limits.h */
    CHECK(CHAR_BIT == 8);
    CHECK(INT_MIN == (-2147483647 - 1));
    CHECK(INT_MAX == 2147483647);
    CHECK(UINT_MAX == 0xFFFFFFFFu);
#if __SIZEOF_LONG__ == 4
    CHECK(LONG_MIN == (-2147483647L - 1));
    CHECK(LONG_MAX == 2147483647L);
    CHECK(ULONG_MAX == 0xFFFFFFFFUL);
#else
    CHECK(LONG_MIN == (-9223372036854775807L - 1));
    CHECK(LONG_MAX == 9223372036854775807L);
    CHECK(ULONG_MAX == 0xFFFFFFFFFFFFFFFFUL);
#endif
    CHECK(LLONG_MIN == INT64_MIN);
    CHECK(LLONG_MAX == INT64_MAX);
    CHECK(ULLONG_MAX == UINT64_MAX);

    /* errno.h */
    CHECK(EDOM == 33);
    CHECK(ERANGE == 34);
    CHECK(EILSEQ == 84);
    { int *ep = wwa_errno_loc(); CHECK(ep != 0); *ep = 42; CHECK(errno == 42); *ep = 0; }

    /* assert.h — include compiles, assert(true) does not fire */
    assert(1 == 1);
    assert(sizeof(int) > 0);
}

static void test_os(void) {
    char_t t[24];
    i32 n = wwa_time_str(t, sizeof(t));
    CHECK(n == 19);
    CHECK(t[4] == '-' && t[7] == '-' && t[10] == ' ' && t[13] == ':');
    CHECK(wwa_time_ticks_per_sec() > 0);
    CHECK(wwa_time_us() > 1000000000000000ull && wwa_time_us() < 10000000000000000ull);
    CHECK(wwa_time_pid() > 0);
    CHECK(wwa_os_pid() == wwa_time_pid());
    CHECK(wwa_os_ticks_per_sec() > 0);
    CHECK(wwa_os_ticks() > 0);
    CHECK(wwa_os_time_us() > 1000000000000000ull && wwa_os_time_us() < 10000000000000000ull);
    CHECK(wwa_time_ms() > 1000000000000ull && wwa_time_ms() < 10000000000000ull);
    {
        char_t t2[24];
        CHECK(wwa_os_time_str(t2, sizeof(t2)) == 19);
        CHECK(t2[4] == '-' && t2[7] == '-' && t2[10] == ' ' && t2[13] == ':');
    }
    CHECK(wwa_os_cmdline() != NULL && wwa_strlen(wwa_os_cmdline()) > 0);
    CHECK(wwa_os_cpu_avx2() == 0 || wwa_os_cpu_avx2() == 1);
    CHECK(wwa_os_cpu_avx512() == 0 || wwa_os_cpu_avx512() == 1);
    CHECK(wwa_os_cpu_avx512() == 0 || wwa_os_cpu_avx2() == 1);
    {
        char_t dpath[80];
        char_t fpath[96];
        i32 fd;
        wwa_snprintf(dpath, sizeof(dpath), "res/logs/dirtest_%d", (i32)wwa_time_pid());
        CHECK(wwa_os_dir_create(dpath) == 0);
        CHECK(wwa_os_file_exists(dpath) == 1);
        CHECK(wwa_os_dir_create(dpath) == 0);
        wwa_snprintf(fpath, sizeof(fpath), "%s/f.txt", dpath);
        fd = wwa_os_file_open(fpath, WWA_OS_FILE_WRITE | WWA_OS_FILE_CREATE | WWA_OS_FILE_TRUNC);
        CHECK(fd >= 0);
        if (fd >= 0) {
            CHECK(wwa_os_write(fd, "d", 1) == 1);
            wwa_os_file_close(fd);
        }
        CHECK(wwa_os_file_exists(fpath) == 1);
        CHECK(wwa_os_file_delete(fpath) == 0);
    }
    {
        char_t path[64];
        i32 fd;
        i64 m0;
        wwa_snprintf(path, sizeof(path), "res/logs/mtime_%d.txt", (i32)wwa_time_pid());
        fd = wwa_os_file_open(path, WWA_OS_FILE_WRITE | WWA_OS_FILE_CREATE | WWA_OS_FILE_TRUNC);
        CHECK(fd >= 0);
        if (fd >= 0) {
            const char_t* data = "hello mtime";
            CHECK(wwa_os_write(fd, data, wwa_strlen(data)) == (i32)wwa_strlen(data));
            wwa_os_file_close(fd);
        }
        CHECK(wwa_os_file_exists(path) == 1);
        m0 = wwa_os_file_mtime(path);
        CHECK(m0 > 1500000000ll);
        CHECK(wwa_os_file_delete(path) == 0);
        CHECK(wwa_os_file_exists(path) == 0);
        CHECK(wwa_os_file_mtime(path) < 0);
    }
    {
        i32 code = -1;
        const char_t* args[3];
        args[0] = "std_selftest";
        args[1] = "--child";
        args[2] = NULL;
        CHECK(wwa_os_spawn(args[0], args, &code) == 0);
        CHECK(code == 7);
    }
    {
        i32 code = -1;
        const char_t* args[3];
        args[0] = "std_selftest";
        args[1] = "--child-exit";
        args[2] = NULL;
        CHECK(wwa_os_spawn(args[0], args, &code) == 0);
        CHECK(code == 5);
    }
    {
        i32 code = -1;
        const char_t* args[3];
        args[0] = "std_selftest";
        args[1] = "--child-abort";
        args[2] = NULL;
        CHECK(wwa_os_spawn(args[0], args, &code) == 0);
        CHECK(code != 0);
    }
}

static void test_fenv(void) {
    wwa_fenv_t e0, e1, e2;
    wwa_fexcept_t fl;
    volatile real64_t a = 1.0, b = 3.0;
    CHECK(wwa_fegetround() == WWA_FE_TONEAREST);
    CHECK(wwa_fesetround(WWA_FE_UPWARD) == 0);
    CHECK(wwa_fegetround() == WWA_FE_UPWARD);
    CHECK(a / b > 0.33333333333333331);
    CHECK(wwa_fesetround(WWA_FE_DOWNWARD) == 0);
    CHECK(wwa_fegetround() == WWA_FE_DOWNWARD);
    CHECK(a / b < 0.33333333333333337);
    CHECK(wwa_fesetround(WWA_FE_TOWARDZERO) == 0);
    CHECK(wwa_fegetround() == WWA_FE_TOWARDZERO);
    CHECK(wwa_fesetround(7) == -1);
    CHECK(wwa_fesetround(WWA_FE_TONEAREST) == 0);
    CHECK(wwa_fegetround() == WWA_FE_TONEAREST);
    CHECK(wwa_fegetenv(&e0) == 0);
    CHECK(wwa_feclearexcept(WWA_FE_ALL_EXCEPT) == 0);
    CHECK(wwa_fetestexcept(WWA_FE_ALL_EXCEPT) == 0);
    CHECK(wwa_feraiseexcept(WWA_FE_OVERFLOW | WWA_FE_INEXACT) == 0);
    CHECK(wwa_fetestexcept(WWA_FE_OVERFLOW) == WWA_FE_OVERFLOW);
    CHECK(wwa_fetestexcept(WWA_FE_INVALID) == 0);
    CHECK(wwa_fetestexcept(WWA_FE_OVERFLOW | WWA_FE_INEXACT) ==
          (WWA_FE_OVERFLOW | WWA_FE_INEXACT));
    CHECK(wwa_fegetexceptflag(&fl, WWA_FE_OVERFLOW) == 0);
    CHECK(fl == WWA_FE_OVERFLOW);
    CHECK(wwa_feclearexcept(WWA_FE_ALL_EXCEPT) == 0);
    CHECK(wwa_fetestexcept(WWA_FE_OVERFLOW) == 0);
    CHECK(wwa_fesetexceptflag(&fl, WWA_FE_OVERFLOW) == 0);
    CHECK(wwa_fetestexcept(WWA_FE_OVERFLOW) == WWA_FE_OVERFLOW);
    CHECK(wwa_feclearexcept(WWA_FE_ALL_EXCEPT) == 0);
    CHECK(wwa_feraiseexcept(WWA_FE_INEXACT) == 0);
    CHECK(wwa_fegetenv(&e1) == 0);
    CHECK(wwa_feholdexcept(&e2) != 0);
    CHECK(wwa_fetestexcept(WWA_FE_ALL_EXCEPT) == 0);
    CHECK(wwa_feraiseexcept(WWA_FE_OVERFLOW) == 0);
    CHECK(wwa_fetestexcept(WWA_FE_ALL_EXCEPT) == WWA_FE_OVERFLOW);
    CHECK(wwa_feupdateenv(&e1) == 0);
    CHECK(wwa_fetestexcept(WWA_FE_ALL_EXCEPT) ==
          (WWA_FE_OVERFLOW | WWA_FE_INEXACT));
    CHECK(wwa_feclearexcept(WWA_FE_ALL_EXCEPT) == 0);
    CHECK(wwa_fesetenv(&e1) == 0);
    CHECK(wwa_fetestexcept(WWA_FE_ALL_EXCEPT) == WWA_FE_INEXACT);
    CHECK(wwa_fesetenv(&e0) == 0);
    CHECK(wwa_fegetround() == WWA_FE_TONEAREST);
}

i32 main(i32 argc, string_t argv[]) {
    if (argc >= 2 && wwa_strcmp(argv[1], "--child") == 0) return 7;
    if (argc >= 2 && wwa_strcmp(argv[1], "--child-exit") == 0) wwa_os_exit(5);
    if (argc >= 2 && wwa_strcmp(argv[1], "--child-abort") == 0) wwa_abort();
    printf("WWA std selftest\n");
    test_mem();
    test_str();
    test_num();
    test_math();
    test_math_double();
    test_fenv();
    test_float();
    test_alloc();
    test_sort();
    test_stdio();
    test_log();
    test_os();
    test_std_headers();
    printf("tests: %d, fails: %d\n", g_tests, g_fails);
    if (g_fails != 0) {
        printf("STD_SELFTEST_FAIL\n");
        return 1;
    }
    printf("STD_SELFTEST_OK\n");
    return 0;
}