#include <wwa_render.h>
#include <stdio.h>
#include <stdtime.h>

static wwa_render_state_t r;
static u32 color2[320*240];
static f32 depth2[320*240];

i32 main(i32 argc, string_t argv[]) {
    (void)argc;(void)argv;
    static u32 color_buf[320*240];
    static f32 depth_buf[320*240];
    r.fb.color=color_buf; r.fb.depth=depth_buf;
    r.fb.width=320; r.fb.height=240; r.fb.stride=320;
    for(i32 i=0;i<320*240;i++){color_buf[i]=0xFF000000;depth_buf[i]=100.0f;}
    wwa_printf("t1: direct avx2 fill\n");
    wwa_render_triangle_fill_avx2(&r, 10,10, 300,20, 150,220, 0.1f,0.5f,0.9f, 0xFF00FF00);
    wwa_printf("t1 ok pixels=%u\n", r.pixels_drawn);
    /* snapshot avx2 result */
    for(i32 i=0;i<320*240;i++){color2[i]=color_buf[i];depth2[i]=depth_buf[i];}
    for(i32 i=0;i<320*240;i++){color_buf[i]=0xFF000000;depth_buf[i]=100.0f;}
    r.pixels_drawn=0;
    wwa_printf("t2: scalar fill same tri\n");
    wwa_render_triangle_fill(&r, 10,10, 300,20, 150,220, 0.1f,0.5f,0.9f, 0xFF00FF00);
    wwa_printf("t2 ok pixels=%u\n", r.pixels_drawn);
    i32 diff=0;
    for(i32 i=0;i<320*240;i++){
        if(color2[i]!=color_buf[i] || depth2[i]!=depth_buf[i]) diff++;
    }
    wwa_printf("diff pixels: %d %s\n", diff, diff==0?"(BIT-EXACT)":"(MISMATCH)");
    /* bench: 2000 fills each */
    const i32 N=2000;
    u32 t0=wwa_time_ms();
    for(i32 i=0;i<N;i++) wwa_render_triangle_fill_avx2(&r, 10,10, 300,20, 150,220, 0.1f,0.5f,0.9f, 0xFF00FF00);
    u32 t1=wwa_time_ms();
    for(i32 i=0;i<N;i++) wwa_render_triangle_fill(&r, 10,10, 300,20, 150,220, 0.1f,0.5f,0.9f, 0xFF00FF00);
    u32 t2=wwa_time_ms();
    wwa_printf("bench %d fills: avx2=%ums scalar=%ums speedup=%.2fx\n", N, t1-t0, t2-t1, (f32)(t2-t1)/(f32)(t1-t0));
    return diff==0?0:1;
}
