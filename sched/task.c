/*
 * SlopOS Task Management
 * Basic task structures and task lifecycle management
 * Implements tasks as function pointers with allocated stacks
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"

/* Forward declarations */
uint32_t create_process_vm(void);
int destroy_process_vm(uint32_t process_id);
uint64_t process_vm_alloc(uint32_t process_id, uint64_t size, uint32_t flags);
int process_vm_free(uint32_t process_id, uint64_t vaddr, uint64_t size);
void kernel_panic(const char *message);

/* ========================================================================
 * TASK CONSTANTS
 * ======================================================================== */

#define MAX_TASKS                     32        /* Maximum number of tasks */
#define TASK_STACK_SIZE               0x8000    /* 32KB default stack size */
#define TASK_NAME_MAX_LEN             32        /* Maximum task name length */
#define INVALID_TASK_ID               0xFFFFFFFF /* Invalid task ID */

/* Task states */
#define TASK_STATE_INVALID            0   /* Task slot not in use */
#define TASK_STATE_READY              1   /* Task ready to run */
#define TASK_STATE_RUNNING            2   /* Task currently executing */
#define TASK_STATE_BLOCKED            3   /* Task blocked waiting for resource */
#define TASK_STATE_TERMINATED         4   /* Task has finished execution */

/* Task priority levels (lower numbers = higher priority) */
#define TASK_PRIORITY_HIGH            0   /* High priority task */
#define TASK_PRIORITY_NORMAL          1   /* Normal priority task */
#define TASK_PRIORITY_LOW             2   /* Low priority task */
#define TASK_PRIORITY_IDLE            3   /* Idle/background task */

/* Task creation flags */
#define TASK_FLAG_USER_MODE           0x01  /* Task runs in user mode */
#define TASK_FLAG_KERNEL_MODE         0x02  /* Task runs in kernel mode */
#define TASK_FLAG_NO_PREEMPT          0x04  /* Task cannot be preempted */
#define TASK_FLAG_SYSTEM              0x08  /* System/critical task */

/* ========================================================================
 * TASK STRUCTURES
 * ======================================================================== */

/* Task entry point function signature */
typedef void (*task_entry_t)(void *arg);

/* CPU register state for context switching */
typedef struct task_context {
    /* General purpose registers */
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;

    /* Instruction pointer and flags */
    uint64_t rip;
    uint64_t rflags;

    /* Segment registers */
    uint64_t cs, ds, es, fs, gs, ss;

    /* Control registers (saved during context switch) */
    uint64_t cr3;  /* Page directory base register */
} __attribute__((packed)) task_context_t;

/* Task control block */
typedef struct task {
    uint32_t task_id;                    /* Unique task identifier */
    char name[TASK_NAME_MAX_LEN];        /* Task name for debugging */

    /* Task execution state */
    uint8_t state;                       /* Current task state */
    uint8_t priority;                    /* Task priority level */
    uint16_t flags;                      /* Task creation flags */

    /* Memory management */
    uint32_t process_id;                 /* Associated process VM space */
    uint64_t stack_base;                 /* Stack base address */
    uint64_t stack_size;                 /* Stack size in bytes */
    uint64_t stack_pointer;              /* Current stack pointer */

    /* Task entry point */
    task_entry_t entry_point;            /* Task function entry point */
    void *entry_arg;                     /* Argument passed to entry point */

    /* CPU context for switching */
    task_context_t context;              /* Saved CPU state */

    /* Scheduling information */
    uint64_t time_slice;                 /* CPU time quantum */
    uint64_t total_runtime;              /* Total CPU time used */
    uint64_t creation_time;              /* Task creation timestamp */
    uint32_t yield_count;                /* Number of voluntary yields */

    /* Task relationships */
    struct task *next;                   /* Next task in scheduler queue */
    struct task *prev;                   /* Previous task in scheduler queue */
} task_t;

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
    task->creation_time = 0;  /* TODO: Get system time */
    task->yield_count = 0;
    task->next = NULL;
    task->prev = NULL;

    /* Initialize CPU context */
    init_task_context(task);

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

    task_manager.num_tasks = 0;
    task_manager.next_task_id = 1;  /* Start from 1, 0 reserved for kernel */
    task_manager.current_task = NULL;
    task_manager.idle_task = NULL;
    task_manager.ready_queue_head = NULL;
    task_manager.ready_queue_tail = NULL;
    task_manager.total_context_switches = 0;
    task_manager.total_yields = 0;
    task_manager.tasks_created = 0;
    task_manager.tasks_terminated = 0;

    /* Initialize all task slots */
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        task_manager.tasks[i].task_id = INVALID_TASK_ID;
        task_manager.tasks[i].state = TASK_STATE_INVALID;
        task_manager.tasks[i].process_id = INVALID_PROCESS_ID;
        task_manager.tasks[i].next = NULL;
        task_manager.tasks[i].prev = NULL;
    }

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