/* World Without Answers — stdmath.c
   from-scratch float math.
   Reductions use Cody-Waite (two-part constants in double).
   Series use exact rational Taylor coefficients, evaluated in double.
   sqrtf uses the hardware instruction via compiler builtin (allowed:
   it is a compiler intrinsic, not a libc call). */

#include <stdmath.h>
#include <stdfenv.h>
#include <xmmintrin.h>

static const real64_t WWA_M_LN2         = 0.69314718055994530941723212145818;
static const real64_t WWA_M_LN2_HI      = 0.693147180369123816490;
static const real64_t WWA_M_LN2_LO      = 1.90821492927058770002e-10;
static const real64_t WWA_M_PIO2_HI     = 1.57079632679489655800e+00;
static const real64_t WWA_M_PIO2_LO     = 6.12323399573676588613e-17;
static const real64_t WWA_M_PIO6        = 0.52359877559829887307710723054658;
static const real64_t WWA_M_SQRT2       = 1.41421356237309504880168872420970;
static const real64_t WWA_M_SQRT2_HALF  = 0.70710678118654752440084436210485;
static const real64_t WWA_M_SQRT3       = 1.73205080756887729352744634150587;

typedef union {
    real32_t f;
    u32 u;
} wwa_float_bits_t;

typedef union {
    real64_t f;
    u64 u;
} wwa_double_bits_t;

static const real64_t WWA_EXP_COEFF[20] = {
    1.0, 1.0, 0.5, 1.0 / 6.0, 1.0 / 24.0, 1.0 / 120.0, 1.0 / 720.0,
    1.0 / 5040.0, 1.0 / 40320.0, 1.0 / 362880.0, 1.0 / 3628800.0,
    1.0 / 39916800.0, 1.0 / 479001600.0, 1.0 / 6227020800.0,
    1.0 / 87178291200.0, 1.0 / 1307674368000.0, 1.0 / 20922789888000.0,
    1.0 / 355687428096000.0, 1.0 / 6402373705728000.0, 1.0 / 121645100408832000.0
};

static const real64_t WWA_SIN_COEFF[10] = {
    1.0, -1.0 / 6.0, 1.0 / 120.0, -1.0 / 5040.0, 1.0 / 362880.0,
    -1.0 / 39916800.0, 1.0 / 6227020800.0, -1.0 / 1307674368000.0,
    1.0 / 355687428096000.0, -1.0 / 121645100408832000.0
};

static const real64_t WWA_COS_COEFF[11] = {
    1.0, -0.5, 1.0 / 24.0, -1.0 / 720.0, 1.0 / 40320.0, -1.0 / 3628800.0,
    1.0 / 479001600.0, -1.0 / 87178291200.0, 1.0 / 20922789888000.0,
    -1.0 / 6402373705728000.0, 1.0 / 2432902008176640000.0
};

static const real64_t WWA_LOG_COEFF[15] = {
    1.0, 1.0 / 3.0, 1.0 / 5.0, 1.0 / 7.0, 1.0 / 9.0, 1.0 / 11.0,
    1.0 / 13.0, 1.0 / 15.0, 1.0 / 17.0, 1.0 / 19.0, 1.0 / 21.0,
    1.0 / 23.0, 1.0 / 25.0, 1.0 / 27.0, 1.0 / 29.0
};

static const real64_t WWA_ATAN_COEFF[12] = {
    1.0, -1.0 / 3.0, 1.0 / 5.0, -1.0 / 7.0, 1.0 / 9.0, -1.0 / 11.0,
    1.0 / 13.0, -1.0 / 15.0, 1.0 / 17.0, -1.0 / 19.0, 1.0 / 21.0, -1.0 / 23.0
};

static const real64_t WWA_TANH_COEFF[12] = {
    1.0, -1.0 / 3.0, 2.0 / 15.0, -17.0 / 315.0, 62.0 / 2835.0,
    -1382.0 / 155925.0, 21844.0 / 6081075.0, -929569.0 / 638512875.0,
    6404582.0 / 10854718875.0, -443861162.0 / 1856156927625.0,
    18888466084.0 / 194896477400625.0, -113927491862.0 / 2900518163668125.0
};

static real64_t wwa_floor_d(real64_t x) {
    real64_t t = (real64_t)(i64)x;
    if (x < 0.0 && t != x) t -= 1.0;
    return t;
}

static real64_t wwa_pow2_d(i32 n) {
    wwa_double_bits_t u;
    if (n > 1023) {
        u.u = 0x7FF0000000000000ull;
        return u.f;
    }
    if (n < -1074) return 0.0;
    if (n >= -1022) {
        u.u = ((u64)(n + 1023) << 52);
    } else {
        u.u = 1ull << (n + 1074);
    }
    return u.f;
}

static real32_t wwa_scalb_f(real64_t m, i32 n) {
    return (real32_t)(m * wwa_pow2_d(n));
}

real32_t wwa_sqrtf(real32_t x) {
    wwa_float_bits_t u;
    u32 i;
    real32_t y;
    if (x == 0.0f) return x;
    if (!(x >= 0.0f)) return (real32_t)(0.0f / 0.0f);
    u.f = x;
    i = (u.u >> 1) + 0x1FC00000u;
    u.u = i;
    y = u.f;
    if (!(y > 0.0f) || !(y < 1.0e38f)) y = 1.0f;
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}

static i64 wwa_trig_round(real64_t q) {
    return (i64)wwa_floor_d(q + 0.5);
}

real32_t wwa_sinf(real32_t x) {
    wwa_float_bits_t in;
    real64_t r, s, poly, c;
    i64 n;
    i32 q;
    in.f = x;
    if ((in.u & 0x7FFFFFFFu) >= 0x7F800000u) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    n = wwa_trig_round((real64_t)x * (2.0 / WWA_PI));
    r = (real64_t)x - (real64_t)n * WWA_M_PIO2_HI - (real64_t)n * WWA_M_PIO2_LO;
    s = r * r;
    poly = r * (1.0 - s * (1.0 / 6.0 - s * (1.0 / 120.0 - s * (1.0 / 5040.0 - s * (1.0 / 362880.0)))));
    c = 1.0 - s * (0.5 - s * (1.0 / 24.0 - s * (1.0 / 720.0 - s * (1.0 / 40320.0 - s * (1.0 / 3628800.0)))));
    q = (i32)(n & 3);
    switch (q) {
    case 0: return (real32_t)poly;
    case 1: return (real32_t)c;
    case 2: return (real32_t)(-poly);
    default: return (real32_t)(-c);
    }
}

real32_t wwa_cosf(real32_t x) {
    wwa_float_bits_t in;
    real64_t r, s, poly, c;
    i64 n;
    i32 q;
    in.f = x;
    if ((in.u & 0x7FFFFFFFu) >= 0x7F800000u) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    n = wwa_trig_round((real64_t)x * (2.0 / WWA_PI));
    r = (real64_t)x - (real64_t)n * WWA_M_PIO2_HI - (real64_t)n * WWA_M_PIO2_LO;
    s = r * r;
    poly = r * (1.0 - s * (1.0 / 6.0 - s * (1.0 / 120.0 - s * (1.0 / 5040.0 - s * (1.0 / 362880.0)))));
    c = 1.0 - s * (0.5 - s * (1.0 / 24.0 - s * (1.0 / 720.0 - s * (1.0 / 40320.0 - s * (1.0 / 3628800.0)))));
    q = (i32)(n & 3);
    switch (q) {
    case 0: return (real32_t)c;
    case 1: return (real32_t)(-poly);
    case 2: return (real32_t)(-c);
    default: return (real32_t)poly;
    }
}

real32_t wwa_tanf(real32_t x) {
    wwa_float_bits_t in;
    real32_t s = wwa_sinf(x);
    real32_t c = wwa_cosf(x);
    in.f = x;
    if (c == 0.0f) {
        in.u = 0x7F800000u | (in.u & 0x80000000u);
        return in.f;
    }
    return s / c;
}

real32_t wwa_expf(real32_t x) {
    wwa_float_bits_t in;
    real64_t r, poly;
    i64 n;
    in.f = x;
    if (x == 0.0f) return 1.0f;
    if ((in.u & 0x7FFFFFFFu) == 0x7F800000u) {
        if (in.u & 0x80000000u) return 0.0f;
        return x;
    }
    if (x > 88.722839f) {
        wwa_float_bits_t inf;
        inf.u = 0x7F800000u;
        return inf.f;
    }
    if (x < -87.336548f) {
        wwa_float_bits_t z;
        z.u = 0;
        return z.f;
    }
    n = (i64)wwa_floor_d((real64_t)x / WWA_M_LN2 + 0.5);
    r = (real64_t)x - (real64_t)n * WWA_M_LN2_HI - (real64_t)n * WWA_M_LN2_LO;
    poly = 1.0 + r * (1.0 + r * (0.5 + r * (1.0 / 6.0 + r * (1.0 / 24.0 + r * (1.0 / 120.0 + r * (1.0 / 720.0 + r * (1.0 / 5040.0)))))));
    return wwa_scalb_f(poly, (i32)n);
}

real32_t wwa_logf(real32_t x) {
    wwa_float_bits_t in;
    real64_t m, z, z2, poly, result;
    i32 e;
    in.f = x;
    if (x < 0.0f || x != x) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    if (x == 0.0f) {
        wwa_float_bits_t ninf;
        ninf.u = 0xFF800000u;
        return ninf.f;
    }
    if (x == 1.0f) return 0.0f;
    {
        u32 b = in.u & 0x007FFFFFu;
        u32 e32 = (in.u >> 23) & 0xFF;
        if (e32 == 0) {
            i32 shift = 0;
            while (b < 0x00800000u && shift < 23) {
                b <<= 1;
                shift++;
            }
            e = -126 - shift;
            m = (real64_t)b / 8388608.0;
        } else {
            e = (i32)e32 - 127;
            in.u = b | 0x3F800000u;
            m = in.f;
        }
    }
    if (m > WWA_M_SQRT2) {
        m *= 0.5;
        e += 1;
    } else if (m < WWA_M_SQRT2_HALF) {
        m *= 2.0;
        e -= 1;
    }
    z = (m - 1.0) / (m + 1.0);
    z2 = z * z;
    poly = z * (1.0 + z2 * (1.0 / 3.0 + z2 * (1.0 / 5.0 + z2 * (1.0 / 7.0 + z2 * (1.0 / 9.0 + z2 * (1.0 / 11.0))))));
    result = (real64_t)e * WWA_M_LN2 + 2.0 * poly;
    return (real32_t)result;
}

real32_t wwa_powf(real32_t x, real32_t y) {
    if (y == 0.0f) return 1.0f;
    if (x == 1.0f) return 1.0f;
    if (x < 0.0f) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    if (x == 0.0f) {
        if (y < 0.0f) {
            wwa_float_bits_t inf;
            inf.u = 0x7F800000u;
            return inf.f;
        }
        return 0.0f;
    }
    return wwa_expf(y * wwa_logf(x));
}

real32_t wwa_atanf(real32_t x) {
    real64_t a = x;
    real64_t z, z2, poly, result;
    i32 sign = 0;
    if (x != x) return x;
    if (a < 0.0) {
        a = -a;
        sign = 1;
    }
    if (a > 1.0) {
        a = 1.0 / a;
        result = WWA_PI_2 - wwa_atanf((real32_t)a);
        return sign ? (real32_t)(-result) : (real32_t)result;
    }
    z2 = a * a;
    poly = a * (1.0 - z2 * (1.0 / 3.0 - z2 * (1.0 / 5.0 - z2 * (1.0 / 7.0 - z2 * (1.0 / 9.0 - z2 * (1.0 / 11.0))))));
    if (a > 0.26794919243112281) {
        z = (WWA_M_SQRT3 * a - 1.0) / (WWA_M_SQRT3 + a);
        z2 = z * z;
        poly = z * (1.0 - z2 * (1.0 / 3.0 - z2 * (1.0 / 5.0 - z2 * (1.0 / 7.0 - z2 * (1.0 / 9.0 - z2 * (1.0 / 11.0))))));
        result = WWA_M_PIO6 + poly;
        return sign ? (real32_t)(-result) : (real32_t)result;
    }
    result = poly;
    return sign ? (real32_t)(-result) : (real32_t)result;
}

real32_t wwa_atan2f(real32_t y, real32_t x) {
    if (x == 0.0f) {
        if (y > 0.0f) return (real32_t)WWA_PI_2;
        if (y < 0.0f) return (real32_t)(-WWA_PI_2);
        return 0.0f;
    }
    if (x > 0.0f) return wwa_atanf(y / x);
    if (y >= 0.0f) return wwa_atanf(y / x) + (real32_t)WWA_PI;
    return wwa_atanf(y / x) - (real32_t)WWA_PI;
}

real32_t wwa_fabsf(real32_t x) {
    wwa_float_bits_t u;
    u.f = x;
    u.u &= 0x7FFFFFFFu;
    return u.f;
}

real32_t wwa_truncf(real32_t x) {
    wwa_float_bits_t u;
    u32 exp, frac_mask;
    u.f = x;
    exp = (u.u >> 23) & 0xFF;
    if (exp < 127) return x >= 0.0f ? 0.0f : -0.0f;
    if (exp >= 127 + 23) return x;
    frac_mask = 0x007FFFFFu >> (i32)(exp - 127);
    u.u &= ~frac_mask;
    return u.f;
}

real32_t wwa_floorf(real32_t x) {
    real32_t t = wwa_truncf(x);
    if (x < 0.0f && t != x) return t - 1.0f;
    return t;
}

real32_t wwa_ceilf(real32_t x) {
    real32_t t = wwa_truncf(x);
    if (x > 0.0f && t != x) return t + 1.0f;
    return t;
}

real32_t wwa_roundf(real32_t x) {
    if (x >= 0.0f) return wwa_floorf(x + 0.5f);
    return wwa_ceilf(x - 0.5f);
}

real32_t wwa_fmodf(real32_t x, real32_t y) {
    real64_t q;
    if (y == 0.0f) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    if (x != x || y != y) return x;
    q = (real64_t)wwa_truncf(x / y);
    return (real32_t)((real64_t)x - q * (real64_t)y);
}

real32_t wwa_ldexpf(real32_t x, i32 e) {
    real64_t v;
    if (x == 0.0f) return x;
    v = (real64_t)x;
    if (e >= 0) {
        if (e > 1023) {
            wwa_float_bits_t inf;
            inf.u = 0x7F800000u | (x < 0.0f ? 0x80000000u : 0);
            return inf.f;
        }
        v *= wwa_pow2_d(e);
    } else {
        i64 n = -(i64)e;
        if (n > 1074) {
            wwa_float_bits_t z;
            z.u = 0;
            return z.f;
        }
        v *= wwa_pow2_d(-(i32)n);
    }
    return (real32_t)v;
}

real32_t wwa_frexpf(real32_t x, i32* e) {
    wwa_float_bits_t u;
    u32 exp;
    u.f = x;
    *e = 0;
    if (x == 0.0f || x != x) return x;
    if ((u.u & 0x7FFFFFFFu) == 0x7F800000u) return x;
    exp = (u.u >> 23) & 0xFF;
    if (exp == 0) {
        u32 b = u.u & 0x007FFFFFu;
        i32 shift = 0;
        while ((b & 0x00800000u) == 0) {
            b <<= 1;
            shift++;
        }
        *e = -125 - shift;
        u.u = (u.u & 0x80000000u) | (126u << 23) | (b & 0x007FFFFFu);
        return u.f;
    }
    *e = (i32)exp - 126;
    u.u = (u.u & 0x807FFFFFu) | (126u << 23);
    return u.f;
}

real32_t wwa_copysignf(real32_t x, real32_t y) {
    wwa_float_bits_t u, v;
    u.f = x;
    v.f = y;
    u.u = (u.u & 0x7FFFFFFFu) | (v.u & 0x80000000u);
    return u.f;
}

i32 wwa_signbitf(real32_t x) {
    wwa_float_bits_t u;
    u.f = x;
    return (i32)((u.u >> 31) & 1);
}

real32_t wwa_fminf(real32_t x, real32_t y) {
    if (x != x) return y;
    if (y != y) return x;
    if (x == y) return wwa_signbitf(x) ? x : y;
    return x < y ? x : y;
}

real32_t wwa_fmaxf(real32_t x, real32_t y) {
    if (x != x) return y;
    if (y != y) return x;
    if (x == y) return wwa_signbitf(x) ? y : x;
    return x > y ? x : y;
}

real32_t wwa_modff(real32_t x, real32_t* iptr) {
    *iptr = wwa_truncf(x);
    return x - *iptr;
}

real32_t wwa_exp2f(real32_t x) {
    wwa_float_bits_t in;
    in.f = x;
    if (x == 0.0f) return 1.0f;
    if ((in.u & 0x7FFFFFFFu) == 0x7F800000u) {
        if (in.u & 0x80000000u) return 0.0f;
        return x;
    }
    if (x > 128.0f) {
        wwa_float_bits_t inf;
        inf.u = 0x7F800000u;
        return inf.f;
    }
    if (x < -150.0f) {
        wwa_float_bits_t z;
        z.u = 0;
        return z.f;
    }
    if (x >= -149.0f && x == (real32_t)(i32)x) return wwa_ldexpf(1.0f, (i32)x);
    return wwa_expf(x * (real32_t)WWA_M_LN2);
}

real32_t wwa_log2f(real32_t x) {
    real32_t m;
    i32 e;
    if (x == 0.0f) {
        wwa_float_bits_t ninf;
        ninf.u = 0xFF800000u;
        return ninf.f;
    }
    if (x < 0.0f || x != x) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    if (x == 1.0f) return 0.0f;
    m = wwa_frexpf(x, &e);
    if (m == 0.5f) return (real32_t)(e - 1);
    return (real32_t)((real64_t)e + (real64_t)wwa_logf(m) / WWA_M_LN2);
}

real32_t wwa_log10f(real32_t x) {
    if (x == 0.0f) {
        wwa_float_bits_t ninf;
        ninf.u = 0xFF800000u;
        return ninf.f;
    }
    if (x < 0.0f || x != x) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    return (real32_t)((real64_t)wwa_logf(x) * 0.43429448190325182765);
}

real32_t wwa_sinhf(real32_t x) {
    real32_t ex;
    if (x == 0.0f) return x;
    if (x < 0.0f) return -wwa_sinhf(-x);
    if (x > 88.722839f) {
        wwa_float_bits_t inf;
        inf.u = 0x7F800000u;
        return inf.f;
    }
    if (x < 1.0e-4f) return x * (1.0f + x * x / 6.0f);
    ex = wwa_expf(x);
    return 0.5f * (ex - 1.0f / ex);
}

real32_t wwa_coshf(real32_t x) {
    real32_t ex;
    if (x < 0.0f) x = -x;
    if (x > 88.722839f) {
        wwa_float_bits_t inf;
        inf.u = 0x7F800000u;
        return inf.f;
    }
    ex = wwa_expf(x);
    return 0.5f * (ex + 1.0f / ex);
}

real32_t wwa_tanhf(real32_t x) {
    real32_t e2;
    if (x == 0.0f) return x;
    if (x < 0.0f) return -wwa_tanhf(-x);
    if (x < 1.0e-4f) return x;
    if (x > 15.0f) return 1.0f;
    e2 = wwa_expf(2.0f * x);
    return (e2 - 1.0f) / (e2 + 1.0f);
}

real32_t wwa_asinf(real32_t x) {
    wwa_float_bits_t u;
    u.f = x;
    if (x == 0.0f) return x;
    if (x > 1.0f || x < -1.0f || (u.u & 0x7FFFFFFFu) == 0x7F800000u) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    return wwa_atan2f(x, wwa_sqrtf(1.0f - x * x));
}

real32_t wwa_acosf(real32_t x) {
    wwa_float_bits_t u;
    u.f = x;
    if (x > 1.0f || x < -1.0f || (u.u & 0x7FFFFFFFu) == 0x7F800000u) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    return wwa_atan2f(wwa_sqrtf(1.0f - x * x), x);
}

real32_t wwa_erff(real32_t x) {
    real64_t a = (real64_t)x;
    real64_t x2, term, sum;
    i32 n, neg = 0;
    if (a < 0.0) {
        neg = 1;
        a = -a;
    }
    if (a == 0.0) return x;
    if (a > 4.0) return neg ? -1.0f : 1.0f;
    x2 = a * a;
    term = a;
    sum = a;
    for (n = 0; n < 128; n++) {
        term = -term * x2 * (real64_t)(2 * n + 1) / ((real64_t)(n + 1) * (real64_t)(2 * n + 3));
        sum += term;
        if ((term < 0.0 ? -term : term) < 1.0e-15 * (sum < 0.0 ? -sum : sum)) break;
    }
    sum *= 1.12837916709551257390;
    return neg ? (real32_t)(-sum) : (real32_t)sum;
}

real32_t wwa_asinhf(real32_t x) {
    real32_t a = x < 0.0f ? -x : x;
    real32_t r;
    wwa_float_bits_t u;
    if (x != x) return x;
    u.f = a;
    if ((u.u & 0x7FFFFFFFu) == 0x7F800000u) return x;
    if (a >= 65536.0f) {
        r = wwa_logf(2.0f * a);
    } else {
        r = wwa_logf(a + wwa_sqrtf(a * a + 1.0f));
    }
    return x < 0.0f ? -r : r;
}

real32_t wwa_acoshf(real32_t x) {
    real32_t r;
    wwa_float_bits_t u;
    if (x != x) return x;
    u.f = x;
    if ((u.u & 0x7FFFFFFFu) == 0x7F800000u) return x;
    if (x < 1.0f) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    if (x == 1.0f) return 0.0f;
    if (x >= 65536.0f) {
        r = wwa_logf(2.0f * x);
    } else {
        r = wwa_logf(x + wwa_sqrtf(x * x - 1.0f));
    }
    return r;
}

real32_t wwa_atanhf(real32_t x) {
    real32_t a = x < 0.0f ? -x : x;
    real32_t r;
    if (x != x) return x;
    if (a >= 1.0f) {
        if (a == 1.0f) {
            wwa_float_bits_t inf;
            inf.u = x < 0.0f ? 0xFF800000u : 0x7F800000u;
            return inf.f;
        }
        {
            wwa_float_bits_t nan;
            nan.u = 0x7FC00000u;
            return nan.f;
        }
    }
    if (a < 0.25f) {
        real64_t a2 = (real64_t)a * (real64_t)a;
        real64_t term = a;
        real64_t sum = a;
        i32 n;
        for (n = 1; n < 64; n++) {
            term *= a2;
            sum += term / (real64_t)(2 * n + 1);
            if (term / (real64_t)(2 * n + 1) < 1.0e-15 * sum) break;
        }
        r = (real32_t)sum;
    } else {
        r = 0.5f * wwa_logf((1.0f + a) / (1.0f - a));
    }
    return x < 0.0f ? -r : r;
}

real32_t wwa_hypotf(real32_t x, real32_t y) {
    real32_t z, w;
    wwa_float_bits_t ux, uy;
    ux.f = x;
    uy.f = y;
    if ((ux.u & 0x7FFFFFFFu) == 0x7F800000u || (uy.u & 0x7FFFFFFFu) == 0x7F800000u) {
        wwa_float_bits_t inf;
        inf.u = 0x7F800000u;
        return inf.f;
    }
    if (x != x || y != y) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    x = x < 0.0f ? -x : x;
    y = y < 0.0f ? -y : y;
    z = x > y ? x : y;
    w = x > y ? y : x;
    if (z == 0.0f) return 0.0f;
    return z * wwa_sqrtf((w / z) * (w / z) + 1.0f);
}

real32_t wwa_remainderf(real32_t x, real32_t y) {
    real64_t t, r;
    wwa_float_bits_t ux, uy;
    if (x != x || y != y) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    ux.f = x;
    uy.f = y;
    if ((ux.u & 0x7FFFFFFFu) == 0x7F800000u) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    if ((uy.u & 0x7FFFFFFFu) == 0x7F800000u) return x;
    if (y == 0.0f) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    t = (real64_t)wwa_roundf((real32_t)((real64_t)x / (real64_t)y));
    r = (real64_t)x - t * (real64_t)y;
    if (r == 0.0) r = wwa_copysignf(0.0f, x);
    return (real32_t)r;
}

real32_t wwa_scalbnf(real32_t x, i32 e) {
    return wwa_ldexpf(x, e);
}

real32_t wwa_scalblnf(real32_t x, i64 e) {
    if (e > 100000) e = 100000;
    else if (e < -100000) e = -100000;
    return wwa_ldexpf(x, (i32)e);
}

/* ---- double-precision versions ---- */

real64_t wwa_fabs(real64_t x) {
    wwa_double_bits_t u;
    u.f = x;
    u.u &= 0x7FFFFFFFFFFFFFFFull;
    return u.f;
}

real64_t wwa_trunc(real64_t x) {
    wwa_double_bits_t u;
    i32 exp;
    u64 frac_mask;
    u.f = x;
    exp = (i32)((u.u >> 52) & 0x7FF);
    if (exp < 1023) return x >= 0.0 ? 0.0 : -0.0;
    if (exp >= 1023 + 52) return x;
    frac_mask = 0x000FFFFFFFFFFFFFull >> (i32)(exp - 1023);
    u.u &= ~frac_mask;
    return u.f;
}

real64_t wwa_floor(real64_t x) {
    real64_t t = wwa_trunc(x);
    if (x < 0.0 && t != x) return t - 1.0;
    return t;
}

real64_t wwa_ceil(real64_t x) {
    real64_t t = wwa_trunc(x);
    if (x > 0.0 && t != x) return t + 1.0;
    return t;
}

real64_t wwa_round(real64_t x) {
    if (x >= 0.0) return wwa_floor(x + 0.5);
    return wwa_ceil(x - 0.5);
}

real64_t wwa_copysign(real64_t x, real64_t y) {
    wwa_double_bits_t u, v;
    u.f = x;
    v.f = y;
    u.u = (u.u & 0x7FFFFFFFFFFFFFFFull) | (v.u & 0x8000000000000000ull);
    return u.f;
}

i32 wwa_signbit(real64_t x) {
    wwa_double_bits_t u;
    u.f = x;
    return (u.u >> 63) & 1;
}

real64_t wwa_fmin(real64_t x, real64_t y) {
    if (x != x) return y;
    if (y != y) return x;
    if (x == 0.0 && y == 0.0) return wwa_signbit(x) ? x : y;
    return x < y ? x : y;
}

real64_t wwa_fmax(real64_t x, real64_t y) {
    if (x != x) return y;
    if (y != y) return x;
    if (x == 0.0 && y == 0.0) return wwa_signbit(x) ? y : x;
    return x > y ? x : y;
}

real64_t wwa_modf(real64_t x, real64_t* iptr) {
    real64_t t = wwa_trunc(x);
    *iptr = t;
    return x - t;
}

real64_t wwa_frexp(real64_t x, i32* e) {
    wwa_double_bits_t u;
    i32 exp;
    u.f = x;
    *e = 0;
    if (x == 0.0 || x != x) return x;
    if ((u.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) return x;
    exp = (i32)((u.u >> 52) & 0x7FF);
    if (exp == 0) {
        u64 b = u.u & 0x000FFFFFFFFFFFFFull;
        i32 shift = 0;
        while ((b & 0x0010000000000000ull) == 0) {
            b <<= 1;
            shift++;
        }
        *e = -1021 - shift;
        u.u = (u.u & 0x8000000000000000ull) | ((u64)1022 << 52) | (b & 0x000FFFFFFFFFFFFFull);
        return u.f;
    }
    *e = exp - 1022;
    u.u = (u.u & 0x800FFFFFFFFFFFFFull) | ((u64)1022 << 52);
    return u.f;
}

real64_t wwa_ldexp(real64_t x, i32 e) {
    wwa_double_bits_t inf;
    if (x == 0.0 || x != x) return x;
    if (e > 1023) {
        inf.u = 0x7FF0000000000000ull | (wwa_signbit(x) ? 0x8000000000000000ull : 0);
        return inf.f;
    }
    return x * wwa_pow2_d(e);
}

real64_t wwa_scalbn(real64_t x, i32 e) {
    return wwa_ldexp(x, e);
}

real64_t wwa_scalbln(real64_t x, i64 e) {
    if (e > 100000) e = 100000;
    else if (e < -100000) e = -100000;
    return wwa_ldexp(x, (i32)e);
}

real64_t wwa_sqrt(real64_t x) {
    wwa_double_bits_t u;
    u64 i;
    real64_t y;
    i32 k;
    if (x == 0.0) return x;
    if (!(x >= 0.0)) return 0.0 / 0.0;
    if (x == 1.0) return 1.0;
    u.f = x;
    i = (u.u >> 1) + 0x1FF8000000000000ull;
    u.u = i;
    y = u.f;
    if (!(y > 0.0) || !(y < 1.7976931348623157e308)) y = 1.0;
    for (k = 0; k < 6; k++) y = 0.5 * (y + x / y);
    return y;
}

real64_t wwa_exp(real64_t x) {
    wwa_double_bits_t in;
    real64_t r, poly;
    i64 n;
    i32 k;
    if (x == 0.0) return 1.0;
    in.f = x;
    if ((in.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) {
        if (in.u & 0x8000000000000000ull) return 0.0;
        return x;
    }
    if (x != x) return x;
    if (x > 709.7827128933840) {
        wwa_double_bits_t inf;
        inf.u = 0x7FF0000000000000ull;
        return inf.f;
    }
    if (x < -745.1332191019411) {
        wwa_double_bits_t z;
        z.u = 0;
        return z.f;
    }
    n = (i64)wwa_floor(x / WWA_M_LN2 + 0.5);
    r = x - (real64_t)n * WWA_M_LN2_HI - (real64_t)n * WWA_M_LN2_LO;
    poly = WWA_EXP_COEFF[19];
    for (k = 18; k >= 0; k--) poly = poly * r + WWA_EXP_COEFF[k];
    return poly * wwa_pow2_d((i32)n);
}

real64_t wwa_log(real64_t x) {
    wwa_double_bits_t in;
    real64_t m, z, z2, poly, result;
    i32 e;
    i32 k;
    in.f = x;
    if (x < 0.0 || x != x) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    if (x == 0.0) {
        wwa_double_bits_t ninf;
        ninf.u = 0xFFF0000000000000ull;
        return ninf.f;
    }
    if (x == 1.0) return 0.0;
    if ((in.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) return x;
    {
        u64 b = in.u & 0x000FFFFFFFFFFFFFull;
        u64 e64 = (in.u >> 52) & 0x7FF;
        if (e64 == 0) {
            i32 shift = 0;
            while (b < 0x0010000000000000ull && shift < 52) {
                b <<= 1;
                shift++;
            }
            e = -1022 - shift;
            m = (real64_t)b / 4503599627370496.0;
        } else {
            e = (i32)e64 - 1023;
            in.u = b | 0x3FF0000000000000ull;
            m = in.f;
        }
    }
    if (m > WWA_M_SQRT2) {
        m *= 0.5;
        e += 1;
    } else if (m < WWA_M_SQRT2_HALF) {
        m *= 2.0;
        e -= 1;
    }
    z = (m - 1.0) / (m + 1.0);
    z2 = z * z;
    poly = WWA_LOG_COEFF[14];
    for (k = 13; k >= 0; k--) poly = poly * z2 + WWA_LOG_COEFF[k];
    poly *= z;
    result = (real64_t)e * WWA_M_LN2 + 2.0 * poly;
    return result;
}

real64_t wwa_pow(real64_t x, real64_t y) {
    if (y == 0.0) return 1.0;
    if (x == 1.0) return 1.0;
    if (x < 0.0) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    if (x == 0.0) {
        if (y < 0.0) {
            wwa_double_bits_t inf;
            inf.u = 0x7FF0000000000000ull;
            return inf.f;
        }
        return 0.0;
    }
    return wwa_exp(y * wwa_log(x));
}

real64_t wwa_exp2(real64_t x) {
    wwa_double_bits_t in;
    real64_t r, poly;
    i64 n;
    if (x == 0.0) return 1.0;
    in.f = x;
    if ((in.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) {
        if (in.u & 0x8000000000000000ull) return 0.0;
        return x;
    }
    if (x != x) return x;
    if (x > 1024.0) {
        wwa_double_bits_t inf;
        inf.u = 0x7FF0000000000000ull;
        return inf.f;
    }
    if (x < -1074.0) {
        wwa_double_bits_t z;
        z.u = 0;
        return z.f;
    }
    if (x == wwa_floor(x) && x >= -1074.0 && x <= 1023.0) return wwa_pow2_d((i32)x);
    n = (i64)wwa_floor(x);
    r = x - (real64_t)n;
    poly = wwa_exp(r * WWA_M_LN2);
    return poly * wwa_pow2_d((i32)n);
}

real64_t wwa_log2(real64_t x) {
    return wwa_log(x) / WWA_M_LN2;
}

real64_t wwa_log10(real64_t x) {
    static const real64_t WWA_M_LN10 = 2.30258509299404568402;
    return wwa_log(x) / WWA_M_LN10;
}

real64_t wwa_fmod(real64_t x, real64_t y) {
    real64_t q;
    if (y == 0.0) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    if (x != x || y != y) return x;
    q = wwa_trunc(x / y);
    return x - q * y;
}

real64_t wwa_sin(real64_t x) {
    wwa_double_bits_t in;
    real64_t r, s, poly, c;
    i64 n;
    i32 q;
    i32 k;
    in.f = x;
    if ((in.u & 0x7FFFFFFFFFFFFFFFull) >= 0x7FF0000000000000ull || x != x) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    n = (i64)wwa_round(x * (2.0 / WWA_PI));
    r = x - (real64_t)n * WWA_M_PIO2_HI - (real64_t)n * WWA_M_PIO2_LO;
    s = r * r;
    poly = WWA_SIN_COEFF[9];
    for (k = 8; k >= 0; k--) poly = poly * s + WWA_SIN_COEFF[k];
    poly *= r;
    c = WWA_COS_COEFF[10];
    for (k = 9; k >= 0; k--) c = c * s + WWA_COS_COEFF[k];
    q = (i32)(n & 3);
    switch (q) {
    case 0: return poly;
    case 1: return c;
    case 2: return -poly;
    default: return -c;
    }
}

real64_t wwa_cos(real64_t x) {
    wwa_double_bits_t in;
    real64_t r, s, poly, c;
    i64 n;
    i32 q;
    i32 k;
    in.f = x;
    if ((in.u & 0x7FFFFFFFFFFFFFFFull) >= 0x7FF0000000000000ull || x != x) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    n = (i64)wwa_round(x * (2.0 / WWA_PI));
    r = x - (real64_t)n * WWA_M_PIO2_HI - (real64_t)n * WWA_M_PIO2_LO;
    s = r * r;
    poly = WWA_SIN_COEFF[9];
    for (k = 8; k >= 0; k--) poly = poly * s + WWA_SIN_COEFF[k];
    poly *= r;
    c = WWA_COS_COEFF[10];
    for (k = 9; k >= 0; k--) c = c * s + WWA_COS_COEFF[k];
    q = (i32)(n & 3);
    switch (q) {
    case 0: return c;
    case 1: return -poly;
    case 2: return -c;
    default: return poly;
    }
}

real64_t wwa_tan(real64_t x) {
    real64_t s = wwa_sin(x);
    real64_t c = wwa_cos(x);
    wwa_double_bits_t in;
    if (c == 0.0) {
        in.f = x;
        in.u = 0x7FF0000000000000ull | (in.u & 0x8000000000000000ull);
        return in.f;
    }
    return s / c;
}

real64_t wwa_atan(real64_t x) {
    real64_t a = x;
    real64_t z, z2, poly, result;
    i32 sign = 0;
    i32 k2;
    if (x != x) return x;
    if (a < 0.0) {
        a = -a;
        sign = 1;
    }
    if (a > 1.0) {
        a = 1.0 / a;
        result = WWA_PI_2 - wwa_atan(a);
        return sign ? -result : result;
    }
    if (a > 0.26794919243112281) {
        z = (WWA_M_SQRT3 * a - 1.0) / (WWA_M_SQRT3 + a);
        z2 = z * z;
        poly = WWA_ATAN_COEFF[11];
        for (k2 = 10; k2 >= 0; k2--) poly = poly * z2 + WWA_ATAN_COEFF[k2];
        poly *= z;
        result = WWA_M_PIO6 + poly;
        return sign ? -result : result;
    }
    z2 = a * a;
    poly = WWA_ATAN_COEFF[11];
    for (k2 = 10; k2 >= 0; k2--) poly = poly * z2 + WWA_ATAN_COEFF[k2];
    poly *= a;
    result = poly;
    return sign ? -result : result;
}

real64_t wwa_atan2(real64_t y, real64_t x) {
    if (x == 0.0) {
        if (y > 0.0) return WWA_PI_2;
        if (y < 0.0) return -WWA_PI_2;
        return 0.0;
    }
    if (x > 0.0) return wwa_atan(y / x);
    if (y >= 0.0) return wwa_atan(y / x) + WWA_PI;
    return wwa_atan(y / x) - WWA_PI;
}

/* ---- round 5: transcendental doubles ---- */

real64_t wwa_sinh(real64_t x) {
    real64_t ex;
    if (x == 0.0) return x;
    if (x < 0.0) return -wwa_sinh(-x);
    if (x < 1.0e-4) return x * (1.0 + x * x / 6.0);
    ex = wwa_exp(x);
    return 0.5 * (ex - 1.0 / ex);
}

real64_t wwa_cosh(real64_t x) {
    real64_t ex;
    if (x < 0.0) x = -x;
    ex = wwa_exp(x);
    return 0.5 * (ex + 1.0 / ex);
}

real64_t wwa_tanh(real64_t x) {
    real64_t a = x < 0.0 ? -x : x;
    real64_t e2, s2, poly, r;
    i32 k;
    if (x == 0.0) return x;
    if (a < 0.25) {
        s2 = a * a;
        poly = WWA_TANH_COEFF[11];
        for (k = 10; k >= 0; k--) poly = poly * s2 + WWA_TANH_COEFF[k];
        r = poly * a;
        return x < 0.0 ? -r : r;
    }
    e2 = wwa_exp(2.0 * a);
    r = 1.0 - 2.0 / (e2 + 1.0);
    return x < 0.0 ? -r : r;
}

real64_t wwa_asin(real64_t x) {
    wwa_double_bits_t u;
    u.f = x;
    if (x == 0.0) return x;
    if (x > 1.0 || x < -1.0 || (u.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    return wwa_atan2(x, wwa_sqrt(1.0 - x * x));
}

real64_t wwa_acos(real64_t x) {
    wwa_double_bits_t u;
    u.f = x;
    if (x > 1.0 || x < -1.0 || (u.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    return wwa_atan2(wwa_sqrt(1.0 - x * x), x);
}

real64_t wwa_asinh(real64_t x) {
    real64_t a = x < 0.0 ? -x : x;
    real64_t r;
    if (x != x) return x;
    if (a > 1.0e154) {
        r = wwa_log(2.0 * a);
    } else {
        r = wwa_log(a + wwa_sqrt(a * a + 1.0));
    }
    return x < 0.0 ? -r : r;
}

real64_t wwa_acosh(real64_t x) {
    real64_t r;
    if (x != x) return x;
    if (x < 1.0) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    if (x == 1.0) return 0.0;
    if (x > 1.0e154) {
        r = wwa_log(2.0 * x);
    } else {
        r = wwa_log(x + wwa_sqrt(x * x - 1.0));
    }
    return r;
}

real64_t wwa_atanh(real64_t x) {
    real64_t a = x < 0.0 ? -x : x;
    real64_t r;
    if (x != x) return x;
    if (a >= 1.0) {
        if (a == 1.0) {
            wwa_double_bits_t inf;
            inf.u = x < 0.0 ? 0xFFF0000000000000ull : 0x7FF0000000000000ull;
            return inf.f;
        }
        {
            wwa_double_bits_t nan;
            nan.u = 0x7FF8000000000000ull;
            return nan.f;
        }
    }
    if (a < 0.25) {
        real64_t a2 = a * a;
        real64_t term = a;
        real64_t sum = a;
        i32 n;
        for (n = 1; n < 128; n++) {
            term *= a2;
            sum += term / (real64_t)(2 * n + 1);
            if (term / (real64_t)(2 * n + 1) < 1.0e-17 * sum) break;
        }
        r = sum;
    } else {
        r = 0.5 * wwa_log((1.0 + a) / (1.0 - a));
    }
    return x < 0.0 ? -r : r;
}

real64_t wwa_hypot(real64_t x, real64_t y) {
    real64_t z, w;
    wwa_double_bits_t ux, uy;
    ux.f = x;
    uy.f = y;
    if ((ux.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull ||
        (uy.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) {
        wwa_double_bits_t inf;
        inf.u = 0x7FF0000000000000ull;
        return inf.f;
    }
    if (x != x || y != y) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    x = x < 0.0 ? -x : x;
    y = y < 0.0 ? -y : y;
    z = x > y ? x : y;
    w = x > y ? y : x;
    if (z == 0.0) return 0.0;
    return z * wwa_sqrt((w / z) * (w / z) + 1.0);
}

real64_t wwa_remainder(real64_t x, real64_t y) {
    real64_t r, ay, q;
    wwa_double_bits_t ux, uy;
    if (x != x || y != y) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    ux.f = x;
    uy.f = y;
    if ((ux.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    if ((uy.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) return x;
    if (y == 0.0) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    ay = y < 0.0 ? -y : y;
    r = wwa_fmod(x, y);
    if (r == 0.0) return wwa_copysign(0.0, x);
    if (2.0 * r > ay || 2.0 * r < -ay) {
        r -= wwa_copysign(ay, x);
    } else if (2.0 * r == ay || 2.0 * r == -ay) {
        q = (x - r) / y;
        if (wwa_fmod(q, 2.0) != 0.0) r -= wwa_copysign(ay, x);
    }
    return r;
}

static real64_t wwa_erfc_cf(real64_t a) {
    real64_t z = a * a;
    real64_t s = 0.0;
    i32 n;
    for (n = 60; n >= 1; n--) {
        real64_t d = (n & 1) ? z : 1.0;
        real64_t num = (real64_t)n * 0.5;
        s = 1.0 / (d + num * s);
    }
    return wwa_exp(-z) * a * s / 1.77245385090551602730;
}

real64_t wwa_erf(real64_t x) {
    real64_t a = x < 0.0 ? -x : x;
    real64_t x2, term, sum, c;
    i32 n;
    if (a == 0.0) return x;
    if (a <= 1.5) {
        x2 = a * a;
        term = a;
        sum = a;
        for (n = 0; n < 256; n++) {
            term = -term * x2 * (real64_t)(2 * n + 1) / ((real64_t)(n + 1) * (real64_t)(2 * n + 3));
            sum += term;
            if ((term < 0.0 ? -term : term) < 1.0e-17 * (sum < 0.0 ? -sum : sum)) break;
        }
        sum *= 1.12837916709551257390;
        return x < 0.0 ? -sum : sum;
    }
    if (a > 27.0) return x < 0.0 ? -1.0 : 1.0;
    c = wwa_erfc_cf(a);
    return x < 0.0 ? -1.0 + c : 1.0 - c;
}

real64_t wwa_erfc(real64_t x) {
    real64_t a = x < 0.0 ? -x : x;
    if (x != x) return x;
    if (a <= 1.5) return x < 0.0 ? 1.0 + wwa_erf(a) : 1.0 - wwa_erf(a);
    if (a > 27.0) return x < 0.0 ? 2.0 : 0.0;
    return x < 0.0 ? 2.0 - wwa_erfc_cf(a) : wwa_erfc_cf(a);
}

real64_t wwa_expm1(real64_t x) {
    static const real64_t WWA_EXPM1_COEFF[20] = {
        1.0,
        0.5,
        0.16666666666666666667,
        0.04166666666666666667,
        0.00833333333333333333,
        0.00138888888888888889,
        0.00019841269841269841,
        2.48015873015873016e-5,
        2.75573192239858907e-6,
        2.75573192239858907e-7,
        2.50521083854417188e-8,
        2.08767569878680990e-9,
        1.60590438368216146e-10,
        1.14707455977297247e-11,
        7.64716373181981648e-13,
        4.77947733238738530e-14,
        2.81145725434552076e-15,
        1.56192069685862264e-16,
        8.22063524662432979e-18,
        4.11031762331216490e-19
    };
    real64_t a, poly;
    i32 k;
    if (x != x) return x;
    if (x == 0.0) return x;
    a = x < 0.0 ? -x : x;
    if (x > 709.78) {
        wwa_double_bits_t infu;
        infu.u = 0x7FF0000000000000ull;
        return infu.f;
    }
    if (x < -745.13) return -1.0;
    if (a > 1.0) return wwa_exp(x) - 1.0;
    poly = WWA_EXPM1_COEFF[19];
    for (k = 18; k >= 0; k--) poly = poly * x + WWA_EXPM1_COEFF[k];
    return poly * x;
}

real64_t wwa_log1p(real64_t x) {
    real64_t z;
    if (x != x) return x;
    if (x == 0.0) return x;
    if (x >= 1.0e16) return wwa_log(1.0 + x);
    if (x <= -1.0) {
        if (x == -1.0) {
            wwa_double_bits_t ninf;
            ninf.u = 0xFFF0000000000000ull;
            return ninf.f;
        }
        {
            wwa_double_bits_t nan;
            nan.u = 0x7FF8000000000000ull;
            return nan.f;
        }
    }
    z = x / (x + 2.0);
    return 2.0 * wwa_atanh(z);
}

real64_t wwa_cbrt(real64_t x) {
    real64_t a, m, y, y2;
    i32 e, n;
    if (x != x) return x;
    if (x == 0.0) return x;
    a = x < 0.0 ? -x : x;
    m = wwa_frexp(a, &e);
    if (m != m) return x;
    y = wwa_pow2_d(e / 3);
    for (n = 0; n < 8; n++) {
        y2 = y * y;
        y = (2.0 * y + a / y2) / 3.0;
    }
    return x < 0.0 ? -y : y;
}

real64_t wwa_fma(real64_t x, real64_t y, real64_t z) {
    static const real64_t SPLIT = 134217729.0;
    static const real64_t SCALE = 0x1p-512;
    static const real64_t RESCALE = 0x1p512;
    wwa_double_bits_t infu, ninf;
    real64_t inf, a, b, xh, xl, yh, yl, p, q, r, s, t, e2;
    infu.u = 0x7FF0000000000000ull;
    inf = infu.f;
    ninf.u = 0xFFF0000000000000ull;
    if (x != x || y != y || z != z) {
        wwa_double_bits_t nan;
        nan.u = 0x7FF8000000000000ull;
        return nan.f;
    }
    if (x == 0.0 || y == 0.0) {
        if ((x == 0.0 && (y == inf || y == ninf.f)) ||
            (y == 0.0 && (x == inf || x == ninf.f))) {
            wwa_double_bits_t nan;
            nan.u = 0x7FF8000000000000ull;
            return nan.f;
        }
        return z;
    }
    if (x == inf || x == ninf.f || y == inf || y == ninf.f) {
        real64_t pxy = (x > 0.0) == (y > 0.0) ? inf : ninf.f;
        if (z == -pxy) {
            wwa_double_bits_t nan;
            nan.u = 0x7FF8000000000000ull;
            return nan.f;
        }
        return pxy;
    }
    if (z == inf || z == ninf.f) return z;
    a = x < 0.0 ? -x : x;
    b = y < 0.0 ? -y : y;
    if (a > 1.7976931348623157e308 / b) {
        real64_t pxy = (x > 0.0) == (y > 0.0) ? inf : ninf.f;
        if (z == -pxy) {
            wwa_double_bits_t nan;
            nan.u = 0x7FF8000000000000ull;
            return nan.f;
        }
        return pxy;
    }
    if (a > 1.0e300) {
        x *= SCALE;
        z *= SCALE;
    }
    if (b > 1.0e300) {
        y *= SCALE;
        z *= SCALE;
    }
    xh = SPLIT * x - (SPLIT * x - x);
    xl = x - xh;
    yh = SPLIT * y - (SPLIT * y - y);
    yl = y - yh;
    p = xh * yh;
    q = xh * yl + xl * yh;
    r = xl * yl;
    s = p + q;
    e2 = q - (s - p);
    e2 += r;
    t = s + z;
    if (t == inf || t == ninf.f) return t;
    e2 += z - (t - s);
    if (a > 1.0e300 || b > 1.0e300) return (t + e2) * RESCALE;
    return t + e2;
}

real64_t wwa_nextafter(real64_t x, real64_t y) {
    wwa_double_bits_t u, v;
    u64 d;
    u.f = x;
    v.f = y;
    if (x != x || y != y) return x + y;
    if (x == y) return y;
    if (x == 0.0) {
        u.u = (v.u & 0x8000000000000000ull) ? 0x8000000000000001ull : 1ull;
        return u.f;
    }
    d = u.u;
    if ((d & 0x8000000000000000ull) == 0) {
        if (x < y) d += 1;
        else d -= 1;
    } else {
        if (x < y) d -= 1;
        else d += 1;
    }
    u.u = d;
    return u.f;
}

real32_t wwa_expm1f(real32_t x) {
    static const real32_t C[10] = {
        1.0f,
        0.5f,
        0.16666667f,
        0.04166667f,
        0.00833333f,
        0.00138889f,
        0.00019841f,
        2.48016e-5f,
        2.75573e-6f,
        2.75573e-7f
    };
    real32_t a, poly;
    i32 k;
    if (x != x) return x;
    if (x == 0.0f) return x;
    a = x < 0.0f ? -x : x;
    if (x > 88.7f) {
        wwa_float_bits_t infu;
        infu.u = 0x7F800000u;
        return infu.f;
    }
    if (x < -87.5f) return -1.0f;
    if (a > 1.0f) return wwa_expf(x) - 1.0f;
    poly = C[9];
    for (k = 8; k >= 0; k--) poly = poly * x + C[k];
    return poly * x;
}

real32_t wwa_log1pf(real32_t x) {
    real32_t z;
    if (x != x) return x;
    if (x == 0.0f) return x;
    if (x >= 1.0e8f) return wwa_logf(1.0f + x);
    if (x <= -1.0f) {
        if (x == -1.0f) {
            wwa_float_bits_t ninf;
            ninf.u = 0xFF800000u;
            return ninf.f;
        }
        {
            wwa_float_bits_t nan;
            nan.u = 0x7FC00000u;
            return nan.f;
        }
    }
    z = x / (x + 2.0f);
    return 2.0f * wwa_atanhf(z);
}

real32_t wwa_cbrtf(real32_t x) {
    real32_t a, m, y, y2;
    i32 e, n;
    if (x != x) return x;
    if (x == 0.0f) return x;
    a = x < 0.0f ? -x : x;
    m = wwa_frexpf(a, &e);
    if (m != m) return x;
    y = (real32_t)wwa_pow2_d(e / 3);
    for (n = 0; n < 8; n++) {
        y2 = y * y;
        y = (2.0f * y + a / y2) / 3.0f;
    }
    return x < 0.0f ? -y : y;
}

real32_t wwa_fmaf(real32_t x, real32_t y, real32_t z) {
    static const real32_t SPLIT = 4097.0f;
    static const real32_t SCALE = 0x1p-64f;
    static const real32_t RESCALE = 0x1p64f;
    wwa_float_bits_t infu, ninf;
    real32_t inf, a, b, xh, xl, yh, yl, p, q, r, s, t, e2;
    infu.u = 0x7F800000u;
    inf = infu.f;
    ninf.u = 0xFF800000u;
    if (x != x || y != y || z != z) {
        wwa_float_bits_t nan;
        nan.u = 0x7FC00000u;
        return nan.f;
    }
    if (x == 0.0f || y == 0.0f) {
        if ((x == 0.0f && (y == inf || y == ninf.f)) ||
            (y == 0.0f && (x == inf || x == ninf.f))) {
            wwa_float_bits_t nan;
            nan.u = 0x7FC00000u;
            return nan.f;
        }
        return z;
    }
    if (x == inf || x == ninf.f || y == inf || y == ninf.f) {
        real32_t pxy = (x > 0.0f) == (y > 0.0f) ? inf : ninf.f;
        if (z == -pxy) {
            wwa_float_bits_t nan;
            nan.u = 0x7FC00000u;
            return nan.f;
        }
        return pxy;
    }
    if (z == inf || z == ninf.f) return z;
    a = x < 0.0f ? -x : x;
    b = y < 0.0f ? -y : y;
    if (a > 3.4028234663852886e38f / b) {
        real32_t pxy = (x > 0.0f) == (y > 0.0f) ? inf : ninf.f;
        if (z == -pxy) {
            wwa_float_bits_t nan;
            nan.u = 0x7FC00000u;
            return nan.f;
        }
        return pxy;
    }
    if (a > 1.0e34f) {
        x *= SCALE;
        z *= SCALE;
    }
    if (b > 1.0e34f) {
        y *= SCALE;
        z *= SCALE;
    }
    xh = SPLIT * x - (SPLIT * x - x);
    xl = x - xh;
    yh = SPLIT * y - (SPLIT * y - y);
    yl = y - yh;
    p = xh * yh;
    q = xh * yl + xl * yh;
    r = xl * yl;
    s = p + q;
    e2 = q - (s - p);
    e2 += r;
    t = s + z;
    if (t == inf || t == ninf.f) return t;
    e2 += z - (t - s);
    if (a > 1.0e34f || b > 1.0e34f) return (t + e2) * RESCALE;
    return t + e2;
}

real32_t wwa_erfcf(real32_t x) {
    real32_t a = x < 0.0f ? -x : x;
    if (x != x) return x;
    return x < 0.0f ? 2.0f - (1.0f - wwa_erff(a)) : 1.0f - wwa_erff(a);
}

real32_t wwa_nextafterf(real32_t x, real32_t y) {
    wwa_float_bits_t u, v;
    u32 d;
    u.f = x;
    v.f = y;
    if (x != x || y != y) return x + y;
    if (x == y) return y;
    if (x == 0.0f) {
        u.u = (v.u & 0x80000000u) ? 0x80000001u : 1u;
        return u.f;
    }
    d = u.u;
    if ((d & 0x80000000u) == 0) {
        if (x < y) d += 1;
        else d -= 1;
    } else {
        if (x < y) d -= 1;
        else d += 1;
    }
    u.u = d;
    return u.f;
}

real32_t wwa_rintf(real32_t x) {
    real32_t r, d;
    if (x != x) return x;
    if (x >= 16777216.0f || x <= -16777216.0f) return x;
    r = wwa_roundf(x);
    d = x - r;
    if (d == 0.5f || d == -0.5f) {
        if (wwa_fmodf(r, 2.0f) != 0.0f) {
            r = wwa_copysignf(wwa_fabsf(r) - 1.0f, x);
        }
    }
    return r;
}

real32_t wwa_nearbyintf(real32_t x) {
    return wwa_rintf(x);
}

i32 wwa_lrintf(real32_t x) {
    real32_t r = wwa_rintf(x);
    if (r != r || r >= 2147483648.0f || r < -2147483648.0f) return -2147483647 - 1;
    return (i32)r;
}

i64 wwa_llrintf(real32_t x) {
    real32_t r = wwa_rintf(x);
    if (r != r || r >= 9223372036854775808.0f || r < -9223372036854775808.0f) return -9223372036854775807ll - 1;
    return (i64)r;
}

i32 wwa_lroundf(real32_t x) {
    real32_t r = wwa_roundf(x);
    if (r != r || r >= 2147483648.0f || r < -2147483648.0f) return -2147483647 - 1;
    return (i32)r;
}

i64 wwa_llroundf(real32_t x) {
    real32_t r = wwa_roundf(x);
    if (r != r || r >= 9223372036854775808.0f || r < -9223372036854775808.0f) return -9223372036854775807ll - 1;
    return (i64)r;
}

real32_t wwa_fdimf(real32_t x, real32_t y) {
    if (x != x || y != y) return x + y;
    return x > y ? x - y : 0.0f;
}

i32 wwa_ilogbf(real32_t x) {
    wwa_float_bits_t u;
    i32 e;
    if (x != x) return 2147483647;
    u.f = x;
    if (x == 0.0f) return -2147483647 - 1;
    if ((u.u & 0x7FFFFFFFu) == 0x7F800000u) return 2147483647;
    (void)wwa_frexpf(x, &e);
    return e - 1;
}

real32_t wwa_logbf(real32_t x) {
    wwa_float_bits_t u, infu, ninf;
    i32 e;
    infu.u = 0x7F800000u;
    ninf.u = 0xFF800000u;
    if (x != x) return x;
    if (x == 0.0f) return ninf.f;
    u.f = x;
    if ((u.u & 0x7FFFFFFFu) == 0x7F800000u) return infu.f;
    (void)wwa_frexpf(x, &e);
    return (real32_t)(e - 1);
}

real64_t wwa_rint(real64_t x) {
    real64_t r, d;
    if (x != x) return x;
    if (x >= 9007199254740992.0 || x <= -9007199254740992.0) return x;
    r = wwa_round(x);
    d = x - r;
    if (d == 0.5 || d == -0.5) {
        if (wwa_fmod(r, 2.0) != 0.0) {
            r = wwa_copysign(wwa_fabs(r) - 1.0, x);
        }
    }
    return r;
}

real64_t wwa_nearbyint(real64_t x) {
    return wwa_rint(x);
}

i32 wwa_lrint(real64_t x) {
    real64_t r = wwa_rint(x);
    if (r != r || r >= 2147483648.0 || r < -2147483648.0) return -2147483647 - 1;
    return (i32)r;
}

i64 wwa_llrint(real64_t x) {
    real64_t r = wwa_rint(x);
    if (r != r || r >= 9223372036854775808.0 || r < -9223372036854775808.0) return -9223372036854775807ll - 1;
    return (i64)r;
}

i32 wwa_lround(real64_t x) {
    real64_t r = wwa_round(x);
    if (r != r || r >= 2147483648.0 || r < -2147483648.0) return -2147483647 - 1;
    return (i32)r;
}

i64 wwa_llround(real64_t x) {
    real64_t r = wwa_round(x);
    if (r != r || r >= 9223372036854775808.0 || r < -9223372036854775808.0) return -9223372036854775807ll - 1;
    return (i64)r;
}

real64_t wwa_fdim(real64_t x, real64_t y) {
    if (x != x || y != y) return x + y;
    return x > y ? x - y : 0.0;
}

i32 wwa_ilogb(real64_t x) {
    wwa_double_bits_t u;
    i32 e;
    if (x != x) return 2147483647;
    u.f = x;
    if (x == 0.0) return -2147483647 - 1;
    if ((u.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) return 2147483647;
    (void)wwa_frexp(x, &e);
    return e - 1;
}

real64_t wwa_logb(real64_t x) {
    wwa_double_bits_t u, infu, ninf;
    i32 e;
    infu.u = 0x7FF0000000000000ull;
    ninf.u = 0xFFF0000000000000ull;
    if (x != x) return x;
    if (x == 0.0) return ninf.f;
    u.f = x;
    if ((u.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull) return infu.f;
    (void)wwa_frexp(x, &e);
    return (real64_t)(e - 1);
}

real32_t wwa_nanf(const char *s) {
    wwa_float_bits_t u;
    (void)s;
    u.u = 0x7FC00000u;
    return u.f;
}

i32 wwa_fpclassifyf(real32_t x) {
    wwa_float_bits_t u;
    u32 e;
    u.f = x;
    e = (u.u >> 23) & 0xFFu;
    if (e == 0xFFu) return (u.u & 0x7FFFFFu) != 0 ? 0 : 1;
    if (e == 0) return (u.u & 0x7FFFFFFFu) != 0 ? 3 : 2;
    return 4;
}

i32 wwa_isnanf(real32_t x) {
    wwa_float_bits_t u;
    u.f = x;
    return (u.u & 0x7FFFFFFFu) > 0x7F800000u;
}

i32 wwa_isinff(real32_t x) {
    wwa_float_bits_t u;
    u.f = x;
    return (u.u & 0x7FFFFFFFu) == 0x7F800000u;
}

i32 wwa_isfinitef(real32_t x) {
    wwa_float_bits_t u;
    u.f = x;
    return ((u.u >> 23) & 0xFFu) != 0xFFu;
}

i32 wwa_isnormalf(real32_t x) {
    wwa_float_bits_t u;
    u32 e;
    u.f = x;
    e = (u.u >> 23) & 0xFFu;
    return e != 0 && e != 0xFFu;
}

i32 wwa_isgreaterf(real32_t x, real32_t y) {
    return !(x != x || y != y) && x > y;
}

i32 wwa_isgreaterequalf(real32_t x, real32_t y) {
    return !(x != x || y != y) && x >= y;
}

i32 wwa_islessf(real32_t x, real32_t y) {
    return !(x != x || y != y) && x < y;
}

i32 wwa_islessequalf(real32_t x, real32_t y) {
    return !(x != x || y != y) && x <= y;
}

i32 wwa_islessgreaterf(real32_t x, real32_t y) {
    return !(x != x || y != y) && (x < y || x > y);
}

i32 wwa_isunorderedf(real32_t x, real32_t y) {
    return x != x || y != y;
}

real64_t wwa_nan(const char *s) {
    wwa_double_bits_t u;
    (void)s;
    u.u = 0x7FF8000000000000ull;
    return u.f;
}

i32 wwa_fpclassify(real64_t x) {
    wwa_double_bits_t u;
    u32 e;
    u.f = x;
    e = (u32)((u.u >> 52) & 0x7FFull);
    if (e == 0x7FFu) return (u.u & 0xFFFFFFFFFFFFFull) != 0 ? 0 : 1;
    if (e == 0) return (u.u & 0x7FFFFFFFFFFFFFFFull) != 0 ? 3 : 2;
    return 4;
}

i32 wwa_isnan(real64_t x) {
    wwa_double_bits_t u;
    u.f = x;
    return (u.u & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull;
}

i32 wwa_isinf(real64_t x) {
    wwa_double_bits_t u;
    u.f = x;
    return (u.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull;
}

i32 wwa_isfinite(real64_t x) {
    wwa_double_bits_t u;
    u.f = x;
    return ((u.u >> 52) & 0x7FFull) != 0x7FFull;
}

i32 wwa_isnormal(real64_t x) {
    wwa_double_bits_t u;
    u32 e;
    u.f = x;
    e = (u32)((u.u >> 52) & 0x7FFull);
    return e != 0 && e != 0x7FFu;
}

i32 wwa_isgreater(real64_t x, real64_t y) {
    return !(x != x || y != y) && x > y;
}

i32 wwa_isgreaterequal(real64_t x, real64_t y) {
    return !(x != x || y != y) && x >= y;
}

i32 wwa_isless(real64_t x, real64_t y) {
    return !(x != x || y != y) && x < y;
}

i32 wwa_islessequal(real64_t x, real64_t y) {
    return !(x != x || y != y) && x <= y;
}

i32 wwa_islessgreater(real64_t x, real64_t y) {
    return !(x != x || y != y) && (x < y || x > y);
}

i32 wwa_isunordered(real64_t x, real64_t y) {
    return x != x || y != y;
}

static real64_t wwa_lgamma_pos(real64_t x) {
    real64_t acc = 0.0, x2, z3, z5, z7, z9, z11, z13, z15, z17, t;
    while (x < 8.0) {
        acc += wwa_log(x);
        x += 1.0;
    }
    x2 = x * x;
    z3 = x * x2;
    z5 = z3 * x2;
    z7 = z5 * x2;
    z9 = z7 * x2;
    z11 = z9 * x2;
    z13 = z11 * x2;
    z15 = z13 * x2;
    z17 = z15 * x2;
    t = 1.0 / (12.0 * x) - 1.0 / (360.0 * z3) + 1.0 / (1260.0 * z5)
        - 1.0 / (1680.0 * z7) + 1.0 / (1188.0 * z9) - 691.0 / (360360.0 * z11)
        + 7.0 / (1092.0 * z13) - 3617.0 / (122400.0 * z15)
        + 43867.0 / (244188.0 * z17);
    return (x - 0.5) * wwa_log(x) - x + 0.91893853320467274 + t - acc;
}

real64_t wwa_lgamma(real64_t x) {
    if (x != x) return x;
    if (wwa_isinf(x)) return wwa_fabs(x);
    if (x <= 0.0) {
        if (wwa_floor(x) == x) return wwa_ldexp(1.0, 2000);
        return 1.1447298858494002 - wwa_log(wwa_fabs(wwa_sin(WWA_PI * x)))
               - wwa_lgamma_pos(1.0 - x);
    }
    return wwa_lgamma_pos(x + 1.0) - wwa_log(x);
}

real64_t wwa_tgamma(real64_t x) {
    real64_t g;
    if (x != x) return x;
    if (x == 0.0 || (x < 0.0 && wwa_floor(x) == x)) return wwa_ldexp(1.0, 2000);
    g = wwa_exp(wwa_lgamma(x));
    if (x < 0.0 && (((i64)wwa_floor(x)) & 1)) g = -g;
    return g;
}

real32_t wwa_lgammaf(real32_t x) {
    return (real32_t)wwa_lgamma((real64_t)x);
}

real32_t wwa_tgammaf(real32_t x) {
    return (real32_t)wwa_tgamma((real64_t)x);
}

real64_t wwa_creal(wwa_complex_t z) {
    return z.re;
}

real64_t wwa_cimag(wwa_complex_t z) {
    return z.im;
}

wwa_complex_t wwa_conj(wwa_complex_t z) {
    wwa_complex_t r;
    r.re = z.re;
    r.im = -z.im;
    return r;
}

real64_t wwa_cabs(wwa_complex_t z) {
    return wwa_hypot(z.re, z.im);
}

real64_t wwa_carg(wwa_complex_t z) {
    return wwa_atan2(z.im, z.re);
}

wwa_complex_t wwa_cproj(wwa_complex_t z) {
    wwa_complex_t r = z;
    if (wwa_isinf(z.re) || wwa_isinf(z.im)) {
        r.re = wwa_ldexp(1.0, 2000);
        r.im = wwa_copysign(0.0, z.im);
    }
    return r;
}

real32_t wwa_crealf(wwa_complexf_t z) {
    return z.re;
}

real32_t wwa_cimagf(wwa_complexf_t z) {
    return z.im;
}

wwa_complexf_t wwa_conjf(wwa_complexf_t z) {
    wwa_complexf_t r;
    r.re = z.re;
    r.im = -z.im;
    return r;
}

real32_t wwa_cabsf(wwa_complexf_t z) {
    return wwa_hypotf(z.re, z.im);
}

real32_t wwa_cargf(wwa_complexf_t z) {
    return wwa_atan2f(z.im, z.re);
}

wwa_complexf_t wwa_cprojf(wwa_complexf_t z) {
    wwa_complexf_t r = z;
    if (wwa_isinff(z.re) || wwa_isinff(z.im)) {
        r.re = wwa_ldexpf(1.0f, 2000);
        r.im = wwa_copysignf(0.0f, z.im);
    }
    return r;
}

i32 wwa_fegetround(void) {
    return (i32)((_mm_getcsr() >> 13) & 0x3u);
}

i32 wwa_fesetround(i32 r) {
    u32 csr;
    if (r < 0 || r > 3) return -1;
    csr = _mm_getcsr();
    _mm_setcsr((csr & ~(0x3u << 13)) | ((u32)r << 13));
    return 0;
}

i32 wwa_feclearexcept(i32 e) {
    _mm_setcsr(_mm_getcsr() & ~((u32)e & WWA_FE_ALL_EXCEPT));
    return 0;
}

i32 wwa_fetestexcept(i32 e) {
    return (i32)(_mm_getcsr() & ((u32)e & WWA_FE_ALL_EXCEPT));
}

i32 wwa_feraiseexcept(i32 e) {
    _mm_setcsr(_mm_getcsr() | ((u32)e & WWA_FE_ALL_EXCEPT));
    return 0;
}

i32 wwa_fegetexceptflag(wwa_fexcept_t* flagp, i32 e) {
    *flagp = (wwa_fexcept_t)(_mm_getcsr() & ((u32)e & WWA_FE_ALL_EXCEPT));
    return 0;
}

i32 wwa_fesetexceptflag(const wwa_fexcept_t* flagp, i32 e) {
    u32 mask = (u32)e & WWA_FE_ALL_EXCEPT;
    _mm_setcsr((_mm_getcsr() & ~mask) | ((u32)*flagp & mask));
    return 0;
}

i32 wwa_fegetenv(wwa_fenv_t* envp) {
    *envp = (wwa_fenv_t)_mm_getcsr();
    return 0;
}

i32 wwa_feholdexcept(wwa_fenv_t* envp) {
    *envp = (wwa_fenv_t)_mm_getcsr();
    _mm_setcsr(_mm_getcsr() & ~WWA_FE_ALL_EXCEPT);
    return 1;
}

i32 wwa_fesetenv(const wwa_fenv_t* envp) {
    _mm_setcsr((u32)*envp);
    return 0;
}

i32 wwa_feupdateenv(const wwa_fenv_t* envp) {
    u32 saved = _mm_getcsr() & WWA_FE_ALL_EXCEPT;
    _mm_setcsr((u32)*envp);
    _mm_setcsr(_mm_getcsr() | saved);
    return 0;
}