/*
 * SlopOS Task Management
 * Basic task structures and task lifecycle management
 * Implements tasks as function pointers with allocated stacks
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../boot/debug.h"
#include "../drivers/serial.h"
#include "../mm/paging.h"
#include "task.h"

/* Forward declarations */
uint32_t create_process_vm(void);
int destroy_process_vm(uint32_t process_id);
int destroy_process_vma_space(uint32_t process_id);
uint64_t process_vm_alloc(uint32_t process_id, uint64_t size, uint32_t flags);
int process_vm_free(uint32_t process_id, uint64_t vaddr, uint64_t size);
void kernel_panic(const char *message);
process_page_dir_t *process_vm_get_page_dir(uint32_t process_id);

/* Task manager structure */
typedef struct task_manager {
    task_t tasks[MAX_TASKS];             /* Task pool */
    uint32_t num_tasks;                  /* Number of active tasks */
    uint32_t next_task_id;               /* Next task ID to assign */

    /* Current execution state */
    task_t *current_task;                /* Currently running task */
    task_t *idle_task;                   /* Idle task (always ready) */

    /* Ready queue */
    task_t *ready_queue_head;            /* Head of ready task queue */
    task_t *ready_queue_tail;            /* Tail of ready task queue */

    /* Statistics */
    uint64_t total_context_switches;     /* Total context switches performed */
    uint64_t total_yields;               /* Total voluntary yields */
    uint32_t tasks_created;              /* Total tasks created */
    uint32_t tasks_terminated;           /* Total tasks terminated */
} task_manager_t;

/* Global task manager instance */
static task_manager_t task_manager = {0};

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/*
 * Find task by task ID
 * Returns pointer to task, NULL if not found
 */
static task_t *find_task_by_id(uint32_t task_id) {
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (task_manager.tasks[i].task_id == task_id) {
            return &task_manager.tasks[i];
        }
    }
    return NULL;
}

/*
 * Find free task slot
 * Returns pointer to free task slot, NULL if none available
 */
static task_t *find_free_task_slot(void) {
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (task_manager.tasks[i].state == TASK_STATE_INVALID) {
            return &task_manager.tasks[i];
        }
    }
    return NULL;
}

/*
 * Initialize a task context for first execution
 */
static void init_task_context(task_t *task) {
    /* Clear all registers */
    task->context.rax = 0;
    task->context.rbx = 0;
    task->context.rcx = 0;
    task->context.rdx = 0;
    task->context.rsi = 0;
    task->context.rdi = (uint64_t)task->entry_arg;  /* First argument */
    task->context.rbp = 0;
    task->context.rsp = task->stack_pointer;
    task->context.r8 = 0;
    task->context.r9 = 0;
    task->context.r10 = 0;
    task->context.r11 = 0;
    task->context.r12 = 0;
    task->context.r13 = 0;
    task->context.r14 = 0;
    task->context.r15 = 0;

    /* Set instruction pointer to task entry point */
    task->context.rip = (uint64_t)task->entry_point;

    /* Set default flags register */
    task->context.rflags = 0x202;  /* IF=1 (interrupts enabled), reserved bit 1 */

    /* Set segment registers for kernel mode */
    if (task->flags & TASK_FLAG_KERNEL_MODE) {
        task->context.cs = GDT_CODE_SELECTOR;
        task->context.ds = GDT_DATA_SELECTOR;
        task->context.es = GDT_DATA_SELECTOR;
        task->context.fs = 0;
        task->context.gs = 0;
        task->context.ss = GDT_DATA_SELECTOR;
    }

    /* Page directory will be set by scheduler when switching */
    task->context.cr3 = 0;
}

/* ========================================================================
 * TASK LIFECYCLE MANAGEMENT
 * ======================================================================== */

/*
 * Create a new task
 * Returns task ID on success, INVALID_TASK_ID on failure
 */
uint32_t task_create(const char *name, task_entry_t entry_point, void *arg,
                     uint8_t priority, uint16_t flags) {
    if (!name || !entry_point) {
        kprint("task_create: Invalid parameters\n");
        return INVALID_TASK_ID;
    }

    if (task_manager.num_tasks >= MAX_TASKS) {
        kprint("task_create: Maximum tasks reached\n");
        return INVALID_TASK_ID;
    }

    /* Find free task slot */
    task_t *task = find_free_task_slot();
    if (!task) {
        kprint("task_create: No free task slots\n");
        return INVALID_TASK_ID;
    }

    /* Create process VM space for task */
    uint32_t process_id = create_process_vm();
    if (process_id == INVALID_PROCESS_ID) {
        kprint("task_create: Failed to create process VM\n");
        return INVALID_TASK_ID;
    }

    /* Allocate stack for task */
    uint64_t stack_base = process_vm_alloc(process_id, TASK_STACK_SIZE,
                                          0x02 | 0x08);  /* Write + User */
    if (!stack_base) {
        kprint("task_create: Failed to allocate stack\n");
        destroy_process_vm(process_id);
        return INVALID_TASK_ID;
    }

    /* Assign task ID */
    uint32_t task_id = task_manager.next_task_id++;

    /* Initialize task control block */
    task->task_id = task_id;

    /* Copy task name */
    const char *src = name;
    char *dst = task->name;
    for (int i = 0; i < TASK_NAME_MAX_LEN - 1 && *src; i++) {
        *dst++ = *src++;
    }
    *dst = '\0';

    task->state = TASK_STATE_READY;
    task->priority = priority;
    task->flags = flags;
    task->process_id = process_id;
    task->stack_base = stack_base;
    task->stack_size = TASK_STACK_SIZE;
    task->stack_pointer = stack_base + TASK_STACK_SIZE - 16;  /* 16-byte align */
    task->entry_point = entry_point;
    task->entry_arg = arg;
    task->time_slice = 10;  /* Default time slice */
    task->total_runtime = 0;
    task->creation_time = debug_get_timestamp();
    task->yield_count = 0;
    task->next = NULL;
    task->prev = NULL;

    /* Initialize CPU context */
    init_task_context(task);

    /* Record page directory for context switches */
    process_page_dir_t *page_dir = process_vm_get_page_dir(process_id);
    if (page_dir && page_dir->pml4_phys) {
        task->context.cr3 = page_dir->pml4_phys;
    }

    /* Update task manager */
    task_manager.num_tasks++;
    task_manager.tasks_created++;

    kprint("Created task '");
    kprint(name);
    kprint("' with ID ");
    kprint_decimal(task_id);
    kprint("\n");

    return task_id;
}

/*
 * Terminate a task and clean up resources
 */
int task_terminate(uint32_t task_id) {
    task_t *task = find_task_by_id(task_id);
    if (!task || task->state == TASK_STATE_INVALID) {
        kprint("task_terminate: Task not found\n");
        return -1;
    }

    kprint("Terminating task '");
    kprint(task->name);
    kprint("' (ID ");
    kprint_decimal(task_id);
    kprint(")\n");

    /* Mark task as terminated */
    task->state = TASK_STATE_TERMINATED;

    /* Free process VM space */
    if (task->process_id != INVALID_PROCESS_ID) {
        destroy_process_vm(task->process_id);
        destroy_process_vma_space(task->process_id);
    }

    /* Clear task control block */
    task->task_id = INVALID_TASK_ID;
    task->state = TASK_STATE_INVALID;
    task->process_id = INVALID_PROCESS_ID;
    task->next = NULL;
    task->prev = NULL;

    /* Update task manager */
    task_manager.num_tasks--;
    task_manager.tasks_terminated++;

    return 0;
}

/*
 * Terminate all tasks except the current one
 * Used during shutdown sequences to release task resources
 */
int task_shutdown_all(void) {
    int result = 0;
    task_t *current = task_manager.current_task;

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        task_t *task = &task_manager.tasks[i];

        if (task->state == TASK_STATE_INVALID) {
            continue;
        }

        if (task == current) {
            continue;
        }

        if (task->task_id == INVALID_TASK_ID) {
            continue;
        }

        if (task_terminate(task->task_id) != 0) {
            result = -1;
        }
    }

    task_manager.num_tasks = 0;

    return result;
}

/*
 * Get task information
 */
int task_get_info(uint32_t task_id, task_t **task_info) {
    if (!task_info) {
        return -1;
    }

    task_t *task = find_task_by_id(task_id);
    if (!task || task->state == TASK_STATE_INVALID) {
        *task_info = NULL;
        return -1;
    }

    *task_info = task;
    return 0;
}

/*
 * Change task state
 */
int task_set_state(uint32_t task_id, uint8_t new_state) {
    task_t *task = find_task_by_id(task_id);
    if (!task || task->state == TASK_STATE_INVALID) {
        return -1;
    }

    uint8_t old_state = task->state;
    task->state = new_state;

    kprint("Task ");
    kprint_decimal(task_id);
    kprint(" state: ");
    kprint_decimal(old_state);
    kprint(" -> ");
    kprint_decimal(new_state);
    kprint("\n");

    return 0;
}

/* ========================================================================
 * INITIALIZATION AND QUERY FUNCTIONS
 * ======================================================================== */

/*
 * Initialize the task management system
 */
int init_task_manager(void) {
    kprint("Initializing task management system\n");

    kprint("Setting num_tasks\n");
    volatile uint32_t *num_tasks_ptr = &task_manager.num_tasks;
    *num_tasks_ptr = 0;
    
    kprint("Setting next_task_id\n");
    volatile uint32_t *next_id_ptr = &task_manager.next_task_id;
    *next_id_ptr = 1;
    
    kprint("Setting current_task\n");
    task_manager.current_task = NULL;
    
    kprint("Setting idle_task\n");
    task_manager.idle_task = NULL;
    
    kprint("Setting ready_queue pointers\n");
    task_manager.ready_queue_head = NULL;
    task_manager.ready_queue_tail = NULL;
    
    /* NOTE: Statistics fields and task array are already zero-initialized
     * because task_manager is a static global with = {0} initializer.
     * We skip explicit initialization to avoid compiler-generated instructions
     * that cause "Invalid Opcode" exceptions. */

    kprint("Task manager structure ready (fields zero-initialized)\n");
    
    /* Skip clearing task array for now - it seems to cause issues
     * The array is already zero-initialized as a static global */
    kprint("Skipping task array clearing (already zero-initialized)\n");

    kprint("Task management system initialized\n");
    return 0;
}

/*
 * Get task manager statistics
 */
void get_task_stats(uint32_t *total_tasks, uint32_t *active_tasks,
                   uint64_t *context_switches) {
    if (total_tasks) {
        *total_tasks = task_manager.tasks_created;
    }
    if (active_tasks) {
        *active_tasks = task_manager.num_tasks;
    }
    if (context_switches) {
        *context_switches = task_manager.total_context_switches;
    }
}

/*
 * Get current task ID
 */
uint32_t task_get_current_id(void) {
    if (task_manager.current_task) {
        return task_manager.current_task->task_id;
    }
    return 0;  /* Kernel/no task */
}

/*
 * Get current task structure
 */
task_t *task_get_current(void) {
    return task_manager.current_task;
}

/*
 * Set current task (used by scheduler)
 */
void task_set_current(task_t *task) {
    task_manager.current_task = task;
    if (task) {
        task->state = TASK_STATE_RUNNING;
    }
}
