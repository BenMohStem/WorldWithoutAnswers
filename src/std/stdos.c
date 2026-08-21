/* World Without Answers — stdos.c
   OS platform layer implementation.
   Windows: kernel32 via manual dllimport declarations (no windows.h).
   Linux: inline-asm syscalls (no libc). */

#include <stdos.h>
#include <stdstr.h>
#include <stdmem.h>

void (*wwa_os_exit_hook)(void) = NULL;

#if WWA_OS_WINDOWS

#define WWA_WAPI __declspec(dllimport)

WWA_WAPI void_p __stdcall GetStdHandle(u32 n);
WWA_WAPI i32    __stdcall WriteFile(void_p h, const void_p buf, u32 n, u32* written, void_p ov);
WWA_WAPI i32    __stdcall ReadFile(void_p h, void_p buf, u32 n, u32* rd, void_p ov);
WWA_WAPI WWA_NORETURN void __stdcall ExitProcess(u32 code);
WWA_WAPI void_p __stdcall VirtualAlloc(void_p addr, usize size, u32 type, u32 prot);
WWA_WAPI i32    __stdcall VirtualFree(void_p addr, usize size, u32 type);
WWA_WAPI void_p __stdcall CreateFileA(const char_t* path, u32 access, u32 share, void_p sa, u32 disp, u32 flags, void_p tmpl);
WWA_WAPI i32    __stdcall CloseHandle(void_p h);
WWA_WAPI i32    __stdcall DeleteFileA(const char_t* path);
WWA_WAPI i32    __stdcall MoveFileA(const char_t* oldp, const char_t* newp);
WWA_WAPI i32    __stdcall MoveFileExA(const char_t* oldp, const char_t* newp, u32 flags);
WWA_WAPI u32    __stdcall SetFilePointer(void_p h, i32 dist, i32* disthi, u32 method);
WWA_WAPI i32    __stdcall CreateDirectoryA(const char_t* path, void_p sa);
WWA_WAPI i32    __stdcall GetFileAttributesExA(const char_t* path, i32 level, void_p info);
WWA_WAPI u32    __stdcall GetCurrentProcessId(void);
WWA_WAPI i32    __stdcall QueryPerformanceCounter(i64* c);
WWA_WAPI i32    __stdcall QueryPerformanceFrequency(i64* f);
WWA_WAPI void   __stdcall GetLocalTime(void* t);
WWA_WAPI void   __stdcall GetSystemTimeAsFileTime(void* ft);
WWA_WAPI i32    __stdcall CreateProcessA(const char_t* app, char_t* cmd, void_p pa, void_p ta, i32 inh, u32 flags, void_p env, const char_t* cwd, void_p si, void_p pi);
WWA_WAPI u32    __stdcall WaitForSingleObject(void_p h, u32 ms);
WWA_WAPI i32    __stdcall GetExitCodeProcess(void_p h, u32* code);
WWA_WAPI char_t* __stdcall GetCommandLineA(void);
WWA_WAPI void   __stdcall GetSystemInfo(void* info);

#define WWA_WIN_MEM_COMMIT   0x1000
#define WWA_WIN_MEM_RESERVE  0x2000
#define WWA_WIN_MEM_RELEASE  0x8000
#define WWA_WIN_PAGE_RW      0x04
#define WWA_WIN_INFINITE     0xFFFFFFFFu
#define WWA_WIN_OPEN_EXISTING 3
#define WWA_WIN_CREATE_ALWAYS 2
#define WWA_WIN_OPEN_ALWAYS   4
#define WWA_WIN_ATTR_NORMAL   0x80

#define WWA_OS_MAX_FILES 64
static void_p g_files[WWA_OS_MAX_FILES];

static void_p wwa_win_std_handle(i32 fd) {
    if (fd == WWA_OS_FD_STDIN)  return GetStdHandle((u32)(i32)-10);
    if (fd == WWA_OS_FD_STDOUT) return GetStdHandle((u32)(i32)-11);
    if (fd == WWA_OS_FD_STDERR) return GetStdHandle((u32)(i32)-12);
    return NULL;
}

static i32 wwa_os_register_handle(void_p h) {
    i32 i;
    for (i = 0; i < WWA_OS_MAX_FILES; i++) {
        if (g_files[i] == NULL) {
            g_files[i] = h;
            return 3 + i;
        }
    }
    CloseHandle(h);
    return -1;
}

#else

#define WWA_LINUX_SYS_READ        0
#define WWA_LINUX_SYS_WRITE       1
#define WWA_LINUX_SYS_OPEN        2
#define WWA_LINUX_SYS_CLOSE       3
#define WWA_LINUX_SYS_MMAP        9
#define WWA_LINUX_SYS_MUNMAP      11
#define WWA_LINUX_SYS_ACCESS      21
#define WWA_LINUX_SYS_GETPID      39
#define WWA_LINUX_SYS_FORK        57
#define WWA_LINUX_SYS_EXECVE      59
#define WWA_LINUX_SYS_WAIT4       61
#define WWA_LINUX_SYS_MKDIR       83
#define WWA_LINUX_SYS_UNLINK      87
#define WWA_LINUX_SYS_RENAME      82
#define WWA_LINUX_SYS_CLOCK_GETTIME 228
#define WWA_LINUX_SYS_EXIT_GROUP  231
#define WWA_LINUX_SYS_NEWFSTATAT  262

#define WWA_LINUX_O_RDONLY 0
#define WWA_LINUX_O_WRONLY 1
#define WWA_LINUX_O_RDWR   2
#define WWA_LINUX_O_CREAT  64
#define WWA_LINUX_O_TRUNC  512
#define WWA_LINUX_O_APPEND 1024

typedef struct {
    i64 sec;
    i64 nsec;
} wwa_linux_timespec;

extern char_t** wwa_linux_envp;

static isize wwa_linux_syscall0(i64 n) {
    register i64 rax __asm__("rax") = n;
    __asm__ __volatile__("syscall" : "+r"(rax) : : "rcx", "r11", "memory");
    return (isize)rax;
}

static isize wwa_linux_syscall1(i64 n, i64 a) {
    register i64 rax __asm__("rax") = n;
    register i64 rdi __asm__("rdi") = a;
    __asm__ __volatile__("syscall" : "+r"(rax) : "r"(rdi) : "rcx", "r11", "memory");
    return (isize)rax;
}

static isize wwa_linux_syscall2(i64 n, i64 a, i64 b) {
    register i64 rax __asm__("rax") = n;
    register i64 rdi __asm__("rdi") = a;
    register i64 rsi __asm__("rsi") = b;
    __asm__ __volatile__("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi) : "rcx", "r11", "memory");
    return (isize)rax;
}

static isize wwa_linux_syscall3(i64 n, i64 a, i64 b, i64 c) {
    register i64 rax __asm__("rax") = n;
    register i64 rdi __asm__("rdi") = a;
    register i64 rsi __asm__("rsi") = b;
    register i64 rdx __asm__("rdx") = c;
    __asm__ __volatile__("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx) : "rcx", "r11", "memory");
    return (isize)rax;
}

static isize wwa_linux_syscall4(i64 n, i64 a, i64 b, i64 c, i64 d) {
    register i64 rax __asm__("rax") = n;
    register i64 rdi __asm__("rdi") = a;
    register i64 rsi __asm__("rsi") = b;
    register i64 rdx __asm__("rdx") = c;
    register i64 r10 __asm__("r10") = d;
    __asm__ __volatile__("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10) : "rcx", "r11", "memory");
    return (isize)rax;
}

static isize wwa_linux_syscall6(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
    register i64 rax __asm__("rax") = n;
    register i64 rdi __asm__("rdi") = a;
    register i64 rsi __asm__("rsi") = b;
    register i64 rdx __asm__("rdx") = c;
    register i64 r10 __asm__("r10") = d;
    register i64 r8  __asm__("r8")  = e;
    register i64 r9  __asm__("r9")  = f;
    __asm__ __volatile__("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return (isize)rax;
}

static void wwa_linux_civil_from_days(i64 z, i64* y, u32* m, u32* d) {
    i64 era;
    u64 doe, yoe, doy, mp, dd, mm;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (u64)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    dd = doy - (153 * mp + 2) / 5 + 1;
    mm = mp < 10 ? mp + 3 : mp - 9;
    *y = (i64)yoe + era * 400 + (mm <= 2 ? 1 : 0);
    *m = (u32)mm;
    *d = (u32)dd;
}

static const char_t* wwa_linux_getenv(const char_t* name) {
    usize nl = wwa_strlen(name);
    i32 i;
    if (wwa_linux_envp == NULL) return NULL;
    for (i = 0; wwa_linux_envp[i] != NULL; i++) {
        if (wwa_strncmp(wwa_linux_envp[i], name, nl) == 0 && wwa_linux_envp[i][nl] == '=') {
            return wwa_linux_envp[i] + nl + 1;
        }
    }
    return NULL;
}

static i32 wwa_linux_find_in_path(const char_t* name, char_t* out, usize cap) {
    const char_t* path = wwa_linux_getenv("PATH");
    usize nlen = wwa_strlen(name);
    if (path == NULL) return 0;
    while (*path != 0) {
        const char_t* end = path;
        while (*end != 0 && *end != ':') end++;
        {
            usize plen = (usize)(end - path);
            if (plen + 1 + nlen + 1 < cap) {
                usize j;
                for (j = 0; j < plen; j++) out[j] = path[j];
                out[plen] = '/';
                for (j = 0; j < nlen; j++) out[plen + 1 + j] = name[j];
                out[plen + 1 + nlen] = 0;
                if (wwa_os_file_exists(out)) return 1;
            }
        }
        if (*end == 0) break;
        path = end + 1;
    }
    return 0;
}

#endif

static i32 wwa_os_fmt_u64(char_t* buf, usize cap, u64 v, i32 width) {
    char_t tmp[24];
    i32 n = 0, i;
    do {
        tmp[n++] = (char_t)('0' + (i32)(v % 10));
        v /= 10;
    } while (v);
    while (n < width) tmp[n++] = '0';
    if ((usize)n >= cap) return -1;
    for (i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    buf[n] = 0;
    return n;
}

static void wwa_os_cpuid(u32 leaf, u32 sub, u32* a, u32* b, u32* c, u32* d) {
    __asm__ __volatile__("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(sub));
}

static u64 wwa_os_xgetbv(u32 x) {
    u32 lo, hi;
    __asm__ __volatile__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(x));
    return ((u64)hi << 32) | lo;
}

static i32 wwa_os_cpu_osxsave(void) {
    u32 a, b, c, d;
    wwa_os_cpuid(1, 0, &a, &b, &c, &d);
    return (i32)((c >> 27) & 1);
}

i32 wwa_os_cpu_avx2(void) {
    u32 a, b, c, d;
    if (!wwa_os_cpu_osxsave()) return 0;
    if ((wwa_os_xgetbv(0) & 6) != 6) return 0;
    wwa_os_cpuid(7, 0, &a, &b, &c, &d);
    return (i32)((b >> 5) & 1);
}

i32 wwa_os_cpu_avx512(void) {
    u32 a, b, c, d;
    if (!wwa_os_cpu_osxsave()) return 0;
    if ((wwa_os_xgetbv(0) & 0xE6) != 0xE6) return 0;
    wwa_os_cpuid(7, 0, &a, &b, &c, &d);
    return (i32)((b >> 16) & 1);
}

i32 wwa_os_write(i32 fd, const void* buf, usize count) {
#if WWA_OS_WINDOWS
    void_p h;
    u32 written = 0;
    if (fd == WWA_OS_FD_STDIN || fd == WWA_OS_FD_STDOUT || fd == WWA_OS_FD_STDERR) {
        h = wwa_win_std_handle(fd);
    } else if (fd >= 3 && fd < 3 + WWA_OS_MAX_FILES) {
        h = g_files[fd - 3];
    } else {
        return -1;
    }
    if (h == NULL || h == (void_p)-1) return -1;
    if (count > 0xFFFFFFFFu) count = 0xFFFFFFFFu;
    if (!WriteFile(h, (const void_p)buf, (u32)count, &written, NULL)) return -1;
    return (i32)written;
#else
    return (i32)wwa_linux_syscall3(WWA_LINUX_SYS_WRITE, fd, (i64)buf, (i64)count);
#endif
}

i32 wwa_os_read(i32 fd, void_p buf, usize count) {
#if WWA_OS_WINDOWS
    void_p h;
    u32 rd = 0;
    if (fd == WWA_OS_FD_STDIN || fd == WWA_OS_FD_STDOUT || fd == WWA_OS_FD_STDERR) {
        h = wwa_win_std_handle(fd);
    } else if (fd >= 3 && fd < 3 + WWA_OS_MAX_FILES) {
        h = g_files[fd - 3];
    } else {
        return -1;
    }
    if (h == NULL || h == (void_p)-1) return -1;
    if (count > 0xFFFFFFFFu) count = 0xFFFFFFFFu;
    if (!ReadFile(h, buf, (u32)count, &rd, NULL)) return -1;
    return (i32)rd;
#else
    return (i32)wwa_linux_syscall3(WWA_LINUX_SYS_READ, fd, (i64)buf, (i64)count);
#endif
}

WWA_NORETURN void wwa_os_exit(i32 code) {
    if (wwa_os_exit_hook != NULL) wwa_os_exit_hook();
#if WWA_OS_WINDOWS
    ExitProcess((u32)code);
#else
    wwa_linux_syscall1(WWA_LINUX_SYS_EXIT_GROUP, code);
    for (;;) { }
#endif
}

void_p wwa_os_alloc(usize size) {
    if (size == 0) size = 1;
#if WWA_OS_WINDOWS
    return VirtualAlloc(NULL, size, WWA_WIN_MEM_COMMIT | WWA_WIN_MEM_RESERVE, WWA_WIN_PAGE_RW);
#else
    {
        isize r = wwa_linux_syscall6(WWA_LINUX_SYS_MMAP, 0, (i64)size, 3, 0x22, -1, 0);
        return r < 0 ? NULL : (void_p)r;
    }
#endif
}

void wwa_os_free(void_p ptr, usize size) {
    UNUSED(size);
#if WWA_OS_WINDOWS
    VirtualFree(ptr, 0, WWA_WIN_MEM_RELEASE);
#else
    wwa_linux_syscall2(WWA_LINUX_SYS_MUNMAP, (i64)ptr, (i64)size);
#endif
}

u64 wwa_os_ticks(void) {
#if WWA_OS_WINDOWS
    i64 c = 0;
    QueryPerformanceCounter(&c);
    return (u64)c;
#else
    wwa_linux_timespec ts;
    wwa_linux_syscall2(WWA_LINUX_SYS_CLOCK_GETTIME, 1, (i64)&ts);
    return (u64)ts.sec * 1000000000ull + (u64)ts.nsec;
#endif
}

u64 wwa_os_ticks_per_sec(void) {
#if WWA_OS_WINDOWS
    i64 f = 0;
    QueryPerformanceFrequency(&f);
    return (u64)f;
#else
    return 1000000000ull;
#endif
}

u64 wwa_os_time_us(void) {
#if WWA_OS_WINDOWS
    u64 ft = 0;
    GetSystemTimeAsFileTime(&ft);
    return (ft - 116444736000000000ull) / 10ull;
#else
    wwa_linux_timespec ts;
    wwa_linux_syscall2(WWA_LINUX_SYS_CLOCK_GETTIME, 0, (i64)&ts);
    return (u64)ts.sec * 1000000ull + (u64)ts.nsec / 1000;
#endif
}

u32 wwa_os_pid(void) {
#if WWA_OS_WINDOWS
    return GetCurrentProcessId();
#else
    return (u32)wwa_linux_syscall0(WWA_LINUX_SYS_GETPID);
#endif
}

i32 wwa_os_time_str(char_t* buf, usize cap) {
#if WWA_OS_WINDOWS
    u16 t[8];
    i32 n = 0;
    GetLocalTime(t);
    n += wwa_os_fmt_u64(buf + n, cap - n, t[0], 4); if (n < 0) return -1; buf[n++] = '-';
    n += wwa_os_fmt_u64(buf + n, cap - n, t[1], 2); if (n < 0) return -1; buf[n++] = '-';
    n += wwa_os_fmt_u64(buf + n, cap - n, t[3], 2); if (n < 0) return -1; buf[n++] = ' ';
    n += wwa_os_fmt_u64(buf + n, cap - n, t[4], 2); if (n < 0) return -1; buf[n++] = ':';
    n += wwa_os_fmt_u64(buf + n, cap - n, t[5], 2); if (n < 0) return -1; buf[n++] = ':';
    n += wwa_os_fmt_u64(buf + n, cap - n, t[6], 2);
    return n;
#else
    wwa_linux_timespec ts;
    i64 days, rem, y;
    u32 mo, da;
    i32 n = 0;
    if (wwa_linux_syscall2(WWA_LINUX_SYS_CLOCK_GETTIME, 0, (i64)&ts) != 0) return -1;
    days = ts.sec / 86400;
    rem = ts.sec % 86400;
    if (rem < 0) {
        rem += 86400;
        days -= 1;
    }
    wwa_linux_civil_from_days(days, &y, &mo, &da);
    n += wwa_os_fmt_u64(buf + n, cap - n, (u64)y, 4); if (n < 0) return -1; buf[n++] = '-';
    n += wwa_os_fmt_u64(buf + n, cap - n, mo, 2); if (n < 0) return -1; buf[n++] = '-';
    n += wwa_os_fmt_u64(buf + n, cap - n, da, 2); if (n < 0) return -1; buf[n++] = ' ';
    n += wwa_os_fmt_u64(buf + n, cap - n, (u64)(rem / 3600), 2); if (n < 0) return -1; buf[n++] = ':';
    n += wwa_os_fmt_u64(buf + n, cap - n, (u64)((rem % 3600) / 60), 2); if (n < 0) return -1; buf[n++] = ':';
    n += wwa_os_fmt_u64(buf + n, cap - n, (u64)(rem % 60), 2);
    return n;
#endif
}

i32 wwa_os_file_open(const char_t* path, u32 flags) {
#if WWA_OS_WINDOWS
    u32 access = 0, share = 3, disp = WWA_WIN_OPEN_EXISTING;
    u32 r = flags & WWA_OS_FILE_READ;
    u32 w = flags & WWA_OS_FILE_WRITE;
    void_p h;
    if (r && w) access = 0xC0000000;
    else if (w) access = 0x40000000;
    else access = 0x80000000;
    if (flags & WWA_OS_FILE_CREATE) disp = (flags & WWA_OS_FILE_TRUNC) ? WWA_WIN_CREATE_ALWAYS : WWA_WIN_OPEN_ALWAYS;
    else if (flags & WWA_OS_FILE_TRUNC) disp = WWA_WIN_CREATE_ALWAYS;
    h = CreateFileA(path, access, share, NULL, disp, WWA_WIN_ATTR_NORMAL, NULL);
    if (h == (void_p)-1) return -1;
    if (flags & WWA_OS_FILE_APPEND) SetFilePointer(h, 0, NULL, 2); /* FILE_END */
    return wwa_os_register_handle(h);
#else
    i32 oflags = 0;
    u32 r = flags & WWA_OS_FILE_READ;
    u32 w = flags & WWA_OS_FILE_WRITE;
    if (r && w) oflags = WWA_LINUX_O_RDWR;
    else if (w) oflags = WWA_LINUX_O_WRONLY;
    if (flags & WWA_OS_FILE_CREATE) oflags |= WWA_LINUX_O_CREAT;
    if (flags & WWA_OS_FILE_TRUNC) oflags |= WWA_LINUX_O_TRUNC;
    if (flags & WWA_OS_FILE_APPEND) oflags |= WWA_LINUX_O_APPEND;
    return (i32)wwa_linux_syscall3(WWA_LINUX_SYS_OPEN, (i64)path, oflags, 0644);
#endif
}

i32 wwa_os_file_close(i32 fd) {
#if WWA_OS_WINDOWS
    void_p h;
    if (fd < 3 || fd >= 3 + WWA_OS_MAX_FILES) return -1;
    h = g_files[fd - 3];
    if (h == NULL) return -1;
    g_files[fd - 3] = NULL;
    return CloseHandle(h) ? 0 : -1;
#else
    return (i32)wwa_linux_syscall1(WWA_LINUX_SYS_CLOSE, fd);
#endif
}

i32 wwa_os_file_delete(const char_t* path) {
#if WWA_OS_WINDOWS
    return DeleteFileA(path) ? 0 : -1;
#else
    return (i32)wwa_linux_syscall1(WWA_LINUX_SYS_UNLINK, (i64)path) == 0 ? 0 : -1;
#endif
}

i32 wwa_os_rename(const char_t* oldp, const char_t* newp) {
#if WWA_OS_WINDOWS
    return MoveFileA(oldp, newp) ? 0 : -1;
#else
    return (i32)wwa_linux_syscall2(WWA_LINUX_SYS_RENAME, (i64)oldp, (i64)newp) == 0 ? 0 : -1;
#endif
}

/* rename with replace; replace=1 overwrites existing destination
   (fails only if destination is a locked running image) */
i32 wwa_os_rename_ex(const char_t* oldp, const char_t* newp, i32 replace) {
#if WWA_OS_WINDOWS
    u32 flags = replace ? 0x1 : 0; /* MOVEFILE_REPLACE_EXISTING */
    return MoveFileExA(oldp, newp, flags) ? 0 : -1;
#else
    (void)replace;
    return (i32)wwa_linux_syscall2(WWA_LINUX_SYS_RENAME, (i64)oldp, (i64)newp) == 0 ? 0 : -1;
#endif
}

i32 wwa_os_round_f32_to_f16(f16* out, f32 f) {
    /* IEEE-754 float16: 1 sign, 5 exponent, 10 mantissa */
    union { f32 f; u32 i; } fi;
    fi.f = f;
    u32 fi_val = fi.i;
    u32 sign = fi_val >> 16;
    u32 exp  = (fi_val >> 23) & 0xFF;
    u32 mant = fi_val & 0x7FFFFF;
    i32 e = (i32)exp - 127;
    u32 f16;
    if (f == 0.0f) { f16 = 0; }
    else if (e > 15) { f16 = (sign << 15) | 0x7C00; } /* Inf */
    else if (e < -14) { f16 = (sign << 15); } /* Zero / Denorm */
    else {
        u32 mant16 = (mant >> 13) | (e << 10) | (1 << 10); /* hidden bit */
        f16 = (sign << 15) | mant16;
    }
    *out = f16;
    return 0;
}

i32 wwa_os_round_f32_to_f8(f8* out, f32 f) {
    /* IEEE-754 float8 (mini float): 1 sign, 3 exponent, 4 mantissa - NVIDIA style */
    union { f32 f; u32 i; } fi;
    fi.f = f;
    u32 fi_val = fi.i;
    u32 sign = fi_val >> 16;
    u32 exp  = (fi_val >> 23) & 0xFF;
    u32 mant = fi_val & 0x7FFFFF;
    i32 e = (i32)exp - 127;
    u32 f8;
    if (f == 0.0f) { f8 = 0; }
    else if (e > 7) { f8 = (sign << 3) | 0x78; } /* Inf */
    else if (e < -7) { f8 = (sign << 3); } /* Zero */
    else {
        u32 mant4 = mant >> (23 - 4);
        f8 = (sign << 3) | (e + 7) << 1 | (mant4 >> 3);
    }
    *out = f8;
    return 0;
}

i32 wwa_os_round_f16_to_f32(f32* out, f16 i) {
    u16 bits = (u16)i;
    u32 sign = bits >> 15;
    u32 exp  = (bits >> 10) & 0x1F;
    u32 mant = bits & 0x3FF;
    f32 f;
    if (bits == 0) { f = 0.0f; }
    else if (exp == 0) { /* Denormal */
        /* IEEE-754 half denorm: value = sign * mant * 2^(-24) */
        f = (f32)mant * (1.0f / 16777216.0f); /* 2^24 */
    }
    else if (exp == 31) { /* Inf or NaN */
        union { f32 f; u32 i; } u;
        u.i = (sign << 31) | 0x7F800000; /* +-Inf */
        f = u.f * 1e38f;
    }
    else { /* Normalized */
        /* IEEE-754 half: value = sign * (1 + mant/1024) * 2^(exp-15) */
        /* Convert to float32:
           new_exp = (exp - 15) + 127 = exp + 112
           new_mant = mant << 13  (shift 10 bits to 23-bit mantissa) */
        union { f32 f; u32 i; } u;
        u.i = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
        f = u.f * (1.0f + (f32)mant / 1024.0f);
    }
    *out = f;
    return 0;
}

i32 wwa_os_round_f8_to_f32(f32* out, f8 i) {
    u8 bits = (u8)i;
    u32 sign = bits >> 3;
    u32 exp  = bits & 7;
    u32 mant = bits & 0xF; /* 4 mantissa bits */
    f32 f;
    if (bits == 0) { f = 0.0f; }
    else if (exp == 0) { /* Denormal */
        /* NVIDIA float8 denorm: value = sign * mant * 2^(-7) */
        f = (f32)mant * (1.0f / 128.0f); /* 2^7 */
    }
    else if (exp == 7) { /* Inf */
        f = sign * 1e38f;
    }
    else { /* Normalized */
        /* NVIDIA float8: value = sign * (1 + mant/16) * 2^(exp-3) */
        /* Convert to float32:
           new_exp = (exp - 3) + 127 = exp + 124
           new_mant = mant << 19  (shift 4 bits to 23-bit mantissa) */
        union { f32 f; u32 i; } u;
        u.i = (sign << 31) | ((exp + 124) << 23) | (mant << 19);
        f = u.f * (1.0f + (f32)mant / 16.0f);
    }
    *out = f;
    return 0;
}

i32 wwa_os_file_exists(const char_t* path) {
#if WWA_OS_WINDOWS
    u32 info[9];
    return GetFileAttributesExA(path, 0, info) ? 1 : 0;
#else
    return wwa_linux_syscall2(WWA_LINUX_SYS_ACCESS, (i64)path, 0) == 0 ? 1 : 0;
#endif
}

i64 wwa_os_file_mtime(const char_t* path) {
#if WWA_OS_WINDOWS
    u32 info[9];
    u64 ft;
    if (!GetFileAttributesExA(path, 0, info)) return -1;
    ft = ((u64)info[6] << 32) | info[5];
    return (i64)(ft / 10000000ull) - 11644473600ll;
#else
    u64 st[18];
    if (wwa_linux_syscall4(WWA_LINUX_SYS_NEWFSTATAT, -100, (i64)path, (i64)st, 0) != 0) return -1;
    return (i64)((i64*)st)[11];
#endif
}

i64 wwa_os_file_size(const char_t* path) {
#if WWA_OS_WINDOWS
    u32 info[9];
    if (!GetFileAttributesExA(path, 0, info)) return -1;
    return (i64)(((u64)info[7] << 32) | info[8]);
#else
    u64 st[18];
    if (wwa_linux_syscall4(WWA_LINUX_SYS_NEWFSTATAT, -100, (i64)path, (i64)st, 0) != 0) return -1;
    return (i64)st[6]; /* st_size */
#endif
}

u32 wwa_os_cpu_count(void) {
#if WWA_OS_WINDOWS
    /* SYSTEM_INFO.x64: dwNumberOfProcessors at byte offset 32 */
    static u32 cached = 0;
    if (cached) return cached;
    {
        u64 si[6];
        wwa_memset(si, 0, sizeof(si));
        GetSystemInfo(si);
        cached = ((u32*)si)[8]; /* dwNumberOfProcessors @ byte 32 */
        if (cached == 0 || cached > 256) cached = 1;
    }
    return cached;
#else
    return 1; /* serial until raw-clone threads land */
#endif
}

i32 wwa_os_dir_create(const char_t* path) {
#if WWA_OS_WINDOWS
    u32 info[9];
    if (GetFileAttributesExA(path, 0, info)) return 0;
    return CreateDirectoryA(path, NULL) ? 0 : -1;
#else
    if (wwa_linux_syscall2(WWA_LINUX_SYS_ACCESS, (i64)path, 0) == 0) return 0;
    return wwa_linux_syscall2(WWA_LINUX_SYS_MKDIR, (i64)path, 0755) == 0 ? 0 : -1;
#endif
}

i32 wwa_os_spawn(const char_t* exe, const char_t* const args[], i32* exit_code) {
    UNUSED(exe);
#if WWA_OS_WINDOWS
    char_t cmd[4096];
    usize pos = 0;
    i32 i;
    for (i = 0; args[i] != NULL; i++) {
        const char_t* a = args[i];
        usize len = wwa_strlen(a);
        i32 need_quotes = wwa_strchr(a, ' ') != NULL || wwa_strchr(a, '\t') != NULL;
        usize j;
        if (need_quotes) {
            if (pos < sizeof(cmd) - 1) cmd[pos++] = '"';
        }
        for (j = 0; j < len && pos < sizeof(cmd) - 1; j++) {
            char_t c = a[j];
            if (c == '"' && pos < sizeof(cmd) - 2) cmd[pos++] = '"';
            cmd[pos++] = c;
        }
        if (need_quotes) {
            if (pos < sizeof(cmd) - 1) cmd[pos++] = '"';
        }
        if (args[i + 1] != NULL && pos < sizeof(cmd) - 1) cmd[pos++] = ' ';
    }
    cmd[pos] = 0;
    {
        u32 si[18];
        void_p pi[4];
        u32 code = 0;
        for (i = 0; i < 18; i++) si[i] = 0;
        si[0] = 72;
        if (!CreateProcessA(NULL, cmd, NULL, NULL, 1, 0, NULL, NULL, si, pi)) return -1;
        WaitForSingleObject(pi[0], WWA_WIN_INFINITE);
        GetExitCodeProcess(pi[0], &code);
        CloseHandle(pi[0]);
        CloseHandle(pi[1]);
        if (exit_code != NULL) *exit_code = (i32)code;
        return 0;
    }
#else
    {
        i64 pid = wwa_linux_syscall0(WWA_LINUX_SYS_FORK);
        if (pid < 0) return -1;
        if (pid == 0) {
            const char_t* full = exe;
            char_t buf[4096];
            if (wwa_strchr(exe, '/') == NULL && wwa_linux_find_in_path(exe, buf, sizeof(buf))) full = buf;
            wwa_linux_syscall3(WWA_LINUX_SYS_EXECVE, (i64)full, (i64)args, (i64)wwa_linux_envp);
            wwa_linux_syscall1(WWA_LINUX_SYS_EXIT_GROUP, 127);
            for (;;) { }
        }
        {
            i64 status = 0;
            wwa_linux_syscall4(WWA_LINUX_SYS_WAIT4, pid, (i64)&status, 0, 0);
            if (exit_code != NULL) *exit_code = (i32)((status >> 8) & 0xFF);
            return 0;
        }
    }
#endif
}

string_t wwa_os_cmdline(void) {
#if WWA_OS_WINDOWS
    return GetCommandLineA();
#else
    static char_t buf[4096];
    i32 fd;
    isize n, i;
    fd = (i32)wwa_linux_syscall3(WWA_LINUX_SYS_OPEN, (i64)"/proc/self/cmdline", 0, 0);
    if (fd < 0) {
        buf[0] = 0;
        return buf;
    }
    n = wwa_linux_syscall3(WWA_LINUX_SYS_READ, fd, (i64)buf, 4095);
    wwa_linux_syscall1(WWA_LINUX_SYS_CLOSE, fd);
    if (n <= 0) {
        buf[0] = 0;
        return buf;
    }
    for (i = 0; i < n; i++) {
        if (buf[i] == 0) buf[i] = ' ';
    }
    buf[n] = 0;
    return buf;
#endif
}
/* clean */
