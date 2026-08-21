/* World Without Answers — wwa_window.c
   Win32: RegisterClass, CreateWindowEx, DIBSection, StretchDIBits, message pump.
   No libc, uses only kernel32/user32/gdi32 dllimport'd via windows.h. */

#include <wwa_window.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdmem.h>
#include <stdos.h>

struct wwa_window {
    HWND hwnd;
    HDC  hdc;
    HDC  mem_dc;
    HBITMAP bitmap;
    void* bits;        /* BGRA u32* backing the DIB */
    u32*  color;       /* alias of bits */
    f32*  depth;       /* separate f32 buffer */
    i32   width, height;
    i32   stride;
    wwa_input_snapshot_t inp;
};

static wwa_window_t* g_active = NULL; /* for WndProc */

static LRESULT CALLBACK wwa_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    wwa_window_t* w = g_active;
    if (!w) return DefWindowProcA(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_CLOSE:
        w->inp.quit_requested = 1;
        return 0;
    case WM_KEYDOWN: {
        i32 vk = (i32)(wp & 0xFF);
        w->inp.keys[vk] = 1;
        if (vk == VK_ESCAPE) w->inp.quit_requested = 1;
        return 0;
    }
    case WM_KEYUP: {
        i32 vk = (i32)(wp & 0xFF);
        w->inp.keys[vk] = 0;
        return 0;
    }
    case WM_LBUTTONDOWN: w->inp.mouse_btn[0] = 1; return 0;
    case WM_LBUTTONUP:   w->inp.mouse_btn[0] = 0; return 0;
    case WM_RBUTTONDOWN: w->inp.mouse_btn[1] = 1; return 0;
    case WM_RBUTTONUP:   w->inp.mouse_btn[1] = 0; return 0;
    case WM_MBUTTONDOWN: w->inp.mouse_btn[2] = 1; return 0;
    case WM_MBUTTONUP:   w->inp.mouse_btn[2] = 0; return 0;
    case WM_MOUSEWHEEL: {
        short delta = (short)HIWORD(wp);
        w->inp.mouse_wheel += (i32)delta;
        return 0;
    }
    case WM_MOUSEMOVE: {
        i32 nx = (i32)(short)LOWORD(lp);
        i32 ny = (i32)(short)HIWORD(lp);
        w->inp.mouse_dx += nx - w->inp.mouse_x;
        w->inp.mouse_dy += ny - w->inp.mouse_y;
        w->inp.mouse_x = nx;
        w->inp.mouse_y = ny;
        return 0;
    }
    default: break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

wwa_window_t* wwa_window_create(const wwa_window_cfg_t* cfg) {
    i32 w = cfg->width, h = cfg->height;
    const char* title = cfg->title ? cfg->title : "WWA";

    wwa_window_t* win = (wwa_window_t*)wwa_os_alloc(sizeof(wwa_window_t));
    if (!win) return NULL;
    wwa_memset(win, 0, sizeof(wwa_window_t));
    win->width = w; win->height = h; win->stride = w;

    /* Allocate depth buffer */
    usize depth_bytes = (usize)w * (usize)h * 4;
    win->depth = (f32*)wwa_os_alloc(depth_bytes);
    if (!win->depth) { wwa_os_free(win, sizeof(wwa_window_t)); return NULL; }

    /* Register class */
    HINSTANCE hinst = GetModuleHandleA(NULL);
    WNDCLASSA wc;
    wwa_memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = wwa_wndproc;
    wc.hInstance     = hinst;
    wc.lpszClassName = "WWAWindowClass";
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);

    /* Create window */
    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    i32 win_w = rc.right - rc.left;
    i32 win_h = rc.bottom - rc.top;

    win->hwnd = CreateWindowExA(0, "WWAWindowClass", title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, win_w, win_h,
        NULL, NULL, hinst, NULL);
    if (!win->hwnd) { wwa_os_free(win->depth, depth_bytes); wwa_os_free(win, sizeof(wwa_window_t)); return NULL; }

    win->hdc = GetDC(win->hwnd);
    win->mem_dc = CreateCompatibleDC(win->hdc);

    /* DIB Section — BGRA, bottom-up (negative height would be top-down) */
    BITMAPINFO bmi;
    wwa_memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h; /* top-down */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    win->bitmap = CreateDIBSection(win->hdc, &bmi, DIB_RGB_COLORS, &win->bits, NULL, 0);
    win->color = (u32*)win->bits;
    SelectObject(win->mem_dc, win->bitmap);

    g_active = win;
    ShowWindow(win->hwnd, SW_SHOW);
    UpdateWindow(win->hwnd);
    return win;
}

void wwa_window_destroy(wwa_window_t* w) {
    if (!w) return;
    if (w->mem_dc) DeleteDC(w->mem_dc);
    if (w->bitmap) DeleteObject(w->bitmap);
    if (w->hwnd && w->hdc) ReleaseDC(w->hwnd, w->hdc);
    if (w->hwnd) DestroyWindow(w->hwnd);
    usize depth_bytes = (usize)w->width * (usize)w->height * 4;
    if (w->depth) wwa_os_free(w->depth, depth_bytes);
    if (g_active == w) g_active = NULL;
    wwa_os_free(w, sizeof(wwa_window_t));
}

i32 wwa_window_should_close(const wwa_window_t* w) { return w ? w->inp.quit_requested : 1; }

void wwa_window_poll(wwa_window_t* w) {
    if (!w) return;
    /* Snapshot prev state */
    wwa_memmove(w->inp.keys_prev, w->inp.keys, 256);
    wwa_memmove(w->inp.mouse_btn_prev, w->inp.mouse_btn, 3);
    w->inp.mouse_dx = 0;
    w->inp.mouse_dy = 0;

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

u32* wwa_window_color(wwa_window_t* w) { return w ? w->color : NULL; }
f32* wwa_window_depth(wwa_window_t* w) { return w ? w->depth : NULL; }
i32 wwa_window_width(const wwa_window_t* w) { return w ? w->width : 0; }
i32 wwa_window_height(const wwa_window_t* w) { return w ? w->height : 0; }
const wwa_input_snapshot_t* wwa_window_input(const wwa_window_t* w) { return w ? &w->inp : NULL; }

void wwa_window_present(wwa_window_t* w) {
    if (!w || !w->hwnd || !w->hdc || !w->mem_dc) return;
    /* Blit entire DIB to window DC */
    BitBlt(w->hdc, 0, 0, w->width, w->height, w->mem_dc, 0, 0, SRCCOPY);
    /* Also process any pending paint */
    ValidateRect(w->hwnd, NULL);
}

#else /* Linux stub */

#include <stdmem.h>
struct wwa_window { i32 w, h; u32* c; f32* d; wwa_input_snapshot_t inp; };
wwa_window_t* wwa_window_create(const wwa_window_cfg_t* cfg) {
    (void)cfg; return NULL;
}
void wwa_window_destroy(wwa_window_t* w) { (void)w; }
i32 wwa_window_should_close(const wwa_window_t* w) { (void)w; return 1; }
void wwa_window_poll(wwa_window_t* w) { (void)w; }
u32* wwa_window_color(wwa_window_t* w) { (void)w; return NULL; }
f32* wwa_window_depth(wwa_window_t* w) { (void)w; return NULL; }
i32 wwa_window_width(const wwa_window_t* w) { (void)w; return 0; }
i32 wwa_window_height(const wwa_window_t* w) { (void)w; return 0; }
const wwa_input_snapshot_t* wwa_window_input(const wwa_window_t* w) { (void)w; return NULL; }
void wwa_window_present(wwa_window_t* w) { (void)w; }

#endif
