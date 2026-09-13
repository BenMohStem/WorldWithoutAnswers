/* World Without Answers — wwa_main.c
   demo entry: exercises stdos, stdstr, stdmath, stdfloat, stdio, stdlog,
   and self-spawn (child mode returns 9). */

#include <stdtype.h>
#include <stdos.h>
#include <stdtime.h>
#include <stdstr.h>
#include <stdmath.h>
#include <stdfloat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlog.h>
#include <stderr.h>

static void demo_quantize(void) {
    real32_t data[8] = {0.5f, -1.5f, 448.0f, -0.25f, 0.001f, 12345.0f, -3.0f, 7.5f};
    u8 q[8];
    u8 scales[1];
    real32_t back[8];
    u32 i;
    wwa_mxfp8_e4m3_quantize(data, 8, q, scales, 8);
    wwa_mxfp8_e4m3_dequantize(q, scales, 8, back, 8);
    wwa_printf("mxfp8 e4m3 roundtrip (scale byte 0x%02x = %g):\n", scales[0], (double)wwa_mx_scale_to_f32(scales[0]));
    for (i = 0; i < 8; i++) {
        wwa_printf("  %8.3f -> %02x -> %8.3f\n", (double)data[i], q[i], (double)back[i]);
    }
    {
        u16 h = wwa_f16_from_f32(0.1f);
        wwa_printf("f16(0.1) = 0x%04x -> %f\n", h, (double)wwa_f16_to_f32(h));
        h = wwa_bf16_from_f32(0.1f);
        wwa_printf("bf16(0.1) = 0x%04x -> %f\n", h, (double)wwa_bf16_to_f32(h));
    }
}

i32 main(i32 argc, string_t argv[]) {
    char_t t[24];
    i32 child = 0;
    if (argc >= 2 && wwa_strcmp(argv[1], "--child") == 0) {
        printf("child process running with %d args\n", argc);
        return 9;
    }
    wwa_log_init(NULL);
    wwa_log_write(WWA_LOG_LEVEL_NOTE, "wwa", "started");
    printf("World Without Answers demo\n");
    printf("platform: %s\n", WWA_OS_WINDOWS ? "windows" : "linux");
    wwa_time_str(t, sizeof(t));
    printf("time: %s\n", t);
    printf("avx2: %s, avx512: %s\n", wwa_os_cpu_avx2() ? "yes" : "no", wwa_os_cpu_avx512() ? "yes" : "no");
    demo_quantize();
    {
        const char_t* args[3];
        i32 code = -1;
        args[0] = "wwa_main";
        args[1] = "--child";
        args[2] = NULL;
        if (wwa_os_spawn(args[0], args, &code) == 0) {
            wwa_printf("spawned child, exit code %d\n", code);
        } else {
            printf("spawn failed\n");
        }
        child = code;
    }
    wwa_log_write(WWA_LOG_LEVEL_NOTE, "wwa", "done, child=%d", child);
    LOG_HINT("demo finished via stderr.h LOG_ macros");
    wwa_log_close();
    return child == 9 ? 0 : 1;
}