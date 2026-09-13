/* World Without Answers — stdthread.c
   Windows: CRITICAL_SECTION + CONDITION_VARIABLE + CreateThread.
   Linux:   serial fallback — spawn returns 0 and runs inline,
            mutex/cond become no-ops (single-threaded correctness). */
#include <stdthread.h>
#include <stdmem.h>
#include <stdos.h>

#if WWA_OS_WINDOWS

#define WWA_WAPI __declspec(dllimport)

typedef unsigned long WWA_DWORD;
typedef struct { void* p; } WWA_CS;
typedef struct { void* p; } WWA_CV;

WWA_WAPI void_p __stdcall CreateThread(void* sa, usize stack, void* fn, void* arg, u32 flags, u32* tid);
WWA_WAPI i32    __stdcall CloseHandle(void_p h);
WWA_WAPI WWA_DWORD __stdcall WaitForSingleObject(void_p h, WWA_DWORD ms);
WWA_WAPI void   __stdcall InitializeCriticalSection(WWA_CS* cs);
WWA_WAPI void   __stdcall DeleteCriticalSection(WWA_CS* cs);
WWA_WAPI void   __stdcall EnterCriticalSection(WWA_CS* cs);
WWA_WAPI void   __stdcall LeaveCriticalSection(WWA_CS* cs);
WWA_WAPI void   __stdcall InitializeConditionVariable(WWA_CV* cv);
WWA_WAPI void   __stdcall SleepConditionVariableCS(WWA_CV* cv, WWA_CS* cs, WWA_DWORD ms);
WWA_WAPI void   __stdcall WakeConditionVariable(WWA_CV* cv);
WWA_WAPI void   __stdcall WakeAllConditionVariable(WWA_CV* cv);
WWA_WAPI WWA_DWORD __stdcall GetCurrentThreadId(void);

typedef WWA_DWORD (__stdcall *WWA_THREAD_PROC)(void*);

static unsigned long __stdcall wwa_thread_entry(void* arg) {
    /* arg = pointer to [fn, user_arg] pair allocated by spawn */
    void** slot = (void**)arg;
    wwa_thread_fn fn = (wwa_thread_fn)slot[0];
    void* user = slot[1];
    wwa_os_free(slot, 16);
    fn(user);
    return 0;
}

i32 wwa_thread_spawn(wwa_thread_t* out, wwa_thread_fn fn, void* arg) {
    void** slot = (void**)wwa_os_alloc(16);
    if (!slot) return -1;
    slot[0] = (void*)fn;
    slot[1] = arg;
    void_p h = CreateThread(NULL, 0, (void*)wwa_thread_entry, slot, 0, NULL);
    if (!h) { wwa_os_free(slot, 16); return -1; }
    *out = h;
    return 0;
}

void wwa_thread_join(wwa_thread_t t) {
    WaitForSingleObject(t, 0xFFFFFFFFu);
    CloseHandle(t);
}

u32 wwa_thread_self_id(void) { return (u32)GetCurrentThreadId(); }

void wwa_mutex_init(wwa_mutex_t* m) {
    /* RTL_CRITICAL_SECTION is 40 bytes on x64 — allocate generously */
    *m = wwa_os_alloc(64);
    InitializeCriticalSection((WWA_CS*)*m);
}
void wwa_mutex_lock(wwa_mutex_t* m)   { EnterCriticalSection((WWA_CS*)*m); }
void wwa_mutex_unlock(wwa_mutex_t* m) { LeaveCriticalSection((WWA_CS*)*m); }
void wwa_mutex_destroy(wwa_mutex_t* m){ DeleteCriticalSection((WWA_CS*)*m); wwa_os_free(*m, 64); }

void wwa_cond_init(wwa_cond_t* c) {
    *c = wwa_os_alloc(sizeof(WWA_CV));
    InitializeConditionVariable((WWA_CV*)*c);
}
void wwa_cond_wait(wwa_cond_t* c, wwa_mutex_t* m) {
    SleepConditionVariableCS((WWA_CV*)*c, (WWA_CS*)*m, 0xFFFFFFFFu);
}
void wwa_cond_signal(wwa_cond_t* c)   { WakeConditionVariable((WWA_CV*)*c); }
void wwa_cond_broadcast(wwa_cond_t* c){ WakeAllConditionVariable((WWA_CV*)*c); }
void wwa_cond_destroy(wwa_cond_t* c)  { wwa_os_free(*c, sizeof(WWA_CV)); }

#else /* Linux serial fallback */

#include <stdos.h>

i32 wwa_thread_spawn(wwa_thread_t* out, wwa_thread_fn fn, void* arg) {
    (void)out;
    fn(arg); /* run inline; scheduler detects workers==1 */
    return 1; /* flag: ran synchronously */
}
void wwa_thread_join(wwa_thread_t t) { (void)t; }
u32 wwa_thread_self_id(void) { return wwa_os_pid(); }

void wwa_mutex_init(wwa_mutex_t* m) { *m = NULL; }
void wwa_mutex_lock(wwa_mutex_t* m)   { (void)m; }
void wwa_mutex_unlock(wwa_mutex_t* m) { (void)m; }
void wwa_mutex_destroy(wwa_mutex_t* m){ (void)m; }

void wwa_cond_init(wwa_cond_t* c) { *c = NULL; }
void wwa_cond_wait(wwa_cond_t* c, wwa_mutex_t* m) { (void)c;(void)m; }
void wwa_cond_signal(wwa_cond_t* c)   { (void)c; }
void wwa_cond_broadcast(wwa_cond_t* c){ (void)c; }
void wwa_cond_destroy(wwa_cond_t* c)  { (void)c; }

#endif
