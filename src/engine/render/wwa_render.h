/* World Without Answers — wwa_render.h
   Software renderer with depth buffer.
   Pineda's edge function rasterization (GPU-style).
   Tile-based rendering for cache locality.

   Research references:
   - "A Parallel Algorithm for Polygon Rasterization" (Pineda 1988)
   - "Optimizing the basic rasterizer" (Fabian Giesen)
   - "Rasterization on Larrabee" (Intel)
   - Nanite-style software rasterizer (Epic Games 2021)

   Design: screen-space tile grid, each tile 16x16 pixels.
   Triangles binned to overlapping tiles. Depth tested per-pixel.
   Framebuffer: RGBA8888 (32 bits per pixel). */

#ifndef WWA_RENDER_H
#define WWA_RENDER_H

#include <stdtype.h>

#define WWA_RENDER_MAX_VERTICES  16384
#define WWA_RENDER_MAX_TRIANGLES 16384
#define WWA_RENDER_MAX_FILLS     8192

/* --- Vertex (position + normal for lighting) --- */
typedef struct {
    f32 x, y, z;        /* world space */
    f32 nx, ny, nz;     /* normal */
    u32 color;          /* RGBA8888 */
} wwa_render_vertex_t;

/* --- Triangle (indices into vertex buffer) --- */
typedef struct {
    u32 v0, v1, v2;    /* vertex indices */
    u32 color;          /* flat-shaded color */
} wwa_render_triangle_t;

/* --- Screen-space triangle (rasterization ready) --- */
typedef struct {
    f32 x0, y0, x1, y1, x2, y2;  /* screen coords */
    f32 z0, z1, z2;               /* depth */
    f32 inv_w0, inv_w1, inv_w2;   /* 1/w for perspective-correct */
    u32 color;
} wwa_render_screentri_t;

/* --- Tile (16x16 block of screen) --- */
#define WWA_TILE_SIZE 16

typedef struct {
    u16 triangles[128];  /* indices into screentri buffer */
    u16 count;
} wwa_render_tile_t;

/* --- Framebuffer --- */
typedef struct {
    u32* color;          /* RGBA8888, width*height */
    f32* depth;          /* depth buffer, width*height */
    i32 width;
    i32 height;
    i32 stride;          /* u32 stride for color buffer */
} wwa_render_framebuffer_t;

/* --- Render state --- */
typedef struct {
    wwa_render_framebuffer_t fb;

    /* Matrices (column-major) */
    f32 proj[16];        /* projection matrix */
    f32 view[16];        /* view matrix */
    f32 mvp[16];         /* model-view-projection */

    /* Camera */
    f32 cam_x, cam_y, cam_z;
    f32 cam_yaw, cam_pitch;

    /* Tile grid */
    i32 tiles_x;
    i32 tiles_y;
    wwa_render_tile_t tiles[256][256];  /* max 256x256 tiles */

    /* Vertex/triangle buffers */
    wwa_render_vertex_t verts[WWA_RENDER_MAX_VERTICES];
    wwa_render_triangle_t tris[WWA_RENDER_MAX_TRIANGLES];
    u32 vert_count;
    u32 tri_count;

    /* Screen-space triangles (rasterization output) */
    wwa_render_screentri_t screentris[WWA_RENDER_MAX_TRIANGLES];
    u32 screentri_count;

    /* Stats */
    u32 pixels_drawn;
    u32 triangles_clipped;
} wwa_render_state_t;

/* --- API --- */
void wwa_render_init(wwa_render_state_t* r, i32 width, i32 height);
void wwa_render_clear(wwa_render_state_t* r, u32 color, f32 depth);
void wwa_render_set_perspective(wwa_render_state_t* r, f32 fov_deg,
                                f32 aspect, f32 near, f32 far);
void wwa_render_set_ortho(wwa_render_state_t* r,
                          f32 left, f32 right, f32 bottom, f32 top,
                          f32 near, f32 far);
void wwa_render_set_lookat(wwa_render_state_t* r,
                           f32 eye_x, f32 eye_y, f32 eye_z,
                           f32 target_x, f32 target_y, f32 target_z,
                           f32 up_x, f32 up_y, f32 up_z);
u32  wwa_render_add_vertex(wwa_render_state_t* r,
                           f32 x, f32 y, f32 z,
                           f32 nx, f32 ny, f32 nz,
                           u32 color);
u32  wwa_render_add_triangle(wwa_render_state_t* r, u32 v0, u32 v1, u32 v2);
void wwa_render_transform(wwa_render_state_t* r);
void wwa_render_clip_and_rasterize(wwa_render_state_t* r);
void wwa_render_triangle_fill(wwa_render_state_t* r,
                              f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2,
                              f32 z0, f32 z1, f32 z2,
                              u32 color);
/* AVX2 variant (8 pixels/iter); caller must check wwa_os_cpu_avx2() */
void wwa_render_triangle_fill_avx2(wwa_render_state_t* r,
                                   f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2,
                                   f32 z0, f32 z1, f32 z2,
                                   u32 color);
void wwa_render_line(wwa_render_state_t* r,
                     i32 x0, i32 y0, i32 x1, i32 y1,
                     u32 color);

/* --- Matrix ops --- */
void wwa_mat4_identity(f32 m[16]);
void wwa_mat4_multiply(f32 out[16], const f32 a[16], const f32 b[16]);
void wwa_mat4_perspective(f32 m[16], f32 fov_deg, f32 aspect,
                          f32 near, f32 far);
void wwa_mat4_lookat(f32 m[16],
                     f32 ex, f32 ey, f32 ez,
                     f32 tx, f32 ty, f32 tz,
                     f32 ux, f32 uy, f32 uz);
void wwa_mat4_ortho(f32 m[16], f32 l, f32 r, f32 b, f32 t, f32 n, f32 f);
void wwa_vec4_transform(f32 out[4], const f32 m[16], const f32 v[4]);

#endif /* WWA_RENDER_H */
