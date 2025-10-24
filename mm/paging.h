/*
 * SlopOS Memory Management - Paging Shared Definitions
 * Exposes paging data structures and helpers across the kernel
 */

#ifndef MM_PAGING_H
#define MM_PAGING_H

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"

/* Page table structure - aligned as required by x86_64 */
typedef struct {
    uint64_t entries[ENTRIES_PER_PAGE_TABLE];
} __attribute__((aligned(PAGE_ALIGN))) page_table_t;

/* Process page directory structure shared between paging and process VM */
typedef struct process_page_dir {
    page_table_t *pml4;                    /* Process PML4 table */
    uint64_t pml4_phys;                    /* Physical address of PML4 */
    uint32_t ref_count;                    /* Reference count for sharing */
    uint32_t process_id;                   /* Process ID for debugging */
    struct process_page_dir *next;         /* Link for process list */
} process_page_dir_t;

/* Copy higher-half kernel mappings into a fresh PML4 */
void paging_copy_kernel_mappings(page_table_t *dest_pml4);

#endif /* MM_PAGING_H */

