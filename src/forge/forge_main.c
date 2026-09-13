/* World Without Answers — forge_main.c
   Front-end: declares the WWA project as a DAG via forge's API,
   then dispatches CLI commands. Self-hosting: forge builds itself. */
#include <forge.h>
#include <forge_internal.h>
#include <stdtype.h>
#include <stdos.h>
#include <stdstr.h>
#include <stdmem.h>
#include <stdhash.h>
#include <stdlib.h>
#include <stdio.h>

/* function prototypes */
i32 cmd_selftest(void);

#define OBJ "bin_obj/windows/release/obj"
#define EXE "bin_exe/windows/release"

static const char* g_cc = "gcc";

/* ---------- graph construction ---------- */

typedef struct {
    const char* src;
    const char* obj;
    const char* extra;   /* NULL or e.g. -mavx2 */
} forge_src_t;

static const forge_src_t g_std_srcs[] = {
    {"src/std/stdos.c",        OBJ "/stdos.o",         NULL},
    {"src/std/stdtime.c",      OBJ "/stdtime.o",       NULL},
    {"src/std/stdmem.c",       OBJ "/stdmem.o",        NULL},
    {"src/std/stdmem_avx2.c",  OBJ "/stdmem_avx2.o",   "-mavx2"},
    {"src/std/stdstr.c",       OBJ "/stdstr.o",        NULL},
    {"src/std/stdstr_avx2.c",  OBJ "/stdstr_avx2.o",   "-mavx2"},
    {"src/std/stdhash.c",      OBJ "/stdhash.o",       NULL},
    {"src/std/stdthread.c",    OBJ "/stdthread.o",     NULL},
    {"src/std/stdmath.c",      OBJ "/stdmath.o",       NULL},
    {"src/std/stdfloat.c",     OBJ "/stdfloat.o",      NULL},
    {"src/std/stdio.c",        OBJ "/stdio.o",         NULL},
    {"src/std/stdlib.c",       OBJ "/stdlib.o",        NULL},
    {"src/std/stdlog.c",       OBJ "/stdlog.o",        NULL},
    {"src/std/wwa_crt.c",      OBJ "/wwa_crt.o",       NULL},
    {"src/engine/physics/wwa_physics.c",     OBJ "/wwa_physics.o",     NULL},
    {"src/engine/render/wwa_render.c",       OBJ "/wwa_render.o",      NULL},
    {"src/engine/render/wwa_render_avx2.c",  OBJ "/wwa_render_avx2.o","-mavx2"},
    {"src/engine/audio/wwa_audio.c",         OBJ "/wwa_audio.o",       NULL},
    {"src/engine/input/wwa_input.c",         OBJ "/wwa_input.o",       NULL},
    {"src/engine/window/wwa_window.c",       OBJ "/wwa_window.o",      NULL},
    {"src/forge/forge_graph.c",    OBJ "/forge_graph.o",    NULL},
    {"src/forge/forge_manifest.c", OBJ "/forge_manifest.o", NULL},
    {"src/forge/forge_sched.c",    OBJ "/forge_sched.o",    NULL},
};

#define N_STD_SRCS (i32)(sizeof(g_std_srcs)/sizeof(g_std_srcs[0]))

static const char* g_includes[] = {
    "-Isrc/std",
    "-Isrc/engine/physics", "-Isrc/engine/render",
    "-Isrc/engine/audio", "-Isrc/engine/input", "-Isrc/engine/window",
    "-Isrc/forge",
};

typedef struct {
    const char* name;
    const char* src;
    const char** extra_objs; i32 n_extra_objs;
} forge_target_t;

static const forge_target_t g_targets[] = {
    {"wwa_main",        "src/app/wwa_main.c",        NULL, 0},
    {"wwa_demo",        "src/app/wwa_demo.c",        NULL, 0},
    {"wwa_demo_window", "src/app/wwa_demo_window.c", NULL, 0},
    {"std_selftest",    "src/tests/std_selftest.c",  NULL, 0},
    {"std_bench",       "src/tests/std_bench.c",     NULL, 0},
    {"forge",           "src/forge/forge_main.c",    NULL, 0},
};
#define N_TARGETS (i32)(sizeof(g_targets)/sizeof(g_targets[0]))

static void forge_declare_compile(forge_graph_t* g, const forge_src_t* s) {
    const char* argv[32];
    i32 argc = 0;
    argv[argc++] = g_cc;
    argv[argc++] = "-c";
    argv[argc++] = "-O3";
    argv[argc++] = "-std=c11";
    argv[argc++] = "-Wall";
    argv[argc++] = "-Wextra";
    argv[argc++] = "-fno-builtin";
    argv[argc++] = "-fno-stack-protector";
    for (u32 i = 0; i < sizeof(g_includes)/sizeof(g_includes[0]); i++)
        argv[argc++] = g_includes[i];
    if (s->extra) argv[argc++] = s->extra;
    argv[argc++] = s->src;
    argv[argc++] = "-o";
    argv[argc++] = s->obj;
    const char* deps[1] = { s->src };
    forge_add_node(g, s->obj, deps, 1, argv, argc);
}

static void forge_declare_link(forge_graph_t* g, const forge_target_t* t) {
    char out[256];
    wwa_snprintf(out, sizeof(out), "%s/%s.exe", EXE, t->name);

    const char* argv[128];
    i32 argc = 0;
    argv[argc++] = g_cc;
    argv[argc++] = "-O3";
    argv[argc++] = "-std=c11";
    argv[argc++] = "-fno-builtin";
    argv[argc++] = "-fno-stack-protector";
    for (u32 i = 0; i < sizeof(g_includes)/sizeof(g_includes[0]); i++)
        argv[argc++] = g_includes[i];
    for (i32 i = 0; i < N_STD_SRCS; i++) argv[argc++] = g_std_srcs[i].obj;
    for (i32 i = 0; i < t->n_extra_objs; i++) argv[argc++] = t->extra_objs[i];
    argv[argc++] = t->src;
    argv[argc++] = "-o";
    argv[argc++] = out;
    argv[argc++] = "-nostdlib";
    argv[argc++] = "-lkernel32";
    argv[argc++] = "-luser32";
    argv[argc++] = "-lgdi32";
    argv[argc++] = "-Wl,-e,mainCRTStartup";

    /* deps: all objs + own source */
    static char depbuf[64][256];
    const char* deps[80];
    i32 nd = 0;
    for (i32 i = 0; i < N_STD_SRCS; i++) {
        wwa_snprintf(depbuf[nd], sizeof(depbuf[nd]), "%s", g_std_srcs[i].obj);
        deps[nd] = depbuf[nd];
        nd++;
    }
    deps[nd++] = t->src;

    forge_add_node(g, out, deps, nd, argv, argc);
}

static forge_graph_t* forge_project(void) {
    forge_graph_t* g = forge_graph_create();
    for (i32 i = 0; i < N_STD_SRCS; i++)
        forge_declare_compile(g, &g_std_srcs[i]);
    for (i32 i = 0; i < N_TARGETS; i++)
        forge_declare_link(g, &g_targets[i]);
    return g;
}

/* ---------- commands ---------- */

static i32 cmd_build(i32 workers) {
    forge_graph_t* g = forge_project();
    if (forge_graph_check(g) != 0) return 1;
    forge_graph_critical_path(g);
    i32 fails = forge_sched_run(g, workers);
    forge_graph_free(g);
    return fails ? 1 : 0;
}

static i32 cmd_run(const char* target, i32 workers) {
    if (cmd_build(workers)) return 1;
    char exe[256];
    wwa_snprintf(exe, sizeof(exe), "%s/%s.exe", EXE, target);
    const char* args[2] = { exe, NULL };
    i32 code = -1;
    if (wwa_os_spawn(exe, args, &code) != 0) {
        wwa_printf("forge: run failed\n");
        return 1;
    }
    return code;
}

static i32 wwa_str_ends_with(const char* s, const char* suf) {
    usize ls = wwa_strlen(s), lf = wwa_strlen(suf);
    return ls >= lf && wwa_strcmp(s + ls - lf, suf) == 0;
}

static i32 cmd_clean(void) {
    forge_graph_t* g = forge_project();
    for (i32 i = 0; i < forge_node_count(g); i++) {
        const char* p = forge_node_path(g, i);
        if (wwa_str_ends_with(p, ".o") || wwa_str_ends_with(p, ".exe"))
            wwa_os_file_delete(p);
    }
    forge_graph_free(g);
    wwa_os_file_delete("bin_obj/forge_manifest.bin");
    wwa_printf("forge: clean\n");
    return 0;
}

static i32 cmd_graph(void) {
    forge_graph_t* g = forge_project();
    if (forge_graph_check(g) != 0) return 1;
    forge_dump_dot(g, "bin_obj/forge_graph.dot");
    wwa_printf("forge: wrote bin_obj/forge_graph.dot (%d nodes)\n",
               forge_node_count(g));
    forge_graph_free(g);
    return 0;
}

static void cmd_help(void) {
    wwa_printf("forge — WWA build engine (from scratch)\n\n");
    wwa_printf("usage: forge [command] [-j N]\n\n");
    wwa_printf("commands:\n");
    wwa_printf("  build            parallel incremental build of everything\n");
    wwa_printf("  run <target>     build + run (wwa_main|wwa_demo|wwa_demo_window|std_selftest|std_bench)\n");
    wwa_printf("  clean            remove outputs + manifest\n");
    wwa_printf("  graph            dump build DAG to bin_obj/forge_graph.dot\n");
    wwa_printf("  selftest         verify engine invariants (cycles, cutoff, hashing)\n");
    wwa_printf("  bench            cold/warm/incremental/scaling benchmarks\n");
    wwa_printf("  help             this text\n");
    wwa_printf("\ndefaults: -j = cpu_count (%u)\n", wwa_os_cpu_count());
}

int main(i32 argc, string_t argv[]) {
    i32 workers = (i32)wwa_os_cpu_count();
    const char* cmd = "build";
    const char* target = "wwa_main";
    for (i32 i = 1; i < argc; i++) {
        if (wwa_strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
            workers = wwa_atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            cmd = argv[i];
        } else if (wwa_strcmp(argv[i], "run") == 0 && i + 1 < argc) {
            target = argv[++i];
        }
    }
    /* second positional after run = target */
    if (wwa_strcmp(cmd, "run") == 0 || wwa_strcmp(cmd, "selftest") == 0) {
        /* find first non-flag arg after cmd */
        for (i32 i = 1; i < argc - 1; i++) {
            if (wwa_strcmp(argv[i], cmd) == 0 && argv[i+1][0] != '-') {
                target = argv[i+1];
                break;
            }
        }
    }

    forge_manifest_load();

    if (wwa_strcmp(cmd, "build") == 0)  return cmd_build(workers);
    if (wwa_strcmp(cmd, "run") == 0)    return cmd_run(target, workers);
    if (wwa_strcmp(cmd, "clean") == 0)  return cmd_clean();
    if (wwa_strcmp(cmd, "graph") == 0)  return cmd_graph();
    if (wwa_strcmp(cmd, "selftest") == 0) return cmd_selftest();
    if (wwa_strcmp(cmd, "bench") == 0) { wwa_printf("bench command not yet implemented\n"); return 1; }
    if (wwa_strcmp(cmd, "help") == 0)  { cmd_help(); return 0; }
    cmd_help();
    return 1;
}

i32 cmd_selftest(void) {
    i32 passes = 0, fails = 0;

    /* --- 1. Manifest roundtrip test --- */
    {
        forge_manifest_load();
        u64 h1 = wwa_hash_str("src/std/stdio.c");
        forge_manifest_rec_t rec;
        i32 found = forge_manifest_get(h1, &rec);
        if (found && rec.content_hash != 0) {
            passes++;
            wwa_printf("PASS: manifest get for stdio.c (hash=%016llx content=%016llx)\n",
                       (unsigned long long)h1, (unsigned long long)rec.content_hash);
        } else {
            fails++;
            wwa_printf("FAIL: manifest get for stdio.c not found or empty\n");
        }

        /* Test latest-record-wins. Uses a synthetic path so the selftest never
           writes a bogus record for a real source file into the manifest. */
        const char* probe = "wwa://selftest/latest-record-wins";
        u64 h2 = wwa_hash_str(probe);
        forge_manifest_append(probe, h2, 0xC0FFEEull, 0, 100, 42);
        forge_manifest_append(probe, h2, 0xDEADBEEFDEADBEEFull, 0, 100, 42);
        i32 found2 = forge_manifest_get(h2, &rec);
        if (found2 && rec.content_hash == 0xDEADBEEFDEADBEEFull) {
            passes++;
            wwa_printf("PASS: manifest latest-record-wins test\n");
        } else {
            fails++;
            wwa_printf("FAIL: manifest latest-record-wins test\n");
        }
    }

    /* --- 2. Cycle detection test --- */
    {
        forge_graph_t* g = forge_graph_create();
        /* Create a cycle: A -> B -> C -> A */
        /* Add nodes with deps forming a cycle */
        const char* paths[] = {"A", "B", "C"};
        i32 ids[3];
        for (i32 i = 0; i < 3; i++) {
            const char* deps[1] = {paths[(i + 1) % 3]}; /* A depends on C, B depends on A, C depends on B */
            ids[i] = forge_add_node(g, paths[i], (const char* const*)deps, 1, NULL, 0);
        }
        i32 cycle_result = forge_graph_check(g);
        if (cycle_result != 0) {
            passes++;
            wwa_printf("PASS: cycle detection detected cycle A->B->C->A\n");
        } else {
            fails++;
            wwa_printf("FAIL: cycle detection should have detected A->B->C->A cycle\n");
        }
        forge_graph_free(g);
    }

    /* --- 3. Forge graph basic integrity --- */
    {
        forge_graph_t* g = forge_project();
        if (g && g->node_count == 58) {
            passes++;
            wwa_printf("PASS: forge project has 58 nodes as expected\n");
        } else {
            fails++;
            wwa_printf("FAIL: forge project node count = %d (expected 58)\n", g ? g->node_count : -1);
        }
        forge_graph_free(g);
    }

    /* --- 4. Build integrity: a warm build must execute nothing --- */
    {
        i32 res = cmd_build(1);
        if (res != 0) {
            fails++;
            wwa_printf("FAIL: initial build failed\n");
        } else if (cmd_build(1) != 0 ||
                   g_forge_n_built != 0 || g_forge_n_cut != 0) {
            fails++;
            wwa_printf("FAIL: warm build re-executed %d nodes (%d cut)\n",
                       g_forge_n_built, g_forge_n_cut);
        } else {
            passes++;
            wwa_printf("PASS: build converges — warm build executes nothing\n");
        }
    }

    /* --- 5. Incrementality: an edited input must be rebuilt ---
       Regression test for the staleness bug where up-to-dateness was decided
       by comparing a dependency against its own manifest record: the record
       had already been rewritten in the same pass, so edits were invisible
       and stale objects were silently linked. Runs on a throwaway 2-node
       graph (source -> object) so no project file is touched. */
    {
        const char* dir = "bin_obj/selftest";
        const char* src = "bin_obj/selftest/incr.c";
        const char* obj = "bin_obj/selftest/incr.o";
        wwa_os_dir_create("bin_obj");
        wwa_os_dir_create(dir);
        wwa_os_file_delete(obj);

        const char* argv[8];
        i32 argc = 0;
        argv[argc++] = g_cc;
        argv[argc++] = "-c";
        argv[argc++] = "-O0";
        argv[argc++] = "-fno-builtin";
        argv[argc++] = src;
        argv[argc++] = "-o";
        argv[argc++] = obj;

        i32 built_first = -1, built_warm = -1, built_edit = -1;
        for (i32 pass = 0; pass < 3; pass++) {
            if (pass != 1) {
                const char* body = pass == 0 ? "int wwa_incr(void){return 1;}\n"
                                             : "int wwa_incr(void){return 2;}\n";
                i32 fd = wwa_os_file_open(src, WWA_OS_FILE_WRITE |
                                               WWA_OS_FILE_CREATE |
                                               WWA_OS_FILE_TRUNC);
                if (fd < 0) break;
                wwa_os_write(fd, body, wwa_strlen(body));
                wwa_os_file_close(fd);
            }
            forge_graph_t* g = forge_graph_create();
            const char* deps[1] = { src };
            forge_add_node(g, obj, deps, 1, argv, argc);
            forge_graph_check(g);
            forge_graph_critical_path(g);
            forge_sched_run(g, 1);
            forge_graph_free(g);
            /* count executions, not output changes: re-running the test
               reproduces a byte-identical object, which forge reports as an
               early cutoff rather than a build */
            i32 executed = g_forge_n_built + g_forge_n_cut;
            if (pass == 0) built_first = executed;
            else if (pass == 1) built_warm = executed;
            else built_edit = executed;
        }
        wwa_os_file_delete(obj);
        wwa_os_file_delete(src);

        if (built_first >= 1 && built_warm == 0 && built_edit >= 1) {
            passes++;
            wwa_printf("PASS: edited source triggers rebuild, unedited does not\n");
        } else {
            fails++;
            wwa_printf("FAIL: incrementality (first=%d warm=%d edited=%d)\n",
                       built_first, built_warm, built_edit);
        }
    }

    /* --- Summary --- */
    wwa_printf("\nforged selftest: passes=%d fails=%d\n", passes, fails);
    return (fails == 0) ? 0 : 1;
}
