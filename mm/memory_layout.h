#ifndef MM_MEMORY_LAYOUT_H
#define MM_MEMORY_LAYOUT_H

#include <stdint.h>

uint64_t mm_get_kernel_phys_start(void);
uint64_t mm_get_kernel_phys_end(void);
uint64_t mm_get_kernel_virt_start(void);
uint64_t mm_get_identity_map_limit(void);

#endif /* MM_MEMORY_LAYOUT_H */

