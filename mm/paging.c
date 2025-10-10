/*
 * SlopOS Memory Management - Core Paging Infrastructure
 * Process-centric page table operations for x86_64
 * Maps/unmaps pages, handles table traversal, process isolation
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"

/* Forward declarations */
void kernel_panic(const char *message);

/* ========================================================================
 * PAGE TABLE CONSTANTS - Using boot/constants.h definitions
 * ======================================================================== */

/* Process virtual memory layout constants */
#define USER_SPACE_START              0x400000        /* Typical user space start (4MB) */
#define USER_SPACE_END                0x800000000000ULL /* User space end (128TB) */
#define KERNEL_HEAP_START             0xFFFF800000000000ULL /* Kernel heap space */
#define KERNEL_HEAP_END               0xFFFFFFFF80000000ULL /* Before higher-half kernel */

/* Page table entry mask for extracting physical address */
#define PTE_ADDRESS_MASK              0x000FFFFFFFFFF000ULL

/* ========================================================================
 * PAGE TABLE STRUCTURES AND MANAGEMENT
 * ======================================================================== */

/* Page table structure - aligned to 4KB boundary as required by x86_64 */
typedef struct {
    uint64_t entries[ENTRIES_PER_PAGE_TABLE];
} __attribute__((aligned(PAGE_ALIGN))) page_table_t;

/* Process page directory structure for process isolation */
typedef struct process_page_dir {
    page_table_t *pml4;                    /* Process PML4 table */
    uint64_t pml4_phys;                    /* Physical address of PML4 */
    uint32_t ref_count;                    /* Reference count for sharing */
    uint32_t process_id;                   /* Process ID for debugging */
    struct process_page_dir *next;         /* Link for process list */
} process_page_dir_t;

/* External references to early boot page tables from linker */
extern page_table_t early_pml4;
extern page_table_t early_pdpt;
extern page_table_t early_pd;

/* Kernel page directory - always active in higher-half */
static process_page_dir_t kernel_page_dir = {
    .pml4 = &early_pml4,
    .pml4_phys = 0,  /* Will be set during initialization */
    .ref_count = 1,
    .process_id = 0, /* Kernel process ID */
    .next = NULL
};

/* Current active page directory for running process */
static process_page_dir_t *current_page_dir = &kernel_page_dir;

/* ========================================================================
 * PAGE TABLE UTILITY FUNCTIONS
 * ======================================================================== */

/*
 * Extract page table indices from virtual address
 * Returns indices for traversing the 4-level page table hierarchy
 */
static inline uint16_t pml4_index(uint64_t vaddr) {
    return (vaddr >> 39) & 0x1FF;  /* Bits 47-39 */
}

static inline uint16_t pdpt_index(uint64_t vaddr) {
    return (vaddr >> 30) & 0x1FF;  /* Bits 38-30 */
}

static inline uint16_t pd_index(uint64_t vaddr) {
    return (vaddr >> 21) & 0x1FF;  /* Bits 29-21 */
}

static inline uint16_t pt_index(uint64_t vaddr) {
    return (vaddr >> 12) & 0x1FF;  /* Bits 20-12 */
}

/*
 * Extract physical address from page table entry
 * Masks out flags and returns 4KB-aligned physical address
 */
static inline uint64_t pte_address(uint64_t pte) {
    return pte & PTE_ADDRESS_MASK;
}

/*
 * Check if page table entry is present in memory
 */
static inline int pte_present(uint64_t pte) {
    return pte & PAGE_PRESENT;
}

/*
 * Check if page table entry represents a large page (2MB/1GB)
 */
static inline int pte_huge(uint64_t pte) {
    return pte & PAGE_SIZE;
}

/*
 * Validate virtual address for user space operations
 * Returns 1 if address is in valid user space range
 */
static inline int is_user_address(uint64_t vaddr) {
    return (vaddr >= USER_SPACE_START && vaddr < USER_SPACE_END);
}

/*
 * Validate virtual address for kernel space operations
 * Returns 1 if address is in kernel space
 */
static inline int is_kernel_address(uint64_t vaddr) {
    return (vaddr >= KERNEL_VIRTUAL_BASE) ||
           (vaddr >= KERNEL_HEAP_START && vaddr < KERNEL_HEAP_END);
}

/* ========================================================================
 * TLB AND HARDWARE CONTROL FUNCTIONS
 * ======================================================================== */

/*
 * Invalidate TLB entry for a specific virtual page
 * Used after changing page table mappings
 */
static inline void invlpg(uint64_t vaddr) {
    __asm__ volatile ("invlpg (%0)" :: "r" (vaddr) : "memory");
}

/*
 * Flush entire TLB by reloading CR3
 * More expensive but ensures all TLB entries are invalidated
 */
static inline void flush_tlb(void) {
    uint64_t cr3;
    __asm__ volatile ("movq %%cr3, %0" : "=r" (cr3));
    __asm__ volatile ("movq %0, %%cr3" :: "r" (cr3) : "memory");
}

/*
 * Get current CR3 (page table base register)
 * Returns physical address of current PML4 table
 */
static inline uint64_t get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("movq %%cr3, %0" : "=r" (cr3));
    return cr3;
}

/*
 * Set CR3 (page table base register)
 * Switches to specified PML4 table for process context switching
 */
static inline void set_cr3(uint64_t pml4_phys) {
    __asm__ volatile ("movq %0, %%cr3" :: "r" (pml4_phys) : "memory");
}

/* ========================================================================
 * CORE PAGE TABLE TRAVERSAL AND TRANSLATION
 * ======================================================================== */

/*
 * Convert virtual address to physical address using current page directory
 * Returns 0 if not mapped or on error
 * Supports 4KB, 2MB, and 1GB pages
 */
uint64_t virt_to_phys(uint64_t vaddr) {
    if (!current_page_dir || !current_page_dir->pml4) {
        kprint("virt_to_phys: No current page directory\n");
        return 0;
    }

    uint16_t pml4_idx = pml4_index(vaddr);
    uint16_t pdpt_idx = pdpt_index(vaddr);
    uint16_t pd_idx = pd_index(vaddr);
    uint16_t pt_idx = pt_index(vaddr);

    /* Traverse PML4 -> PDPT */
    uint64_t pml4_entry = current_page_dir->pml4->entries[pml4_idx];
    if (!pte_present(pml4_entry)) {
        return 0;  /* Page not mapped */
    }

    /* Get PDPT table */
    page_table_t *pdpt = (page_table_t*)pte_address(pml4_entry);
    if (!pdpt) {
        kprint("virt_to_phys: Invalid PDPT address\n");
        return 0;
    }

    uint64_t pdpt_entry = pdpt->entries[pdpt_idx];
    if (!pte_present(pdpt_entry)) {
        return 0;  /* Page not mapped */
    }

    /* Check for 1GB huge page */
    if (pte_huge(pdpt_entry)) {
        uint64_t page_offset = vaddr & (PAGE_SIZE_1GB - 1);
        return pte_address(pdpt_entry) + page_offset;
    }

    /* Traverse PDPT -> PD */
    page_table_t *pd = (page_table_t*)pte_address(pdpt_entry);
    if (!pd) {
        kprint("virt_to_phys: Invalid PD address\n");
        return 0;
    }

    uint64_t pd_entry = pd->entries[pd_idx];
    if (!pte_present(pd_entry)) {
        return 0;  /* Page not mapped */
    }

    /* Check for 2MB large page */
    if (pte_huge(pd_entry)) {
        uint64_t page_offset = vaddr & (PAGE_SIZE_2MB - 1);
        return pte_address(pd_entry) + page_offset;
    }

    /* Traverse PD -> PT */
    page_table_t *pt = (page_table_t*)pte_address(pd_entry);
    if (!pt) {
        kprint("virt_to_phys: Invalid PT address\n");
        return 0;
    }

    uint64_t pt_entry = pt->entries[pt_idx];
    if (!pte_present(pt_entry)) {
        return 0;  /* Page not mapped */
    }

    /* 4KB page translation */
    uint64_t page_offset = vaddr & (PAGE_SIZE_4KB - 1);
    return pte_address(pt_entry) + page_offset;
}

/*
 * Convert virtual address to physical address for specific process
 * Used for cross-process memory operations
 */
uint64_t virt_to_phys_process(uint64_t vaddr, process_page_dir_t *page_dir) {
    if (!page_dir || !page_dir->pml4) {
        kprint("virt_to_phys_process: Invalid page directory\n");
        return 0;
    }

    /* Temporarily switch context for translation */
    process_page_dir_t *saved_page_dir = current_page_dir;
    current_page_dir = page_dir;

    uint64_t phys_addr = virt_to_phys(vaddr);

    /* Restore original context */
    current_page_dir = saved_page_dir;

    return phys_addr;
}

/* ========================================================================
 * PAGE MAPPING FUNCTIONS
 * ======================================================================== */

/*
 * Map a 2MB large page in current process page directory
 * Used for kernel initialization and efficient memory mapping
 */
int map_page_2mb(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (!current_page_dir || !current_page_dir->pml4) {
        kprint("map_page_2mb: No current page directory\n");
        return -1;
    }

    /* Ensure 2MB alignment for both virtual and physical addresses */
    if ((vaddr & (PAGE_SIZE_2MB - 1)) || (paddr & (PAGE_SIZE_2MB - 1))) {
        kprint("map_page_2mb: Address not 2MB aligned\n");
        return -1;
    }

    uint16_t pml4_idx = pml4_index(vaddr);
    uint16_t pdpt_idx = pdpt_index(vaddr);
    uint16_t pd_idx = pd_index(vaddr);

    /* Check PML4 entry exists */
    uint64_t pml4_entry = current_page_dir->pml4->entries[pml4_idx];
    if (!pte_present(pml4_entry)) {
        kprint("map_page_2mb: PML4 entry not present\n");
        return -1;
    }

    /* Get PDPT table */
    page_table_t *pdpt = (page_table_t*)pte_address(pml4_entry);
    if (!pdpt) {
        kprint("map_page_2mb: Invalid PDPT address\n");
        return -1;
    }

    uint64_t pdpt_entry = pdpt->entries[pdpt_idx];
    if (!pte_present(pdpt_entry)) {
        kprint("map_page_2mb: PDPT entry not present\n");
        return -1;
    }

    /* Get PD table */
    page_table_t *pd = (page_table_t*)pte_address(pdpt_entry);
    if (!pd) {
        kprint("map_page_2mb: Invalid PD address\n");
        return -1;
    }

    /* Create 2MB page entry with large page flag */
    pd->entries[pd_idx] = paddr | flags | PAGE_SIZE | PAGE_PRESENT;

    /* Invalidate TLB for this virtual address */
    invlpg(vaddr);

    return 0;
}

/*
 * Unmap a page from current process page directory
 * Supports 4KB, 2MB, and 1GB pages
 */
int unmap_page(uint64_t vaddr) {
    if (!current_page_dir || !current_page_dir->pml4) {
        kprint("unmap_page: No current page directory\n");
        return -1;
    }

    uint16_t pml4_idx = pml4_index(vaddr);
    uint16_t pdpt_idx = pdpt_index(vaddr);
    uint16_t pd_idx = pd_index(vaddr);
    uint16_t pt_idx = pt_index(vaddr);

    /* Traverse page tables to find the mapping */
    uint64_t pml4_entry = current_page_dir->pml4->entries[pml4_idx];
    if (!pte_present(pml4_entry)) {
        return 0;  /* Already unmapped */
    }

    page_table_t *pdpt = (page_table_t*)pte_address(pml4_entry);
    uint64_t pdpt_entry = pdpt->entries[pdpt_idx];
    if (!pte_present(pdpt_entry)) {
        return 0;  /* Already unmapped */
    }

    /* Check for 1GB huge page */
    if (pte_huge(pdpt_entry)) {
        pdpt->entries[pdpt_idx] = 0;
        invlpg(vaddr);
        return 0;
    }

    page_table_t *pd = (page_table_t*)pte_address(pdpt_entry);
    uint64_t pd_entry = pd->entries[pd_idx];
    if (!pte_present(pd_entry)) {
        return 0;  /* Already unmapped */
    }

    /* Check for 2MB large page */
    if (pte_huge(pd_entry)) {
        pd->entries[pd_idx] = 0;
        invlpg(vaddr);
        return 0;
    }

    /* 4KB page - clear PT entry */
    page_table_t *pt = (page_table_t*)pte_address(pd_entry);
    pt->entries[pt_idx] = 0;
    invlpg(vaddr);

    return 0;
}

/* ========================================================================
 * PROCESS PAGE DIRECTORY MANAGEMENT
 * ======================================================================== */

/*
 * Switch to a different process page directory
 * Used for process context switching
 */
int switch_page_directory(process_page_dir_t *page_dir) {
    if (!page_dir || !page_dir->pml4) {
        kprint("switch_page_directory: Invalid page directory\n");
        return -1;
    }

    /* Update CR3 to switch to new page directory */
    set_cr3(page_dir->pml4_phys);
    current_page_dir = page_dir;

    kprint("Switched to process page directory\n");
    return 0;
}

/*
 * Get current process page directory
 */
process_page_dir_t *get_current_page_directory(void) {
    return current_page_dir;
}

/* ========================================================================
 * SYSTEM INITIALIZATION
 * ======================================================================== */

/*
 * Initialize paging system during early kernel boot
 * Sets up process-centric paging infrastructure
 */
void init_paging(void) {
    kprint("Initializing paging system\n");

    /* Get current CR3 value set by bootloader */
    uint64_t cr3 = get_cr3();
    kernel_page_dir.pml4_phys = cr3 & ~0xFFF;

    /* Verify early page tables are properly set up */
    if ((uint64_t)kernel_page_dir.pml4 != (kernel_page_dir.pml4_phys)) {
        /* Update kernel page dir if CR3 points elsewhere */
        kernel_page_dir.pml4 = (page_table_t*)(kernel_page_dir.pml4_phys);
        kprint("Updated kernel PML4 pointer from CR3\n");
    }

    /* Verify higher-half kernel mapping exists */
    uint64_t kernel_phys = virt_to_phys(KERNEL_VIRTUAL_BASE);
    if (kernel_phys == 0) {
        kernel_panic("Higher-half kernel mapping not found");
    }

    kprint("Higher-half kernel mapping verified at ");
    kprint_hex(kernel_phys);
    kprint("\n");

    /* Verify identity mapping exists for early boot hardware access */
    uint64_t identity_phys = virt_to_phys(0x100000); /* 1MB mark */
    if (identity_phys == 0x100000) {
        kprint("Identity mapping verified\n");
    } else {
        kprint("Identity mapping not found (may be normal after early boot)\n");
    }

    kprint("Paging system initialized successfully\n");
}

/* ========================================================================
 * UTILITY AND QUERY FUNCTIONS
 * ======================================================================== */

/*
 * Get memory layout information for current mapping
 */
void get_memory_layout_info(uint64_t *kernel_virt_base, uint64_t *kernel_phys_base) {
    if (kernel_virt_base) {
        *kernel_virt_base = KERNEL_VIRTUAL_BASE;
    }

    if (kernel_phys_base) {
        *kernel_phys_base = virt_to_phys(KERNEL_VIRTUAL_BASE);
    }
}

/*
 * Check if a virtual address is currently mapped
 */
int is_mapped(uint64_t vaddr) {
    return virt_to_phys(vaddr) != 0;
}

/*
 * Get page size for a mapped virtual address
 * Returns page size in bytes, 0 if not mapped
 */
uint64_t get_page_size(uint64_t vaddr) {
    if (!current_page_dir || !current_page_dir->pml4) {
        return 0;
    }

    uint16_t pml4_idx = pml4_index(vaddr);
    uint16_t pdpt_idx = pdpt_index(vaddr);
    uint16_t pd_idx = pd_index(vaddr);

    /* Check PML4 entry */
    uint64_t pml4_entry = current_page_dir->pml4->entries[pml4_idx];
    if (!pte_present(pml4_entry)) {
        return 0;
    }

    /* Get PDPT table */
    page_table_t *pdpt = (page_table_t*)pte_address(pml4_entry);
    uint64_t pdpt_entry = pdpt->entries[pdpt_idx];
    if (!pte_present(pdpt_entry)) {
        return 0;
    }

    /* Check for 1GB huge page */
    if (pte_huge(pdpt_entry)) {
        return PAGE_SIZE_1GB;
    }

    /* Get PD table */
    page_table_t *pd = (page_table_t*)pte_address(pdpt_entry);
    uint64_t pd_entry = pd->entries[pd_idx];
    if (!pte_present(pd_entry)) {
        return 0;
    }

    /* Check for 2MB large page */
    if (pte_huge(pd_entry)) {
        return PAGE_SIZE_2MB;
    }

    /* Must be 4KB page if PT entry exists */
    return PAGE_SIZE_4KB;
}