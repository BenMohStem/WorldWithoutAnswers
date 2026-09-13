/* World Without Answers — stdtime.c */

#include <stdtime.h>
#include <stdos.h>

u64 wwa_time_ticks(void) {
    return wwa_os_ticks();
}

u64 wwa_time_ticks_per_sec(void) {
    return wwa_os_ticks_per_sec();
}

u64 wwa_time_us(void) {
    return wwa_os_time_us();
}

u64 wwa_time_ms(void) {
    return wwa_os_time_us() / 1000ull;
}

u32 wwa_time_pid(void) {
    return wwa_os_pid();
}

i32 wwa_time_str(char_t* buf, usize cap) {
    return wwa_os_time_str(buf, cap);
}