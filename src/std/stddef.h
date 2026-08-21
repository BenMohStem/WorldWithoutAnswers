/* World Without Answers — stddef.h
   Standard definitions. Thin wrapper over stdtype.h. */

#ifndef WWA_STDDEF_H
#define WWA_STDDEF_H

#include "stdtype.h"

typedef wwa_usize  size_t;
typedef wwa_isize  ptrdiff_t;
typedef long double max_align_t;
typedef void *      nullptr_t;

/* wchar_t/wint_t: only define when Windows/system headers are not included */
#if !defined(_WIN32) && !defined(_WCHAR_T_DEFINED) && !defined(__WCHAR_TYPE__)
typedef unsigned short wchar_t;
typedef unsigned short wint_t;
#define _WCHAR_T_DEFINED
#endif

#define offsetof(type, member) ((size_t)WWA_OFFSETOF(type, member))

#endif /* WWA_STDDEF_H */
