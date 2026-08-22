/* World Without Answers — stdos.h
   OS platform layer: memory, io, time, files, processes, cpu features.
   Windows: kernel32 via manual dllimport declarations (no windows.h).
   Linux: inline-asm syscalls (no libc). */

#ifndef WWA_STDOS_H
#define WWA_STDOS_H

#include <stdtype.h>

#if defined(_WIN32)
#define WWA_OS_WINDOWS 1
#define WWA_OS_LINUX   0
#elif defined(__linux__)
#define WWA_OS_WINDOWS 0
#define WWA_OS_LINUX   1
#else
#error "stdos: only Windows and Linux are supported"
#endif

#if defined(__GNUC__) || defined(__clang__)
#define WWA_NORETURN __attribute__((noreturn))
#else
#define WWA_NORETURN __declspec(noreturn)
#endif

#define WWA_OS_FD_STDIN  0
#define WWA_OS_FD_STDOUT 1
#define WWA_OS_FD_STDERR 2

#define WWA_OS_FILE_READ   1
#define WWA_OS_FILE_WRITE  2
#define WWA_OS_FILE_CREATE 4
#define WWA_OS_FILE_TRUNC  8
#define WWA_OS_FILE_APPEND 16

i32   wwa_os_write(i32 fd, const void* buf, usize count);
i32   wwa_os_read(i32 fd, void_p buf, usize count);
WWA_NORETURN void wwa_os_exit(i32 code);

void_p wwa_os_alloc(usize size);
void   wwa_os_free(void_p ptr, usize size);

u64   wwa_os_ticks(void);
u64   wwa_os_ticks_per_sec(void);
u64   wwa_os_time_us(void);
u32   wwa_os_pid(void);
i32   wwa_os_time_str(char_t* buf, usize cap);

i32      wwa_os_file_open(const char_t* path, u32 flags);
i32      wwa_os_file_close(i32 fd);
i32      wwa_os_file_delete(const char_t* path);
i32      wwa_os_rename(const char_t* oldp, const char_t* newp);
i32 wwa_os_rename_ex(const char_t* oldp, const char_t* newp, i32 replace);
i32 wwa_os_round_f32_to_f16(f16* out, f32 f);
i32 wwa_os_round_f32_to_f8(f8* out, f32 f);
i32 wwa_os_round_f16_to_f32(f32* out, f16 i);
i32 wwa_os_round_f8_to_f32(f32* out, f8 i);
i32      wwa_os_file_exists(const char_t* path);
/* modification time in microseconds since the Unix epoch (same clock as
   wwa_os_time_us), -1 when the file cannot be stat'd. Sub-second precision
   matters: whole-second stamps let two edits inside one second look
   identical to an incremental builder. */
i64      wwa_os_file_mtime(const char_t* path);
i64      wwa_os_file_size(const char_t* path);
i32      wwa_os_dir_create(const char_t* path);
i32      wwa_os_spawn(const char_t* exe, const char_t* const args[], i32* exit_code);
string_t wwa_os_cmdline(void);

i32 wwa_os_cpu_avx2(void);
i32 wwa_os_cpu_avx512(void);
u32 wwa_os_cpu_count(void);

extern void (*wwa_os_exit_hook)(void);

#endif /* WWA_STDOS_H */