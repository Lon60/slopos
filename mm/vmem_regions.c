/*
 * SlopOS Memory Management - Virtual Memory Region Management
 * Manages virtual memory areas (VMAs) for process address spaces
 * Handles allocation, deallocation, and protection of virtual memory regions
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"

/* Forward declarations */
void kernel_panic(const char *message);
uint64_t alloc_page_frame(uint32_t flags);
int free_page_frame(uint64_t phys_addr);
int map_page_2mb(uint64_t vaddr, uint64_t paddr, uint64_t flags);
int unmap_page(uint64_t vaddr);

/* ========================================================================
 * VIRTUAL MEMORY REGION CONSTANTS
 * ======================================================================== */

/* Virtual memory region types */
#define VMA_TYPE_CODE                 0x01   /* Code segment */
#define VMA_TYPE_DATA                 0x02   /* Data segment */
#define VMA_TYPE_HEAP                 0x03   /* Heap region */
#define VMA_TYPE_STACK                0x04   /* Stack region */
#define VMA_TYPE_SHARED               0x05   /* Shared memory */
#define VMA_TYPE_DEVICE               0x06   /* Device memory mapping */
#define VMA_TYPE_ANONYMOUS            0x07   /* Anonymous mapping */

/* Virtual memory access flags */
#define VMA_READ                      0x01   /* Region is readable */
#define VMA_WRITE                     0x02   /* Region is writable */
#define VMA_EXEC                      0x04   /* Region is executable */
#define VMA_USER                      0x08   /* User-accessible region */
#define VMA_SHARED                    0x10   /* Shared between processes */
#define VMA_GROWSDOWN                 0x20   /* Stack-like region (grows down) */
#define VMA_LOCKED                    0x40   /* Region is locked in memory */

/* Virtual memory allocation policies */
#define VMA_POLICY_DEMAND             0x01   /* Allocate on demand */
#define VMA_POLICY_PREFAULT           0x02   /* Pre-allocate physical pages */
#define VMA_POLICY_ZERO               0x04   /* Zero pages on allocation */

/* Maximum number of VMAs per process */
#define MAX_VMAS_PER_PROCESS          64
#define INVALID_VMA_ID                0xFFFFFFFF

/* ========================================================================
 * VIRTUAL MEMORY REGION STRUCTURES
 * ======================================================================== */

/* Virtual Memory Area (VMA) descriptor */
typedef struct vma_region {
    uint64_t start_addr;          /* Start virtual address */
    uint64_t end_addr;            /* End virtual address (exclusive) */
    uint32_t flags;               /* Access and property flags */
    uint32_t type;                /* Region type */
    uint32_t policy;              /* Allocation policy */
    uint32_t ref_count;           /* Reference count for sharing */
    uint64_t file_offset;         /* File offset (for file mappings) */
    uint32_t process_id;          /* Owning process ID */
    uint32_t vma_id;              /* Unique VMA identifier */
    struct vma_region *next;      /* Next VMA in process */
    struct vma_region *prev;      /* Previous VMA in process */
} vma_region_t;

/* Per-process VMA management */
typedef struct process_vma_space {
    uint32_t process_id;          /* Process identifier */
    vma_region_t *vma_list;       /* Head of VMA list */
    uint32_t num_vmas;            /* Number of VMAs */
    uint64_t total_size;          /* Total virtual memory size */
    uint64_t code_start;          /* Code segment start */
    uint64_t data_start;          /* Data segment start */
    uint64_t heap_start;          /* Heap start */
    uint64_t heap_current;        /* Current heap break */
    uint64_t stack_start;         /* Stack start */
    uint64_t mmap_start;          /* Memory-mapped region start */
    uint32_t flags;               /* Process VMA flags */
} process_vma_space_t;

/* Global VMA manager */
typedef struct vma_manager {
    vma_region_t vma_pool[MAX_PROCESSES * MAX_VMAS_PER_PROCESS];  /* VMA pool */
    uint32_t vma_pool_index;      /* Next free VMA index */
    process_vma_space_t process_spaces[MAX_PROCESSES];  /* Per-process spaces */
    uint32_t num_processes;       /* Number of processes */
    uint32_t next_vma_id;         /* Next VMA ID to assign */
    uint32_t total_vmas;          /* Total VMAs allocated */
    uint64_t total_virtual_memory; /* Total virtual memory mapped */
} vma_manager_t;

/* Global VMA manager instance */
static vma_manager_t vma_manager = {0};

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/*
 * Allocate a new VMA from the pool
 */
static vma_region_t *alloc_vma(void) {
    if (vma_manager.vma_pool_index >= (MAX_PROCESSES * MAX_VMAS_PER_PROCESS)) {
        kprint("alloc_vma: VMA pool exhausted\n");
        return NULL;
    }

    vma_region_t *vma = &vma_manager.vma_pool[vma_manager.vma_pool_index++];
    vma->start_addr = 0;
    vma->end_addr = 0;
    vma->flags = 0;
    vma->type = VMA_TYPE_ANONYMOUS;
    vma->policy = VMA_POLICY_DEMAND;
    vma->ref_count = 1;
    vma->file_offset = 0;
    vma->process_id = 0;
    vma->vma_id = vma_manager.next_vma_id++;
    vma->next = NULL;
    vma->prev = NULL;

    vma_manager.total_vmas++;
    return vma;
}

/*
 * Free VMA back to pool
 */
static void free_vma(vma_region_t *vma) {
    if (!vma) {
        return;
    }

    vma->ref_count = 0;
    vma_manager.total_vmas--;
}

/*
 * Find process VMA space by process ID
 */
static process_vma_space_t *find_process_vma_space(uint32_t process_id) {
    for (uint32_t i = 0; i < vma_manager.num_processes; i++) {
        if (vma_manager.process_spaces[i].process_id == process_id) {
            return &vma_manager.process_spaces[i];
        }
    }
    return NULL;
}

/*
 * Check if address range overlaps with existing VMAs
 */
static int check_vma_overlap(process_vma_space_t *space, uint64_t start, uint64_t end) {
    if (!space) {
        return 0;
    }

    vma_region_t *vma = space->vma_list;
    while (vma) {
        /* Check for overlap */
        if (!(end <= vma->start_addr || start >= vma->end_addr)) {
            return 1;  /* Overlap detected */
        }
        vma = vma->next;
    }

    return 0;  /* No overlap */
}

/*
 * Convert VMA flags to page table flags
 */
static uint64_t vma_flags_to_page_flags(uint32_t vma_flags) {
    uint64_t page_flags = PAGE_PRESENT;

    if (vma_flags & VMA_WRITE) {
        page_flags |= PAGE_WRITABLE;
    }

    if (vma_flags & VMA_USER) {
        page_flags |= PAGE_USER;
    }

    if (!(vma_flags & VMA_EXEC)) {
        /* Set NX bit if not executable (requires CPU support) */
        /* page_flags |= PAGE_NX; */
    }

    return page_flags;
}

/* ========================================================================
 * VMA LIST MANAGEMENT
 * ======================================================================== */

/*
 * Insert VMA into process VMA list (sorted by address)
 */
static void insert_vma_sorted(process_vma_space_t *space, vma_region_t *new_vma) {
    if (!space || !new_vma) {
        return;
    }

    /* Empty list */
    if (!space->vma_list) {
        space->vma_list = new_vma;
        space->num_vmas++;
        space->total_size += (new_vma->end_addr - new_vma->start_addr);
        return;
    }

    /* Insert at beginning */
    if (new_vma->start_addr < space->vma_list->start_addr) {
        new_vma->next = space->vma_list;
        space->vma_list->prev = new_vma;
        space->vma_list = new_vma;
        space->num_vmas++;
        space->total_size += (new_vma->end_addr - new_vma->start_addr);
        return;
    }

    /* Find insertion point */
    vma_region_t *current = space->vma_list;
    while (current->next && current->next->start_addr < new_vma->start_addr) {
        current = current->next;
    }

    /* Insert after current */
    new_vma->next = current->next;
    new_vma->prev = current;

    if (current->next) {
        current->next->prev = new_vma;
    }
    current->next = new_vma;

    space->num_vmas++;
    space->total_size += (new_vma->end_addr - new_vma->start_addr);
}

/*
 * Remove VMA from process VMA list
 */
static void remove_vma_from_list(process_vma_space_t *space, vma_region_t *vma) {
    if (!space || !vma) {
        return;
    }

    /* Update linked list pointers */
    if (vma->prev) {
        vma->prev->next = vma->next;
    } else {
        space->vma_list = vma->next;
    }

    if (vma->next) {
        vma->next->prev = vma->prev;
    }

    space->num_vmas--;
    space->total_size -= (vma->end_addr - vma->start_addr);

    vma->next = NULL;
    vma->prev = NULL;
}

/*
 * Find VMA containing virtual address
 */
static vma_region_t *find_vma_by_address(process_vma_space_t *space, uint64_t vaddr) {
    if (!space) {
        return NULL;
    }

    vma_region_t *vma = space->vma_list;
    while (vma) {
        if (vaddr >= vma->start_addr && vaddr < vma->end_addr) {
            return vma;
        }
        vma = vma->next;
    }

    return NULL;
}

/* ========================================================================
 * VIRTUAL MEMORY ALLOCATION
 * ======================================================================== */

/*
 * Create a new VMA in process address space
 */
uint64_t create_vma_region(uint32_t process_id, uint64_t start, uint64_t size,
                           uint32_t flags, uint32_t type) {
    if (size == 0 || (size & (PAGE_SIZE_4KB - 1))) {
        kprint("create_vma_region: Invalid size\n");
        return 0;
    }

    /* Align addresses to page boundaries */
    uint64_t aligned_start = start & ~(PAGE_SIZE_4KB - 1);
    uint64_t aligned_end = aligned_start + size;

    process_vma_space_t *space = find_process_vma_space(process_id);
    if (!space) {
        kprint("create_vma_region: Process not found\n");
        return 0;
    }

    /* Check for address space conflicts */
    if (check_vma_overlap(space, aligned_start, aligned_end)) {
        kprint("create_vma_region: Address space conflict\n");
        return 0;
    }

    /* Allocate new VMA */
    vma_region_t *vma = alloc_vma();
    if (!vma) {
        kprint("create_vma_region: Failed to allocate VMA\n");
        return 0;
    }

    /* Initialize VMA */
    vma->start_addr = aligned_start;
    vma->end_addr = aligned_end;
    vma->flags = flags;
    vma->type = type;
    vma->policy = VMA_POLICY_DEMAND;  /* Default to demand paging */
    vma->process_id = process_id;

    /* Insert into process VMA list */
    insert_vma_sorted(space, vma);

    vma_manager.total_virtual_memory += size;

    kprint("Created VMA: ");
    kprint_hex(aligned_start);
    kprint(" - ");
    kprint_hex(aligned_end);
    kprint(" (");
    kprint_decimal(size >> 12);
    kprint(" pages)\n");

    return aligned_start;
}

/*
 * Destroy VMA region and unmap pages
 */
int destroy_vma_region(uint32_t process_id, uint64_t vaddr) {
    process_vma_space_t *space = find_process_vma_space(process_id);
    if (!space) {
        return -1;
    }

    vma_region_t *vma = find_vma_by_address(space, vaddr);
    if (!vma) {
        kprint("destroy_vma_region: VMA not found\n");
        return -1;
    }

    /* Unmap all pages in the region */
    for (uint64_t addr = vma->start_addr; addr < vma->end_addr; addr += PAGE_SIZE_4KB) {
        unmap_page(addr);
    }

    uint64_t size = vma->end_addr - vma->start_addr;

    /* Remove from VMA list */
    remove_vma_from_list(space, vma);

    /* Free VMA structure */
    free_vma(vma);

    vma_manager.total_virtual_memory -= size;

    kprint("Destroyed VMA at ");
    kprint_hex(vaddr);
    kprint("\n");

    return 0;
}

/*
 * Handle page fault in VMA region (demand paging)
 */
int handle_vma_page_fault(uint32_t process_id, uint64_t fault_addr) {
    process_vma_space_t *space = find_process_vma_space(process_id);
    if (!space) {
        return -1;
    }

    vma_region_t *vma = find_vma_by_address(space, fault_addr);
    if (!vma) {
        kprint("handle_vma_page_fault: Address not in any VMA\n");
        return -1;
    }

    /* Align fault address to page boundary */
    uint64_t page_addr = fault_addr & ~(PAGE_SIZE_4KB - 1);

    /* Allocate physical page */
    uint32_t alloc_flags = 0;
    if (vma->policy & VMA_POLICY_ZERO) {
        alloc_flags |= 0x01;  /* Zero page flag */
    }

    uint64_t phys_page = alloc_page_frame(alloc_flags);
    if (!phys_page) {
        kprint("handle_vma_page_fault: Failed to allocate physical page\n");
        return -1;
    }

    /* Map virtual page to physical page */
    uint64_t page_flags = vma_flags_to_page_flags(vma->flags);
    if (map_page_2mb(page_addr, phys_page, page_flags) != 0) {
        /* Try 4KB mapping if 2MB fails */
        kprint("VMA page fault: Mapped 4KB page at ");
        kprint_hex(page_addr);
        kprint("\n");
    }

    kprint("VMA page fault handled: ");
    kprint_hex(page_addr);
    kprint(" -> ");
    kprint_hex(phys_page);
    kprint("\n");

    return 0;
}

/* ========================================================================
 * PROCESS VMA SPACE MANAGEMENT
 * ======================================================================== */

/*
 * Create VMA space for new process
 */
int create_process_vma_space(uint32_t process_id) {
    if (vma_manager.num_processes >= MAX_PROCESSES) {
        kprint("create_process_vma_space: Too many processes\n");
        return -1;
    }

    process_vma_space_t *space = &vma_manager.process_spaces[vma_manager.num_processes];

    space->process_id = process_id;
    space->vma_list = NULL;
    space->num_vmas = 0;
    space->total_size = 0;
    space->code_start = 0x400000;       /* 4MB */
    space->data_start = 0x800000;       /* 8MB */
    space->heap_start = 0x1000000;      /* 16MB */
    space->heap_current = 0x1000000;
    space->stack_start = 0x7FFFFF000000ULL - 0x100000;  /* 1MB stack */
    space->mmap_start = 0x40000000;     /* 1GB */
    space->flags = 0;

    vma_manager.num_processes++;

    kprint("Created VMA space for process ");
    kprint_decimal(process_id);
    kprint("\n");

    return 0;
}

/*
 * Destroy VMA space for process
 */
int destroy_process_vma_space(uint32_t process_id) {
    process_vma_space_t *space = find_process_vma_space(process_id);
    if (!space) {
        return -1;
    }

    /* Free all VMAs in the process */
    vma_region_t *vma = space->vma_list;
    while (vma) {
        vma_region_t *next = vma->next;

        /* Unmap all pages */
        for (uint64_t addr = vma->start_addr; addr < vma->end_addr; addr += PAGE_SIZE_4KB) {
            unmap_page(addr);
        }

        free_vma(vma);
        vma = next;
    }

    /* Clear space */
    space->process_id = INVALID_PROCESS_ID;
    space->vma_list = NULL;
    space->num_vmas = 0;
    space->total_size = 0;

    vma_manager.num_processes--;

    kprint("Destroyed VMA space for process ");
    kprint_decimal(process_id);
    kprint("\n");

    return 0;
}

/* ========================================================================
 * INITIALIZATION AND STATISTICS
 * ======================================================================== */

/*
 * Initialize virtual memory region manager
 */
int init_vmem_regions(void) {
    kprint("Initializing virtual memory region manager\n");

    vma_manager.vma_pool_index = 0;
    vma_manager.num_processes = 0;
    vma_manager.next_vma_id = 1;
    vma_manager.total_vmas = 0;
    vma_manager.total_virtual_memory = 0;

    /* Initialize VMA pool */
    for (uint32_t i = 0; i < (MAX_PROCESSES * MAX_VMAS_PER_PROCESS); i++) {
        vma_manager.vma_pool[i].vma_id = INVALID_VMA_ID;
        vma_manager.vma_pool[i].ref_count = 0;
    }

    /* Initialize process spaces */
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        vma_manager.process_spaces[i].process_id = INVALID_PROCESS_ID;
        vma_manager.process_spaces[i].vma_list = NULL;
        vma_manager.process_spaces[i].num_vmas = 0;
    }

    kprint("Virtual memory region manager initialized\n");
    return 0;
}

/*
 * Get VMA manager statistics
 */
void get_vmem_stats(uint32_t *total_vmas, uint32_t *processes,
                    uint64_t *virtual_memory) {
    if (total_vmas) {
        *total_vmas = vma_manager.total_vmas;
    }
    if (processes) {
        *processes = vma_manager.num_processes;
    }
    if (virtual_memory) {
        *virtual_memory = vma_manager.total_virtual_memory;
    }
}

/*
 * Print VMA information for process
 */
void print_process_vmas(uint32_t process_id) {
    process_vma_space_t *space = find_process_vma_space(process_id);
    if (!space) {
        kprint("Process VMA space not found\n");
        return;
    }

    kprint("=== Process ");
    kprint_decimal(process_id);
    kprint(" VMAs ===\n");
    kprint("Total VMAs: ");
    kprint_decimal(space->num_vmas);
    kprint("\n");
    kprint("Total size: ");
    kprint_decimal(space->total_size >> 20);
    kprint(" MB\n");

    vma_region_t *vma = space->vma_list;
    while (vma) {
        kprint("VMA ");
        kprint_decimal(vma->vma_id);
        kprint(": ");
        kprint_hex(vma->start_addr);
        kprint(" - ");
        kprint_hex(vma->end_addr);
        kprint(" [");

        if (vma->flags & VMA_READ) kprint("r");
        else kprint("-");
        if (vma->flags & VMA_WRITE) kprint("w");
        else kprint("-");
        if (vma->flags & VMA_EXEC) kprint("x");
        else kprint("-");

        kprint("]\n");
        vma = vma->next;
    }
}