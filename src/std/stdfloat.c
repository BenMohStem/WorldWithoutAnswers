/* World Without Answers — stdfloat.c
   from-scratch quantized float formats with round-to-nearest-even.
   Format rules (OCP / NVIDIA):
     e4m3: bias 7, field 15 = max normal (448 = 0x7E), 0x7F/0xFF = NaN, no inf
     e5m2: bias 15, field 31 = inf/nan
     e3m2: bias 3, field 7 = max normal (28), no inf/nan
     e2m3: bias 1, field 3 = max normal (7.5), no inf/nan
     e2m1: bias 1, field 3 = max normal (6), no inf/nan
   MX scales are E8M0: 2^(s-127), s=0 -> 0.
   Sign bits are always preserved through quantize/dequantize
   (never masked after conversion). */

#include <stdfloat.h>
#include <stdmath.h>

typedef union {
    real32_t f;
    u32 u;
} wwa_float_bits_t;

typedef union {
    real64_t f;
    u64 u;
} wwa_double_bits_t;

static real64_t wwa_pow2_f(i32 n) {
    wwa_double_bits_t u;
    u.u = ((u64)(n + 1023) << 52);
    return u.f;
}

/* nan_mode: 0 = no inf/nan (clamp to max), 1 = NaN only (e4m3), 2 = inf+nan */
static u32 wwa_fx_quant(real32_t x, i32 eb, i32 mb, i32 bias, i32 nan_mode) {
    wwa_float_bits_t in;
    u32 sign, e32, m23, max_field, sign_bit;
    i32 e, field, sh;
    u64 frac, kept, dropped, round_bit;
    in.f = x;
    max_field = (1u << eb) - 1;
    sign_bit = 1u << (eb + mb);
    sign = (in.u & 0x80000000u) ? sign_bit : 0;
    if ((in.u & 0x7FFFFFFFu) >= 0x7F800000u) {
        u32 pat = (sign ? sign_bit : 0) | (max_field << mb);
        if (nan_mode == 2) return pat;
        return pat | ((1u << mb) - 1);
    }
    e32 = (in.u >> 23) & 0xFF;
    m23 = in.u & 0x7FFFFF;
    if (e32 == 0) {
        i32 shift = 0;
        if (m23 == 0) return sign;
        while ((m23 & 0x400000u) == 0) {
            m23 <<= 1;
            shift++;
        }
        e = -127 - shift;
        frac = 0x800000ull | (m23 & 0x3FFFFFull);
    } else {
        e = (i32)e32 - 127;
        frac = 0x800000ull | m23;
    }
    field = e + bias;
    if (field <= 0) {
        i32 es = 1 - field;
        sh = 23 - mb + es;
        if (sh >= 24) return sign;
        kept = frac >> sh;
        dropped = frac & ((1ull << sh) - 1);
        round_bit = 1ull << (sh - 1);
        if ((dropped & round_bit) && ((dropped & (round_bit - 1)) != 0 || (kept & 1))) kept++;
        if (kept == 0) return sign;
        if (kept < (1ull << mb)) return sign | (u32)kept;
        field = 1;
        kept = 0;
    } else {
        sh = 23 - mb;
        kept = frac >> sh;
        dropped = frac & ((1ull << sh) - 1);
        round_bit = 1ull << (sh - 1);
        if ((dropped & round_bit) && ((dropped & (round_bit - 1)) != 0 || (kept & 1))) kept++;
        if (kept >= (1ull << (mb + 1))) {
            kept = 0;
            field += 1;
        }
    }
    if (field > (i32)max_field || (nan_mode == 2 && field == (i32)max_field)) {
        u32 pat = (sign ? sign_bit : 0) | (max_field << mb);
        if (nan_mode == 2) return pat;
        return pat | ((1u << mb) - 1);
    }
    if (nan_mode == 1 && field == (i32)max_field && kept == (1ull << (mb + 1)) - 1) {
        return sign_bit | (max_field << mb) | ((1u << mb) - 1);
    }
    return sign | ((u32)field << mb) | ((u32)kept & ((1u << mb) - 1));
}

static real32_t wwa_fx_dequant(u32 bits, i32 eb, i32 mb, i32 bias, i32 nan_mode) {
    u32 sign = (bits >> (eb + mb)) & 1u;
    u32 e = (bits >> mb) & ((1u << eb) - 1);
    u32 m = bits & ((1u << mb) - 1);
    real64_t v;
    if (e == 0) {
        v = (real64_t)m * 0.5;
        v = v / (real64_t)(1u << (mb - 1));
        v = v / (real64_t)(1u << (bias - 1));
    } else if (e == (u32)((1u << eb) - 1)) {
        if (nan_mode == 2) {
            if (m != 0) {
                wwa_float_bits_t nan;
                nan.u = 0x7FC00000u;
                return nan.f;
            }
            wwa_float_bits_t inf;
            inf.u = 0x7F800000u | (sign << 31);
            return inf.f;
        }
        if (nan_mode == 1 && m == (u32)((1u << mb) - 1)) {
            wwa_float_bits_t nan;
            nan.u = 0x7FC00000u;
            return nan.f;
        }
        v = (1.0 + (real64_t)m / (real64_t)(1u << mb));
        v = v * wwa_pow2_f((i32)e - bias);
    } else {
        v = (1.0 + (real64_t)m / (real64_t)(1u << mb));
        v = v * wwa_pow2_f((i32)e - bias);
    }
    return sign ? (real32_t)(-v) : (real32_t)v;
}

u16 wwa_f16_from_f32(real32_t x) {
    return (u16)wwa_fx_quant(x, 5, 10, 15, 2);
}

real32_t wwa_f16_to_f32(u16 h) {
    return wwa_fx_dequant(h, 5, 10, 15, 2);
}

u16 wwa_bf16_from_f32(real32_t x) {
    return (u16)wwa_fx_quant(x, 8, 7, 127, 2);
}

real32_t wwa_bf16_to_f32(u16 b) {
    return wwa_fx_dequant(b, 8, 7, 127, 2);
}

u8 wwa_fp8_e4m3_from_f32(real32_t x) {
    return (u8)wwa_fx_quant(x, 4, 3, 7, 1);
}

real32_t wwa_fp8_e4m3_to_f32(u8 h) {
    return wwa_fx_dequant(h, 4, 3, 7, 1);
}

u8 wwa_fp8_e5m2_from_f32(real32_t x) {
    return (u8)wwa_fx_quant(x, 5, 2, 15, 2);
}

real32_t wwa_fp8_e5m2_to_f32(u8 h) {
    return wwa_fx_dequant(h, 5, 2, 15, 2);
}

u8 wwa_fp6_e3m2_from_f32(real32_t x) {
    return (u8)wwa_fx_quant(x, 3, 2, 3, 0);
}

real32_t wwa_fp6_e3m2_to_f32(u8 h) {
    return wwa_fx_dequant(h, 3, 2, 3, 0);
}

u8 wwa_fp6_e2m3_from_f32(real32_t x) {
    return (u8)wwa_fx_quant(x, 2, 3, 1, 0);
}

real32_t wwa_fp6_e2m3_to_f32(u8 h) {
    return wwa_fx_dequant(h, 2, 3, 1, 0);
}

u8 wwa_fp4_e2m1_from_f32(real32_t x) {
    return (u8)wwa_fx_quant(x, 2, 1, 1, 0);
}

real32_t wwa_fp4_e2m1_to_f32(u8 h) {
    return wwa_fx_dequant(h, 2, 1, 1, 0);
}

u32 wwa_fp6_pack_4(real32_t a, real32_t b, real32_t c, real32_t d) {
    u32 p = (u32)wwa_fp6_e3m2_from_f32(a);
    p |= (u32)wwa_fp6_e3m2_from_f32(b) << 6;
    p |= (u32)wwa_fp6_e3m2_from_f32(c) << 12;
    p |= (u32)wwa_fp6_e3m2_from_f32(d) << 18;
    return p;
}

void wwa_fp6_unpack_4(u32 p, real32_t* a, real32_t* b, real32_t* c, real32_t* d) {
    if (a) *a = wwa_fp6_e3m2_to_f32((u8)(p & 0x3F));
    if (b) *b = wwa_fp6_e3m2_to_f32((u8)((p >> 6) & 0x3F));
    if (c) *c = wwa_fp6_e3m2_to_f32((u8)((p >> 12) & 0x3F));
    if (d) *d = wwa_fp6_e3m2_to_f32((u8)((p >> 18) & 0x3F));
}

u8 wwa_fp4_pack_2(real32_t a, real32_t b) {
    return (u8)((u8)wwa_fp4_e2m1_from_f32(a) | ((u8)wwa_fp4_e2m1_from_f32(b) << 4));
}

void wwa_fp4_unpack_2(u8 p, real32_t* a, real32_t* b) {
    if (a) *a = wwa_fp4_e2m1_to_f32((u8)(p & 0xF));
    if (b) *b = wwa_fp4_e2m1_to_f32((u8)((p >> 4) & 0xF));
}

real32_t wwa_mx_scale_to_f32(u8 s) {
    if (s == 0) return 0.0f;
    if (s == 255) {
        wwa_float_bits_t inf;
        inf.u = 0x7F800000u;
        return inf.f;
    }
    return (real32_t)wwa_pow2_f((i32)s - 127);
}

u8 wwa_mx_scale_from_f32(real32_t s) {
    i32 k;
    if (s == 0.0f) return 0;
    if (s != s || s < 0.0f) return 255;
    {
        wwa_float_bits_t in;
        in.f = s;
        k = (i32)((in.u >> 23) & 0xFF) - 127;
    }
    k += 127;
    if (k > 254) k = 254;
    if (k < 1) k = 1;
    return (u8)k;
}

/* tightest scale exponent s such that max_abs <= fmax * 2^s */
static i32 wwa_mx_scale_exp_for_max(real32_t max_abs, real32_t fmax) {
    i32 s;
    real64_t est;
    wwa_float_bits_t t, f;
    t.f = max_abs;
    f.f = fmax;
    if ((t.u & 0x7FFFFFFFu) == 0) return 0;
    s = ((i32)((t.u >> 23) & 0xFF) - 127) - ((i32)((f.u >> 23) & 0xFF) - 127);
    if (s > 127) s = 127;
    if (s < -126) s = -126;
    est = wwa_pow2_f(s) * (real64_t)fmax;
    while (est < (real64_t)max_abs) {
        s++;
        est = wwa_pow2_f(s) * (real64_t)fmax;
    }
    while (s > -126 && wwa_pow2_f(s - 1) * (real64_t)fmax >= (real64_t)max_abs) {
        s--;
    }
    return s;
}

static i32 wwa_mx_quant_common(const real32_t* in, usize n, u8* out, u8* scales,
                               usize block, real32_t fmax,
                               u8 (*q)(real32_t)) {
    usize i;
    if (block == 0) block = WWA_MX_BLOCK_DEFAULT;
    for (i = 0; i < n; i += block) {
        usize j, count = (n - i < block) ? (n - i) : block;
        real32_t max_abs = 0.0f;
        i32 k;
        real32_t scale;
        for (j = 0; j < count; j++) {
            real32_t a = wwa_fabsf(in[i + j]);
            if (a > max_abs) max_abs = a;
        }
        if (max_abs == 0.0f) {
            scales[i / block] = 0;
            for (j = 0; j < count; j++) out[i + j] = 0;
            continue;
        }
        k = wwa_mx_scale_exp_for_max(max_abs, fmax);
        scales[i / block] = (u8)(k + 127);
        scale = (real32_t)wwa_pow2_f(k);
        for (j = 0; j < count; j++) {
            out[i + j] = q(in[i + j] / scale);
        }
    }
    return (i32)((n + block - 1) / block);
}

static i32 wwa_mx_dequant_common(const u8* in, const u8* scales, usize n, real32_t* out,
                                 usize block, real32_t (*dq)(u8)) {
    usize i;
    if (block == 0) block = WWA_MX_BLOCK_DEFAULT;
    for (i = 0; i < n; i += block) {
        usize j, count = (n - i < block) ? (n - i) : block;
        real32_t scale = wwa_mx_scale_to_f32(scales[i / block]);
        for (j = 0; j < count; j++) {
            out[i + j] = dq(in[i + j]) * scale;
        }
    }
    return 0;
}

i32 wwa_mxfp8_e4m3_quantize(const real32_t* in, usize n, u8* out, u8* scales, usize block) {
    return wwa_mx_quant_common(in, n, out, scales, block, 448.0f, wwa_fp8_e4m3_from_f32);
}

i32 wwa_mxfp8_e4m3_dequantize(const u8* in, const u8* scales, usize n, real32_t* out, usize block) {
    return wwa_mx_dequant_common(in, scales, n, out, block, wwa_fp8_e4m3_to_f32);
}

i32 wwa_mxfp8_e5m2_quantize(const real32_t* in, usize n, u8* out, u8* scales, usize block) {
    return wwa_mx_quant_common(in, n, out, scales, block, 57344.0f, wwa_fp8_e5m2_from_f32);
}

i32 wwa_mxfp8_e5m2_dequantize(const u8* in, const u8* scales, usize n, real32_t* out, usize block) {
    return wwa_mx_dequant_common(in, scales, n, out, block, wwa_fp8_e5m2_to_f32);
}

i32 wwa_mxfp6_e3m2_quantize(const real32_t* in, usize n, u8* out, u8* scales, usize block) {
    usize i;
    u32 pack_buf[4];
    usize pack_idx = 0;
    u8* pout = out;
    if (block == 0) block = WWA_MX_BLOCK_DEFAULT;
    for (i = 0; i < n; i += block) {
        usize j, count = (n - i < block) ? (n - i) : block;
        real32_t max_abs = 0.0f;
        i32 k;
        real32_t scale;
        for (j = 0; j < count; j++) {
            real32_t a = wwa_fabsf(in[i + j]);
            if (a > max_abs) max_abs = a;
        }
        if (max_abs == 0.0f) {
            scales[i / block] = 0;
            pack_idx = 0;
            continue;
        }
        k = wwa_mx_scale_exp_for_max(max_abs, 28.0f);
        scales[i / block] = (u8)(k + 127);
        scale = (real32_t)wwa_pow2_f(k);
        pack_idx = 0;
        for (j = 0; j < count; j++) {
            pack_buf[pack_idx++] = (u32)wwa_fp6_e3m2_from_f32(in[i + j] / scale);
            if (pack_idx == 4) {
                u32 p = pack_buf[0] | (pack_buf[1] << 6) | (pack_buf[2] << 12) | (pack_buf[3] << 18);
                pout[0] = (u8)(p & 0xFF);
                pout[1] = (u8)((p >> 8) & 0xFF);
                pout[2] = (u8)((p >> 16) & 0xFF);
                pout += 3;
                pack_idx = 0;
            }
        }
        if (pack_idx != 0) {
            u32 p = 0;
            usize t;
            for (t = 0; t < 4; t++) {
                p |= (pack_buf[t] & 0x3Fu) << (6 * t);
            }
            pout[0] = (u8)(p & 0xFF);
            pout[1] = (u8)((p >> 8) & 0xFF);
            pout[2] = (u8)((p >> 16) & 0xFF);
            pout += 3;
        }
    }
    return (i32)((n + block - 1) / block);
}

i32 wwa_mxfp6_e3m2_dequantize(const u8* in, const u8* scales, usize n, real32_t* out, usize block) {
    usize i;
    if (block == 0) block = WWA_MX_BLOCK_DEFAULT;
    for (i = 0; i < n; i += block) {
        usize j, count = (n - i < block) ? (n - i) : block;
        real32_t scale = wwa_mx_scale_to_f32(scales[i / block]);
        usize bidx = (i / block) * (block * 3 / 4);
        for (j = 0; j < count; j += 4) {
            usize k2, cnt = (count - j < 4) ? (count - j) : 4;
            u32 p = (u32)in[bidx] | ((u32)in[bidx + 1] << 8) | ((u32)in[bidx + 2] << 16);
            bidx += 3;
            for (k2 = 0; k2 < cnt; k2++) {
                u8 h = (u8)((p >> (6 * k2)) & 0x3F);
                out[i + j + k2] = wwa_fp6_e3m2_to_f32(h) * scale;
            }
        }
    }
    return 0;
}

i32 wwa_mxfp4_e2m1_quantize(const real32_t* in, usize n, u8* out, u8* scales, usize block) {
    usize i;
    if (block == 0) block = WWA_MX_BLOCK_DEFAULT;
    for (i = 0; i < n; i += block) {
        usize j, count = (n - i < block) ? (n - i) : block;
        real32_t max_abs = 0.0f;
        i32 k;
        real32_t scale;
        for (j = 0; j < count; j++) {
            real32_t a = wwa_fabsf(in[i + j]);
            if (a > max_abs) max_abs = a;
        }
        if (max_abs == 0.0f) {
            scales[i / block] = 0;
            continue;
        }
        k = wwa_mx_scale_exp_for_max(max_abs, 6.0f);
        scales[i / block] = (u8)(k + 127);
        scale = (real32_t)wwa_pow2_f(k);
        for (j = 0; j < count; j += 2) {
            u8 lo = wwa_fp4_e2m1_from_f32(in[i + j] / scale);
            u8 hi = (j + 1 < count) ? wwa_fp4_e2m1_from_f32(in[i + j + 1] / scale) : 0;
            out[(i + j) / 2] = (u8)(lo | (hi << 4));
        }
    }
    return (i32)((n + block - 1) / block);
}

i32 wwa_mxfp4_e2m1_dequantize(const u8* in, const u8* scales, usize n, real32_t* out, usize block) {
    usize i;
    if (block == 0) block = WWA_MX_BLOCK_DEFAULT;
    for (i = 0; i < n; i += block) {
        usize j, count = (n - i < block) ? (n - i) : block;
        real32_t scale = wwa_mx_scale_to_f32(scales[i / block]);
        for (j = 0; j < count; j += 2) {
            u8 p = in[(i + j) / 2];
            out[i + j] = wwa_fp4_e2m1_to_f32((u8)(p & 0xF)) * scale;
            if (j + 1 < count) out[i + j + 1] = wwa_fp4_e2m1_to_f32((u8)((p >> 4) & 0xF)) * scale;
        }
    }
    return 0;
}

i32 wwa_nvfp4_quantize(const real32_t* in, usize n, u8* out, u8* scales) {
    usize i;
    for (i = 0; i < n; i += WWA_NVFP4_BLOCK) {
        usize j, count = (n - i < WWA_NVFP4_BLOCK) ? (n - i) : WWA_NVFP4_BLOCK;
        real32_t max_abs = 0.0f;
        i32 k;
        real32_t scale;
        for (j = 0; j < count; j++) {
            real32_t a = wwa_fabsf(in[i + j]);
            if (a > max_abs) max_abs = a;
        }
        if (max_abs == 0.0f) {
            scales[i / WWA_NVFP4_BLOCK] = 0;
            continue;
        }
        k = wwa_mx_scale_exp_for_max(max_abs, 6.0f);
        scales[i / WWA_NVFP4_BLOCK] = (u8)(k + 127);
        scale = (real32_t)wwa_pow2_f(k);
        for (j = 0; j < count; j += 2) {
            u8 lo = wwa_fp4_e2m1_from_f32(in[i + j] / scale);
            u8 hi = (j + 1 < count) ? wwa_fp4_e2m1_from_f32(in[i + j + 1] / scale) : 0;
            out[(i + j) / 2] = (u8)(lo | (hi << 4));
        }
    }
    return (i32)((n + WWA_NVFP4_BLOCK - 1) / WWA_NVFP4_BLOCK);
}

i32 wwa_nvfp4_dequantize(const u8* in, const u8* scales, usize n, real32_t* out) {
    usize i;
    for (i = 0; i < n; i += WWA_NVFP4_BLOCK) {
        usize j, count = (n - i < WWA_NVFP4_BLOCK) ? (n - i) : WWA_NVFP4_BLOCK;
        real32_t scale = wwa_mx_scale_to_f32(scales[i / WWA_NVFP4_BLOCK]);
        for (j = 0; j < count; j += 2) {
            u8 p = in[(i + j) / 2];
            out[i + j] = wwa_fp4_e2m1_to_f32((u8)(p & 0xF)) * scale;
            if (j + 1 < count) out[i + j + 1] = wwa_fp4_e2m1_to_f32((u8)((p >> 4) & 0xF)) * scale;
        }
    }
    return 0;
}