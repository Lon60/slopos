#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

/* Memory manipulation functions */
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int value, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

#endif /* MEMORY_H */