/*
 * SlopOS Memory Management - Central Memory System Initialization
 * Coordinates initialization of all memory management subsystems
 * Provides single entry point for memory system setup during kernel boot
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"

/* Forward declarations */
void kernel_panic(const char *message);

/* Memory subsystem initialization functions */
int init_early_paging(void);
void init_kernel_memory_layout(void);
void parse_multiboot2_info_strict(uint64_t multiboot_info_addr);
int parse_efi_memory_map_mb2(const void *mboot_info, uint32_t mboot_size);
int init_page_allocator(void *frame_array, uint32_t max_frames);
int init_buddy_allocator(void *block_array, uint32_t max_blocks);
int init_kernel_heap(void);
int init_process_vm(void);
int init_vmem_regions(void);
void init_paging(void);

/* ========================================================================
 * MEMORY INITIALIZATION STATE TRACKING
 * ======================================================================== */

typedef struct memory_init_state {
    int early_paging_done;
    int memory_layout_done;
    int multiboot_parsed;
    int efi_memory_parsed;
    int page_allocator_done;
    int buddy_allocator_done;
    int kernel_heap_done;
    int process_vm_done;
    int vmem_regions_done;
    int paging_done;
    uint64_t total_memory_bytes;
    uint64_t available_memory_bytes;
    uint32_t memory_regions_count;
} memory_init_state_t;

static memory_init_state_t init_state = {0};

/* ========================================================================
 * MEMORY ARRAYS FOR ALLOCATORS
 * ======================================================================== */

/* Static arrays for allocator data structures - sized conservatively */
#define MAX_PAGE_FRAMES     32768    /* Support up to 128MB of physical memory */
#define MAX_BUDDY_BLOCKS    16384    /* Support buddy allocator block tracking */

/* Page frame array for physical page allocator */
static uint8_t page_frame_array[MAX_PAGE_FRAMES * 32] __attribute__((aligned(16)));

/* Buddy allocator block array */
static uint8_t buddy_block_array[MAX_BUDDY_BLOCKS * 64] __attribute__((aligned(16)));

/* ========================================================================
 * INITIALIZATION SEQUENCE FUNCTIONS
 * ======================================================================== */

/**
 * Initialize early paging structures for kernel boot
 * Must be called first before any other memory operations
 */
static int initialize_early_memory(void) {
    kprint("MM: Initializing early paging structures...\n");

    if (init_early_paging() != 0) {
        kernel_panic("MM: Early paging initialization failed");
        return -1;
    }

    init_state.early_paging_done = 1;
    kprint("MM: Early paging structures initialized successfully\n");
    return 0;
}

/**
 * Parse Multiboot2 information and EFI memory map
 * Discovers available physical memory regions
 */
static int initialize_memory_discovery(uint64_t multiboot_info_addr) {
    kprint("MM: Parsing Multiboot2 information...\n");

    /* Parse Multiboot2 structure with strict validation */
    parse_multiboot2_info_strict(multiboot_info_addr);
    init_state.multiboot_parsed = 1;

    /* Parse EFI memory map from Multiboot2 */
    kprint("MM: Parsing EFI memory map...\n");
    if (parse_efi_memory_map_mb2((const void *)multiboot_info_addr,
                                 0x8000) != 0) {  /* Reasonable size limit */
        kernel_panic("MM: EFI memory map parsing failed");
        return -1;
    }

    init_state.efi_memory_parsed = 1;
    kprint("MM: Memory discovery completed successfully\n");
    return 0;
}

/**
 * Initialize physical memory allocators
 * Sets up page allocator and buddy allocator with discovered memory
 */
static int initialize_physical_allocators(void) {
    kprint("MM: Initializing physical memory allocators...\n");

    /* Initialize page allocator with static frame array */
    if (init_page_allocator(page_frame_array, MAX_PAGE_FRAMES) != 0) {
        kernel_panic("MM: Page allocator initialization failed");
        return -1;
    }
    init_state.page_allocator_done = 1;

    /* Initialize buddy allocator with static block array */
    if (init_buddy_allocator(buddy_block_array, MAX_BUDDY_BLOCKS) != 0) {
        kernel_panic("MM: Buddy allocator initialization failed");
        return -1;
    }
    init_state.buddy_allocator_done = 1;

    kprint("MM: Physical memory allocators initialized successfully\n");
    return 0;
}

/**
 * Initialize kernel memory layout and virtual memory
 * Sets up higher-half mapping and kernel heap
 */
static int initialize_virtual_memory(void) {
    kprint("MM: Initializing virtual memory management...\n");

    /* Initialize kernel memory layout constants */
    init_kernel_memory_layout();
    init_state.memory_layout_done = 1;

    /* Initialize kernel heap for dynamic allocation */
    if (init_kernel_heap() != 0) {
        kernel_panic("MM: Kernel heap initialization failed");
        return -1;
    }
    init_state.kernel_heap_done = 1;

    /* Initialize full paging system */
    init_paging();
    init_state.paging_done = 1;

    kprint("MM: Virtual memory management initialized successfully\n");
    return 0;
}

/**
 * Initialize process memory management
 * Sets up per-process virtual memory and region management
 */
static int initialize_process_memory(void) {
    kprint("MM: Initializing process memory management...\n");

    /* Initialize process virtual memory management */
    if (init_process_vm() != 0) {
        kernel_panic("MM: Process VM initialization failed");
        return -1;
    }
    init_state.process_vm_done = 1;

    /* Initialize virtual memory region management */
    if (init_vmem_regions() != 0) {
        kernel_panic("MM: Virtual memory regions initialization failed");
        return -1;
    }
    init_state.vmem_regions_done = 1;

    kprint("MM: Process memory management initialized successfully\n");
    return 0;
}

/**
 * Display memory initialization summary
 */
static void display_memory_summary(void) {
    kprint("\n========== SlopOS Memory System Initialized ==========\n");
    kprint("Early Paging:          ");
    kprint(init_state.early_paging_done ? "OK" : "FAILED");
    kprint("\n");
    kprint("Memory Layout:         ");
    kprint(init_state.memory_layout_done ? "OK" : "FAILED");
    kprint("\n");
    kprint("Multiboot Parsing:     ");
    kprint(init_state.multiboot_parsed ? "OK" : "FAILED");
    kprint("\n");
    kprint("EFI Memory Parsing:    ");
    kprint(init_state.efi_memory_parsed ? "OK" : "FAILED");
    kprint("\n");
    kprint("Page Allocator:        ");
    kprint(init_state.page_allocator_done ? "OK" : "FAILED");
    kprint("\n");
    kprint("Buddy Allocator:       ");
    kprint(init_state.buddy_allocator_done ? "OK" : "FAILED");
    kprint("\n");
    kprint("Kernel Heap:           ");
    kprint(init_state.kernel_heap_done ? "OK" : "FAILED");
    kprint("\n");
    kprint("Process VM:            ");
    kprint(init_state.process_vm_done ? "OK" : "FAILED");
    kprint("\n");
    kprint("VMem Regions:          ");
    kprint(init_state.vmem_regions_done ? "OK" : "FAILED");
    kprint("\n");
    kprint("Full Paging:           ");
    kprint(init_state.paging_done ? "OK" : "FAILED");
    kprint("\n");

    if (init_state.total_memory_bytes > 0) {
        kprint("Total Memory:          ");
        kprint_decimal(init_state.total_memory_bytes / (1024 * 1024));
        kprint(" MB\n");
        kprint("Available Memory:      ");
        kprint_decimal(init_state.available_memory_bytes / (1024 * 1024));
        kprint(" MB\n");
    }
    kprint("Memory Regions:        ");
    kprint_decimal(init_state.memory_regions_count);
    kprint(" regions\n");
    kprint("=====================================================\n\n");
}

/* ========================================================================
 * PUBLIC INTERFACE
 * ======================================================================== */

/**
 * Initialize the complete memory management system
 * Must be called early during kernel boot after basic CPU setup
 *
 * @param multiboot_info_addr Physical address of Multiboot2 information structure
 * @return 0 on success, -1 on failure (calls kernel_panic)
 */
int init_memory_system(uint64_t multiboot_info_addr) {
    kprint("\n========== SlopOS Memory System Initialization ==========\n");
    kprint("Initializing complete memory management system...\n");
    kprint("Multiboot2 info at: 0x");
    kprint_hex(multiboot_info_addr);
    kprint("\n");

    /* Phase 1: Early paging for basic memory access */
    if (initialize_early_memory() != 0) {
        return -1;
    }

    /* Phase 2: Discover available physical memory */
    if (initialize_memory_discovery(multiboot_info_addr) != 0) {
        return -1;
    }

    /* Phase 3: Set up physical memory allocators */
    if (initialize_physical_allocators() != 0) {
        return -1;
    }

    /* Phase 4: Set up virtual memory management */
    if (initialize_virtual_memory() != 0) {
        return -1;
    }

    /* Phase 5: Set up process memory management */
    if (initialize_process_memory() != 0) {
        return -1;
    }

    /* Display final summary */
    display_memory_summary();

    kprint("MM: Complete memory system initialization successful!\n");
    kprint("MM: Ready for scheduler and video subsystem initialization\n\n");

    return 0;
}

/**
 * Check if memory system is fully initialized
 * @return 1 if fully initialized, 0 otherwise
 */
int is_memory_system_initialized(void) {
    return (init_state.early_paging_done &&
            init_state.memory_layout_done &&
            init_state.multiboot_parsed &&
            init_state.efi_memory_parsed &&
            init_state.page_allocator_done &&
            init_state.buddy_allocator_done &&
            init_state.kernel_heap_done &&
            init_state.process_vm_done &&
            init_state.vmem_regions_done &&
            init_state.paging_done);
}

/**
 * Get memory system statistics
 * @param total_memory_out Output parameter for total system memory
 * @param available_memory_out Output parameter for available memory
 * @param regions_count_out Output parameter for number of memory regions
 */
void get_memory_statistics(uint64_t *total_memory_out,
                          uint64_t *available_memory_out,
                          uint32_t *regions_count_out) {
    if (total_memory_out) *total_memory_out = init_state.total_memory_bytes;
    if (available_memory_out) *available_memory_out = init_state.available_memory_bytes;
    if (regions_count_out) *regions_count_out = init_state.memory_regions_count;
}
