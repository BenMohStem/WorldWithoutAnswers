/* World Without Answers — wwa_input.c
   Input via Win32 API (GetAsyncKeyState, GetCursorPos). */
#include <wwa_input.h>
#include <stdmem.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void wwa_input_init(wwa_input_state_t* input) {
    wwa_memset(input, 0, sizeof(wwa_input_state_t));
}

void wwa_input_update(wwa_input_state_t* input) {
    wwa_memmove(input->keys_prev, input->keys, 256);
    wwa_memmove(input->mouse_buttons_prev, input->mouse_buttons, 3);
    for(i32 i=0;i<256;i++) input->keys[i]=(u8)(GetAsyncKeyState(i)&0x80)?1:0;
    POINT pt; GetCursorPos(&pt);
    input->mouse_dx=pt.x-input->mouse_x;
    input->mouse_dy=pt.y-input->mouse_y;
    input->mouse_x=pt.x; input->mouse_y=pt.y;
    input->mouse_buttons[0]=(u8)(GetAsyncKeyState(VK_LBUTTON)&0x80)?1:0;
    input->mouse_buttons[1]=(u8)(GetAsyncKeyState(VK_RBUTTON)&0x80)?1:0;
    input->mouse_buttons[2]=(u8)(GetAsyncKeyState(VK_MBUTTON)&0x80)?1:0;
}

#else
/* Linux stub */
void wwa_input_init(wwa_input_state_t* input) { wwa_memset(input,0,sizeof(*input)); }
void wwa_input_update(wwa_input_state_t* input) { (void)input; }
#endif

i32 wwa_input_key_down(const wwa_input_state_t* input, u8 key) { return input->keys[key]; }
i32 wwa_input_key_pressed(const wwa_input_state_t* input, u8 key) {
    return input->keys[key] && !input->keys_prev[key];
}
i32 wwa_input_key_released(const wwa_input_state_t* input, u8 key) {
    return !input->keys[key] && input->keys_prev[key];
}
i32 wwa_input_mouse_button(const wwa_input_state_t* input, i32 btn) {
    return input->mouse_buttons[btn];
}
