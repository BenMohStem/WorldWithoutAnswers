/* World Without Answers — stdio.c
   from-scratch printf family with sink-based formatter.
   Exact for integers and for float integer parts up to 2^64
   (exact big-digit path beyond 2^64, up to the full double range).
   Fraction digits beyond ~15 significant are documented-approximate.
   Line-buffered stdout, flushed on newline, buffer full, or exit. */

#include <stdio.h>
#include <stdos.h>
#include <stdstr.h>
#include <stdmem.h>

#if defined(__GNUC__) || defined(__clang__)
typedef unsigned __int128 wwa_u128;
#else
#error "stdio: 128-bit integer required for exact big-digit formatting"
#endif

typedef union {
    real64_t f;
    u64 u;
} wwa_double_bits_t;

static i32 wwa_dbl_is_nan(real64_t v) {
    wwa_double_bits_t u;
    u.f = v;
    return (u.u & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull;
}

static i32 wwa_dbl_is_inf(real64_t v) {
    wwa_double_bits_t u;
    u.f = v;
    return (u.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull;
}

#define WWA_STDIO_BUF_SIZE 4096

static char_t g_out[WWA_STDIO_BUF_SIZE];
static usize g_out_len = 0;
static i32 g_init = 0;

static char_t g_in[WWA_STDIO_BUF_SIZE];
static usize g_in_pos = 0;
static usize g_in_len = 0;

static char_t g_fmt_buf[2048];

static real64_t wwa_fmt_pow10(i32 k) {
    real64_t v = 1.0;
    while (k > 22) { v *= 1.0e22; k -= 22; }
    while (k < -22) { v *= 1.0e-22; k += 22; }
    if (k > 0) {
        real64_t pw = 1.0;
        i32 i;
        for (i = 0; i < k; i++) pw *= 5.0;
        v *= (real64_t)(1 << k) * pw;
    } else if (k < 0) {
        real64_t pw = 1.0;
        i32 i, nk = -k;
        for (i = 0; i < nk; i++) pw *= 5.0;
        v /= (real64_t)(1 << nk) * pw;
    }
    return v;
}

void wwa_stdio_flush(void) {
    if (g_out_len != 0) {
        wwa_os_write(WWA_OS_FD_STDOUT, g_out, g_out_len);
        g_out_len = 0;
    }
}

void wwa_stdio_init(void) {
    if (!g_init) {
        wwa_os_exit_hook = wwa_stdio_flush;
        g_init = 1;
    }
}

static i32 wwa_stdout_put(void_p ctx, const char_t* s, usize n) {
    UNUSED(ctx);
    if (g_out_len + n > sizeof(g_out)) wwa_stdio_flush();
    if (n >= sizeof(g_out)) return wwa_os_write(WWA_OS_FD_STDOUT, s, n);
    wwa_memcpy(g_out + g_out_len, s, n);
    g_out_len += n;
    if (wwa_memchr(g_out, '\n', g_out_len) != NULL) wwa_stdio_flush();
    return (i32)n;
}

typedef struct {
    char_t* buf;
    usize cap;
    usize len;
} wwa_buf_sink_t;

static i32 wwa_buf_put(void_p ctx, const char_t* s, usize n) {
    wwa_buf_sink_t* b = (wwa_buf_sink_t*)ctx;
    usize room = b->cap > 0 ? b->cap - 1 : 0;
    if (b->len < room) {
        usize c = n;
        if (b->len + c > room) c = room - b->len;
        wwa_memcpy(b->buf + b->len, s, c);
        b->len += c;
    }
    return (i32)n;
}

#define WWA_FMT_LEFT  1
#define WWA_FMT_PLUS  2
#define WWA_FMT_SPACE 4
#define WWA_FMT_HASH  8
#define WWA_FMT_ZERO  16

typedef i32 (*wwa_put_fn)(void_p ctx, const char_t* s, usize n);

static void wwa_emit(wwa_put_fn put, void_p ctx, i64* total, const char_t* s, usize n) {
    put(ctx, s, n);
    *total += (i64)n;
}

static void wwa_fmt_uint(wwa_put_fn put, void_p ctx, i64* total,
                         u64 v, i32 base, i32 upper, i32 width, i32 flags, i32 prec,
                         const char_t* prefix, usize prefix_len) {
    char_t digits[68];
    i32 nd = 0;
    i32 pad = 0;
    i32 i;
    do {
        u32 d = (u32)(v % (u64)base);
        digits[nd++] = (char_t)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + (d - 10));
        v /= (u64)base;
    } while (v != 0);
    if (prec > nd) pad = prec - nd;
    else if ((flags & WWA_FMT_ZERO) && !(flags & WWA_FMT_LEFT) && prec < 0) {
        i32 room = width - nd - (i32)prefix_len;
        if (room > 0) pad = room;
    }
    {
        i32 sp = width - nd - pad - (i32)prefix_len;
        if (sp < 0) sp = 0;
        if (!(flags & WWA_FMT_LEFT)) {
            while (sp-- > 0) wwa_emit(put, ctx, total, " ", 1);
        }
        if (prefix_len) wwa_emit(put, ctx, total, prefix, prefix_len);
        while (pad-- > 0) wwa_emit(put, ctx, total, "0", 1);
        for (i = nd - 1; i >= 0; i--) wwa_emit(put, ctx, total, &digits[i], 1);
        if (flags & WWA_FMT_LEFT) {
            while (sp-- > 0) wwa_emit(put, ctx, total, " ", 1);
        }
    }
}

static void wwa_fmt_signed(wwa_put_fn put, void_p ctx, i64* total,
                           i64 v, u32 base, i32 upper, i32 width, i32 flags, i32 prec) {
    char_t prefix[2];
    usize plen = 0;
    u64 uv;
    if (v < 0) {
        prefix[plen++] = '-';
        uv = (u64)(-(v + 1)) + 1;
    } else {
        if (flags & WWA_FMT_PLUS) prefix[plen++] = '+';
        else if (flags & WWA_FMT_SPACE) prefix[plen++] = ' ';
        uv = (u64)v;
    }
    wwa_fmt_uint(put, ctx, total, uv, (i32)base, upper, width, flags, prec, prefix, plen);
}

/* exact decimal digits of an integral double v >= 2^64 (v = m*2^k) */
static i32 wwa_fmt_big_digits(real64_t v, char_t* out, usize cap) {
    wwa_double_bits_t u;
    u64 m;
    i32 k, shift_limbs, shift_bits;
    u64 limbs[17];
    i32 nlimb = 0;
    i32 top, i;
    char_t tmp[400];
    i32 nd = 0;
    u.f = v;
    m = (u.u & 0xFFFFFFFFFFFFFull) | 0x10000000000000ull;
    k = (i32)((u.u >> 52) & 0x7FF) - 1023 - 52;
    limbs[nlimb++] = m;
    shift_limbs = k / 64;
    shift_bits = k % 64;
    if (shift_limbs) {
        for (i = nlimb - 1; i >= 0; i--) limbs[i + shift_limbs] = limbs[i];
        for (i = 0; i < shift_limbs; i++) limbs[i] = 0;
        nlimb += shift_limbs;
    }
    if (shift_bits) {
        u64 carry = 0;
        for (i = 0; i < nlimb; i++) {
            u64 cur = limbs[i];
            limbs[i] = (cur << shift_bits) | carry;
            carry = cur >> (64 - shift_bits);
        }
        if (carry) limbs[nlimb++] = carry;
    }
    top = nlimb - 1;
    while (top > 0 && limbs[top] == 0) top--;
    for (;;) {
        u64 rem = 0;
        i32 all_zero = 1;
        for (i = top; i >= 0; i--) {
            wwa_u128 cur = ((wwa_u128)rem << 64) | limbs[i];
            limbs[i] = (u64)(cur / 10);
            rem = (u64)(cur % 10);
            if (limbs[i] != 0) all_zero = 0;
        }
        if (nd < (i32)cap) tmp[nd] = (char_t)('0' + (i32)rem);
        nd++;
        if (all_zero) break;
    }
    for (i = 0; i < nd; i++) out[i] = tmp[i];
    out[nd] = 0;
    return nd;
}

/* fixed-point into buffer; returns length (>= 0) or -1 if it did not fit */
static i32 wwa_fmt_fixed_buf(real64_t v, i32 prec, i32 force_dot, char_t* out, usize cap) {
    usize pos = 0;
    if (prec < 0) prec = 6;
    if (v >= 9007199254740992.0) {
        /* integral */
        char_t digits[400];
        i32 nd;
        i32 i;
        if (v < 18446744073709551616.0) {
            u64 ip = (u64)v;
            nd = 0;
            do {
                digits[nd++] = (char_t)('0' + (i32)(ip % 10));
                ip /= 10;
            } while (ip != 0);
        } else {
            nd = wwa_fmt_big_digits(v, digits, sizeof(digits));
        }
        if ((usize)nd + (prec > 0 || force_dot ? (usize)prec + 1 : 0) >= cap) return -1;
        for (i = nd - 1; i >= 0; i--) out[pos++] = digits[i];
        if (prec > 0 || force_dot) {
            out[pos++] = '.';
            while (prec-- > 0) out[pos++] = '0';
        }
        out[pos] = 0;
        return (i32)pos;
    }
    {
        u64 ip = (u64)v;
        real64_t frac = v - (real64_t)ip;
        char_t idigits[24];
        i32 ind = 0;
        i32 i;
        u32 fdigits[1024];
        i32 fd = 0;
        for (i = 0; i <= prec && i < 1024; i++) {
            frac *= 10.0;
            fdigits[i] = (u32)frac;
            frac -= (real64_t)fdigits[i];
        }
        fd = prec;
        if (fd < 1024 && fdigits[fd] >= 5) {
            i32 j = fd - 1;
            while (j >= 0) {
                fdigits[j]++;
                if (fdigits[j] < 10) break;
                fdigits[j] = 0;
                j--;
            }
            if (j < 0) ip++;
        }
        do {
            idigits[ind++] = (char_t)('0' + (i32)(ip % 10));
            ip /= 10;
        } while (ip != 0);
        if ((usize)ind + (prec > 0 || force_dot ? (usize)prec + 1 : 0) >= cap) return -1;
        for (i = ind - 1; i >= 0; i--) out[pos++] = idigits[i];
        if (prec > 0 || force_dot) {
            out[pos++] = '.';
            for (i = 0; i < prec && i < 1024; i++) out[pos++] = (char_t)('0' + fdigits[i]);
        }
        out[pos] = 0;
        return (i32)pos;
    }
}

static i32 wwa_fmt_exp_buf(real64_t v, i32 prec, i32 force_dot, char_t* out, usize cap) {
    i32 e10 = 0;
    real64_t a = v;
    usize pos = 0;
    char_t digits[1024];
    i32 i;
    if (prec < 0) prec = 6;
    if (a != 0.0) {
        real64_t t = a;
        while (t >= 10.0) { t *= 0.1; e10++; }
        while (t < 1.0) { t *= 10.0; e10--; }
        a = v * wwa_fmt_pow10(-e10);
    }
    {
        real64_t frac = a - (real64_t)(i32)a;
        u32 d0 = (u32)a;
        for (i = 0; i <= prec && i < 1024; i++) {
            frac *= 10.0;
            digits[i] = (char_t)('0' + (i32)frac);
            frac -= (real64_t)(i32)frac;
        }
        if (prec < 1024 && digits[prec] >= '5') {
            i32 j = prec - 1;
            while (j >= 1) {
                digits[j]++;
                if (digits[j] <= '9') break;
                digits[j] = '0';
                j--;
            }
            if (j <= 0) {
                d0++;
                if (d0 == 10) {
                    d0 = 1;
                    e10++;
                }
            }
        }
        if ((usize)1 + (prec > 0 || force_dot ? (usize)prec + 1 : 0) + 5 >= cap) return -1;
        out[pos++] = (char_t)('0' + d0);
        if (prec > 0 || force_dot) {
            out[pos++] = '.';
            for (i = 0; i < prec && i < 1024; i++) out[pos++] = digits[i];
        }
    }
    out[pos++] = 'e';
    out[pos++] = e10 < 0 ? '-' : '+';
    {
        i32 e = e10 < 0 ? -e10 : e10;
        char_t eb[4];
        i32 en = 0;
        if (e == 0) eb[en++] = '0';
        while (e != 0) {
            eb[en++] = (char_t)('0' + e % 10);
            e /= 10;
        }
        while (en < 2) eb[en++] = '0';
        for (i = en - 1; i >= 0; i--) out[pos++] = eb[i];
    }
    out[pos] = 0;
    return (i32)pos;
}

static i32 wwa_fmt_hexfloat_buf(real64_t v, i32 prec, i32 upper, char_t* out) {
    typedef union {
        real64_t f;
        u64 u;
    } wwa_dbl_bits_t;
    wwa_dbl_bits_t x;
    i32 pos = 0, e, i, lead;
    u64 mant;
    i32 shift;
    if (prec < 0) prec = 6;
    if (prec > 13) prec = 13;
    x.f = v;
    if (x.u & 0x8000000000000000ull) {
        out[pos++] = '-';
        x.u &= 0x7FFFFFFFFFFFFFFFull;
    }
    out[pos++] = '0';
    out[pos++] = upper ? 'X' : 'x';
    e = (i32)((x.u >> 52) & 0x7FF);
    mant = x.u & 0xFFFFFFFFFFFFFull;
    if (e == 0 && mant == 0) {
        out[pos++] = '0';
        out[pos++] = '.';
        for (i = 0; i < prec; i++) out[pos++] = '0';
        out[pos++] = upper ? 'P' : 'p';
        out[pos++] = '+';
        out[pos++] = '0';
        out[pos] = 0;
        return pos;
    }
    if (e == 0) {
        lead = 0;
        e = -1022;
    } else {
        lead = 1;
        mant |= 0x10000000000000ull;
        e -= 1023;
    }
    shift = 52 - 4 * prec;
    if (shift > 0) {
        u64 round_bit = (mant >> (shift - 1)) & 1;
        u64 rest = mant & ((1ull << (shift - 1)) - 1);
        mant >>= shift;
        if (round_bit && (rest || (mant & 1))) mant += 1;
        if (mant >= (1ull << (4 * prec + 1))) {
            mant >>= 1;
            e += 1;
            if (lead == 0 && e == -1022) lead = 1;
        }
    }
    out[pos++] = (char_t)('0' + lead);
    out[pos++] = '.';
    for (i = 4 * prec - 4; i >= 0; i -= 4) {
        u32 d = (u32)((mant >> i) & 0xF);
        out[pos++] = (char_t)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + (d - 10));
    }
    out[pos++] = upper ? 'P' : 'p';
    if (e >= 0) {
        out[pos++] = '+';
    } else {
        out[pos++] = '-';
        e = -e;
    }
    {
        char_t eb[8];
        i32 en = 0;
        do {
            eb[en++] = (char_t)('0' + e % 10);
            e /= 10;
        } while (e);
        while (en) out[pos++] = eb[--en];
    }
    out[pos] = 0;
    return pos;
}

static void wwa_fmt_float(wwa_put_fn put, void_p ctx, i64* total,
                          real64_t v, i32 spec, i32 prec, i32 width, i32 flags) {
    char_t prefix[3];
    usize plen = 0;
    i32 len = 0;
    i32 force_dot = (flags & WWA_FMT_HASH) ? 1 : 0;
    if (wwa_dbl_is_nan(v)) {
        i32 room = width - 3;
        if (!(flags & WWA_FMT_LEFT)) {
            while (room-- > 0) wwa_emit(put, ctx, total, " ", 1);
        }
        wwa_emit(put, ctx, total, "nan", 3);
        if (flags & WWA_FMT_LEFT) {
            room = width - 3;
            while (room-- > 0) wwa_emit(put, ctx, total, " ", 1);
        }
        return;
    }
    if (wwa_dbl_is_inf(v)) {
        if (v < 0) prefix[plen++] = '-';
        {
            i32 room = width - 3 - (i32)plen;
            if (!(flags & WWA_FMT_LEFT)) {
                while (room-- > 0) wwa_emit(put, ctx, total, " ", 1);
            }
            wwa_emit(put, ctx, total, prefix, plen);
            wwa_emit(put, ctx, total, "inf", 3);
            if (flags & WWA_FMT_LEFT) {
                room = width - 3 - (i32)plen;
                while (room-- > 0) wwa_emit(put, ctx, total, " ", 1);
            }
        }
        return;
    }
    if (v < 0) {
        prefix[plen++] = '-';
        v = -v;
    } else {
        if (flags & WWA_FMT_PLUS) prefix[plen++] = '+';
        else if (flags & WWA_FMT_SPACE) prefix[plen++] = ' ';
    }
    if (spec == 'g' || spec == 'G') {
        i32 e10 = 0;
        real64_t a = v;
        i32 use_exp;
        if (a != 0.0) {
            while (a >= 10.0) {
                a *= 0.1;
                e10++;
            }
            while (a < 1.0) {
                a *= 10.0;
                e10--;
            }
        }
        if (prec < 0) prec = 6;
        if (prec == 0) prec = 1;
        use_exp = (e10 < -4 || e10 >= prec) ? 1 : 0;
        if (use_exp) {
            len = wwa_fmt_exp_buf(v, prec - 1, force_dot, g_fmt_buf, sizeof(g_fmt_buf));
        } else {
            i32 dec = prec - 1 - e10;
            if (dec < 0) dec = 0;
            len = wwa_fmt_fixed_buf(v, dec, force_dot, g_fmt_buf, sizeof(g_fmt_buf));
        }
        if (len > 0 && !(flags & WWA_FMT_HASH)) {
            i32 i = len - 1;
            i32 epos = -1;
            i32 j;
            for (j = 0; j < len; j++) {
                if (g_fmt_buf[j] == 'e' || g_fmt_buf[j] == 'E') {
                    epos = j;
                    break;
                }
            }
            if (epos >= 0) {
                i = epos - 1;
                while (i > 0 && g_fmt_buf[i] == '0') i--;
                if (g_fmt_buf[i] == '.') i--;
                i++;
                wwa_memmove(g_fmt_buf + i, g_fmt_buf + epos, (usize)(len - epos));
                len = i + (len - epos);
            } else {
                while (i > 0 && g_fmt_buf[i] == '0') i--;
                if (g_fmt_buf[i] == '.') i--;
                len = i + 1;
            }
        }
        if (spec == 'G') {
            i32 i;
            for (i = 0; i < len; i++) {
                if (g_fmt_buf[i] == 'e') g_fmt_buf[i] = 'E';
            }
        }
    } else if (spec == 'e' || spec == 'E') {
        len = wwa_fmt_exp_buf(v, prec, force_dot, g_fmt_buf, sizeof(g_fmt_buf));
        if (spec == 'E') {
            i32 i;
            for (i = 0; i < len; i++) {
                if (g_fmt_buf[i] == 'e') g_fmt_buf[i] = 'E';
            }
        }
    } else if (spec == 'a' || spec == 'A') {
        len = wwa_fmt_hexfloat_buf(v, prec, spec == 'A', g_fmt_buf);
    } else {
        len = wwa_fmt_fixed_buf(v, prec, force_dot, g_fmt_buf, sizeof(g_fmt_buf));
    }
    if (len < 0) len = 0;
    {
        i32 room = width - len - (i32)plen;
        if (flags & WWA_FMT_ZERO && !(flags & WWA_FMT_LEFT) && room > 0) {
            wwa_emit(put, ctx, total, prefix, plen);
            while (room-- > 0) wwa_emit(put, ctx, total, "0", 1);
            wwa_emit(put, ctx, total, g_fmt_buf, (usize)len);
            return;
        }
        if (!(flags & WWA_FMT_LEFT)) {
            while (room-- > 0) wwa_emit(put, ctx, total, " ", 1);
        }
        wwa_emit(put, ctx, total, prefix, plen);
        wwa_emit(put, ctx, total, g_fmt_buf, (usize)len);
        if (flags & WWA_FMT_LEFT) {
            room = width - len - (i32)plen;
            while (room-- > 0) wwa_emit(put, ctx, total, " ", 1);
        }
    }
}

static i32 wwa_format(wwa_put_fn put, void_p ctx, const char_t* fmt, va_list ap) {
    i64 total = 0;
    const char_t* p = fmt;
    while (*p != 0) {
        if (*p != '%') {
            const char_t* start = p;
            while (*p != 0 && *p != '%') p++;
            wwa_emit(put, ctx, &total, start, (usize)(p - start));
            continue;
        }
        p++;
        {
            i32 flags = 0;
            i32 width = 0;
            i32 prec = -1;
            i32 got_prec = 0;
            i32 length = 0;
            char_t spec;
            for (;;) {
                if (*p == '-') { flags |= WWA_FMT_LEFT; p++; }
                else if (*p == '+') { flags |= WWA_FMT_PLUS; p++; }
                else if (*p == ' ') { flags |= WWA_FMT_SPACE; p++; }
                else if (*p == '#') { flags |= WWA_FMT_HASH; p++; }
                else if (*p == '0') { flags |= WWA_FMT_ZERO; p++; }
                else break;
            }
            if (*p == '*') {
                width = va_arg(ap, i32);
                if (width < 0) { flags |= WWA_FMT_LEFT; width = -width; }
                p++;
            } else {
                while (*p >= '0' && *p <= '9') {
                    width = width * 10 + (*p - '0');
                    p++;
                }
            }
            if (*p == '.') {
                got_prec = 1;
                p++;
                prec = 0;
                if (*p == '*') {
                    prec = va_arg(ap, i32);
                    p++;
                } else {
                    while (*p >= '0' && *p <= '9') {
                        prec = prec * 10 + (*p - '0');
                        p++;
                    }
                }
            }
            if (*p == 'h') {
                length = 'h';
                p++;
                if (*p == 'h') p++;
            } else if (*p == 'l') {
                length = 'l';
                p++;
                if (*p == 'l') p++;
            } else if (*p == 'z') {
                length = 'z';
                p++;
            } else if (*p == 't') {
                length = 't';
                p++;
            } else if (*p == 'L') {
                length = 'L';
                p++;
            }
            spec = *p;
            if (spec != 0) p++;
            if (prec < 0 && got_prec) prec = 0;
            switch (spec) {
            case 'd':
            case 'i': {
                i64 v;
                if (length == 'l' || length == 'L') v = va_arg(ap, i64);
                else if (length == 'z') v = (i64)va_arg(ap, isize);
                else if (length == 't') v = (i64)va_arg(ap, isize);
                else if (length == 'h') v = (i64)(i16)va_arg(ap, i32);
                else v = (i64)va_arg(ap, i32);
                wwa_fmt_signed(put, ctx, &total, v, 10, 0, width, flags, prec);
                break;
            }
            case 'u':
            case 'x':
            case 'X':
            case 'o': {
                u64 v;
                i32 base = (spec == 'x' || spec == 'X') ? 16 : (spec == 'o' ? 8 : 10);
                char_t prefix[2];
                usize plen = 0;
                if (length == 'l' || length == 'L') v = va_arg(ap, u64);
                else if (length == 'z') v = (u64)va_arg(ap, usize);
                else if (length == 't') v = (u64)va_arg(ap, isize);
                else if (length == 'h') v = (u64)(u16)va_arg(ap, u32);
                else v = (u64)va_arg(ap, u32);
                if ((flags & WWA_FMT_HASH) && v != 0) {
                    if (spec == 'x') { prefix[0] = '0'; prefix[1] = 'x'; plen = 2; }
                    else if (spec == 'X') { prefix[0] = '0'; prefix[1] = 'X'; plen = 2; }
                    else if (spec == 'o') { prefix[0] = '0'; plen = 1; }
                }
                wwa_fmt_uint(put, ctx, &total, v, base, (spec == 'X') ? 1 : 0, width, flags, prec, prefix, plen);
                break;
            }
            case 'c': {
                char_t c[1];
                i32 room;
                c[0] = (char_t)va_arg(ap, i32);
                room = width - 1;
                if (!(flags & WWA_FMT_LEFT)) {
                    while (room-- > 0) wwa_emit(put, ctx, &total, " ", 1);
                }
                wwa_emit(put, ctx, &total, c, 1);
                if (flags & WWA_FMT_LEFT) {
                    room = width - 1;
                    while (room-- > 0) wwa_emit(put, ctx, &total, " ", 1);
                }
                break;
            }
            case 's': {
                const char_t* s = va_arg(ap, const char_t*);
                usize slen;
                i32 room;
                if (s == NULL) s = "(null)";
                slen = wwa_strlen(s);
                if (prec >= 0 && (i32)slen > prec) slen = (usize)prec;
                room = width - (i32)slen;
                if (!(flags & WWA_FMT_LEFT)) {
                    while (room-- > 0) wwa_emit(put, ctx, &total, " ", 1);
                }
                wwa_emit(put, ctx, &total, s, slen);
                if (flags & WWA_FMT_LEFT) {
                    while (room-- > 0) wwa_emit(put, ctx, &total, " ", 1);
                }
                break;
            }
            case 'p': {
                void_p v = va_arg(ap, void_p);
                wwa_fmt_uint(put, ctx, &total, (u64)v, 16, 0, width, flags, prec, "0x", 2);
                break;
            }
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'a':
            case 'A': {
                real64_t v = va_arg(ap, real64_t);
                wwa_fmt_float(put, ctx, &total, v, spec, prec, width, flags);
                break;
            }
            case '%':
                wwa_emit(put, ctx, &total, "%", 1);
                break;
            case 'n': {
                if (length == 'l' || length == 'L') {
                    i64* p = va_arg(ap, i64*);
                    if (p != NULL) *p = total;
                } else if (length == 'h') {
                    i16* p = va_arg(ap, i16*);
                    if (p != NULL) *p = (i16)total;
                } else {
                    i32* p = va_arg(ap, i32*);
                    if (p != NULL) *p = (i32)total;
                }
                break;
            }
            default:
                if (spec != 0) {
                    char_t c[2];
                    c[0] = '%';
                    c[1] = spec;
                    wwa_emit(put, ctx, &total, c, 2);
                } else {
                    wwa_emit(put, ctx, &total, "%", 1);
                }
                break;
            }
        }
    }
    return (i32)total;
}

i32 wwa_vprintf(const char_t* fmt, va_list ap) {
    wwa_stdio_init();
    return wwa_format(wwa_stdout_put, NULL, fmt, ap);
}

i32 wwa_printf(const char_t* fmt, ...) {
    va_list ap;
    i32 r;
    va_start(ap, fmt);
    r = wwa_vprintf(fmt, ap);
    va_end(ap);
    return r;
}

i32 wwa_vsnprintf(char_t* buf, usize cap, const char_t* fmt, va_list ap) {
    wwa_buf_sink_t b;
    i32 r;
    b.buf = buf;
    b.cap = cap;
    b.len = 0;
    r = wwa_format(wwa_buf_put, &b, fmt, ap);
    if (cap > 0) buf[b.len] = 0;
    return r;
}

i32 wwa_snprintf(char_t* buf, usize cap, const char_t* fmt, ...) {
    va_list ap;
    i32 r;
    va_start(ap, fmt);
    r = wwa_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return r;
}

i32 wwa_putchar(i32 c) {
    char_t ch = (char_t)c;
    wwa_stdio_init();
    wwa_stdout_put(NULL, &ch, 1);
    return c;
}

i32 wwa_puts(const char_t* s) {
    wwa_stdio_init();
    wwa_stdout_put(NULL, s, wwa_strlen(s));
    wwa_stdout_put(NULL, "\n", 1);
    wwa_stdio_flush();
    return 0;
}

i32 wwa_display(const char_t* s) {
    wwa_stdio_init();
    wwa_stdout_put(NULL, s, wwa_strlen(s));
    wwa_stdio_flush();
    return 0;
}

i32 wwa_getchar(void) {
    if (g_in_pos >= g_in_len) {
        i32 r;
        g_in_pos = 0;
        g_in_len = 0;
        r = wwa_os_read(WWA_OS_FD_STDIN, g_in, sizeof(g_in));
        if (r <= 0) return -1;
        g_in_len = (usize)r;
    }
    return (u8)g_in[g_in_pos++];
}

int printf(const char* fmt, ...) {
    va_list ap;
    i32 r;
    va_start(ap, fmt);
    r = wwa_vprintf(fmt, ap);
    va_end(ap);
    return r;
}

int vprintf(const char* fmt, va_list ap) {
    return wwa_vprintf(fmt, ap);
}

int putchar(int c) {
    return wwa_putchar(c);
}

int puts(const char* s) {
    return wwa_puts(s);
}

int display(const char* s) {
    return wwa_display(s);
}

int getchar(void) {
    return wwa_getchar();
}