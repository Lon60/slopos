/*
 * SlopOS Memory Management - Physical <-> Virtual translation helpers
 */

#include <stdint.h>
#include <stddef.h>

#include "../drivers/serial.h"
#include "../boot/limine_protocol.h"
#include "memory_layout.h"
#include "phys_virt.h"

extern uint64_t virt_to_phys(uint64_t virt_addr);

static uint64_t cached_identity_limit;
static uint64_t kernel_phys_start;
static uint64_t kernel_phys_end;
static uint64_t kernel_virt_start;
static int translation_initialized;

void mm_init_phys_virt_helpers(void) {
    kernel_phys_start = mm_get_kernel_phys_start();
    kernel_phys_end = mm_get_kernel_phys_end();
    kernel_virt_start = mm_get_kernel_virt_start();
    cached_identity_limit = mm_get_identity_map_limit();

    translation_initialized = (kernel_phys_end > kernel_phys_start);
}

uint64_t mm_phys_to_virt(uint64_t phys_addr) {
    if (phys_addr == 0) {
        return 0;
    }

    if (is_hhdm_available()) {
        return phys_addr + get_hhdm_offset();
    }

    if (translation_initialized) {
        if (phys_addr >= kernel_phys_start && phys_addr < kernel_phys_end) {
            return kernel_virt_start + (phys_addr - kernel_phys_start);
        }

        if (phys_addr < cached_identity_limit) {
            return phys_addr;
        }
    }

    kprint("mm_phys_to_virt: No mapping available for physical address\n");
    return 0;
}

uint64_t mm_virt_to_phys(uint64_t virt_addr) {
    if (virt_addr == 0) {
        return 0;
    }

    return virt_to_phys(virt_addr);
}

