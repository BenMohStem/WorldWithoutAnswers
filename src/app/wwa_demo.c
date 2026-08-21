/* World Without Answers — wwa_demo.c
   PLAYABLE 3D DEMO: physics + software rendering + WASD movement.
   Renders to framebuffer, outputs ASCII art to console.
   60 FPS physics simulation with stacking boxes.
   WASD + mouse look camera. Space to jump. Click to spawn boxes. */
#include <wwa_physics.h>
#include <wwa_render.h>
#include <wwa_audio.h>
#include <wwa_input.h>
#include <stdtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdstr.h>
#include <stdmath.h>

#define DEMO_W 160
#define DEMO_H 80
#define DEMO_FPS 30

static wwa_physics_world_t phys;
static wwa_render_state_t renderer;
static wwa_audio_state_t audio;
static wwa_input_state_t input;

static f32 cam_yaw = 0.0f;
static f32 cam_pitch = -0.3f;
static f32 cam_speed = 8.0f;
static f32 mouse_sens = 0.003f;

static void build_cube(wwa_render_state_t* r, f32 cx,f32 cy,f32 cz, f32 s, u32 color) {
    f32 h = s * 0.5f;
    u32 v[8];
    v[0]=wwa_render_add_vertex(r,cx-h,cy-h,cz-h, 0,0,-1, color);
    v[1]=wwa_render_add_vertex(r,cx+h,cy-h,cz-h, 0,0,-1, color);
    v[2]=wwa_render_add_vertex(r,cx+h,cy+h,cz-h, 0,0,-1, color);
    v[3]=wwa_render_add_vertex(r,cx-h,cy+h,cz-h, 0,0,-1, color);
    v[4]=wwa_render_add_vertex(r,cx-h,cy-h,cz+h, 0,0,1, color);
    v[5]=wwa_render_add_vertex(r,cx+h,cy-h,cz+h, 0,0,1, color);
    v[6]=wwa_render_add_vertex(r,cx+h,cy+h,cz+h, 0,0,1, color);
    v[7]=wwa_render_add_vertex(r,cx-h,cy+h,cz+h, 0,0,1, color);
    wwa_render_add_triangle(r,v[0],v[1],v[2]); wwa_render_add_triangle(r,v[0],v[2],v[3]);
    wwa_render_add_triangle(r,v[5],v[4],v[7]); wwa_render_add_triangle(r,v[5],v[7],v[6]);
    wwa_render_add_triangle(r,v[4],v[0],v[3]); wwa_render_add_triangle(r,v[4],v[3],v[7]);
    wwa_render_add_triangle(r,v[1],v[5],v[6]); wwa_render_add_triangle(r,v[1],v[6],v[2]);
    wwa_render_add_triangle(r,v[3],v[2],v[6]); wwa_render_add_triangle(r,v[3],v[6],v[7]);
    wwa_render_add_triangle(r,v[4],v[5],v[1]); wwa_render_add_triangle(r,v[4],v[1],v[0]);
}

static void build_ground(wwa_render_state_t* r) {
    u32 c = 0xFF33AA33;
    u32 v0=wwa_render_add_vertex(r,-20,0,-20, 0,1,0, c);
    u32 v1=wwa_render_add_vertex(r, 20,0,-20, 0,1,0, c);
    u32 v2=wwa_render_add_vertex(r, 20,0, 20, 0,1,0, c);
    u32 v3=wwa_render_add_vertex(r,-20,0, 20, 0,1,0, c);
    wwa_render_add_triangle(r,v0,v1,v2); wwa_render_add_triangle(r,v0,v2,v3);
}

static void spawn_box(void) {
    f32 x = (f32)((wwa_rand() % 20) - 10);
    f32 z = (f32)((wwa_rand() % 20) - 10);
    wwa_physics_add_body(&phys, x, 10.0f, z, 0.5f,0.5f,0.5f,
                         1.0f, 0.3f, 0.5f, WWA_BODY_DYNAMIC);
}

static void scene_init(void) {
    wwa_physics_init(&phys, 1.0f/60.0f);
    wwa_physics_add_body(&phys, 0, -0.5f, 0, 20, 0.5f, 20, 0, 0.4f, 0.6f, WWA_BODY_STATIC);
    for(i32 y=0;y<5;y++)
        for(i32 x=-1;x<=1;x++)
            for(i32 z=-1;z<=1;z++)
                wwa_physics_add_body(&phys,(f32)x*1.1f,(f32)(y+1)*1.1f+0.5f,(f32)z*1.1f,
                                     0.5f,0.5f,0.5f,1.0f,0.3f,0.5f,WWA_BODY_DYNAMIC);
    wwa_render_init(&renderer, DEMO_W, DEMO_H);
    wwa_audio_init(&audio);
}

static void scene_update(f32 dt) {
    f32 spd = cam_speed * dt;
    if(wwa_input_key_down(&input, WWA_KEY_SHIFT)) spd *= 2.0f;
    f32 fx=0,fy=0,fz=0;
    if(wwa_input_key_down(&input, WWA_KEY_W)) { fx+=wwa_sinf(cam_yaw); fz+=wwa_cosf(cam_yaw); }
    if(wwa_input_key_down(&input, WWA_KEY_S)) { fx-=wwa_sinf(cam_yaw); fz-=wwa_cosf(cam_yaw); }
    if(wwa_input_key_down(&input, WWA_KEY_A)) { fx-=wwa_cosf(cam_yaw); fz+=wwa_sinf(cam_yaw); }
    if(wwa_input_key_down(&input, WWA_KEY_D)) { fx+=wwa_cosf(cam_yaw); fz-=wwa_sinf(cam_yaw); }
    f32 len = wwa_sqrtf(fx*fx+fz*fz);
    if(len>0.001f){fx/=len;fz/=len;}
    renderer.cam_x += fx*spd;
    renderer.cam_z += fz*spd;
    cam_yaw   += (f32)input.mouse_dx * mouse_sens;
    cam_pitch -= (f32)input.mouse_dy * mouse_sens;
    if(cam_pitch > 1.5f) cam_pitch = 1.5f;
    if(cam_pitch < -1.5f) cam_pitch = -1.5f;
    if(wwa_input_key_down(&input, WWA_KEY_SPACE)) renderer.cam_y += spd;
    if(wwa_input_key_down(&input, WWA_KEY_Q)) renderer.cam_y -= spd;
    static i32 last_spawn = 0;
    if(wwa_input_mouse_button(&input, 0) && !last_spawn) {
        spawn_box();
        wwa_audio_play(&audio, WWA_WAVE_SINE, 440.0f, 0.3f, 0.1f);
    }
    last_spawn = wwa_input_mouse_button(&input, 0);
    wwa_physics_step(&phys, dt);
    f32 tx = renderer.cam_x + wwa_sinf(cam_yaw)*wwa_cosf(cam_pitch)*10.0f;
    f32 ty = renderer.cam_y + wwa_sinf(cam_pitch)*10.0f;
    f32 tz = renderer.cam_z + wwa_cosf(cam_yaw)*wwa_cosf(cam_pitch)*10.0f;
    wwa_render_set_perspective(&renderer, 70.0f,(f32)DEMO_W/(f32)DEMO_H, 0.1f, 100.0f);
    wwa_render_set_lookat(&renderer, renderer.cam_x,renderer.cam_y,renderer.cam_z, tx,ty,tz, 0,1,0);
}

static void scene_render(void) {
    renderer.vert_count = 0;
    renderer.tri_count = 0;
    build_ground(&renderer);
    u32 colors[] = {0xFFFF4444, 0xFF44FF44, 0xFF4444FF, 0xFFFFFF44, 0xFFFF44FF, 0xFF44FFFF};
    for(u32 i=1;i<phys.bodies.count;i++) {
        build_cube(&renderer, phys.bodies.pos_x[i],phys.bodies.pos_y[i],phys.bodies.pos_z[i],
                   1.0f, colors[i % 6]);
    }
    wwa_render_clear(&renderer, 0xFF1a1a2e, 1000.0f);
    wwa_render_transform(&renderer);
    wwa_render_clip_and_rasterize(&renderer);
}

static void render_ascii(void) {
    static char buf[DEMO_H/2 * (DEMO_W+1) + 1];
    char* p = buf;
    const char* shades = " .:-=+*#%@";
    i32 nw = wwa_strlen(shades);
    for(i32 y=0;y<DEMO_H;y+=2) {
        for(i32 x=0;x<DEMO_W;x++) {
            u32 color = renderer.fb.color[y*renderer.fb.stride+x];
            u32 cr=(color>>16)&0xFF, cg=(color>>8)&0xFF, cb=color&0xFF;
            i32 brightness = (i32)(cr*0.299f + cg*0.587f + cb*0.114f);
            i32 idx = brightness * (nw-1) / 255;
            if(idx<0)idx=0; if(idx>=nw)idx=nw-1;
            *p++ = shades[idx];
        }
        *p++ = '\n';
    }
    *p = '\0';
    wwa_printf("\033[H%s", buf);
}

i32 main(i32 argc, string_t argv[]) {
    (void)argc; (void)argv;
    wwa_printf("WWA 3D DEMO - WASD:move Mouse:look Space:up Q:down Click:spawn ESC:quit\n");
    wwa_printf("\033[2J");
    wwa_input_init(&input);
    scene_init();
    u32 last = wwa_time_ms();
    u32 frames = 0;
    i32 running = 1;
    while(running) {
        u32 now = wwa_time_ms();
        f32 dt = (f32)(now - last) / 1000.0f;
        last = now;
        if(dt > 0.05f) dt = 0.05f;
        wwa_input_update(&input);
        if(wwa_input_key_pressed(&input, WWA_KEY_ESC)) running = 0;
        scene_update(dt);
        scene_render();
        render_ascii();
        frames++;
        if(frames % DEMO_FPS == 0) {
            wwa_printf("[Bodies:%u Tris:%u Pixels:%u]  ",
                    phys.bodies.count, renderer.screentri_count, renderer.pixels_drawn);
        }
    }
    wwa_printf("\033[0m\nDemo ended. Bodies: %u, Triangles: %u\n",
           phys.bodies.count, renderer.screentri_count);
    return 0;
}
