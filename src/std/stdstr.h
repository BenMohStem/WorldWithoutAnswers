/* World Without Answers — stdstr.h
   from-scratch string + ctype functions. No libc. */

#ifndef WWA_STDSTR_H
#define WWA_STDSTR_H

#include <stdtype.h>

usize   wwa_strlen(const char_t* s);
usize   wwa_strnlen(const char_t* s, usize maxlen);
usize   wwa_strlcpy(char_t* dst, const char_t* src, usize size);
usize   wwa_strlcat(char_t* dst, const char_t* src, usize size);
char_t* wwa_strcpy(char_t* dst, const char_t* src);
char_t* wwa_strncpy(char_t* dst, const char_t* src, usize n);
char_t* wwa_strcat(char_t* dst, const char_t* src);
char_t* wwa_strncat(char_t* dst, const char_t* src, usize n);
i32     wwa_strcmp(const char_t* a, const char_t* b);
i32     wwa_strncmp(const char_t* a, const char_t* b, usize n);
i32     wwa_strcasecmp(const char_t* a, const char_t* b);
i32     wwa_strncasecmp(const char_t* a, const char_t* b, usize n);
char_t* wwa_strchr(const char_t* s, i32 c);
char_t* wwa_strrchr(const char_t* s, i32 c);
char_t* wwa_strstr(const char_t* hay, const char_t* needle);
char_t* wwa_strtok(char_t* s, const char_t* delim);
char_t* wwa_strtok_r(char_t* s, const char_t* delim, char_t** save);
usize   wwa_strspn(const char_t* s, const char_t* accept);
usize   wwa_strcspn(const char_t* s, const char_t* reject);
char_t* wwa_strpbrk(const char_t* s, const char_t* accept);

#define WWA_ENOENT 2
#define WWA_EIO    5
#define WWA_EBADF  9
#define WWA_EAGAIN 11
#define WWA_ENOMEM 12
#define WWA_EACCES 13
#define WWA_EFAULT 14
#define WWA_EINVAL 22
#define WWA_EDOM   33
#define WWA_ERANGE 34
char_t* wwa_strerror(i32 err);
i32 *wwa_errno_loc(void);

i32 wwa_tolower(i32 c);
i32 wwa_toupper(i32 c);
i32 wwa_isdigit(i32 c);
i32 wwa_isxdigit(i32 c);
i32 wwa_isalpha(i32 c);
i32 wwa_isalnum(i32 c);
i32 wwa_isspace(i32 c);
i32 wwa_isupper(i32 c);
i32 wwa_islower(i32 c);
i32 wwa_isprint(i32 c);
i32 wwa_ispunct(i32 c);
i32 wwa_iscntrl(i32 c);

#endif /* WWA_STDSTR_H */