#ifndef MM_PAGE_ALLOC_H
#define MM_PAGE_ALLOC_H

#include <stdint.h>

void get_page_allocator_stats(uint32_t *total, uint32_t *free, uint32_t *allocated);

#endif /* MM_PAGE_ALLOC_H */
