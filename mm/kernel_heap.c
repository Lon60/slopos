/*
 * SlopOS Memory Management - Kernel Heap Allocator
 * Provides kmalloc/kfree functionality for kernel memory allocation
 * Uses buddy allocator for efficient memory management
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"

/* Forward declarations */
void kernel_panic(const char *message);
uint64_t alloc_page_frame(uint32_t flags);
int free_page_frame(uint64_t phys_addr);

/* ========================================================================
 * KERNEL HEAP CONSTANTS
 * ======================================================================== */

/* Kernel heap configuration */
#define KERNEL_HEAP_START             0xFFFF800000000000ULL  /* Kernel heap virtual base */
#define KERNEL_HEAP_SIZE              0x10000000             /* 256MB initial heap */
#define KERNEL_HEAP_PAGE_COUNT        (KERNEL_HEAP_SIZE / PAGE_SIZE_4KB)

/* Allocation size constants */
#define MIN_ALLOC_SIZE                16        /* Minimum allocation size */
#define MAX_ALLOC_SIZE                0x100000  /* Maximum single allocation (1MB) */
#define HEAP_ALIGNMENT                8         /* Default alignment */

/* Block header magic values for debugging */
#define BLOCK_MAGIC_ALLOCATED         0xDEADBEEF
#define BLOCK_MAGIC_FREE              0xFEEDFACE

/* Heap allocation flags */
#define HEAP_FLAG_ZERO                0x01     /* Zero memory after allocation */
#define HEAP_FLAG_ATOMIC              0x02     /* Atomic allocation (no sleep) */

/* ========================================================================
 * HEAP BLOCK STRUCTURES
 * ======================================================================== */

/* Heap block header - tracks allocated and free memory blocks */
typedef struct heap_block {
    uint32_t magic;               /* Magic number for validation */
    uint32_t size;                /* Size of data area in bytes */
    uint32_t flags;               /* Block flags */
    uint32_t checksum;            /* Header checksum for corruption detection */
    struct heap_block *next;      /* Next block in free list */
    struct heap_block *prev;      /* Previous block in free list */
} heap_block_t;

/* Free list entry for different size classes */
typedef struct free_list {
    heap_block_t *head;           /* Head of free list */
    uint32_t count;               /* Number of blocks in list */
    uint32_t size_class;          /* Size class for this list */
} free_list_t;

/* Heap statistics */
typedef struct heap_stats {
    uint64_t total_size;          /* Total heap size */
    uint64_t allocated_size;      /* Currently allocated bytes */
    uint64_t free_size;           /* Currently free bytes */
    uint32_t total_blocks;        /* Total number of blocks */
    uint32_t allocated_blocks;    /* Number of allocated blocks */
    uint32_t free_blocks;         /* Number of free blocks */
    uint32_t allocation_count;    /* Total allocations made */
    uint32_t free_count;          /* Total frees made */
} heap_stats_t;

/* Kernel heap manager */
typedef struct kernel_heap {
    uint64_t start_addr;          /* Heap start virtual address */
    uint64_t end_addr;            /* Heap end virtual address */
    uint64_t current_break;       /* Current heap break */
    free_list_t free_lists[16];   /* Free lists for different sizes */
    heap_stats_t stats;           /* Heap statistics */
    uint32_t initialized;         /* Initialization flag */
} kernel_heap_t;

/* Global kernel heap instance */
static kernel_heap_t kernel_heap = {0};

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/*
 * Calculate header checksum for corruption detection
 */
static uint32_t calculate_checksum(heap_block_t *block) {
    return block->magic ^ block->size ^ block->flags;
}

/*
 * Validate block header integrity
 */
static int validate_block(heap_block_t *block) {
    if (!block) {
        return 0;
    }

    /* Check magic number */
    if (block->magic != BLOCK_MAGIC_ALLOCATED &&
        block->magic != BLOCK_MAGIC_FREE) {
        return 0;
    }

    /* Check checksum */
    uint32_t expected = calculate_checksum(block);
    if (block->checksum != expected) {
        return 0;
    }

    return 1;
}

/*
 * Get size class index for allocation size
 */
static uint32_t get_size_class(uint32_t size) {
    if (size <= 16) return 0;
    if (size <= 32) return 1;
    if (size <= 64) return 2;
    if (size <= 128) return 3;
    if (size <= 256) return 4;
    if (size <= 512) return 5;
    if (size <= 1024) return 6;
    if (size <= 2048) return 7;
    if (size <= 4096) return 8;
    if (size <= 8192) return 9;
    if (size <= 16384) return 10;
    if (size <= 32768) return 11;
    if (size <= 65536) return 12;
    if (size <= 131072) return 13;
    if (size <= 262144) return 14;
    return 15;  /* Large allocations */
}

/*
 * Round up size to next power of 2 or minimum allocation size
 */
static uint32_t round_up_size(uint32_t size) {
    if (size < MIN_ALLOC_SIZE) {
        return MIN_ALLOC_SIZE;
    }

    /* Round up to next power of 2 */
    uint32_t rounded = MIN_ALLOC_SIZE;
    while (rounded < size) {
        rounded <<= 1;
    }

    return rounded;
}

/* ========================================================================
 * FREE LIST MANAGEMENT
 * ======================================================================== */

/*
 * Add block to appropriate free list
 */
static void add_to_free_list(heap_block_t *block) {
    if (!validate_block(block)) {
        kprint("add_to_free_list: Invalid block\n");
        return;
    }

    uint32_t size_class = get_size_class(block->size);
    free_list_t *list = &kernel_heap.free_lists[size_class];

    block->magic = BLOCK_MAGIC_FREE;
    block->checksum = calculate_checksum(block);

    /* Add to head of list */
    block->next = list->head;
    block->prev = NULL;

    if (list->head) {
        list->head->prev = block;
    }

    list->head = block;
    list->count++;

    kernel_heap.stats.free_blocks++;
    kernel_heap.stats.allocated_blocks--;
}

/*
 * Remove block from free list
 */
static void remove_from_free_list(heap_block_t *block) {
    if (!validate_block(block)) {
        kprint("remove_from_free_list: Invalid block\n");
        return;
    }

    uint32_t size_class = get_size_class(block->size);
    free_list_t *list = &kernel_heap.free_lists[size_class];

    /* Remove from linked list */
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        list->head = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }

    list->count--;

    block->magic = BLOCK_MAGIC_ALLOCATED;
    block->next = NULL;
    block->prev = NULL;
    block->checksum = calculate_checksum(block);

    kernel_heap.stats.allocated_blocks++;
    kernel_heap.stats.free_blocks--;
}

/*
 * Find suitable block in free lists
 */
static heap_block_t *find_free_block(uint32_t size) {
    uint32_t size_class = get_size_class(size);

    /* Search from appropriate size class up to larger ones */
    for (uint32_t i = size_class; i < 16; i++) {
        free_list_t *list = &kernel_heap.free_lists[i];

        if (list->head && list->head->size >= size) {
            return list->head;
        }
    }

    return NULL;
}

/* ========================================================================
 * HEAP EXPANSION
 * ======================================================================== */

/*
 * Expand heap by allocating more pages
 */
static int expand_heap(uint32_t min_size) {
    /* Calculate pages needed */
    uint32_t pages_needed = (min_size + PAGE_SIZE_4KB - 1) / PAGE_SIZE_4KB;

    /* Ensure minimum expansion */
    if (pages_needed < 4) {
        pages_needed = 4;
    }

    kprint("Expanding heap by ");
    kprint_decimal(pages_needed);
    kprint(" pages\n");

    /* Allocate physical pages and map them */
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint64_t phys_page = alloc_page_frame(0);
        if (!phys_page) {
            kprint("expand_heap: Failed to allocate physical page\n");
            return -1;
        }

        /* TODO: Map virtual page to physical page */
        /* For now, assume identity mapping in kernel space */
    }

    /* Create large free block from new pages */
    uint64_t new_block_addr = kernel_heap.current_break;
    uint32_t new_block_size = pages_needed * PAGE_SIZE_4KB - sizeof(heap_block_t);

    heap_block_t *new_block = (heap_block_t*)new_block_addr;
    new_block->magic = BLOCK_MAGIC_FREE;
    new_block->size = new_block_size;
    new_block->flags = 0;
    new_block->next = NULL;
    new_block->prev = NULL;
    new_block->checksum = calculate_checksum(new_block);

    /* Update heap break */
    kernel_heap.current_break += pages_needed * PAGE_SIZE_4KB;
    kernel_heap.stats.total_size += pages_needed * PAGE_SIZE_4KB;
    kernel_heap.stats.free_size += new_block_size;

    /* Add to free lists */
    add_to_free_list(new_block);

    return 0;
}

/* ========================================================================
 * MEMORY ALLOCATION AND DEALLOCATION
 * ======================================================================== */

/*
 * Allocate memory from kernel heap
 * Returns pointer to allocated memory, NULL on failure
 */
void *kmalloc(size_t size) {
    if (!kernel_heap.initialized) {
        kprint("kmalloc: Heap not initialized\n");
        return NULL;
    }

    if (size == 0 || size > MAX_ALLOC_SIZE) {
        return NULL;
    }

    /* Round up size and add header overhead */
    uint32_t rounded_size = round_up_size(size);
    uint32_t total_size = rounded_size + sizeof(heap_block_t);

    /* Find suitable free block */
    heap_block_t *block = find_free_block(total_size);

    /* Expand heap if no suitable block found */
    if (!block) {
        if (expand_heap(total_size) != 0) {
            return NULL;
        }
        block = find_free_block(total_size);
    }

    if (!block) {
        kprint("kmalloc: No suitable block found after expansion\n");
        return NULL;
    }

    /* Remove from free list */
    remove_from_free_list(block);

    /* Split block if it's significantly larger */
    if (block->size > total_size + sizeof(heap_block_t) + MIN_ALLOC_SIZE) {
        /* Create new block from remainder */
        heap_block_t *new_block = (heap_block_t*)((uint8_t*)block + sizeof(heap_block_t) + rounded_size);
        new_block->magic = BLOCK_MAGIC_FREE;
        new_block->size = block->size - total_size;
        new_block->flags = 0;
        new_block->next = NULL;
        new_block->prev = NULL;
        new_block->checksum = calculate_checksum(new_block);

        /* Update original block size */
        block->size = rounded_size;
        block->checksum = calculate_checksum(block);

        /* Add remainder to free list */
        add_to_free_list(new_block);
    }

    /* Update statistics */
    kernel_heap.stats.allocated_size += block->size;
    kernel_heap.stats.free_size -= block->size;
    kernel_heap.stats.allocation_count++;

    /* Return pointer to data area */
    return (void*)((uint8_t*)block + sizeof(heap_block_t));
}

/*
 * Allocate zeroed memory from kernel heap
 */
void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (!ptr) {
        return NULL;
    }

    /* Zero the allocated memory */
    uint8_t *data = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        data[i] = 0;
    }

    return ptr;
}

/*
 * Free memory to kernel heap
 */
void kfree(void *ptr) {
    if (!ptr || !kernel_heap.initialized) {
        return;
    }

    /* Get block header */
    heap_block_t *block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));

    if (!validate_block(block) || block->magic != BLOCK_MAGIC_ALLOCATED) {
        kprint("kfree: Invalid block or double free detected\n");
        return;
    }

    /* Update statistics */
    kernel_heap.stats.allocated_size -= block->size;
    kernel_heap.stats.free_size += block->size;
    kernel_heap.stats.free_count++;

    /* Add to free list */
    add_to_free_list(block);

    /* TODO: Implement block coalescing for better memory utilization */
}

/* ========================================================================
 * INITIALIZATION AND DIAGNOSTICS
 * ======================================================================== */

/*
 * Initialize kernel heap
 * Sets up initial heap area and free lists
 */
int init_kernel_heap(void) {
    kprint("Initializing kernel heap\n");

    kernel_heap.start_addr = KERNEL_HEAP_START;
    kernel_heap.end_addr = KERNEL_HEAP_START + KERNEL_HEAP_SIZE;
    kernel_heap.current_break = KERNEL_HEAP_START;

    /* Initialize free lists */
    for (uint32_t i = 0; i < 16; i++) {
        kernel_heap.free_lists[i].head = NULL;
        kernel_heap.free_lists[i].count = 0;
        kernel_heap.free_lists[i].size_class = i;
    }

    /* Initialize statistics */
    kernel_heap.stats.total_size = 0;
    kernel_heap.stats.allocated_size = 0;
    kernel_heap.stats.free_size = 0;
    kernel_heap.stats.total_blocks = 0;
    kernel_heap.stats.allocated_blocks = 0;
    kernel_heap.stats.free_blocks = 0;
    kernel_heap.stats.allocation_count = 0;
    kernel_heap.stats.free_count = 0;

    /* Perform initial heap expansion */
    if (expand_heap(PAGE_SIZE_4KB * 4) != 0) {
        kernel_panic("Failed to initialize kernel heap");
    }

    kernel_heap.initialized = 1;

    kprint("Kernel heap initialized at ");
    kprint_hex(KERNEL_HEAP_START);
    kprint("\n");

    return 0;
}

/*
 * Get kernel heap statistics
 */
void get_heap_stats(heap_stats_t *stats) {
    if (stats) {
        *stats = kernel_heap.stats;
    }
}

/*
 * Print heap statistics for debugging
 */
void print_heap_stats(void) {
    kprint("=== Kernel Heap Statistics ===\n");
    kprint("Total size: ");
    kprint_decimal(kernel_heap.stats.total_size);
    kprint(" bytes\n");
    kprint("Allocated: ");
    kprint_decimal(kernel_heap.stats.allocated_size);
    kprint(" bytes\n");
    kprint("Free: ");
    kprint_decimal(kernel_heap.stats.free_size);
    kprint(" bytes\n");
    kprint("Allocations: ");
    kprint_decimal(kernel_heap.stats.allocation_count);
    kprint("\n");
    kprint("Frees: ");
    kprint_decimal(kernel_heap.stats.free_count);
    kprint("\n");
}