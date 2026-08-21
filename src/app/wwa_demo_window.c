/* World Without Answers — wwa_demo_window.c
   PLAYABLE 3D DEMO (windowed): physics + software rendering + input + audio.
   Opens a real Win32 window, draws via DIBSection, 60Hz game loop.
   WASD: move  Space/Q: fly  Mouse: look  Click: spawn box  ESC: quit */
#include <wwa_window.h>
#include <wwa_physics.h>
#include <wwa_render.h>
#include <wwa_audio.h>
#include <stdtime.h>
#include <stdtype.h>
#include <stdlib.h>

#define DEMO_TITLE "WWA — Playable 3D Demo  [WASD move  Space/Q fly  Mouse look  Click spawn  ESC quit]"
#define DEMO_W 1280
#define DEMO_H 720
#define TARGET_FPS 60

static wwa_physics_world_t g_phys;
static wwa_render_state_t  g_rend;
static wwa_audio_state_t   g_audio;

static f32 g_cam_yaw = 0.0f;
static f32 g_cam_pitch = -0.20f;
static f32 g_cam_speed = 10.0f;
static f32 g_mouse_sens = 0.0035f;

static void build_cube_world(wwa_render_state_t* r, f32 cx,f32 cy,f32 cz, f32 s, u32 color) {
    f32 h = s*0.5f;
    u32 v[8];
    v[0]=wwa_render_add_vertex(r,cx-h,cy-h,cz-h,0,0,-1,color);
    v[1]=wwa_render_add_vertex(r,cx+h,cy-h,cz-h,0,0,-1,color);
    v[2]=wwa_render_add_vertex(r,cx+h,cy+h,cz-h,0,0,-1,color);
    v[3]=wwa_render_add_vertex(r,cx-h,cy+h,cz-h,0,0,-1,color);
    v[4]=wwa_render_add_vertex(r,cx-h,cy-h,cz+h,0,0,1,color);
    v[5]=wwa_render_add_vertex(r,cx+h,cy-h,cz+h,0,0,1,color);
    v[6]=wwa_render_add_vertex(r,cx+h,cy+h,cz+h,0,0,1,color);
    v[7]=wwa_render_add_vertex(r,cx-h,cy+h,cz+h,0,0,1,color);
    wwa_render_add_triangle(r,v[0],v[1],v[2]); wwa_render_add_triangle(r,v[0],v[2],v[3]);
    wwa_render_add_triangle(r,v[5],v[4],v[7]); wwa_render_add_triangle(r,v[5],v[7],v[6]);
    wwa_render_add_triangle(r,v[4],v[0],v[3]); wwa_render_add_triangle(r,v[4],v[3],v[7]);
    wwa_render_add_triangle(r,v[1],v[5],v[6]); wwa_render_add_triangle(r,v[1],v[6],v[2]);
    wwa_render_add_triangle(r,v[3],v[2],v[6]); wwa_render_add_triangle(r,v[3],v[6],v[7]);
    wwa_render_add_triangle(r,v[4],v[5],v[1]); wwa_render_add_triangle(r,v[4],v[1],v[0]);
}

static void build_ground_world(wwa_render_state_t* r) {
    u32 c = 0xFF33AA33;
    u32 v0=wwa_render_add_vertex(r,-20,0,-20,0,1,0,c);
    u32 v1=wwa_render_add_vertex(r, 20,0,-20,0,1,0,c);
    u32 v2=wwa_render_add_vertex(r, 20,0, 20,0,1,0,c);
    u32 v3=wwa_render_add_vertex(r,-20,0, 20,0,1,0,c);
    wwa_render_add_triangle(r,v0,v1,v2); wwa_render_add_triangle(r,v0,v2,v3);
}

i32 main(i32 argc, string_t argv[]) {
    (void)argc; (void)argv;

    /* --- Window --- */
    wwa_window_cfg_t wcfg; wcfg.width=DEMO_W; wcfg.height=DEMO_H; wcfg.title=DEMO_TITLE;
    wwa_window_t* win = wwa_window_create(&wcfg);
    if (!win) {
        return 1;
    }

    /* --- Physics --- */
    wwa_physics_init(&g_phys, 1.0f/60.0f);
    wwa_physics_add_body(&g_phys, 0,-0.5f,0, 20,0.5f,20, 0, 0.4f,0.6f, WWA_BODY_STATIC);
    for(i32 y=0;y<5;y++) for(i32 x=-1;x<=1;x++) for(i32 z=-1;z<=1;z++)
        wwa_physics_add_body(&g_phys,(f32)x*1.1f,(f32)(y+1)*1.1f+0.5f,(f32)z*1.1f, 0.5f,0.5f,0.5f, 1.0f,0.3f,0.5f, WWA_BODY_DYNAMIC);

    /* --- Renderer: attach window backbuffer --- */
    g_rend.fb.color  = wwa_window_color(win);
    g_rend.fb.depth  = wwa_window_depth(win);
    g_rend.fb.width  = wwa_window_width(win);
    g_rend.fb.height = wwa_window_height(win);
    g_rend.fb.stride = g_rend.fb.width;
    g_rend.tiles_x   = (g_rend.fb.width  + WWA_TILE_SIZE - 1)/WWA_TILE_SIZE;
    g_rend.tiles_y   = (g_rend.fb.height + WWA_TILE_SIZE - 1)/WWA_TILE_SIZE;
    g_rend.cam_x = 0; g_rend.cam_y = 6; g_rend.cam_z = 14;

    /* --- Audio --- */
    wwa_audio_init(&g_audio);

    /* --- Game loop --- */
    u32 last_ms = wwa_time_ms();
    i32 prev_click = 0;

    while (!wwa_window_should_close(win)) {
        /* Time */
        u32 now = wwa_time_ms();
        f32 dt = (f32)(now - last_ms) / 1000.0f;
        last_ms = now;
        if (dt > 0.05f) dt = 0.05f;

        /* Input: poll window */
        wwa_window_poll(win);
        const wwa_input_snapshot_t* inp = wwa_window_input(win);

        /* Camera: WASD + mouse look */
        f32 spd = g_cam_speed * dt;
        if (wwa_key_down(inp,WWA_VK_SHIFT)) spd *= 2.0f;

        f32 fx=0,fz=0;
        if (wwa_key_down(inp,WWA_VK_W)) { fx+=0; fz+=0; }
        f32 yaw_sin = 0, yaw_cos = 1;
        /* Use sin/cos of yaw for ground-plane movement */
        {
            /* Cheap sincos via wwa: use integer table or call math */
            /* We'll approximate with the renderer math via wwa_mat */
            extern f32 wwa_sinf(f32); extern f32 wwa_cosf(f32);
            yaw_sin = wwa_sinf(g_cam_yaw);
            yaw_cos = wwa_cosf(g_cam_yaw);
        }
        fx=0; fz=0;
        if (wwa_key_down(inp,WWA_VK_W)) { fx += yaw_sin; fz += yaw_cos; }
        if (wwa_key_down(inp,WWA_VK_S)) { fx -= yaw_sin; fz -= yaw_cos; }
        if (wwa_key_down(inp,WWA_VK_A)) { fx -= yaw_cos; fz += yaw_sin; }
        if (wwa_key_down(inp,WWA_VK_D)) { fx += yaw_cos; fz -= yaw_sin; }

        {
            f32 len2 = fx*fx+fz*fz;
            if (len2 > 0.001f) {
                extern f32 wwa_sqrtf(f32);
                f32 inv = 1.0f / wwa_sqrtf(len2);
                fx *= inv; fz *= inv;
            }
        }
        g_rend.cam_x += fx * spd;
        g_rend.cam_z += fz * spd;
        if (wwa_key_down(inp,WWA_VK_SPACE)) g_rend.cam_y += spd;
        if (wwa_key_down(inp,WWA_VK_Q))     g_rend.cam_y -= spd;

        /* Mouse look */
        g_cam_yaw   += (f32)inp->mouse_dx * g_mouse_sens;
        g_cam_pitch -= (f32)inp->mouse_dy * g_mouse_sens;
        if (g_cam_pitch > 1.5f)  g_cam_pitch = 1.5f;
        if (g_cam_pitch < -1.5f) g_cam_pitch = -1.5f;

        /* Spawn box on click */
        i32 click = inp->mouse_btn[0] ? 1 : 0;
        if (click && !prev_click) {
            f32 sx = (f32)((wwa_rand() % 20) - 10);
            f32 sz = (f32)((wwa_rand() % 20) - 10);
            wwa_physics_add_body(&g_phys, sx, 12.0f, sz, 0.5f,0.5f,0.5f, 1.0f,0.3f,0.5f, WWA_BODY_DYNAMIC);
            wwa_audio_play(&g_audio, WWA_WAVE_SINE, 440.0f, 0.3f, 0.08f);
        }
        prev_click = click;

        /* Physics */
        wwa_physics_step(&g_phys, dt);

        /* Camera matrices */
        {
            extern f32 wwa_sinf(f32); extern f32 wwa_cosf(f32);
            f32 tx = g_rend.cam_x + wwa_sinf(g_cam_yaw)*wwa_cosf(g_cam_pitch)*10.0f;
            f32 ty = g_rend.cam_y + wwa_sinf(g_cam_pitch)*10.0f;
            f32 tz = g_rend.cam_z + wwa_cosf(g_cam_yaw)*wwa_cosf(g_cam_pitch)*10.0f;
            wwa_render_set_perspective(&g_rend, 70.0f,(f32)DEMO_W/(f32)DEMO_H, 0.1f,100.0f);
            wwa_render_set_lookat(&g_rend, g_rend.cam_x,g_rend.cam_y,g_rend.cam_z, tx,ty,tz, 0,1,0);
        }

        /* Geometry: rebuild from physics state */
        g_rend.vert_count = 0; g_rend.tri_count = 0;
        build_ground_world(&g_rend);
        {
            u32 col[6]={0xFFFF4444,0xFF44FF44,0xFF4444FF,0xFFFFFF44,0xFFFF44FF,0xFF44FFFF};
            for(u32 i=1;i<g_phys.bodies.count;i++){
                build_cube_world(&g_rend, g_phys.bodies.pos_x[i],g_phys.bodies.pos_y[i],g_phys.bodies.pos_z[i], 1.0f, col[i%6]);
            }
        }

        /* Rasterize */
        wwa_render_clear(&g_rend, 0xFF1a1a2e, 1.0f);
        wwa_render_transform(&g_rend);
        wwa_render_clip_and_rasterize(&g_rend);

        /* Blit to window */
        wwa_window_present(win);
    }

    wwa_window_destroy(win);
    return 0;
}
