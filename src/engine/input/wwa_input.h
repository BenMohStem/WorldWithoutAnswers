/* World Without Answers — wwa_input.h
   Keyboard + mouse input via OS syscalls.
   Direct Win32 GetAsyncKeyState / GetCursorPos.
   No libc, no 3rd-party libs. */
#ifndef WWA_INPUT_H
#define WWA_INPUT_H
#include <stdtype.h>

typedef struct {
    i32 mouse_x, mouse_y;
    i32 mouse_dx, mouse_dy;
    i32 mouse_wheel;
    u8 keys[256];
    u8 keys_prev[256];
    u8 mouse_buttons[3];
    u8 mouse_buttons_prev[3];
} wwa_input_state_t;

void wwa_input_init(wwa_input_state_t* input);
void wwa_input_update(wwa_input_state_t* input);

/* Key states: 0=up, 1=pressed this frame, 2=held, 0=released */
i32 wwa_input_key_down(const wwa_input_state_t* input, u8 key);
i32 wwa_input_key_pressed(const wwa_input_state_t* input, u8 key);
i32 wwa_input_key_released(const wwa_input_state_t* input, u8 key);
i32 wwa_input_mouse_button(const wwa_input_state_t* input, i32 btn);

/* Virtual key codes (Windows) */
#define WWA_KEY_W     0x57
#define WWA_KEY_A     0x41
#define WWA_KEY_S     0x53
#define WWA_KEY_D     0x44
#define WWA_KEY_SPACE 0x20
#define WWA_KEY_SHIFT 0x10
#define WWA_KEY_CTRL  0x11
#define WWA_KEY_ESC   0x1B
#define WWA_KEY_UP    0x26
#define WWA_KEY_DOWN  0x28
#define WWA_KEY_LEFT  0x25
#define WWA_KEY_RIGHT 0x27
#define WWA_KEY_E     0x45
#define WWA_KEY_Q     0x51
#define WWA_KEY_R     0x52
#define WWA_KEY_F     0x46

#endif /* WWA_INPUT_H */
