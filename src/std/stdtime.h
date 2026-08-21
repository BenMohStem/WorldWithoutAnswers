/* World Without Answers — stdtime.h
   time layer on top of stdos. */

#ifndef WWA_STDTIME_H
#define WWA_STDTIME_H

#include <stdtype.h>

u64 wwa_time_ticks(void);
u64 wwa_time_ticks_per_sec(void);
u64 wwa_time_us(void);
u64 wwa_time_ms(void);
u32 wwa_time_pid(void);
i32 wwa_time_str(char_t* buf, usize cap);

#endif /* WWA_STDTIME_H */