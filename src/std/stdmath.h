/* World Without Answers — stdmath.h
   from-scratch float math. Exact rational (Taylor) coefficients only,
   never memorized minimax. No libc. */

#ifndef WWA_STDMATH_H
#define WWA_STDMATH_H

#include <stdtype.h>

#define WWA_PI   3.14159265358979323846
#define WWA_PI_2 (WWA_PI / 2.0)
#define WWA_PI_4 (WWA_PI / 4.0)
#define WWA_E    2.71828182845904523536

real32_t wwa_sqrtf(real32_t x);
real32_t wwa_sinf(real32_t x);
real32_t wwa_cosf(real32_t x);
real32_t wwa_tanf(real32_t x);
real32_t wwa_expf(real32_t x);
real32_t wwa_logf(real32_t x);
real32_t wwa_powf(real32_t x, real32_t y);
real32_t wwa_atanf(real32_t x);
real32_t wwa_atan2f(real32_t y, real32_t x);
real32_t wwa_fabsf(real32_t x);
real32_t wwa_floorf(real32_t x);
real32_t wwa_ceilf(real32_t x);
real32_t wwa_roundf(real32_t x);
real32_t wwa_truncf(real32_t x);
real32_t wwa_fmodf(real32_t x, real32_t y);

real32_t wwa_ldexpf(real32_t x, i32 e);
real32_t wwa_frexpf(real32_t x, i32* e);
real32_t wwa_copysignf(real32_t x, real32_t y);
i32     wwa_signbitf(real32_t x);
real32_t wwa_fminf(real32_t x, real32_t y);
real32_t wwa_fmaxf(real32_t x, real32_t y);
real32_t wwa_modff(real32_t x, real32_t* iptr);
real32_t wwa_exp2f(real32_t x);
real32_t wwa_log2f(real32_t x);
real32_t wwa_log10f(real32_t x);
real32_t wwa_sinhf(real32_t x);
real32_t wwa_coshf(real32_t x);
real32_t wwa_tanhf(real32_t x);
real32_t wwa_asinf(real32_t x);
real32_t wwa_acosf(real32_t x);
real32_t wwa_erff(real32_t x);
real32_t wwa_asinhf(real32_t x);
real32_t wwa_acoshf(real32_t x);
real32_t wwa_atanhf(real32_t x);
real32_t wwa_hypotf(real32_t x, real32_t y);
real32_t wwa_remainderf(real32_t x, real32_t y);
real32_t wwa_scalbnf(real32_t x, i32 e);
real32_t wwa_scalblnf(real32_t x, i64 e);
real32_t wwa_expm1f(real32_t x);
real32_t wwa_log1pf(real32_t x);
real32_t wwa_cbrtf(real32_t x);
real32_t wwa_fmaf(real32_t x, real32_t y, real32_t z);
real32_t wwa_erfcf(real32_t x);
real32_t wwa_nextafterf(real32_t x, real32_t y);
real32_t wwa_rintf(real32_t x);
real32_t wwa_nearbyintf(real32_t x);
i32     wwa_lrintf(real32_t x);
i64     wwa_llrintf(real32_t x);
i32     wwa_lroundf(real32_t x);
i64     wwa_llroundf(real32_t x);
real32_t wwa_fdimf(real32_t x, real32_t y);
i32     wwa_ilogbf(real32_t x);
real32_t wwa_logbf(real32_t x);
real32_t wwa_lgammaf(real32_t x);
real32_t wwa_tgammaf(real32_t x);
real32_t wwa_nanf(const char *s);
i32     wwa_fpclassifyf(real32_t x);
i32     wwa_isnanf(real32_t x);
i32     wwa_isinff(real32_t x);
i32     wwa_isfinitef(real32_t x);
i32     wwa_isnormalf(real32_t x);
i32     wwa_isgreaterf(real32_t x, real32_t y);
i32     wwa_isgreaterequalf(real32_t x, real32_t y);
i32     wwa_islessf(real32_t x, real32_t y);
i32     wwa_islessequalf(real32_t x, real32_t y);
i32     wwa_islessgreaterf(real32_t x, real32_t y);
i32     wwa_isunorderedf(real32_t x, real32_t y);

real64_t wwa_sqrt(real64_t x);
real64_t wwa_sin(real64_t x);
real64_t wwa_cos(real64_t x);
real64_t wwa_tan(real64_t x);
real64_t wwa_exp(real64_t x);
real64_t wwa_log(real64_t x);
real64_t wwa_pow(real64_t x, real64_t y);
real64_t wwa_atan(real64_t x);
real64_t wwa_atan2(real64_t y, real64_t x);
real64_t wwa_fabs(real64_t x);
real64_t wwa_floor(real64_t x);
real64_t wwa_ceil(real64_t x);
real64_t wwa_round(real64_t x);
real64_t wwa_trunc(real64_t x);
real64_t wwa_fmod(real64_t x, real64_t y);

real64_t wwa_ldexp(real64_t x, i32 e);
real64_t wwa_frexp(real64_t x, i32* e);
real64_t wwa_copysign(real64_t x, real64_t y);
i32     wwa_signbit(real64_t x);
real64_t wwa_fmin(real64_t x, real64_t y);
real64_t wwa_fmax(real64_t x, real64_t y);
real64_t wwa_modf(real64_t x, real64_t* iptr);
real64_t wwa_exp2(real64_t x);
real64_t wwa_log2(real64_t x);
real64_t wwa_log10(real64_t x);
real64_t wwa_scalbn(real64_t x, i32 e);
real64_t wwa_scalbln(real64_t x, i64 e);
real64_t wwa_sinh(real64_t x);
real64_t wwa_cosh(real64_t x);
real64_t wwa_tanh(real64_t x);
real64_t wwa_asin(real64_t x);
real64_t wwa_acos(real64_t x);
real64_t wwa_erf(real64_t x);
real64_t wwa_asinh(real64_t x);
real64_t wwa_acosh(real64_t x);
real64_t wwa_atanh(real64_t x);
real64_t wwa_hypot(real64_t x, real64_t y);
real64_t wwa_remainder(real64_t x, real64_t y);
real64_t wwa_expm1(real64_t x);
real64_t wwa_log1p(real64_t x);
real64_t wwa_cbrt(real64_t x);
real64_t wwa_fma(real64_t x, real64_t y, real64_t z);
real64_t wwa_erfc(real64_t x);
real64_t wwa_nextafter(real64_t x, real64_t y);
real64_t wwa_rint(real64_t x);
real64_t wwa_nearbyint(real64_t x);
i32     wwa_lrint(real64_t x);
i64     wwa_llrint(real64_t x);
i32     wwa_lround(real64_t x);
i64     wwa_llround(real64_t x);
real64_t wwa_fdim(real64_t x, real64_t y);
i32     wwa_ilogb(real64_t x);
real64_t wwa_logb(real64_t x);
real64_t wwa_lgamma(real64_t x);
real64_t wwa_tgamma(real64_t x);
real64_t wwa_nan(const char *s);

typedef struct { real64_t re, im; } wwa_complex_t;
typedef struct { real32_t re, im; } wwa_complexf_t;
real64_t wwa_creal(wwa_complex_t z);
real64_t wwa_cimag(wwa_complex_t z);
wwa_complex_t wwa_conj(wwa_complex_t z);
real64_t wwa_cabs(wwa_complex_t z);
real64_t wwa_carg(wwa_complex_t z);
wwa_complex_t wwa_cproj(wwa_complex_t z);
real32_t wwa_crealf(wwa_complexf_t z);
real32_t wwa_cimagf(wwa_complexf_t z);
wwa_complexf_t wwa_conjf(wwa_complexf_t z);
real32_t wwa_cabsf(wwa_complexf_t z);
real32_t wwa_cargf(wwa_complexf_t z);
wwa_complexf_t wwa_cprojf(wwa_complexf_t z);
i32     wwa_fpclassify(real64_t x);
i32     wwa_isnan(real64_t x);
i32     wwa_isinf(real64_t x);
i32     wwa_isfinite(real64_t x);
i32     wwa_isnormal(real64_t x);
i32     wwa_isgreater(real64_t x, real64_t y);
i32     wwa_isgreaterequal(real64_t x, real64_t y);
i32     wwa_isless(real64_t x, real64_t y);
i32     wwa_islessequal(real64_t x, real64_t y);
i32     wwa_islessgreater(real64_t x, real64_t y);
i32     wwa_isunordered(real64_t x, real64_t y);

#endif /* WWA_STDMATH_H */