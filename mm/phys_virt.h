/*
 * SlopOS Memory Management - Physical <-> Virtual translation helpers
 */

#ifndef MM_PHYS_VIRT_H
#define MM_PHYS_VIRT_H

#include <stdint.h>
#include <stddef.h>

void mm_init_phys_virt_helpers(void);
uint64_t mm_phys_to_virt(uint64_t phys_addr);
uint64_t mm_virt_to_phys(uint64_t virt_addr);
int mm_zero_physical_page(uint64_t phys_addr);
void *mm_map_mmio_region(uint64_t phys_addr, size_t size);
int mm_unmap_mmio_region(void *virt_addr, size_t size);

#endif /* MM_PHYS_VIRT_H */
