/* World Without Answers — stdmem.h
   from-scratch memory functions. No libc. */

#ifndef WWA_STDMEM_H
#define WWA_STDMEM_H

#include <stdtype.h>

void_p wwa_memcpy(void_p dst, const void* src, usize n);
void_p wwa_memmove(void_p dst, const void* src, usize n);
void_p wwa_memset(void_p dst, i32 c, usize n);
i32    wwa_memcmp(const void* a, const void* b, usize n);
void_p wwa_memchr(const void* s, i32 c, usize n);
void_p wwa_memccpy(void_p dst, const void* src, i32 c, usize n);
void   wwa_memset_explicit(void_p dst, i32 c, usize n);

#endif /* WWA_STDMEM_H */