/* World Without Answers — wwa_build.c
   from-scratch build driver: compiles the std sources + one target
   with gcc, no libc, incremental via mtime checks, then runs it.
   commands: build | run | std-check | bench-std | all | clean | info | help */

#include <stdtype.h>
#include <stdos.h>
#include <stdtime.h>
#include <stdstr.h>
#include <stdio.h>
#include <stdlib.h>
#include <stderr.h>

#if WWA_OS_WINDOWS
#define WWA_EXE_EXT ".exe"
#define WWA_OBJ_DIR "bin_obj/windows/release"
#define WWA_EXE_DIR "bin_exe/windows/release"
#else
#define WWA_EXE_EXT ""
#define WWA_OBJ_DIR "bin_obj/linux/release"
#define WWA_EXE_DIR "bin_exe/linux/release"
#endif

#define WWA_CC "gcc"
#define WWA_AVX2_FLAG "-mavx2"

typedef struct {
    const char_t* src;
    const char_t* obj;
    const char_t* extra;
} wwa_src_t;

static i32 wwa_build_push_base(const char_t** args, i32 n) {
    args[n++] = "-O3";
    args[n++] = "-std=c11";
    args[n++] = "-Wall";
    args[n++] = "-Wextra";
    args[n++] = "-fno-builtin";
    args[n++] = "-fno-stack-protector";
    args[n++] = "-Isrc/std";
    args[n++] = "-Isrc/engine/physics";
    args[n++] = "-Isrc/engine/render";
    args[n++] = "-Isrc/engine/audio";
    args[n++] = "-Isrc/engine/input";
    args[n++] = "-Isrc/engine/window";
    return n;
}

static const wwa_src_t g_std_srcs[] = {
    {"src/std/stdos.c",      WWA_OBJ_DIR "/obj/stdos.o",        NULL},
    {"src/std/stdtime.c",    WWA_OBJ_DIR "/obj/stdtime.o",      NULL},
    {"src/std/stdmem.c",     WWA_OBJ_DIR "/obj/stdmem.o",       NULL},
    {"src/std/stdmem_avx2.c",WWA_OBJ_DIR "/obj/stdmem_avx2.o",  WWA_AVX2_FLAG},
    {"src/std/stdstr.c",     WWA_OBJ_DIR "/obj/stdstr.o",       NULL},
    {"src/std/stdstr_avx2.c",WWA_OBJ_DIR "/obj/stdstr_avx2.o",  WWA_AVX2_FLAG},
    {"src/std/stdhash.c",    WWA_OBJ_DIR "/obj/stdhash.o",      NULL},
    {"src/std/stdthread.c",  WWA_OBJ_DIR "/obj/stdthread.o",    NULL},
    {"src/std/stdmath.c",    WWA_OBJ_DIR "/obj/stdmath.o",      NULL},
    {"src/std/stdfloat.c",   WWA_OBJ_DIR "/obj/stdfloat.o",     NULL},
    {"src/std/stdio.c",      WWA_OBJ_DIR "/obj/stdio.o",        NULL},
    {"src/std/stdlib.c",     WWA_OBJ_DIR "/obj/stdlib.o",       NULL},
    {"src/std/stdlog.c",     WWA_OBJ_DIR "/obj/stdlog.o",       NULL},
    {"src/std/wwa_crt.c",    WWA_OBJ_DIR "/obj/wwa_crt.o",      NULL},
    {"src/engine/physics/wwa_physics.c", WWA_OBJ_DIR "/obj/wwa_physics.o", NULL},
    {"src/engine/render/wwa_render.c",   WWA_OBJ_DIR "/obj/wwa_render.o",   NULL},
    {"src/engine/render/wwa_render_avx2.c", WWA_OBJ_DIR "/obj/wwa_render_avx2.o", WWA_AVX2_FLAG},
    {"src/engine/audio/wwa_audio.c",     WWA_OBJ_DIR "/obj/wwa_audio.o",     NULL},
    {"src/engine/input/wwa_input.c",     WWA_OBJ_DIR "/obj/wwa_input.o",     NULL},
    {"src/engine/window/wwa_window.c",   WWA_OBJ_DIR "/obj/wwa_window.o",   NULL},
};

static const char_t* g_obj_list[ARRAY_COUNT(g_std_srcs)];
static i32 g_obj_count = 0;

static i32 wwa_build_dir_create(const char_t* dir) {
    char_t buf[256];
    usize i;
    for (i = 1; dir[i] != 0; i++) {
        if (dir[i] == '/' || dir[i] == '\\') {
            usize j;
            for (j = 0; j < i; j++) buf[j] = dir[j];
            buf[i] = 0;
            wwa_os_dir_create(buf);
        }
    }
    return wwa_os_dir_create(dir);
}

static i32 wwa_build_compile(const char_t* src, const char_t* obj, const char_t* extra) {
    const char_t* args[64];
    i32 n = 0;
    i32 code = -1;
    i64 src_m, obj_m;
    if (wwa_os_file_exists(obj)) {
        src_m = wwa_os_file_mtime(src);
        obj_m = wwa_os_file_mtime(obj);
        if (src_m >= 0 && obj_m >= src_m) return 0;
    }
    wwa_printf("cc %s\n", src);
    args[n++] = WWA_CC;
    args[n++] = "-c";
    n = wwa_build_push_base(args, n);
    if (extra != NULL) args[n++] = extra;
    args[n++] = src;
    args[n++] = "-o";
    args[n++] = obj;
    args[n] = NULL;
    if (wwa_os_spawn(WWA_CC, args, &code) != 0 || code != 0) return -1;
    return 0;
}

static i32 wwa_build_target(const char_t* src, const char_t* exe, const char_t* define) {
    const char_t* args[64];
    i32 n = 0;
    i32 code = -1;
    i64 src_m, exe_m;
    i32 i;
    if (wwa_os_file_exists(exe)) {
        src_m = wwa_os_file_mtime(src);
        exe_m = wwa_os_file_mtime(exe);
        if (src_m >= 0 && exe_m >= src_m) {
            i32 up_to_date = 1;
            for (i = 0; i < g_obj_count; i++) {
                if (wwa_os_file_mtime(g_obj_list[i]) > exe_m) {
                    up_to_date = 0;
                    break;
                }
            }
            if (up_to_date) return 0;
        }
    }
    wwa_printf("cc %s\n", src);
    args[n++] = WWA_CC;
    n = wwa_build_push_base(args, n);
    if (define != NULL) args[n++] = define;
    for (i = 0; i < g_obj_count; i++) args[n++] = g_obj_list[i];
    args[n++] = src;
    args[n++] = "-o";
    args[n++] = exe;
#if WWA_OS_WINDOWS
    args[n++] = "-nostdlib";
    args[n++] = "-lkernel32";
    args[n++] = "-luser32";
    args[n++] = "-lgdi32";
    args[n++] = "-Wl,-e,mainCRTStartup";
#else
    args[n++] = "-nostdlib";
    args[n++] = "-static";
#endif
    args[n] = NULL;
    if (wwa_os_spawn(WWA_CC, args, &code) != 0 || code != 0) return -1;
    return 0;
}

static i32 wwa_build_all(void) {
    const char_t* targets[][2] = {
        {"src/app/wwa_main.c",        WWA_EXE_DIR "/wwa_main" WWA_EXE_EXT},
        {"src/app/wwa_demo.c",        WWA_EXE_DIR "/wwa_demo" WWA_EXE_EXT},
        {"src/app/wwa_demo_window.c", WWA_EXE_DIR "/wwa_demo_window" WWA_EXE_EXT},
        {"src/tests/std_selftest.c",  WWA_EXE_DIR "/std_selftest" WWA_EXE_EXT},
        {"src/tests/std_bench.c",     WWA_EXE_DIR "/std_bench" WWA_EXE_EXT},
    };
    u32 i;
    wwa_build_dir_create(WWA_OBJ_DIR "/obj");
    wwa_build_dir_create(WWA_EXE_DIR);
    g_obj_count = 0;
    for (i = 0; i < ARRAY_COUNT(g_std_srcs); i++) {
        if (wwa_build_compile(g_std_srcs[i].src, g_std_srcs[i].obj, g_std_srcs[i].extra) != 0) {
            printf("build failed: %s\n", g_std_srcs[i].src);
            return -1;
        }
        g_obj_list[g_obj_count++] = g_std_srcs[i].obj;
    }
    for (i = 0; i < ARRAY_COUNT(targets); i++) {
        if (wwa_build_target(targets[i][0], targets[i][1], NULL) != 0) {
            printf("build failed: %s\n", targets[i][0]);
            return -1;
        }
    }
    return 0;
}

static i32 wwa_build_run(const char_t* exe, const char_t* arg) {
    i32 code = -1;
    const char_t* args[3];
    args[0] = exe;
    args[1] = arg;
    args[2] = NULL;
    wwa_printf("run %s\n", exe);
    if (wwa_os_spawn(exe, args, &code) != 0) {
        printf("run failed\n");
        return -1;
    }
    return code;
}

static void wwa_build_clean(void) {
    u32 i;
    wwa_printf("clean %s\n", WWA_OBJ_DIR);
    for (i = 0; i < ARRAY_COUNT(g_std_srcs); i++) {
        wwa_os_file_delete(g_std_srcs[i].obj);
    }
    wwa_os_file_delete(WWA_EXE_DIR "/wwa_main" WWA_EXE_EXT);
    wwa_os_file_delete(WWA_EXE_DIR "/wwa_demo" WWA_EXE_EXT);
    wwa_os_file_delete(WWA_EXE_DIR "/wwa_demo_window" WWA_EXE_EXT);
    wwa_os_file_delete(WWA_EXE_DIR "/std_selftest" WWA_EXE_EXT);
    wwa_os_file_delete(WWA_EXE_DIR "/std_bench" WWA_EXE_EXT);
    wwa_os_file_delete(WWA_EXE_DIR "/wwa_build" WWA_EXE_EXT);
}

static void wwa_build_help(void) {
    printf("wwa_build <command>\n");
    printf("  build      compile all targets (incremental)\n");
    printf("  run        build + run demo\n");
    printf("  demo       build + run 3D demo (ascii, batch-safe)\n");
    printf("  demo-window build + run windowed 3D demo (interactive)\n");
    printf("  std-check  build + run std selftest\n");
    printf("  bench-std  build + run std benchmarks\n");
    printf("  all        build + selftest + bench + demo\n");
    printf("  clean      delete compiled outputs\n");
    printf("  info       platform / compiler info\n");
    printf("  help       this text\n");
}

static void wwa_build_info(void) {
    i32 code = -1;
    const char_t* args[3];
    printf("platform: %s\n", WWA_OS_WINDOWS ? "windows" : "linux");
    printf("avx2: %s, avx512: %s\n", wwa_os_cpu_avx2() ? "yes" : "no", wwa_os_cpu_avx512() ? "yes" : "no");
    args[0] = WWA_CC;
    args[1] = "--version";
    args[2] = NULL;
    if (wwa_os_spawn(WWA_CC, args, &code) != 0) {
        printf("spawn gcc failed\n");
        return;
    }
    printf("gcc exit code: %d\n", code);
}

i32 main(i32 argc, string_t argv[]) {
    const char_t* cmd = argc >= 2 ? argv[1] : "all";
    if (wwa_strcmp(cmd, "build") == 0) {
        return wwa_build_all() == 0 ? 0 : 1;
    }
    if (wwa_strcmp(cmd, "run") == 0) {
        if (wwa_build_all() != 0) return 1;
        return wwa_build_run(WWA_EXE_DIR "/wwa_main" WWA_EXE_EXT, NULL);
    }
    if (wwa_strcmp(cmd, "demo") == 0) {
        if (wwa_build_all() != 0) return 1;
        return wwa_build_run(WWA_EXE_DIR "/wwa_demo" WWA_EXE_EXT, NULL);
    }
    if (wwa_strcmp(cmd, "demo-window") == 0) {
        if (wwa_build_all() != 0) return 1;
        return wwa_build_run(WWA_EXE_DIR "/wwa_demo_window" WWA_EXE_EXT, NULL);
    }
    if (wwa_strcmp(cmd, "std-check") == 0) {
        if (wwa_build_all() != 0) return 1;
        return wwa_build_run(WWA_EXE_DIR "/std_selftest" WWA_EXE_EXT, NULL);
    }
    if (wwa_strcmp(cmd, "bench-std") == 0) {
        if (wwa_build_all() != 0) return 1;
        return wwa_build_run(WWA_EXE_DIR "/std_bench" WWA_EXE_EXT, NULL);
    }
    if (wwa_strcmp(cmd, "all") == 0) {
        i32 code;
        if (wwa_build_all() != 0) return 1;
        code = wwa_build_run(WWA_EXE_DIR "/std_selftest" WWA_EXE_EXT, NULL);
        if (code != 0) return code;
        code = wwa_build_run(WWA_EXE_DIR "/wwa_main" WWA_EXE_EXT, NULL);
        if (code != 0) return code;
        /* demos are interactive — build only, do not auto-run in batch */
        return wwa_build_run(WWA_EXE_DIR "/std_bench" WWA_EXE_EXT, NULL);
    }
    if (wwa_strcmp(cmd, "clean") == 0) {
        wwa_build_clean();
        return 0;
    }
    if (wwa_strcmp(cmd, "info") == 0) {
        wwa_build_info();
        return 0;
    }
    wwa_build_help();
    return 1;
}
