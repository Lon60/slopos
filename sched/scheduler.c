/*
 * SlopOS Cooperative Round-Robin Scheduler
 * Implements fair task scheduling with voluntary yielding
 * Tasks must yield control voluntarily - no preemption
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../boot/debug.h"
#include "../drivers/serial.h"
#include "../mm/paging.h"
#include "scheduler.h"

/* Forward declarations from context_switch.s */
extern void context_switch(void *old_context, void *new_context);
extern void simple_context_switch(void *old_context, void *new_context);

/* Forward declarations from process_vm.c */
extern process_page_dir_t *process_vm_get_page_dir(uint32_t process_id);

/* Task states from task.c */
#define TASK_STATE_INVALID            0
#define TASK_STATE_READY              1
#define TASK_STATE_RUNNING            2
#define TASK_STATE_BLOCKED            3
#define TASK_STATE_TERMINATED         4

/* ========================================================================
 * SCHEDULER CONSTANTS
 * ======================================================================== */

#define SCHED_MAX_READY_TASKS         32        /* Maximum tasks in ready queue */
#define SCHED_DEFAULT_TIME_SLICE      10        /* Default time slice units */
#define SCHED_IDLE_TASK_ID            0xFFFFFFFE /* Special idle task ID */

/* Scheduling policies */
#define SCHED_POLICY_ROUND_ROBIN      0         /* Round-robin scheduling */
#define SCHED_POLICY_PRIORITY         1         /* Priority-based scheduling */
#define SCHED_POLICY_COOPERATIVE      2         /* Pure cooperative scheduling */

/* ========================================================================
 * SCHEDULER DATA STRUCTURES
 * ======================================================================== */

/* Ready queue for runnable tasks */
typedef struct ready_queue {
    task_t *tasks[SCHED_MAX_READY_TASKS];  /* Array of task pointers */
    uint32_t head;                         /* Head index (next to run) */
    uint32_t tail;                         /* Tail index (last added) */
    uint32_t count;                        /* Number of tasks in queue */
} ready_queue_t;

/* Scheduler control structure */
typedef struct scheduler {
    ready_queue_t ready_queue;             /* Queue of ready tasks */
    task_t *current_task;                  /* Currently running task */
    task_t *idle_task;                     /* Idle task (always ready) */

    /* Scheduling policy and configuration */
    uint8_t policy;                        /* Current scheduling policy */
    uint8_t enabled;                       /* Scheduler enabled flag */
    uint16_t time_slice;                   /* Current time slice value */

    /* Return context for testing (when scheduler exits) */
    task_context_t return_context;         /* Context to return to when scheduler exits */

    /* Statistics and monitoring */
    uint64_t total_switches;               /* Total context switches */
    uint64_t total_yields;                 /* Total voluntary yields */
    uint64_t idle_time;                    /* Time spent in idle task */
    uint32_t schedule_calls;               /* Number of schedule() calls */
} scheduler_t;

/* Global scheduler instance */
static scheduler_t scheduler = {0};

/* ========================================================================
 * READY QUEUE MANAGEMENT
 * ======================================================================== */

/*
 * Initialize the ready queue
 */
static void ready_queue_init(ready_queue_t *queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    /* Clear all task pointers */
    for (uint32_t i = 0; i < SCHED_MAX_READY_TASKS; i++) {
        queue->tasks[i] = NULL;
    }
}

/*
 * Check if ready queue is empty
 */
static int ready_queue_empty(ready_queue_t *queue) {
    return queue->count == 0;
}

/*
 * Check if ready queue is full
 */
static int ready_queue_full(ready_queue_t *queue) {
    return queue->count >= SCHED_MAX_READY_TASKS;
}

/*
 * Add task to ready queue
 * Returns 0 on success, -1 if queue is full
 */
static int ready_queue_enqueue(ready_queue_t *queue, task_t *task) {
    if (!task || ready_queue_full(queue)) {
        return -1;
    }

    /* Add task to tail of queue */
    queue->tasks[queue->tail] = task;
    queue->tail = (queue->tail + 1) % SCHED_MAX_READY_TASKS;
    queue->count++;

    return 0;
}

/*
 * Remove task from front of ready queue
 * Returns task pointer, NULL if queue is empty
 */
static task_t *ready_queue_dequeue(ready_queue_t *queue) {
    if (ready_queue_empty(queue)) {
        return NULL;
    }

    /* Remove task from head of queue */
    task_t *task = queue->tasks[queue->head];
    queue->tasks[queue->head] = NULL;
    queue->head = (queue->head + 1) % SCHED_MAX_READY_TASKS;
    queue->count--;

    return task;
}

/*
 * Remove specific task from ready queue
 * Returns 0 on success, -1 if task not found
 */
static int ready_queue_remove(ready_queue_t *queue, task_t *task) {
    if (!task || ready_queue_empty(queue)) {
        return -1;
    }

    /* Find task in queue */
    for (uint32_t i = 0; i < queue->count; i++) {
        uint32_t index = (queue->head + i) % SCHED_MAX_READY_TASKS;

        if (queue->tasks[index] == task) {
            /* Shift all tasks after this one forward */
            for (uint32_t j = i; j < queue->count - 1; j++) {
                uint32_t curr_index = (queue->head + j) % SCHED_MAX_READY_TASKS;
                uint32_t next_index = (queue->head + j + 1) % SCHED_MAX_READY_TASKS;
                queue->tasks[curr_index] = queue->tasks[next_index];
            }

            /* Clear the last slot and update tail */
            queue->tail = (queue->tail - 1 + SCHED_MAX_READY_TASKS) % SCHED_MAX_READY_TASKS;
            queue->tasks[queue->tail] = NULL;
            queue->count--;

            return 0;
        }
    }

    return -1;  /* Task not found */
}

/* ========================================================================
 * CORE SCHEDULING FUNCTIONS
 * ======================================================================== */

/*
 * Add task to ready queue for scheduling
 */
int schedule_task(task_t *task) {
    if (!task) {
        return -1;
    }

    if (ready_queue_enqueue(&scheduler.ready_queue, task) != 0) {
        return -1;
    }

    return 0;
}

/*
 * Remove task from scheduler (task blocked or terminated)
 */
int unschedule_task(task_t *task) {
    if (!task) {
        return -1;
    }

    /* Remove from ready queue if present */
    ready_queue_remove(&scheduler.ready_queue, task);

    /* If this was the current task, mark for rescheduling */
    if (scheduler.current_task == task) {
        scheduler.current_task = NULL;
    }

    return 0;
}

/*
 * Select next task to run using round-robin policy
 */
static task_t *select_next_task(void) {
    task_t *next_task = NULL;

    /* Get next task from ready queue */
    if (!ready_queue_empty(&scheduler.ready_queue)) {
        next_task = ready_queue_dequeue(&scheduler.ready_queue);
    }

    /* If no tasks available, use idle task */
    if (!next_task && scheduler.idle_task) {
        /* Check if idle task is still active */
        extern int task_get_info(uint32_t task_id, task_t **task_info);
        task_t *idle_info;
        if (task_get_info(scheduler.idle_task->task_id, &idle_info) == 0 &&
            idle_info->state != TASK_STATE_TERMINATED) {
            next_task = scheduler.idle_task;
        }
    }

    return next_task;
}

/*
 * Perform context switch to new task
 */
static void switch_to_task(task_t *new_task) {
    if (!new_task) {
        return;
    }

    task_t *old_task = scheduler.current_task;

    if (old_task == new_task) {
        return;
    }

    uint64_t timestamp = debug_get_timestamp();
    task_record_context_switch(old_task, new_task, timestamp);

    /* Update scheduler state */
    scheduler.current_task = new_task;
    task_set_current(new_task);
    scheduler.total_switches++;

    /* Ensure CR3 matches the task's process address space */
    if (new_task->process_id != INVALID_PROCESS_ID) {
        process_page_dir_t *page_dir = process_vm_get_page_dir(new_task->process_id);
        if (page_dir && page_dir->pml4_phys) {
            new_task->context.cr3 = page_dir->pml4_phys;
        }
    }

    /* Perform actual context switch */
    if (old_task) {
        context_switch(&old_task->context, &new_task->context);
    } else {
        /* First task - no old context to save */
        context_switch(NULL, &new_task->context);
    }
}

/* ========================================================================
 * PUBLIC SCHEDULER INTERFACE
 * ======================================================================== */

/*
 * Main scheduling function - select and switch to next task
 * This is the core of the cooperative scheduler
 */
void schedule(void) {
    if (!scheduler.enabled) {
        return;
    }

    scheduler.schedule_calls++;

    /* Get current task and put it back in ready queue if still runnable */
    task_t *current = scheduler.current_task;
    if (current && current != scheduler.idle_task) {
        /* Only re-queue if task is still ready to run */
        task_t *task_info;
        if (task_get_info(current->task_id, &task_info) == 0 &&
            task_info->state == TASK_STATE_RUNNING) {

            /* Mark as ready and add back to queue */
            task_set_state(current->task_id, TASK_STATE_READY);
            ready_queue_enqueue(&scheduler.ready_queue, current);
        }
    }

    /* Select next task to run */
    task_t *next_task = select_next_task();
    if (!next_task) {
        /* No tasks to run - check if we should exit scheduler */
        /* For testing purposes, if idle task has terminated, exit scheduler */
        if (scheduler.idle_task) {
            extern int task_get_info(uint32_t task_id, task_t **task_info);
            task_t *idle_info;
            if (task_get_info(scheduler.idle_task->task_id, &idle_info) == 0 &&
                idle_info->state == TASK_STATE_TERMINATED) {
                /* Idle task terminated - exit scheduler by switching to return context */
                scheduler.enabled = 0;
                /* Switch back to the saved return context */
                if (scheduler.current_task) {
                    context_switch(&scheduler.current_task->context, &scheduler.return_context);
                } else {
                    /* No current task - this shouldn't happen */
                    return;
                }
            }
        }
        /* No tasks available but idle task still exists - shouldn't happen */
        return;
    }

    /* Switch to the selected task */
    switch_to_task(next_task);
}

/*
 * Yield CPU voluntarily (cooperative scheduling)
 * Current task gives up CPU and allows other tasks to run
 */
void yield(void) {
    scheduler.total_yields++;

    if (scheduler.current_task) {
        task_record_yield(scheduler.current_task);
    }

    /* Trigger rescheduling */
    schedule();
}

/*
 * Block current task (remove from ready queue)
 */
void block_current_task(void) {
    task_t *current = scheduler.current_task;
    if (!current) {
        return;
    }

    /* Mark task as blocked */
    task_set_state(current->task_id, TASK_STATE_BLOCKED);

    /* Remove from ready queue and schedule next task */
    unschedule_task(current);
    schedule();
}

int task_wait_for(uint32_t task_id) {
    task_t *current = scheduler.current_task;
    if (!current) {
        return -1;
    }

    if (task_id == INVALID_TASK_ID || current->task_id == task_id) {
        return -1;
    }

    task_t *target = NULL;
    if (task_get_info(task_id, &target) != 0 || !target) {
        current->waiting_on_task_id = INVALID_TASK_ID;
        return 0; /* Target already gone */
    }

    if (target->state == TASK_STATE_INVALID || target->task_id == INVALID_TASK_ID) {
        current->waiting_on_task_id = INVALID_TASK_ID;
        return 0;
    }

    current->waiting_on_task_id = task_id;
    block_current_task();

    current->waiting_on_task_id = INVALID_TASK_ID;
    return 0;
}

/*
 * Unblock task (add back to ready queue)
 */
int unblock_task(task_t *task) {
    if (!task) {
        return -1;
    }

    /* Mark task as ready */
    task_set_state(task->task_id, TASK_STATE_READY);

    /* Add back to ready queue */
    return schedule_task(task);
}

/*
 * Terminate the currently running task and hand control to the scheduler
 */
void scheduler_task_exit(void) {
    task_t *current = scheduler.current_task;

    if (!current) {
        kprintln("scheduler_task_exit: No current task");
        schedule();
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    uint64_t timestamp = debug_get_timestamp();
    task_record_context_switch(current, NULL, timestamp);

    if (task_terminate((uint32_t)-1) != 0) {
        kprintln("scheduler_task_exit: Failed to terminate current task");
    }

    scheduler.current_task = NULL;
    task_set_current(NULL);

    schedule();

    kprintln("scheduler_task_exit: Schedule returned unexpectedly");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* ========================================================================
 * IDLE TASK IMPLEMENTATION
 * ======================================================================== */

/*
 * Idle task function - runs when no other tasks are ready
 */
static void idle_task_function(void *arg) {
    (void)arg;  /* Unused parameter */

    while (1) {
        /* Simple idle loop - could implement power management here */
        scheduler.idle_time++;

        /* Check if we should exit (for testing purposes) */
        /* If there are no user tasks and we're in a test environment, exit */
        extern int is_kernel_initialized(void);
        if (is_kernel_initialized() && scheduler.idle_time > 1000) {
            /* Count active tasks */
            extern void get_task_stats(uint32_t *total_tasks, uint32_t *active_tasks,
                                     uint64_t *context_switches);
            uint32_t active_tasks = 0;
            get_task_stats(NULL, &active_tasks, NULL);
            if (active_tasks <= 1) {  /* Only idle task remains */
                /* Exit idle loop - return to scheduler caller */
                break;
            }
        }

        /* Yield periodically to check for new tasks */
        if (scheduler.idle_time % 1000 == 0) {
            yield();
        }
    }

    /* Return to scheduler - this should only happen in test scenarios */
    scheduler.enabled = 0;  /* Disable scheduler */
}

/* ========================================================================
 * INITIALIZATION AND CONFIGURATION
 * ======================================================================== */

/*
 * Initialize the scheduler system
 */
int init_scheduler(void) {
    /* Initialize ready queue */
    ready_queue_init(&scheduler.ready_queue);

    /* Initialize scheduler state */
    scheduler.current_task = NULL;
    scheduler.idle_task = NULL;
    scheduler.policy = SCHED_POLICY_COOPERATIVE;
    scheduler.enabled = 0;  /* Start disabled */
    scheduler.time_slice = SCHED_DEFAULT_TIME_SLICE;
    scheduler.total_switches = 0;
    scheduler.total_yields = 0;
    scheduler.idle_time = 0;
    scheduler.schedule_calls = 0;

    return 0;
}

/*
 * Create and start the idle task
 */
int create_idle_task(void) {
    /* Create idle task using task management functions */
    extern uint32_t task_create(const char *name, void (*entry_point)(void *),
                               void *arg, uint8_t priority, uint16_t flags);
    extern int task_get_info(uint32_t task_id, task_t **task_info);

    uint32_t idle_task_id = task_create("idle", idle_task_function, NULL,
                                       3, 0x02);  /* Low priority, kernel mode */

    if (idle_task_id == INVALID_TASK_ID) {
        return -1;
    }

    /* Get idle task pointer */
    task_t *idle_task;
    if (task_get_info(idle_task_id, &idle_task) != 0) {
        return -1;
    }

    scheduler.idle_task = idle_task;
    return 0;
}

/*
 * Start the scheduler (enable scheduling)
 */
int start_scheduler(void) {
    if (scheduler.enabled) {
        return -1;
    }

    scheduler.enabled = 1;

    /* Save current context as return context for testing */
    extern void init_kernel_context(task_context_t *context);
    init_kernel_context(&scheduler.return_context);

    /* If we have tasks in ready queue, start scheduling */
    if (!ready_queue_empty(&scheduler.ready_queue)) {
        schedule();
    } else if (scheduler.idle_task) {
        /* Start with idle task */
        switch_to_task(scheduler.idle_task);
    } else {
        return -1;
    }

    /* If we get here, scheduler has exited and switched back to return context */
    return 0;
}

/*
 * Stop the scheduler
 */
void stop_scheduler(void) {
    scheduler.enabled = 0;
}

/*
 * Prepare scheduler for shutdown and clear scheduling state
 */
void scheduler_shutdown(void) {
    if (scheduler.enabled) {
        stop_scheduler();
    }

    ready_queue_init(&scheduler.ready_queue);
    scheduler.current_task = NULL;
    scheduler.idle_task = NULL;
}

/* ========================================================================
 * QUERY AND STATISTICS FUNCTIONS
 * ======================================================================== */

/*
 * Get scheduler statistics
 */
void get_scheduler_stats(uint64_t *context_switches, uint64_t *yields,
                        uint32_t *ready_tasks, uint32_t *schedule_calls) {
    if (context_switches) {
        *context_switches = scheduler.total_switches;
    }
    if (yields) {
        *yields = scheduler.total_yields;
    }
    if (ready_tasks) {
        *ready_tasks = scheduler.ready_queue.count;
    }
    if (schedule_calls) {
        *schedule_calls = scheduler.schedule_calls;
    }
}

/*
 * Check if scheduler is enabled
 */
int scheduler_is_enabled(void) {
    return scheduler.enabled;
}

/*
 * Get current task from scheduler
 */
task_t *scheduler_get_current_task(void) {
    return scheduler.current_task;
}
