/*
 * SlopOS Memory Layout Management
 * Defines and manages kernel memory layout, virtual address spaces,
 * and memory region tracking for both kernel and process memory
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"
#include "../boot/limine_protocol.h"
#include "memory_layout.h"

/* ========================================================================
 * MEMORY LAYOUT CONSTANTS AND STRUCTURES
 * ======================================================================== */

/* Kernel memory layout structure defined in header */

/* Memory region types */
typedef enum {
    MEMORY_REGION_RESERVED = 0,     /* Reserved/unusable memory */
    MEMORY_REGION_AVAILABLE,        /* Available for allocation */
    MEMORY_REGION_KERNEL_CODE,      /* Kernel code section */
    MEMORY_REGION_KERNEL_DATA,      /* Kernel data section */
    MEMORY_REGION_KERNEL_HEAP,      /* Kernel heap */
    MEMORY_REGION_KERNEL_STACK,     /* Kernel stack */
    MEMORY_REGION_PAGE_TABLES,      /* Page table storage */
    MEMORY_REGION_FRAMEBUFFER,      /* Framebuffer memory */
    MEMORY_REGION_ACPI,             /* ACPI tables */
    MEMORY_REGION_BOOTLOADER,       /* Bootloader-reserved */
    MEMORY_REGION_COUNT             /* Number of region types */
} memory_region_type_t;

/* Memory region descriptor */
typedef struct {
    uint64_t start_addr;            /* Start address (physical) */
    uint64_t end_addr;              /* End address (physical) */
    uint64_t size;                  /* Size in bytes */
    memory_region_type_t type;      /* Region type */
    uint32_t flags;                 /* Additional flags */
    char name[32];                  /* Human-readable name */
} memory_region_t;

/* ========================================================================
 * GLOBAL MEMORY LAYOUT STATE
 * ======================================================================== */

/* Current kernel memory layout */
static struct kernel_memory_layout kernel_layout = {0};

/* Memory region tracking */
static memory_region_t memory_regions[MAX_MEMORY_REGIONS];
static int region_count = 0;

/* Memory layout initialization status */
static int layout_initialized = 0;

/* ========================================================================
 * MEMORY LAYOUT INITIALIZATION
 * ======================================================================== */

/*
 * Initialize kernel memory layout with linker-provided symbols
 * Must be called early in boot process
 */
void init_kernel_memory_layout(void) {
    /* Get kernel boundaries from linker script */
    extern char _kernel_start[], _kernel_end[];
    extern char _text_start[], _text_end[];
    extern char _data_start[], _data_end[];
    extern char _bss_start[], _bss_end[];

    /* Calculate physical addresses */
    kernel_layout.kernel_start_phys = (uint64_t)_kernel_start;
    kernel_layout.kernel_end_phys = (uint64_t)_kernel_end;

    /* Calculate virtual addresses (higher-half mapping) */
    kernel_layout.kernel_start_virt = KERNEL_VIRTUAL_BASE;
    kernel_layout.kernel_end_virt = KERNEL_VIRTUAL_BASE +
        (kernel_layout.kernel_end_phys - kernel_layout.kernel_start_phys);

    /* Set up kernel heap (starts after kernel image) */
    kernel_layout.kernel_heap_start =
        (kernel_layout.kernel_end_phys + PAGE_SIZE_4KB - 1) & ~(PAGE_SIZE_4KB - 1);
    kernel_layout.kernel_heap_end = kernel_layout.kernel_heap_start + (16 * 1024 * 1024); /* 16MB heap */

    /* Set up kernel stack */
    kernel_layout.kernel_stack_start = BOOT_STACK_PHYS_ADDR;
    kernel_layout.kernel_stack_end = BOOT_STACK_PHYS_ADDR + BOOT_STACK_SIZE;

    /* Identity mapping covers first 1GB */
    kernel_layout.identity_map_end = PAGE_SIZE_1GB;

    /* User space starts at 1MB, ends at higher-half */
    kernel_layout.user_space_start = 0x100000;  /* 1MB */
    kernel_layout.user_space_end = KERNEL_VIRTUAL_BASE - 1;

    layout_initialized = 1;

    kprintln("SlopOS: Kernel memory layout initialized");
}

/*
 * Add a memory region to the tracking list
 */
int add_memory_region(uint64_t start, uint64_t end, memory_region_type_t type,
                     uint32_t flags, const char *name) {
    if (region_count >= MAX_MEMORY_REGIONS) {
        return -1; /* No space */
    }

    memory_region_t *region = &memory_regions[region_count];
    region->start_addr = start;
    region->end_addr = end;
    region->size = end - start;
    region->type = type;
    region->flags = flags;

    /* Copy name safely */
    int i;
    for (i = 0; i < 31 && name && name[i]; i++) {
        region->name[i] = name[i];
    }
    region->name[i] = '\0';

    region_count++;
    return 0;
}

/*
 * Set up standard kernel memory regions
 */
void setup_kernel_memory_regions(void) {
    if (!layout_initialized) {
        kprintln("ERROR: Memory layout not initialized");
        return;
    }

    /* Add kernel code region */
    add_memory_region(kernel_layout.kernel_start_phys,
                     kernel_layout.kernel_end_phys,
                     MEMORY_REGION_KERNEL_CODE, 0, "Kernel Image");

    /* Add kernel heap region */
    add_memory_region(kernel_layout.kernel_heap_start,
                     kernel_layout.kernel_heap_end,
                     MEMORY_REGION_KERNEL_HEAP, 0, "Kernel Heap");

    /* Add kernel stack region */
    add_memory_region(kernel_layout.kernel_stack_start,
                     kernel_layout.kernel_stack_end,
                     MEMORY_REGION_KERNEL_STACK, 0, "Kernel Stack");

    /* Add page table region */
    add_memory_region(EARLY_PML4_PHYS_ADDR,
                     EARLY_PD_PHYS_ADDR + PAGE_SIZE_4KB,
                     MEMORY_REGION_PAGE_TABLES, 0, "Early Page Tables");

    kprintln("SlopOS: Standard kernel memory regions configured");
}

/* ========================================================================
 * MEMORY LAYOUT QUERIES
 * ======================================================================== */

const struct kernel_memory_layout *get_kernel_memory_layout(void) {
    if (!layout_initialized) {
        return NULL;
    }
    return &kernel_layout;
}

uint64_t mm_get_kernel_phys_start(void) {
    return kernel_layout.kernel_start_phys;
}

uint64_t mm_get_kernel_phys_end(void) {
    return kernel_layout.kernel_end_phys;
}

uint64_t mm_get_kernel_virt_start(void) {
    return kernel_layout.kernel_start_virt;
}

uint64_t mm_get_identity_map_limit(void) {
    return kernel_layout.identity_map_end;
}

/*
 * Check if an address is in kernel space
 */
int is_kernel_address(uint64_t addr) {
    if (!layout_initialized) {
        return 0;
    }

    /* Check if in higher-half kernel space */
    if (addr >= kernel_layout.kernel_start_virt && addr < kernel_layout.kernel_end_virt) {
        return 1;
    }

    /* Check if in kernel heap */
    if (addr >= kernel_layout.kernel_heap_start && addr < kernel_layout.kernel_heap_end) {
        return 1;
    }

    /* Check if in kernel stack */
    if (addr >= kernel_layout.kernel_stack_start && addr < kernel_layout.kernel_stack_end) {
        return 1;
    }

    return 0;
}

/*
 * Check if an address is in user space
 */
int is_user_address(uint64_t addr) {
    if (!layout_initialized) {
        return 0;
    }

    return (addr >= kernel_layout.user_space_start && addr < kernel_layout.user_space_end);
}

/* ========================================================================
 * ADDRESS TRANSLATION HELPERS
 * ======================================================================== */


/* ========================================================================
 * MEMORY REGION MANAGEMENT
 * ======================================================================== */

/*
 * Find memory region containing an address
 */
const memory_region_t *find_memory_region(uint64_t addr) {
    for (int i = 0; i < region_count; i++) {
        if (addr >= memory_regions[i].start_addr && addr < memory_regions[i].end_addr) {
            return &memory_regions[i];
        }
    }
    return NULL;
}

/*
 * Get all memory regions of a specific type
 */
int get_memory_regions_by_type(memory_region_type_t type,
                              const memory_region_t **regions, int max_regions) {
    int found = 0;

    for (int i = 0; i < region_count && found < max_regions; i++) {
        if (memory_regions[i].type == type) {
            regions[found++] = &memory_regions[i];
        }
    }

    return found;
}

/*
 * Calculate total memory of a specific type
 */
uint64_t get_total_memory_by_type(memory_region_type_t type) {
    uint64_t total = 0;

    for (int i = 0; i < region_count; i++) {
        if (memory_regions[i].type == type) {
            total += memory_regions[i].size;
        }
    }

    return total;
}

/* ========================================================================
 * DEBUGGING AND INFORMATION
 * ======================================================================== */

/*
 * Print memory layout information
 */
void print_memory_layout(void) {
    if (!layout_initialized) {
        kprintln("Memory layout not initialized");
        return;
    }

    kprintln("=== SlopOS Memory Layout ===");

    kprint("Kernel Physical: ");
    kprint_hex(kernel_layout.kernel_start_phys);
    kprint(" - ");
    kprint_hex(kernel_layout.kernel_end_phys);
    kprintln("");

    kprint("Kernel Virtual:  ");
    kprint_hex(kernel_layout.kernel_start_virt);
    kprint(" - ");
    kprint_hex(kernel_layout.kernel_end_virt);
    kprintln("");

    kprint("Kernel Heap:     ");
    kprint_hex(kernel_layout.kernel_heap_start);
    kprint(" - ");
    kprint_hex(kernel_layout.kernel_heap_end);
    kprintln("");

    kprint("Identity Map:    0x0 - ");
    kprint_hex(kernel_layout.identity_map_end);
    kprintln("");

    kprint("User Space:      ");
    kprint_hex(kernel_layout.user_space_start);
    kprint(" - ");
    kprint_hex(kernel_layout.user_space_end);
    kprintln("");

    kprint("Total Regions:   ");
    kprint_decimal(region_count);
    kprintln("");
}

/*
 * Print all memory regions
 */
void print_memory_regions(void) {
    kprintln("=== Memory Regions ===");

    for (int i = 0; i < region_count; i++) {
        const memory_region_t *region = &memory_regions[i];

        kprint(region->name);
        kprint(": ");
        kprint_hex(region->start_addr);
        kprint(" - ");
        kprint_hex(region->end_addr);
        kprint(" (");
        kprint_decimal(region->size);
        kprintln(" bytes)");
    }
}

/*
 * Validate memory layout consistency
 */
int validate_memory_layout(void) {
    if (!layout_initialized) {
        kprintln("ERROR: Memory layout not initialized");
        return 0;
    }

    /* Check that kernel virtual addresses are in higher-half */
    if (kernel_layout.kernel_start_virt < KERNEL_VIRTUAL_BASE) {
        kprintln("ERROR: Kernel virtual address not in higher-half");
        return 0;
    }

    /* Check that heap doesn't overlap with kernel image */
    if (kernel_layout.kernel_heap_start < kernel_layout.kernel_end_phys) {
        kprintln("ERROR: Kernel heap overlaps with kernel image");
        return 0;
    }

    /* Check regions for overlaps */
    for (int i = 0; i < region_count; i++) {
        for (int j = i + 1; j < region_count; j++) {
            if (memory_regions[i].start_addr < memory_regions[j].end_addr &&
                memory_regions[j].start_addr < memory_regions[i].end_addr) {
                kprint("ERROR: Memory regions overlap: ");
                kprint(memory_regions[i].name);
                kprint(" and ");
                kprintln(memory_regions[j].name);
                return 0;
            }
        }
    }

    kprintln("Memory layout validation passed");
    return 1;
}
