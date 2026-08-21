/* World Without Answers — stdfloat.h
   from-scratch quantized float formats:
   f16, bf16, fp8 e4m3 / e5m2, fp6 e3m2 / e2m3, fp4 e2m1,
   packed fp6 (4 x 6bit per 24 bits), packed fp4 (2 x 4bit per byte),
   MX block-scale formats (32 elements, one E8M0 scale byte per block),
   NVFP4 two-level (e2m1 elements + 8-bit scale per 16 elements).
   OCP-compatible boundaries: e4m3 0x7E = 448, 0x7F/0xFF = NaN. */

#ifndef WWA_STDFLOAT_H
#define WWA_STDFLOAT_H

#include <stdtype.h>

u16       wwa_f16_from_f32(real32_t x);
real32_t  wwa_f16_to_f32(u16 h);
u16       wwa_bf16_from_f32(real32_t x);
real32_t  wwa_bf16_to_f32(u16 b);

u8        wwa_fp8_e4m3_from_f32(real32_t x);
real32_t  wwa_fp8_e4m3_to_f32(u8 h);
u8        wwa_fp8_e5m2_from_f32(real32_t x);
real32_t  wwa_fp8_e5m2_to_f32(u8 h);

u8        wwa_fp6_e3m2_from_f32(real32_t x);
real32_t  wwa_fp6_e3m2_to_f32(u8 h);
u8        wwa_fp6_e2m3_from_f32(real32_t x);
real32_t  wwa_fp6_e2m3_to_f32(u8 h);

u8        wwa_fp4_e2m1_from_f32(real32_t x);
real32_t  wwa_fp4_e2m1_to_f32(u8 h);

u32       wwa_fp6_pack_4(real32_t a, real32_t b, real32_t c, real32_t d);
void      wwa_fp6_unpack_4(u32 p, real32_t* a, real32_t* b, real32_t* c, real32_t* d);
u8        wwa_fp4_pack_2(real32_t a, real32_t b);
void      wwa_fp4_unpack_2(u8 p, real32_t* a, real32_t* b);

#define WWA_MX_BLOCK_DEFAULT 32
#define WWA_NVFP4_BLOCK 16

real32_t  wwa_mx_scale_to_f32(u8 s);
u8        wwa_mx_scale_from_f32(real32_t s);

i32 wwa_mxfp8_e4m3_quantize(const real32_t* in, usize n, u8* out, u8* scales, usize block);
i32 wwa_mxfp8_e4m3_dequantize(const u8* in, const u8* scales, usize n, real32_t* out, usize block);
i32 wwa_mxfp8_e5m2_quantize(const real32_t* in, usize n, u8* out, u8* scales, usize block);
i32 wwa_mxfp8_e5m2_dequantize(const u8* in, const u8* scales, usize n, real32_t* out, usize block);
i32 wwa_mxfp6_e3m2_quantize(const real32_t* in, usize n, u8* out, u8* scales, usize block);
i32 wwa_mxfp6_e3m2_dequantize(const u8* in, const u8* scales, usize n, real32_t* out, usize block);
i32 wwa_mxfp4_e2m1_quantize(const real32_t* in, usize n, u8* out, u8* scales, usize block);
i32 wwa_mxfp4_e2m1_dequantize(const u8* in, const u8* scales, usize n, real32_t* out, usize block);

i32 wwa_nvfp4_quantize(const real32_t* in, usize n, u8* out, u8* scales);
i32 wwa_nvfp4_dequantize(const u8* in, const u8* scales, usize n, real32_t* out);

#endif /* WWA_STDFLOAT_H */