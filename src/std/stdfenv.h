/* World Without Answers — stdfenv.h
   from-scratch floating-point environment (MXCSR-backed).
   No libc. */

#ifndef WWA_STDFENV_H
#define WWA_STDFENV_H

#include <stdtype.h>

#define WWA_FE_TONEAREST  0
#define WWA_FE_DOWNWARD   1
#define WWA_FE_UPWARD     2
#define WWA_FE_TOWARDZERO 3

#define WWA_FE_INVALID    0x01
#define WWA_FE_DIVBYZERO  0x04
#define WWA_FE_OVERFLOW   0x08
#define WWA_FE_UNDERFLOW  0x10
#define WWA_FE_INEXACT    0x20
#define WWA_FE_ALL_EXCEPT 0x3D

typedef u32 wwa_fexcept_t;
typedef u32 wwa_fenv_t;

i32 wwa_fegetround(void);
i32 wwa_fesetround(i32 r);
i32 wwa_feclearexcept(i32 e);
i32 wwa_fetestexcept(i32 e);
i32 wwa_feraiseexcept(i32 e);
i32 wwa_fegetexceptflag(wwa_fexcept_t* flagp, i32 e);
i32 wwa_fesetexceptflag(const wwa_fexcept_t* flagp, i32 e);
i32 wwa_fegetenv(wwa_fenv_t* envp);
i32 wwa_feholdexcept(wwa_fenv_t* envp);
i32 wwa_fesetenv(const wwa_fenv_t* envp);
i32 wwa_feupdateenv(const wwa_fenv_t* envp);

#endif /* WWA_STDFENV_H */