#ifndef MM_MEMORY_LAYOUT_H
#define MM_MEMORY_LAYOUT_H

#include <stdint.h>

typedef struct kernel_memory_layout {
    uint64_t kernel_start_phys;
    uint64_t kernel_end_phys;
    uint64_t kernel_start_virt;
    uint64_t kernel_end_virt;
    uint64_t kernel_heap_start;
    uint64_t kernel_heap_end;
    uint64_t kernel_stack_start;
    uint64_t kernel_stack_end;
    uint64_t identity_map_end;
    uint64_t user_space_start;
    uint64_t user_space_end;
} kernel_memory_layout_t;

const kernel_memory_layout_t *get_kernel_memory_layout(void);
uint64_t mm_get_kernel_phys_start(void);
uint64_t mm_get_kernel_phys_end(void);
uint64_t mm_get_kernel_virt_start(void);
uint64_t mm_get_identity_map_limit(void);

#endif /* MM_MEMORY_LAYOUT_H */

