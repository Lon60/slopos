/*
 * SlopOS Memory Management - Early Bootstrap Paging Setup
 * Initializes basic paging structures during early kernel boot
 * Sets up identity mapping and higher-half kernel mapping
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"

/* Forward declarations */
void kernel_panic(const char *message);

/* ========================================================================
 * EARLY PAGING CONSTANTS
 * ======================================================================== */

/* Early boot memory regions that need identity mapping */
#define EARLY_IDENTITY_START          0x00000000      /* Start of identity map */
#define EARLY_IDENTITY_SIZE           0x200000        /* 2MB of low memory identity mapping */

/* GRUB-loaded kernel physical location (discovered at runtime) */
#define GRUB_KERNEL_REGION_START      0x1B000000      /* ~432MB - typical GRUB load address */
#define GRUB_KERNEL_REGION_SIZE       0x01000000      /* 16MB region for kernel */

/* Stack region that needs identity mapping */
#define STACK_REGION_START            0x1FE90000      /* ~510MB - typical stack region */
#define STACK_REGION_SIZE             0x00010000      /* 64KB stack region */

/* Traditional kernel addresses (for reference) */
#define KERNEL_PHYSICAL_START         0x100000        /* 1MB - traditional load address */
#define KERNEL_PHYSICAL_SIZE          0x100000        /* 1MB initial kernel size */

/* Page table manipulation macros */
#define PTE_ADDR_MASK                 0x000FFFFFFFFFF000ULL
#define PTE_FLAGS_MASK                0xFFF0000000000FFFULL

/* External symbols from linker script */
extern uint64_t _kernel_start;
extern uint64_t _kernel_end;
extern uint64_t early_pml4;
extern uint64_t early_pdpt;
extern uint64_t early_pd_identity;
extern uint64_t early_pd_kernel;

/* ========================================================================
 * PAGE TABLE STRUCTURES
 * ======================================================================== */

/* Page table entry structure */
typedef struct {
    uint64_t entries[ENTRIES_PER_PAGE_TABLE];
} __attribute__((aligned(PAGE_ALIGN))) page_table_t;

/* Early boot paging state */
typedef struct early_paging {
    page_table_t *pml4;           /* PML4 table pointer */
    page_table_t *pdpt;           /* PDPT table pointer */
    page_table_t *pd_identity;    /* Identity mapping PD table pointer */
    page_table_t *pd_kernel;      /* Higher-half kernel PD table pointer */
    uint64_t pml4_phys;           /* Physical address of PML4 */
    uint64_t pdpt_phys;           /* Physical address of PDPT */
    uint64_t pd_identity_phys;    /* Physical address of identity PD */
    uint64_t pd_kernel_phys;      /* Physical address of kernel PD */
    uint32_t initialized;         /* Initialization flag */
} early_paging_t;

/* Global early paging state */
static early_paging_t early_paging = {0};

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/*
 * Create page table entry with specified physical address and flags
 */
static inline uint64_t create_pte(uint64_t phys_addr, uint64_t flags) {
    return (phys_addr & PTE_ADDR_MASK) | (flags & PTE_FLAGS_MASK);
}

/*
 * Extract physical address from page table entry
 */
static inline uint64_t extract_pte_addr(uint64_t pte) {
    return pte & PTE_ADDR_MASK;
}

/*
 * Zero out a page table
 */
static void zero_page_table(page_table_t *table) {
    if (!table) {
        return;
    }

    for (int i = 0; i < ENTRIES_PER_PAGE_TABLE; i++) {
        table->entries[i] = 0;
    }
}

/*
 * Verify page table alignment
 */
static int verify_page_table_alignment(void *table) {
    uint64_t addr = (uint64_t)table;
    return (addr & (PAGE_ALIGN - 1)) == 0;
}

/* ========================================================================
 * EARLY PAGE TABLE SETUP
 * ======================================================================== */

/*
 * Initialize early page table pointers from linker symbols
 */
static int init_early_page_tables(void) {
    kprint("Initializing early page table pointers\n");

    /* Get pointers to early page tables from linker */
    early_paging.pml4 = (page_table_t*)&early_pml4;
    early_paging.pdpt = (page_table_t*)&early_pdpt;
    early_paging.pd_identity = (page_table_t*)&early_pd_identity;
    early_paging.pd_kernel = (page_table_t*)&early_pd_kernel;

    /* Calculate physical addresses */
    early_paging.pml4_phys = (uint64_t)early_paging.pml4;
    early_paging.pdpt_phys = (uint64_t)early_paging.pdpt;
    early_paging.pd_identity_phys = (uint64_t)early_paging.pd_identity;
    early_paging.pd_kernel_phys = (uint64_t)early_paging.pd_kernel;

    /* Verify alignment */
    if (!verify_page_table_alignment(early_paging.pml4) ||
        !verify_page_table_alignment(early_paging.pdpt) ||
        !verify_page_table_alignment(early_paging.pd_identity) ||
        !verify_page_table_alignment(early_paging.pd_kernel)) {
        kprint("ERROR: Page tables not properly aligned\n");
        return -1;
    }

    kprint("Early page tables located:\n");
    kprint("  PML4: ");
    kprint_hex(early_paging.pml4_phys);
    kprint("\n");
    kprint("  PDPT: ");
    kprint_hex(early_paging.pdpt_phys);
    kprint("\n");
    kprint("  PD Identity: ");
    kprint_hex(early_paging.pd_identity_phys);
    kprint("\n");
    kprint("  PD Kernel: ");
    kprint_hex(early_paging.pd_kernel_phys);
    kprint("\n");

    return 0;
}

/*
 * Set up identity mapping for early boot
 * Maps critical memory regions where GRUB loaded the kernel and stack
 */
static int setup_identity_mapping(void) {
    kprint("Setting up identity mapping for early boot\n");

    /* Zero out page tables */
    zero_page_table(early_paging.pml4);
    zero_page_table(early_paging.pdpt);
    zero_page_table(early_paging.pd_identity);
    zero_page_table(early_paging.pd_kernel);

    /* PML4[0] -> PDPT (for identity mapping) */
    early_paging.pml4->entries[0] = create_pte(early_paging.pdpt_phys,
                                               PAGE_PRESENT | PAGE_WRITABLE);

    /* PDPT[0] -> Identity PD (for first 1GB identity mapping) */
    early_paging.pdpt->entries[0] = create_pte(early_paging.pd_identity_phys,
                                               PAGE_PRESENT | PAGE_WRITABLE);

    /* Map first 2MB using large pages in identity PD (for low memory) */
    early_paging.pd_identity->entries[0] = create_pte(0x000000,
                                                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE);
    kprint("Identity mapped: 0x0 - 0x200000 (2MB low memory)\n");

    /* Map GRUB kernel region using 2MB large pages */
    uint64_t grub_start_2mb = GRUB_KERNEL_REGION_START & ~(PAGE_SIZE_2MB - 1);  /* Align to 2MB */
    uint32_t grub_pages = (GRUB_KERNEL_REGION_SIZE + PAGE_SIZE_2MB - 1) / PAGE_SIZE_2MB;
    uint32_t grub_pd_start = grub_start_2mb / PAGE_SIZE_2MB;

    for (uint32_t i = 0; i < grub_pages; i++) {
        uint32_t pd_idx = grub_pd_start + i;
        if (pd_idx >= ENTRIES_PER_PAGE_TABLE) {
            kprint("ERROR: GRUB region too large for single PD\n");
            return -1;
        }
        early_paging.pd_identity->entries[pd_idx] = create_pte(grub_start_2mb + (i * PAGE_SIZE_2MB),
                                                                PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE);
    }

    kprint("Identity mapped GRUB region: ");
    kprint_hex(grub_start_2mb);
    kprint(" - ");
    kprint_hex(grub_start_2mb + (grub_pages * PAGE_SIZE_2MB));
    kprint(" (");
    kprint_decimal(grub_pages);
    kprint(" * 2MB pages)\n");

    /* Map stack region using 2MB large pages */
    uint64_t stack_start_2mb = STACK_REGION_START & ~(PAGE_SIZE_2MB - 1);  /* Align to 2MB */
    uint32_t stack_pages = (STACK_REGION_SIZE + PAGE_SIZE_2MB - 1) / PAGE_SIZE_2MB;
    uint32_t stack_pd_start = stack_start_2mb / PAGE_SIZE_2MB;

    for (uint32_t i = 0; i < stack_pages; i++) {
        uint32_t pd_idx = stack_pd_start + i;
        if (pd_idx >= ENTRIES_PER_PAGE_TABLE) {
            kprint("ERROR: Stack region too large for single PD\n");
            return -1;
        }
        early_paging.pd_identity->entries[pd_idx] = create_pte(stack_start_2mb + (i * PAGE_SIZE_2MB),
                                                                PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE);
    }

    kprint("Identity mapped stack region: ");
    kprint_hex(stack_start_2mb);
    kprint(" - ");
    kprint_hex(stack_start_2mb + (stack_pages * PAGE_SIZE_2MB));
    kprint(" (");
    kprint_decimal(stack_pages);
    kprint(" * 2MB pages)\n");

    return 0;
}

/*
 * Set up higher-half kernel mapping
 * Maps kernel to higher-half virtual address space
 */
static int setup_kernel_mapping(void) {
    kprint("Setting up higher-half kernel mapping\n");

    /* Calculate kernel mapping indices */
    uint16_t pml4_idx = (KERNEL_VIRTUAL_BASE >> 39) & 0x1FF;
    uint16_t pdpt_idx = (KERNEL_VIRTUAL_BASE >> 30) & 0x1FF;
    uint16_t pd_idx = (KERNEL_VIRTUAL_BASE >> 21) & 0x1FF;

    kprint("Kernel mapping indices: PML4[");
    kprint_decimal(pml4_idx);
    kprint("], PDPT[");
    kprint_decimal(pdpt_idx);
    kprint("], PD[");
    kprint_decimal(pd_idx);
    kprint("]\n");

    /* We'll use the same PDPT and PD for simplicity in early boot */
    /* PML4[511] -> PDPT (for kernel higher-half) */
    early_paging.pml4->entries[pml4_idx] = create_pte(early_paging.pdpt_phys,
                                                      PAGE_PRESENT | PAGE_WRITABLE);

    /* PDPT[510] -> Kernel PD (for kernel region) */
    early_paging.pdpt->entries[pdpt_idx] = create_pte(early_paging.pd_kernel_phys,
                                                      PAGE_PRESENT | PAGE_WRITABLE);

    /* Map kernel using 2MB large pages */
    /* Start at 1MB physical, map to higher-half virtual */
    uint64_t kernel_phys = KERNEL_PHYSICAL_START;
    uint32_t kernel_pages = (KERNEL_PHYSICAL_SIZE + PAGE_SIZE_2MB - 1) / PAGE_SIZE_2MB;

    for (uint32_t i = 0; i < kernel_pages; i++) {
        uint32_t pd_entry = pd_idx + i;
        if (pd_entry >= ENTRIES_PER_PAGE_TABLE) {
            kprint("ERROR: Kernel too large for single PD\n");
            return -1;
        }

        early_paging.pd_kernel->entries[pd_entry] = create_pte(kernel_phys + (i * PAGE_SIZE_2MB),
                                                                PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE);
    }

    kprint("Higher-half mapping: ");
    kprint_hex(KERNEL_VIRTUAL_BASE);
    kprint(" -> ");
    kprint_hex(kernel_phys);
    kprint(" (");
    kprint_decimal(kernel_pages);
    kprint(" * 2MB pages)\n");

    return 0;
}

/*
 * Verify page table setup correctness
 */
static int verify_page_tables(void) {
    kprint("Verifying page table setup\n");

    /* Check PML4[0] for identity mapping */
    uint64_t pml4_entry0 = early_paging.pml4->entries[0];
    if (!(pml4_entry0 & PAGE_PRESENT)) {
        kprint("ERROR: PML4[0] not present for identity mapping\n");
        return -1;
    }

    if (extract_pte_addr(pml4_entry0) != early_paging.pdpt_phys) {
        kprint("ERROR: PML4[0] points to wrong PDPT\n");
        return -1;
    }

    /* Check PML4[511] for kernel mapping */
    uint16_t kernel_pml4_idx = (KERNEL_VIRTUAL_BASE >> 39) & 0x1FF;
    uint64_t pml4_kernel = early_paging.pml4->entries[kernel_pml4_idx];
    if (!(pml4_kernel & PAGE_PRESENT)) {
        kprint("ERROR: Kernel PML4 entry not present\n");
        return -1;
    }

    /* Verify GRUB region identity mapping */
    uint32_t grub_pd_idx = GRUB_KERNEL_REGION_START / PAGE_SIZE_2MB;
    uint64_t grub_pd_entry = early_paging.pd_identity->entries[grub_pd_idx];
    if (!(grub_pd_entry & PAGE_PRESENT) || !(grub_pd_entry & PAGE_SIZE)) {
        kprint("ERROR: GRUB region identity mapping missing\n");
        return -1;
    }

    /* Verify stack region identity mapping */
    uint32_t stack_pd_idx = STACK_REGION_START / PAGE_SIZE_2MB;
    uint64_t stack_pd_entry = early_paging.pd_identity->entries[stack_pd_idx];
    if (!(stack_pd_entry & PAGE_PRESENT) || !(stack_pd_entry & PAGE_SIZE)) {
        kprint("ERROR: Stack region identity mapping missing\n");
        return -1;
    }

    /* Check PDPT entries */
    uint64_t pdpt_entry0 = early_paging.pdpt->entries[0];
    if (!(pdpt_entry0 & PAGE_PRESENT)) {
        kprint("ERROR: PDPT[0] not present\n");
        return -1;
    }

    /* Check identity PD entries */
    uint64_t pd_identity_entry0 = early_paging.pd_identity->entries[0];
    if (!(pd_identity_entry0 & PAGE_PRESENT) || !(pd_identity_entry0 & PAGE_SIZE)) {
        kprint("ERROR: Identity mapping PD[0] incorrect\n");
        return -1;
    }

    kprint("Page table verification passed\n");
    return 0;
}

/* ========================================================================
 * EARLY PAGING ACTIVATION
 * ======================================================================== */

/*
 * Load CR3 with PML4 address to activate paging
 */
static void activate_paging(void) {
    kprint("Activating early paging with CR3 = ");
    kprint_hex(early_paging.pml4_phys);
    kprint("\n");

    /* Load PML4 address into CR3 */
    __asm__ volatile (
        "movq %0, %%cr3"
        :
        : "r" (early_paging.pml4_phys)
        : "memory"
    );

    kprint("Early paging activated successfully\n");
}

/*
 * Test basic virtual memory functionality
 */
static int test_virtual_memory(void) {
    kprint("Testing virtual memory functionality\n");

    /* Test identity mapping - write to low memory */
    volatile uint32_t *test_ptr = (volatile uint32_t*)0x7C00;  /* Safe area */
    uint32_t original_value = *test_ptr;
    uint32_t test_value = 0xDEADBEEF;

    *test_ptr = test_value;
    if (*test_ptr != test_value) {
        kprint("ERROR: Identity mapping write test failed\n");
        return -1;
    }

    *test_ptr = original_value;  /* Restore */

    /* Test higher-half mapping - read kernel memory */
    volatile uint32_t *kernel_ptr = (volatile uint32_t*)KERNEL_VIRTUAL_BASE;
    uint32_t kernel_value = *kernel_ptr;  /* Should not fault */

    kprint("Virtual memory tests passed\n");
    kprint("  Identity test: ");
    kprint_hex((uint64_t)test_ptr);
    kprint(" -> ");
    kprint_hex(test_value);
    kprint("\n");
    kprint("  Kernel test: ");
    kprint_hex((uint64_t)kernel_ptr);
    kprint(" -> ");
    kprint_hex(kernel_value);
    kprint("\n");

    return 0;
}

/* ========================================================================
 * MAIN INITIALIZATION FUNCTION
 * ======================================================================== */

/*
 * Initialize early paging system
 * Sets up minimal paging for kernel boot
 */
int init_early_paging(void) {
    kprint("=== Early Paging Initialization ===\n");

    /* Initialize page table pointers */
    if (init_early_page_tables() != 0) {
        kernel_panic("Failed to initialize early page table pointers");
    }

    /* Set up identity mapping for early boot */
    if (setup_identity_mapping() != 0) {
        kernel_panic("Failed to setup identity mapping");
    }

    /* Set up higher-half kernel mapping */
    if (setup_kernel_mapping() != 0) {
        kernel_panic("Failed to setup kernel mapping");
    }

    /* Verify page table correctness */
    if (verify_page_tables() != 0) {
        kernel_panic("Page table verification failed");
    }

    /* Activate paging by loading CR3 */
    activate_paging();

    /* Test virtual memory functionality */
    if (test_virtual_memory() != 0) {
        kernel_panic("Virtual memory tests failed");
    }

    early_paging.initialized = 1;

    kprint("=== Early Paging Initialization Complete ===\n");
    return 0;
}

/*
 * Get early paging information
 */
void get_early_paging_info(uint64_t *pml4_phys, uint64_t *kernel_virt_base) {
    if (pml4_phys) {
        *pml4_phys = early_paging.pml4_phys;
    }
    if (kernel_virt_base) {
        *kernel_virt_base = KERNEL_VIRTUAL_BASE;
    }
}

/*
 * Check if early paging is initialized
 */
int is_early_paging_initialized(void) {
    return early_paging.initialized;
}
