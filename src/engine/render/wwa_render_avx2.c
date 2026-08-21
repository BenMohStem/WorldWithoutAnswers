/* World Without Answers — wwa_render_avx2.c
   AVX2 triangle fill: 8 pixels per iteration.
   Edge functions are affine in x/y, so 8 lane values come from
   one broadcast + FMA against an iota vector (Pineda 1988).
   Depth test + RGBA pack done in-lane, written with maskstore. */
#include <wwa_render.h>
#include <immintrin.h>

static inline __m256 wwa_avx_iota(void) {
    const __m256i idx = _mm256_setr_epi32(0,1,2,3,4,5,6,7);
    return _mm256_cvtepi32_ps(idx);
}

void wwa_render_triangle_fill_avx2(wwa_render_state_t* r,
    f32 x0,f32 y0,f32 x1,f32 y1,f32 x2,f32 y2, f32 z0,f32 z1,f32 z2, u32 color)
{
    i32 minx=(i32)x0,miny=(i32)y0,maxx=(i32)x0,maxy=(i32)y0;
    if((i32)x1<minx)minx=(i32)x1; if((i32)x1>maxx)maxx=(i32)x1;
    if((i32)y1<miny)miny=(i32)y1; if((i32)y1>maxy)maxy=(i32)y1;
    if((i32)x2<minx)minx=(i32)x2; if((i32)x2>maxx)maxx=(i32)x2;
    if((i32)y2<miny)miny=(i32)y2; if((i32)y2>maxy)maxy=(i32)y2;
    if(minx<0)minx=0; if(miny<0)miny=0;
    if(maxx>=r->fb.width)maxx=r->fb.width-1;
    if(maxy>=r->fb.height)maxy=r->fb.height-1;

    const f32 dx01=x0-x1,dy01=y0-y1,dx12=x1-x2,dy12=y1-y2,dx20=x2-x0,dy20=y2-y0;
    const f32 area=dx01*dy12-dy01*dx12;
    if(area<=0.0f)return;
    const f32 inv=1.0f/area;
    const u32 cr=(color>>16)&0xFF,cg=(color>>8)&0xFF,cb=color&0xFF;

    const __m256 vinv   = _mm256_set1_ps(inv);
    const __m256 vx1    = _mm256_set1_ps(x1);
    const __m256 vy1    = _mm256_set1_ps(y1);
    const __m256 vx2    = _mm256_set1_ps(x2);
    const __m256 vy2    = _mm256_set1_ps(y2);
    const __m256 vx0    = _mm256_set1_ps(x0);
    const __m256 vy0    = _mm256_set1_ps(y0);
    const __m256 vdy01  = _mm256_set1_ps(dy01);
    const __m256 vdx01  = _mm256_set1_ps(dx01);
    const __m256 vdy12  = _mm256_set1_ps(dy12);
    const __m256 vdx12  = _mm256_set1_ps(dx12);
    const __m256 vdy20  = _mm256_set1_ps(dy20);
    const __m256 vdx20  = _mm256_set1_ps(dx20);
    const __m256 vz0    = _mm256_set1_ps(z0);
    const __m256 vz1    = _mm256_set1_ps(z1);
    const __m256 vz2    = _mm256_set1_ps(z2);
    const __m256 zero   = _mm256_setzero_ps();
    const __m256 lbase  = _mm256_set1_ps(0.7f);
    const __m256 lscale = _mm256_set1_ps(0.3f);
    const __m256 v255   = _mm256_set1_ps(255.0f);
    const __m256i cbase = _mm256_set1_epi32(0xFF000000);
    const __m256i iota  = _mm256_setr_epi32(0,1,2,3,4,5,6,7);
    const __m256 viota  = _mm256_cvtepi32_ps(iota);

    const i32 span = maxx - minx + 1;
    const i32 full = span & ~7;

    for(i32 py=miny;py<=maxy;py++){
        const f32 fy=(f32)py+0.5f;
        const __m256 vfy = _mm256_set1_ps(fy);
        const __m256 vfx = _mm256_add_ps(_mm256_set1_ps((f32)minx+0.5f), viota);
        /* row-start edge values */
        __m256 w0 = _mm256_sub_ps(_mm256_mul_ps(_mm256_sub_ps(vfx,vx1),vdy01),
                                  _mm256_mul_ps(_mm256_sub_ps(vfy,vy1),vdx01));
        __m256 w1 = _mm256_sub_ps(_mm256_mul_ps(_mm256_sub_ps(vfx,vx2),vdy12),
                                  _mm256_mul_ps(_mm256_sub_ps(vfy,vy2),vdx12));
        __m256 w2 = _mm256_sub_ps(_mm256_mul_ps(_mm256_sub_ps(vfx,vx0),vdy20),
                                  _mm256_mul_ps(_mm256_sub_ps(vfy,vy0),vdx20));
        /* per-8-pixel x increments (one full lane group per iteration) */
        const __m256 s0=_mm256_set1_ps(dy01*8.0f), s1=_mm256_set1_ps(dy12*8.0f), s2=_mm256_set1_ps(dy20*8.0f);
        f32* dptr = r->fb.depth + (usize)py*r->fb.stride + minx;
        u32* cptr = r->fb.color + (usize)py*r->fb.stride + minx;

        for(i32 px=0;px<full;px+=8){
            const __m256 in0 = _mm256_cmp_ps(w0,zero,_CMP_GE_OQ);
            const __m256 in1 = _mm256_cmp_ps(w1,zero,_CMP_GE_OQ);
            const __m256 in2 = _mm256_cmp_ps(w2,zero,_CMP_GE_OQ);
            __m256 mask = _mm256_and_ps(_mm256_and_ps(in0,in1),in2);

            const __m256 n0 = _mm256_mul_ps(w0,vinv);
            const __m256 n1 = _mm256_mul_ps(w1,vinv);
            const __m256 n2 = _mm256_mul_ps(w2,vinv);
            const __m256 z  = _mm256_add_ps(
                                _mm256_add_ps(_mm256_mul_ps(vz0,n0),_mm256_mul_ps(vz1,n1)),
                                _mm256_mul_ps(vz2,n2));

            const __m256 dvec  = _mm256_loadu_ps(dptr+px);
            const __m256 dpass = _mm256_cmp_ps(z,dvec,_CMP_LT_OQ);
            mask = _mm256_and_ps(mask,dpass);

            const i32 m = _mm256_movemask_ps(mask);
            if(m){
                const __m256 light = _mm256_add_ps(_mm256_mul_ps(n0,lscale),lbase);
                __m256i lr = _mm256_cvttps_epi32(_mm256_mul_ps(_mm256_set1_ps((f32)cr),light));
                __m256i lg = _mm256_cvttps_epi32(_mm256_mul_ps(_mm256_set1_ps((f32)cg),light));
                __m256i lb = _mm256_cvttps_epi32(_mm256_mul_ps(_mm256_set1_ps((f32)cb),light));
                const __m256i m255 = _mm256_castps_si256(v255);
                lr = _mm256_min_epi32(lr,m255);
                lg = _mm256_min_epi32(lg,m255);
                lb = _mm256_min_epi32(lb,m255);
                __m256i pix = _mm256_or_si256(cbase,
                              _mm256_or_si256(_mm256_slli_epi32(lr,16),
                              _mm256_or_si256(_mm256_slli_epi32(lg,8),lb)));
                _mm256_maskstore_ps(dptr+px,_mm256_castps_si256(mask),z);
                _mm256_maskstore_epi32((i32*)(cptr+px),_mm256_castps_si256(mask),pix);
                r->pixels_drawn += (u32)__builtin_popcount(m);
            }

            w0 = _mm256_add_ps(w0,s0);
            w1 = _mm256_add_ps(w1,s1);
            w2 = _mm256_add_ps(w2,s2);
        }
        /* scalar tail */
        for(i32 px=minx+full;px<=maxx;px++){
            const f32 fx=(f32)px+0.5f;
            f32 e0=(fx-x1)*dy01-(fy-y1)*dx01;
            f32 e1=(fx-x2)*dy12-(fy-y2)*dx12;
            f32 e2=(fx-x0)*dy20-(fy-y0)*dx20;
            if(e0>=0&&e1>=0&&e2>=0){
                e0*=inv;e1*=inv;e2*=inv;
                const f32 z=z0*e0+z1*e1+z2*e2;
                const u32 idx=(u32)(py*r->fb.stride+px);
                if(z<r->fb.depth[idx]){
                    r->fb.depth[idx]=z;
                    f32 light=0.7f+0.3f*e0;
                    u32 lr=(u32)(cr*light); if(lr>255)lr=255;
                    u32 lg=(u32)(cg*light); if(lg>255)lg=255;
                    u32 lb=(u32)(cb*light); if(lb>255)lb=255;
                    r->fb.color[idx]=0xFF000000|(lr<<16)|(lg<<8)|lb;
                    r->pixels_drawn++;
                }
            }
        }
    }
}
