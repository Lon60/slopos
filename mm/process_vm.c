/*
 * SlopOS Memory Management - Per-Process Virtual Memory Management
 * Manages virtual memory spaces for individual processes
 * Handles process creation, destruction, and memory isolation
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
 * PROCESS VIRTUAL MEMORY CONSTANTS
 * ======================================================================== */

/* Process virtual memory layout */
#define PROCESS_CODE_START            0x400000        /* Process code segment (4MB) */
#define PROCESS_DATA_START            0x800000        /* Process data segment (8MB) */
#define PROCESS_HEAP_START            0x1000000       /* Process heap start (16MB) */
#define PROCESS_HEAP_MAX              0x40000000      /* Maximum heap size (1GB) */
#define PROCESS_STACK_TOP             0x7FFFFF000000ULL /* User stack top */
#define PROCESS_STACK_SIZE            0x100000        /* Default stack size (1MB) */

/* Process limits are defined in boot/constants.h */

/* Process memory allocation flags */
#define VM_FLAG_READ                  0x01   /* Page is readable */
#define VM_FLAG_WRITE                 0x02   /* Page is writable */
#define VM_FLAG_EXEC                  0x04   /* Page is executable */
#define VM_FLAG_USER                  0x08   /* Page accessible from user mode */
#define VM_FLAG_SHARED                0x10   /* Page is shared between processes */

/* ========================================================================
 * PROCESS VIRTUAL MEMORY STRUCTURES
 * ======================================================================== */

/* Virtual memory area descriptor */
typedef struct vm_area {
    uint64_t start_addr;          /* Start virtual address */
    uint64_t end_addr;            /* End virtual address */
    uint32_t flags;               /* Access flags (read/write/exec) */
    uint32_t ref_count;           /* Reference count for sharing */
    struct vm_area *next;         /* Next VMA in process */
} vm_area_t;

/* Page table structure reference (from paging.c) */
typedef struct {
    uint64_t entries[ENTRIES_PER_PAGE_TABLE];
} __attribute__((aligned(PAGE_ALIGN))) page_table_t;

/* Process page directory structure (from paging.c) */
typedef struct process_page_dir {
    page_table_t *pml4;                    /* Process PML4 table */
    uint64_t pml4_phys;                    /* Physical address of PML4 */
    uint32_t ref_count;                    /* Reference count for sharing */
    uint32_t process_id;                   /* Process ID for debugging */
    struct process_page_dir *next;         /* Link for process list */
} process_page_dir_t;

/* Process virtual memory descriptor */
typedef struct process_vm {
    uint32_t process_id;          /* Unique process identifier */
    process_page_dir_t *page_dir; /* Process page directory */
    vm_area_t *vma_list;          /* List of virtual memory areas */
    uint64_t code_start;          /* Process code segment start */
    uint64_t data_start;          /* Process data segment start */
    uint64_t heap_start;          /* Process heap start */
    uint64_t heap_end;            /* Current heap end */
    uint64_t stack_start;         /* Process stack start */
    uint64_t stack_end;           /* Process stack end */
    uint32_t total_pages;         /* Total allocated pages */
    uint32_t flags;               /* Process VM flags */
    struct process_vm *next;      /* Next process in global list */
} process_vm_t;

/* Global process VM manager */
typedef struct vm_manager {
    process_vm_t processes[MAX_PROCESSES];  /* Process VM descriptors */
    uint32_t num_processes;                 /* Number of active processes */
    uint32_t next_process_id;               /* Next process ID to assign */
    process_vm_t *active_process;           /* Currently active process */
    process_vm_t *process_list;             /* Head of process list */
} vm_manager_t;

/* Global VM manager instance */
static vm_manager_t vm_manager = {0};

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/*
 * Allocate a new VMA structure
 * Returns pointer to VMA, NULL on failure
 */
static vm_area_t *alloc_vma(void) {
    /* For now, use static allocation from a pool */
    /* TODO: Implement dynamic allocation once kernel heap is ready */
    static vm_area_t vma_pool[MAX_PROCESSES * 8];
    static uint32_t vma_pool_index = 0;

    if (vma_pool_index >= (MAX_PROCESSES * 8)) {
        kprint("alloc_vma: VMA pool exhausted\n");
        return NULL;
    }

    vm_area_t *vma = &vma_pool[vma_pool_index++];
    vma->start_addr = 0;
    vma->end_addr = 0;
    vma->flags = 0;
    vma->ref_count = 1;
    vma->next = NULL;

    return vma;
}

/*
 * Free a VMA structure
 */
static void free_vma(vm_area_t *vma) {
    if (!vma) return;

    /* For static allocation, just mark as unused */
    vma->ref_count = 0;
}

/*
 * Find process VM descriptor by process ID
 */
static process_vm_t *find_process_vm(uint32_t process_id) {
    for (uint32_t i = 0; i < vm_manager.num_processes; i++) {
        if (vm_manager.processes[i].process_id == process_id) {
            return &vm_manager.processes[i];
        }
    }
    return NULL;
}

/* ========================================================================
 * VIRTUAL MEMORY AREA MANAGEMENT
 * ======================================================================== */

/*
 * Add a virtual memory area to a process
 */
static int add_vma_to_process(process_vm_t *process, uint64_t start, uint64_t end, uint32_t flags) {
    if (!process || start >= end) {
        return -1;
    }

    vm_area_t *vma = alloc_vma();
    if (!vma) {
        kprint("add_vma_to_process: Failed to allocate VMA\n");
        return -1;
    }

    vma->start_addr = start;
    vma->end_addr = end;
    vma->flags = flags;

    /* Add to process VMA list */
    vma->next = process->vma_list;
    process->vma_list = vma;

    return 0;
}

/*
 * Remove a virtual memory area from a process
 */
static int remove_vma_from_process(process_vm_t *process, uint64_t start, uint64_t end) {
    if (!process) {
        return -1;
    }

    vm_area_t **current = &process->vma_list;

    while (*current) {
        vm_area_t *vma = *current;

        if (vma->start_addr == start && vma->end_addr == end) {
            /* Remove from list */
            *current = vma->next;
            free_vma(vma);
            return 0;
        }

        current = &vma->next;
    }

    return -1;  /* VMA not found */
}

/*
 * Check if a virtual address range is valid for a process
 */
static int is_valid_user_range(uint64_t start, uint64_t end) {
    /* Ensure addresses are in user space */
    if (start >= KERNEL_VIRTUAL_BASE || end >= KERNEL_VIRTUAL_BASE) {
        return 0;
    }

    /* Ensure addresses are page-aligned */
    if ((start & (PAGE_SIZE_4KB - 1)) || (end & (PAGE_SIZE_4KB - 1))) {
        return 0;
    }

    /* Ensure valid range */
    return start < end;
}

/* ========================================================================
 * PROCESS CREATION AND DESTRUCTION
 * ======================================================================== */

/*
 * Create a new process virtual memory space
 * Returns process ID, INVALID_PROCESS_ID on failure
 */
uint32_t create_process_vm(void) {
    if (vm_manager.num_processes >= MAX_PROCESSES) {
        kprint("create_process_vm: Maximum processes reached\n");
        return INVALID_PROCESS_ID;
    }

    /* Allocate new page directory */
    uint64_t pml4_phys = alloc_page_frame(0);
    if (!pml4_phys) {
        kprint("create_process_vm: Failed to allocate PML4\n");
        return INVALID_PROCESS_ID;
    }

    /* Find free process slot */
    process_vm_t *process = &vm_manager.processes[vm_manager.num_processes];
    uint32_t process_id = vm_manager.next_process_id++;

    /* Initialize process VM descriptor */
    process->process_id = process_id;
    process->page_dir = NULL;  /* TODO: Create process_page_dir_t */
    process->vma_list = NULL;
    process->code_start = PROCESS_CODE_START;
    process->data_start = PROCESS_DATA_START;
    process->heap_start = PROCESS_HEAP_START;
    process->heap_end = PROCESS_HEAP_START;
    process->stack_start = PROCESS_STACK_TOP - PROCESS_STACK_SIZE;
    process->stack_end = PROCESS_STACK_TOP;
    process->total_pages = 1;  /* PML4 page */
    process->flags = 0;
    process->next = vm_manager.process_list;

    /* Add standard VMA regions */
    add_vma_to_process(process, process->code_start, process->data_start,
                       VM_FLAG_READ | VM_FLAG_EXEC | VM_FLAG_USER);
    add_vma_to_process(process, process->data_start, process->heap_start,
                       VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_USER);
    add_vma_to_process(process, process->stack_start, process->stack_end,
                       VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_USER);

    /* Add to global list */
    vm_manager.process_list = process;
    vm_manager.num_processes++;

    kprint("Created process VM space for PID ");
    kprint_decimal(process_id);
    kprint("\n");

    return process_id;
}

/*
 * Destroy a process virtual memory space
 * Frees all allocated pages and removes from system
 */
int destroy_process_vm(uint32_t process_id) {
    process_vm_t *process = find_process_vm(process_id);
    if (!process) {
        kprint("destroy_process_vm: Process not found\n");
        return -1;
    }

    kprint("Destroying process VM space for PID ");
    kprint_decimal(process_id);
    kprint("\n");

    /* Free all VMAs */
    vm_area_t *vma = process->vma_list;
    while (vma) {
        vm_area_t *next = vma->next;
        free_vma(vma);
        vma = next;
    }

    /* Free page directory physical page */
    if (process->page_dir && process->page_dir->pml4_phys) {
        free_page_frame(process->page_dir->pml4_phys);
    }

    /* Remove from process list */
    if (vm_manager.process_list == process) {
        vm_manager.process_list = process->next;
    } else {
        process_vm_t *current = vm_manager.process_list;
        while (current && current->next != process) {
            current = current->next;
        }
        if (current) {
            current->next = process->next;
        }
    }

    /* Mark process slot as free */
    process->process_id = INVALID_PROCESS_ID;
    vm_manager.num_processes--;

    return 0;
}

/* ========================================================================
 * PROCESS MEMORY OPERATIONS
 * ======================================================================== */

/*
 * Allocate virtual memory for a process
 * Returns virtual address, 0 on failure
 */
uint64_t process_vm_alloc(uint32_t process_id, uint64_t size, uint32_t flags) {
    process_vm_t *process = find_process_vm(process_id);
    if (!process) {
        return 0;
    }

    /* Align size to page boundary */
    size = (size + PAGE_SIZE_4KB - 1) & ~(PAGE_SIZE_4KB - 1);

    /* For now, allocate from heap area */
    uint64_t start_addr = process->heap_end;
    uint64_t end_addr = start_addr + size;

    if (end_addr > PROCESS_HEAP_MAX) {
        kprint("process_vm_alloc: Heap overflow\n");
        return 0;
    }

    /* Update heap end */
    process->heap_end = end_addr;

    /* Add VMA for this allocation */
    if (add_vma_to_process(process, start_addr, end_addr, flags) != 0) {
        process->heap_end = start_addr;  /* Rollback */
        return 0;
    }

    return start_addr;
}

/*
 * Free virtual memory for a process
 */
int process_vm_free(uint32_t process_id, uint64_t vaddr, uint64_t size) {
    process_vm_t *process = find_process_vm(process_id);
    if (!process) {
        return -1;
    }

    /* Align to page boundary */
    uint64_t start = vaddr & ~(PAGE_SIZE_4KB - 1);
    uint64_t end = (vaddr + size + PAGE_SIZE_4KB - 1) & ~(PAGE_SIZE_4KB - 1);

    /* Remove VMA */
    return remove_vma_from_process(process, start, end);
}

/* ========================================================================
 * INITIALIZATION AND QUERY FUNCTIONS
 * ======================================================================== */

/*
 * Initialize the process virtual memory manager
 */
int init_process_vm(void) {
    kprint("Initializing process virtual memory manager\n");

    vm_manager.num_processes = 0;
    vm_manager.next_process_id = 1;  /* Start from 1, 0 is kernel */
    vm_manager.active_process = NULL;
    vm_manager.process_list = NULL;

    /* Initialize all process slots */
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        vm_manager.processes[i].process_id = INVALID_PROCESS_ID;
        vm_manager.processes[i].page_dir = NULL;
        vm_manager.processes[i].vma_list = NULL;
        vm_manager.processes[i].total_pages = 0;
        vm_manager.processes[i].flags = 0;
        vm_manager.processes[i].next = NULL;
    }

    kprint("Process VM manager initialized\n");
    return 0;
}

/*
 * Get process VM statistics
 */
void get_process_vm_stats(uint32_t *total_processes, uint32_t *active_processes) {
    if (total_processes) {
        *total_processes = MAX_PROCESSES;
    }
    if (active_processes) {
        *active_processes = vm_manager.num_processes;
    }
}

/*
 * Get current active process ID
 */
uint32_t get_current_process_id(void) {
    if (vm_manager.active_process) {
        return vm_manager.active_process->process_id;
    }
    return 0;  /* Kernel process */
}
