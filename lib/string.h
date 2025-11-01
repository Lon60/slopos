#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <stddef.h>

/* Basic string manipulation helpers for the freestanding kernel */
size_t strlen(const char *str);
int strcmp(const char *lhs, const char *rhs);
int strncmp(const char *lhs, const char *rhs, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

#endif /* LIB_STRING_H */
