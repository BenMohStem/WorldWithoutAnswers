/* World Without Answers — errno.h
   Error numbers. Wraps WWA error codes from stdstr.h. */

#ifndef WWA_ERRNO_H
#define WWA_ERRNO_H

#include "stdstr.h"

#define EDOM    WWA_EDOM
#define ERANGE  WWA_ERANGE
#define EILSEQ  84
#define EINVAL  22
#define ENOMEM  12
#define ERMSG   33

#define errno   (*wwa_errno_loc())

extern i32 *wwa_errno_loc(void);

#endif /* WWA_ERRNO_H */
