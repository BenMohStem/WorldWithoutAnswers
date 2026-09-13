/* World Without Answers — wwa_window.h
   Win32 window with backbuffer blit — no libc, no 3rd-party.
   Provides: window create/destroy, double-buffered u32* framebuffer,
   message pump, keyboard+mouse state.
   Linux path uses X11 stub (compiled out on Windows). */

#ifndef WWA_WINDOW_H
#define WWA_WINDOW_H

#include <stdtype.h>

typedef struct wwa_window wwa_window_t;

typedef struct {
    i32 width;
    i32 height;
    const char* title;
} wwa_window_cfg_t;

typedef struct {
    u8  keys[256];
    u8  keys_prev[256];
    i32 mouse_x, mouse_y;
    i32 mouse_dx, mouse_dy;
    i32 mouse_wheel;
    u8  mouse_btn[3];
    u8  mouse_btn_prev[3];
    i32 quit_requested;
} wwa_input_snapshot_t;

/* Create + show + allocate backbuffer */
wwa_window_t* wwa_window_create(const wwa_window_cfg_t* cfg);
void wwa_window_destroy(wwa_window_t* w);

i32 wwa_window_should_close(const wwa_window_t* w);
void wwa_window_poll(wwa_window_t* w);           /* drain Win32 messages */

/* Backbuffer: W*H u32 RGBA, row stride == W */
u32* wwa_window_color(wwa_window_t* w);
f32* wwa_window_depth(wwa_window_t* w);           /* f32 depth buffer */
i32 wwa_window_width(const wwa_window_t* w);
i32 wwa_window_height(const wwa_window_t* w);
void wwa_window_present(wwa_window_t* w);        /* blit to screen */

const wwa_input_snapshot_t* wwa_window_input(const wwa_window_t* w);

/* Inline helpers */
static inline i32 wwa_key_down(const wwa_input_snapshot_t* s, i32 vk) {
    return s->keys[vk & 0xFF] != 0;
}
static inline i32 wwa_key_pressed(const wwa_input_snapshot_t* s, i32 vk) {
    return s->keys[vk & 0xFF] && !s->keys_prev[vk & 0xFF];
}
static inline i32 wwa_key_released(const wwa_input_snapshot_t* s, i32 vk) {
    return !s->keys[vk & 0xFF] && s->keys_prev[vk & 0xFF];
}

/* VK codes (match Win32) */
#define WWA_VK_ESC    0x1B
#define WWA_VK_SPACE  0x20
#define WWA_VK_LEFT   0x25
#define WWA_VK_UP     0x26
#define WWA_VK_RIGHT  0x27
#define WWA_VK_DOWN   0x28
#define WWA_VK_W      0x57
#define WWA_VK_A      0x41
#define WWA_VK_S      0x53
#define WWA_VK_D      0x44
#define WWA_VK_Q      0x51
#define WWA_VK_E      0x45
#define WWA_VK_R      0x52
#define WWA_VK_SHIFT  0x10
#define WWA_VK_CTRL   0x11

#endif /* WWA_WINDOW_H */
