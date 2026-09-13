/* World Without Answers — std_bench.c
   throughput numbers for the from-scratch std library:
   memory, string, numeric, math, quantized formats, allocator. */

#include <stdtype.h>
#include <stdos.h>
#include <stdtime.h>
#include <stdmem.h>
#include <stdstr.h>
#include <stdmath.h>
#include <stdfloat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stderr.h>

#define N 1048576

static real32_t g_in[N];
static real32_t g_out_f[N];
static u16 g_out_u16[N];
static u8 g_out_u8[N + 32768];
static u8 g_scales[32768];
static u8 g_bytes[N];

static u64 g_t0;
static u64 g_t1;

static i32 g_sort[N];

static void bench_start(void) {
    g_t0 = wwa_time_ticks();
}

static void bench_end(const char_t* name, i32 count) {
    real64_t us;
    g_t1 = wwa_time_ticks();
    us = (real64_t)(g_t1 - g_t0) * 1.0e6 / (real64_t)wwa_time_ticks_per_sec();
    wwa_printf("%-28s %8.2f ms  %8.2f ns/op\n", name, us / 1000.0, us * 1000.0 / (real64_t)count);
}

static void fill(void) {
    u32 i;
    for (i = 0; i < N; i++) {
        u32 r = i * 2654435761u;
        real32_t mag = (real32_t)(r % 1000000u) / 100.0f;
        g_in[i] = (i & 1u) ? -mag : mag;
        g_bytes[i] = (u8)(r >> 24);
    }
}

static void bench_mem(void) {
    bench_start();
    wwa_memcpy(g_bytes + 1024, g_bytes, N - 1024);
    bench_end("memcpy 1M", N);
    bench_start();
    wwa_memset(g_bytes, 0xAB, N);
    bench_end("memset 1M", N);
    bench_start();
    (void)wwa_memcmp(g_bytes, g_bytes + 1, N - 1);
    bench_end("memcmp 1M", N);
    bench_start();
    (void)wwa_strlen((const char_t*)g_bytes);
    bench_end("strlen 1M", N);
}

static void bench_num(void) {
    char_t buf[32];
    i32 i;
    bench_start();
    for (i = 0; i < N; i++) {
        (void)wwa_atoi("123456789");
    }
    bench_end("atoi", N);
    bench_start();
    for (i = 0; i < N; i++) {
        wwa_itoa(i * 7 - 3, buf, sizeof(buf));
    }
    bench_end("itoa", N);
    bench_start();
    for (i = 0; i < N; i++) {
        (void)wwa_strcmp("hello world", "hello there");
    }
    bench_end("strcmp", N);
    bench_start();
    for (i = 0; i < N; i++) {
        (void)wwa_strtol("0x1234ab", NULL, 0);
    }
    bench_end("strtol", N);
    bench_start();
    for (i = 0; i < N; i++) {
        (void)wwa_rand();
    }
    bench_end("rand", N);
    bench_start();
    for (i = 0; i < N; i++) {
        (void)wwa_strtof("3.14159", NULL);
    }
    bench_end("strtof", N);
    bench_start();
    for (i = 0; i < N; i++) {
        (void)wwa_strtod("3.14159", NULL);
    }
    bench_end("strtod", N);
}

static i32 bench_cmp_i32(const void_p a, const void_p b) {
    i32 x = *(const i32*)a;
    i32 y = *(const i32*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static void bench_sort(void) {
    u32 i;
    for (i = 0; i < N; i++) g_sort[i] = (i32)(i * 2654435761u);
    bench_start();
    wwa_qsort(g_sort, N, sizeof(i32), bench_cmp_i32);
    bench_end("qsort 1M i32", N);
}

static void bench_math(void) {
    u32 i;
    real32_t acc = 0.0f;
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_sinf(g_in[i]);
    bench_end("sinf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_cosf(g_in[i]);
    bench_end("cosf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_tanf(g_in[i]);
    bench_end("tanf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_expf(g_in[i] * 0.001f);
    bench_end("expf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_logf(wwa_fabsf(g_in[i]) + 1.0f);
    bench_end("logf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_powf(wwa_fabsf(g_in[i]) + 0.5f, 0.75f);
    bench_end("powf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_atanf(g_in[i]);
    bench_end("atanf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_atan2f(g_in[i], g_in[(i + 17) % N] + 1.0f);
    bench_end("atan2f", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_sqrtf(wwa_fabsf(g_in[i]) + 1.0f);
    bench_end("sqrtf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_exp2f(g_in[i] * 0.01f);
    bench_end("exp2f", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_log2f(wwa_fabsf(g_in[i]) + 1.0f);
    bench_end("log2f", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_tanhf(g_in[i] * 0.001f);
    bench_end("tanhf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_erff(g_in[i] * 0.001f);
    bench_end("erff", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_asinhf(g_in[i] * 0.001f);
    bench_end("asinhf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += wwa_hypotf(g_in[i], g_in[(i + 17) % N] * 0.5f);
    bench_end("hypotf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_sin(g_in[i] * 0.001);
    bench_end("sin", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_exp(g_in[i] * 0.001);
    bench_end("exp", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_log(wwa_fabs((real64_t)g_in[i]) + 1.0);
    bench_end("log", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_sqrt(wwa_fabs((real64_t)g_in[i]) + 1.0);
    bench_end("sqrt", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_sinh(g_in[i] * 0.001);
    bench_end("sinh", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_cosh(g_in[i] * 0.001);
    bench_end("cosh", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_tanh(g_in[i] * 0.001);
    bench_end("tanh", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_asin(g_in[i] * 0.0001);
    bench_end("asin", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_acos(g_in[i] * 0.0001);
    bench_end("acos", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_erf(g_in[i] * 0.001);
    bench_end("erf", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_asinh(g_in[i] * 0.001);
    bench_end("asinh", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_acosh(wwa_fabs((real64_t)g_in[i]) + 1.0);
    bench_end("acosh", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_atanh(g_in[i] * 0.0001);
    bench_end("atanh", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_hypot((real64_t)g_in[i], (real64_t)g_in[(i + 17) % N] * 0.5);
    bench_end("hypot", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_remainder((real64_t)g_in[i] + 1.0, 3.0);
    bench_end("remainder", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_expm1((real64_t)g_in[i] * 0.001);
    bench_end("expm1", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_log1p(wwa_fabs((real64_t)g_in[i]) + 0.5);
    bench_end("log1p", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_cbrt(wwa_fabs((real64_t)g_in[i]) + 0.5);
    bench_end("cbrt", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_fma((real64_t)g_in[i] * 0.001, 0.5, 1.0);
    bench_end("fma", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_erfc((real64_t)g_in[i] * 0.001);
    bench_end("erfc", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_nextafter((real64_t)g_in[i], (real64_t)g_in[(i + 17) % N]);
    bench_end("nextafter", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_rint((real64_t)g_in[i]);
    bench_end("rint", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_llrint((real64_t)g_in[i]);
    bench_end("llrint", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_fdim((real64_t)g_in[i], (real64_t)g_in[(i + 17) % N] * 0.5);
    bench_end("fdim", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_scalbn((real64_t)g_in[i], (i32)(i % 37) - 18);
    bench_end("scalbn", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_ilogb(wwa_fabs((real64_t)g_in[i]) + 0.5);
    bench_end("ilogb", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_logb(wwa_fabs((real64_t)g_in[i]) + 0.5);
    bench_end("logb", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_isnan(wwa_fabs((real64_t)g_in[i]) + 0.5);
    bench_end("isnan", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_isfinite(wwa_fabs((real64_t)g_in[i]) + 0.5);
    bench_end("isfinite", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_fpclassify(wwa_fabs((real64_t)g_in[i]) + 0.5);
    bench_end("fpclassify", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_isunordered((real64_t)g_in[i], (real64_t)g_in[(i + 17) % N]);
    bench_end("isunordered", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_lgamma(wwa_fabs((real64_t)g_in[i]) + 0.5);
    bench_end("lgamma", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)(wwa_tgamma(wwa_fmod(wwa_fabs((real64_t)g_in[i]), 34.0) + 0.5) * 1e-30);
    bench_end("tgamma", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_cabs((wwa_complex_t){g_in[i], g_in[(i + 17) % N]});
    bench_end("cabs", N);
    bench_start();
    for (i = 0; i < N; i++) acc += (real32_t)wwa_carg((wwa_complex_t){g_in[i], g_in[(i + 17) % N]});
    bench_end("carg", N);
    wwa_printf("math checksum: %f\n", (double)acc);
}

static void bench_float(void) {
    u32 i;
    bench_start();
    for (i = 0; i < N; i++) g_out_u16[i] = wwa_f16_from_f32(g_in[i]);
    bench_end("f16 quantize", N);
    bench_start();
    for (i = 0; i < N; i++) g_out_f[i] = wwa_f16_to_f32(g_out_u16[i]);
    bench_end("f16 dequantize", N);
    bench_start();
    for (i = 0; i < N; i++) g_out_u16[i] = wwa_bf16_from_f32(g_in[i]);
    bench_end("bf16 quantize", N);
    bench_start();
    for (i = 0; i < N; i++) g_out_u8[i] = wwa_fp8_e4m3_from_f32(g_in[i]);
    bench_end("fp8 e4m3 quantize", N);
    bench_start();
    for (i = 0; i < N; i++) g_out_f[i] = wwa_fp8_e4m3_to_f32(g_out_u8[i]);
    bench_end("fp8 e4m3 dequantize", N);
    bench_start();
    for (i = 0; i < N; i++) g_out_u8[i] = wwa_fp8_e5m2_from_f32(g_in[i]);
    bench_end("fp8 e5m2 quantize", N);
    bench_start();
    for (i = 0; i < N; i++) g_out_u8[i] = wwa_fp6_e3m2_from_f32(g_in[i]);
    bench_end("fp6 e3m2 quantize", N);
    bench_start();
    for (i = 0; i < N; i++) g_out_u8[i] = wwa_fp6_e2m3_from_f32(g_in[i]);
    bench_end("fp6 e2m3 quantize", N);
    bench_start();
    for (i = 0; i < N; i++) g_out_u8[i] = wwa_fp4_e2m1_from_f32(g_in[i]);
    bench_end("fp4 e2m1 quantize", N);
    bench_start();
    for (i = 0; i < N; i++) {
        u32 p = wwa_fp6_pack_4(g_in[i], g_in[i] + 1.0f, g_in[i] - 1.0f, -g_in[i]);
        g_out_u8[i & 3] = (u8)(p >> ((i & 3) * 8));
    }
    bench_end("fp6 pack_4", N);
    bench_start();
    for (i = 0; i < N; i++) {
        u8 p = wwa_fp4_pack_2(g_in[i], -g_in[i]);
        g_out_u8[i & 1] = p;
    }
    bench_end("fp4 pack_2", N);
    bench_start();
    wwa_mxfp8_e4m3_quantize(g_in, N, g_out_u8, g_scales, 32);
    bench_end("mxfp8 e4m3 quantize", N);
    bench_start();
    wwa_mxfp8_e4m3_dequantize(g_out_u8, g_scales, N, g_out_f, 32);
    bench_end("mxfp8 e4m3 dequantize", N);
    bench_start();
    wwa_mxfp8_e5m2_quantize(g_in, N, g_out_u8, g_scales, 32);
    bench_end("mxfp8 e5m2 quantize", N);
    bench_start();
    wwa_mxfp6_e3m2_quantize(g_in, N, g_out_u8, g_scales, 32);
    bench_end("mxfp6 e3m2 quantize", N);
    bench_start();
    wwa_mxfp4_e2m1_quantize(g_in, N, g_out_u8, g_scales, 32);
    bench_end("mxfp4 e2m1 quantize", N);
    bench_start();
    wwa_nvfp4_quantize(g_in, N, g_out_u8, g_scales);
    bench_end("nvfp4 quantize", N);
    bench_start();
    wwa_nvfp4_dequantize(g_out_u8, g_scales, N, g_out_f);
    bench_end("nvfp4 dequantize", N);
    wwa_printf("float checksum: %f\n", (double)g_out_f[N / 2]);
}

static void bench_alloc(void) {
    const i32 loops = 200000;
    i32 i;
    bench_start();
    for (i = 0; i < loops; i++) {
        void_p p = wwa_malloc(128);
        if (p != NULL) wwa_free(p);
    }
    bench_end("malloc/free 128B", loops);
    bench_start();
    for (i = 0; i < loops; i++) {
        void_p p = wwa_malloc((usize)((i * 2654435761u) % 3000) + 32);
        if (p != NULL) wwa_free(p);
    }
    bench_end("malloc/free mixed", loops);
}

static void bench_stdio(void) {
    char_t buf[64];
    const i32 loops = 100000;
    i32 i;
    bench_start();
    for (i = 0; i < loops; i++) {
        wwa_snprintf(buf, sizeof(buf), "%d %f %s", i, 3.14159, "hello");
    }
    bench_end("snprintf", loops);
    bench_start();
    for (i = 0; i < loops; i++) {
        if ((i & 0x3FF) == 0) wwa_printf("%d %f %s\n", i, 3.14159, "hello");
    }
    bench_end("printf (line-buffered)", loops);
}

i32 main(i32 argc, string_t argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    printf("WWA std bench (AVX2: %s)\n", wwa_os_cpu_avx2() ? "yes" : "no");
    fill();
    bench_mem();
    bench_num();
    bench_sort();
    bench_math();
    bench_float();
    bench_alloc();
    bench_stdio();
    printf("STD_BENCH_DONE\n");
    return 0;
}