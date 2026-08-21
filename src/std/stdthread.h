/* World Without Answers — stdthread.h
   Threads, mutexes, condition variables, atomics — from scratch.
   Windows: kernel32 synchronization primitives via dllimport.
   Linux:   serial fallback (workers degrade to 1) until raw-clone
            implementation lands; API stays identical.

   Design notes (Build Systems a la Carte, ICFP 2018):
   the forge scheduler needs exactly: N workers blocked on a
   condvar-fed task queue + one mutex around graph state. */

#ifndef WWA_STHREAD_H
#define WWA_STHREAD_H

#include <stdtype.h>

typedef void* wwa_thread_t;      /* opaque handle */
typedef void* wwa_mutex_t;       /* opaque CRITICAL_SECTION/SRWLOCK */
typedef void* wwa_cond_t;        /* opaque CONDITION_VARIABLE */
typedef void (*wwa_thread_fn)(void* arg);

i32  wwa_thread_spawn(wwa_thread_t* out, wwa_thread_fn fn, void* arg);
void wwa_thread_join(wwa_thread_t t);
u32  wwa_thread_self_id(void);

void wwa_mutex_init(wwa_mutex_t* m);
void wwa_mutex_lock(wwa_mutex_t* m);
void wwa_mutex_unlock(wwa_mutex_t* m);
void wwa_mutex_destroy(wwa_mutex_t* m);

void wwa_cond_init(wwa_cond_t* c);
void wwa_cond_wait(wwa_cond_t* c, wwa_mutex_t* m);
void wwa_cond_signal(wwa_cond_t* c);
void wwa_cond_broadcast(wwa_cond_t* c);
void wwa_cond_destroy(wwa_cond_t* c);

/* atomics (GCC __atomic builtins — plain instructions, no libc) */
static inline i32 wwa_atomic_inc_i32(i32* p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
static inline i32 wwa_atomic_dec_i32(i32* p) { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
static inline i32 wwa_atomic_get_i32(const i32* p) { return __atomic_load_n(p, __ATOMIC_SEQ_CST); }
static inline void wwa_atomic_set_i32(i32* p, i32 v) { __atomic_store_n(p, v, __ATOMIC_SEQ_CST); }
static inline i32 wwa_atomic_cas_i32(i32* p, i32 oldv, i32 newv) {
    i32 expected = oldv;
    return __atomic_compare_exchange_n(p, &expected, newv, 0,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
static inline u64 wwa_atomic_get_u64(const u64* p) { return __atomic_load_n(p, __ATOMIC_SEQ_CST); }
static inline void wwa_atomic_set_u64(u64* p, u64 v) { __atomic_store_n(p, v, __ATOMIC_SEQ_CST); }

#endif /* WWA_STHREAD_H */
