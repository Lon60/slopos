/*
 * SlopOS Memory Management - Physical <-> Virtual translation helpers
 */

#ifndef MM_PHYS_VIRT_H
#define MM_PHYS_VIRT_H

#include <stdint.h>

void mm_init_phys_virt_helpers(void);
uint64_t mm_phys_to_virt(uint64_t phys_addr);
uint64_t mm_virt_to_phys(uint64_t virt_addr);

#endif /* MM_PHYS_VIRT_H */

