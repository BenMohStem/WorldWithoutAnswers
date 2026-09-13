/* World Without Answers — assert.h
   Debug assertion macro. Uses printf for output (no FILE* needed). */

#ifndef WWA_ASSERT_H
#define WWA_ASSERT_H

#include <stdio.h>

#undef assert

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) \
    ((expr) ? (void)0 : \
     (wwa_printf("assert failed: %s:%d: %s\n", __FILE__, __LINE__, #expr), \
      wwa_abort()))
#endif

#define static_assert _Static_assert

#endif /* WWA_ASSERT_H */
