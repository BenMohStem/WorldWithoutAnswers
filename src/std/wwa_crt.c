/* World Without Answers — wwa_crt.c
   process entry points, no libc.
   Windows: mainCRTStartup (enable VT colors, parse cmdline, run main).
   Linux: _start naked entry (argc/argv/envp from the stack, run main).
   Also provides the mingw ___main stub that gcc injects into main(). */

#include <stdtype.h>
#include <stdos.h>
#include <stdio.h>

int32_t main(int32_t argc, string_t argv[]);

void ___main(void) {
}
void __main(void) {
}

#if defined(_WIN32)
/* mingw gcc emits ___chkstk_ms calls for frames > 4KB; with -nostdlib
   the libgcc helper is unavailable. Probe each 4KB page of the frame
   so Windows commits the stack pages (skipping the guard page would
   fault on uncommitted memory). Size comes in eax, must stay in eax. */
__asm__(
    ".global ___chkstk_ms\n"
    "___chkstk_ms:\n"
    "  pushq %rcx\n"
    "  pushq %rax\n"
    "  cmpq $0x1000, %rax\n"
    "  leaq 24(%rsp), %rcx\n"
    "  jb 1f\n"
    "2:\n"
    "  subq $0x1000, %rcx\n"
    "  orl $0, (%rcx)\n"
    "  subq $0x1000, %rax\n"
    "  cmpq $0x1000, %rax\n"
    "  ja 2b\n"
    "1:\n"
    "  subq %rax, %rcx\n"
    "  orl $0, (%rcx)\n"
    "  popq %rax\n"
    "  popq %rcx\n"
    "  ret\n"
);
#endif

#if WWA_OS_WINDOWS

#define WWA_WAPI __declspec(dllimport)

WWA_WAPI void_p __stdcall GetStdHandle(u32 n);
WWA_WAPI i32    __stdcall GetConsoleMode(void_p h, u32* mode);
WWA_WAPI i32    __stdcall SetConsoleMode(void_p h, u32 mode);
WWA_WAPI char_t* __stdcall GetCommandLineA(void);

#define WWA_CRT_MAX_ARGS 64
#define WWA_CRT_ARG_BUF 4096

static char_t g_crt_argbuf[WWA_CRT_ARG_BUF];
static char_t* g_crt_argv[WWA_CRT_MAX_ARGS];

static void wwa_crt_parse_cmdline(const char_t* cmd, i32* argc, char_t*** argv) {
    i32 n = 0;
    char_t* out = g_crt_argbuf;
    const char_t* p = cmd;
    while (*p != 0) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) break;
        if (n >= WWA_CRT_MAX_ARGS) break;
        g_crt_argv[n++] = out;
        if (*p == '"') {
            p++;
            while (*p != 0 && *p != '"') {
                if (*p == '\\' && p[1] == '"') {
                    *out++ = '"';
                    p += 2;
                } else {
                    *out++ = *p++;
                }
            }
            if (*p == '"') p++;
        } else {
            while (*p != 0 && *p != ' ' && *p != '\t') *out++ = *p++;
        }
        *out++ = 0;
    }
    *argc = n;
    *argv = g_crt_argv;
}

void mainCRTStartup(void) {
    void_p out = GetStdHandle((u32)(i32)-11);
    u32 mode = 0;
    i32 argc = 0;
    char_t** argv = NULL;
    if (GetConsoleMode(out, &mode) != 0) {
        SetConsoleMode(out, mode | 0x4u);
    }
    wwa_crt_parse_cmdline(GetCommandLineA(), &argc, &argv);
    wwa_stdio_init();
    wwa_os_exit(main(argc, argv));
}

#else

char_t** wwa_linux_envp = NULL;

void wwa_linux_entry(void* sp) {
    u64* p = (u64*)sp;
    i64 argc = (i64)p[0];
    char_t** argv = (char_t**)(p + 1);
    wwa_linux_envp = argv + argc + 1;
    wwa_stdio_init();
    wwa_os_exit(main((i32)argc, argv));
}

__asm__(
    ".global _start\n"
    "_start:\n"
    "  xor %rbp, %rbp\n"
    "  mov %rsp, %rdi\n"
    "  call wwa_linux_entry\n"
    "  hlt\n"
);

#endif