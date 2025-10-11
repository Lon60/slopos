/*
 * SlopOS Test Tasks
 * Two simple cooperative tasks that yield to each other
 * Demonstrates basic task switching and scheduler functionality
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"

/* Forward declarations from scheduler and task modules */
extern void yield(void);
extern uint32_t task_create(const char *name, void (*entry_point)(void *),
                           void *arg, uint8_t priority, uint16_t flags);
extern int init_task_manager(void);
extern int init_scheduler(void);
extern int create_idle_task(void);
extern int start_scheduler(void);
extern int schedule_task(void *task);
extern int task_get_info(uint32_t task_id, void **task_info);

/* ========================================================================
 * TEST TASK IMPLEMENTATIONS
 * ======================================================================== */

/*
 * Test Task A - Simple counting task
 */
void test_task_a(void *arg) {
    (void)arg;  /* Unused parameter */

    uint32_t counter = 0;

    kprint("Task A starting execution\n");

    while (counter < 20) {
        kprint("Task A: iteration ");
        kprint_decimal(counter);
        kprint("\n");

        counter++;

        /* Yield after every 3 iterations to demonstrate cooperation */
        if (counter % 3 == 0) {
            kprint("Task A: yielding CPU\n");
            yield();
        }
    }

    kprint("Task A completed\n");

    /* Task finished - yield forever to let other tasks run */
    while (1) {
        yield();
    }
}

/*
 * Test Task B - Character printing task
 */
void test_task_b(void *arg) {
    (void)arg;  /* Unused parameter */

    char current_char = 'A';
    uint32_t iterations = 0;

    kprint("Task B starting execution\n");

    while (iterations < 15) {
        kprint("Task B: printing character '");
        kprint_decimal(current_char);  /* Print ASCII value */
        kprint("' (");
        serial_putc_com1(current_char);  /* Print actual character */
        kprint(")\n");

        /* Move to next character, wrap around after Z */
        current_char++;
        if (current_char > 'Z') {
            current_char = 'A';
        }

        iterations++;

        /* Yield after every 2 iterations */
        if (iterations % 2 == 0) {
            kprint("Task B: yielding CPU\n");
            yield();
        }
    }

    kprint("Task B completed\n");

    /* Task finished - yield forever */
    while (1) {
        yield();
    }
}

/* ========================================================================
 * SCHEDULER TEST FUNCTIONS
 * ======================================================================== */

/*
 * Initialize and run the basic scheduler test
 */
int run_scheduler_test(void) {
    kprint("=== Starting SlopOS Cooperative Scheduler Test ===\n");

    /* Initialize task management system */
    if (init_task_manager() != 0) {
        kprint("Failed to initialize task manager\n");
        return -1;
    }

    /* Initialize scheduler */
    if (init_scheduler() != 0) {
        kprint("Failed to initialize scheduler\n");
        return -1;
    }

    /* Create idle task */
    if (create_idle_task() != 0) {
        kprint("Failed to create idle task\n");
        return -1;
    }

    kprint("Creating test tasks...\n");

    /* Create test task A */
    uint32_t task_a_id = task_create("TestTaskA", test_task_a, NULL,
                                    1,    /* Normal priority */
                                    0x02  /* Kernel mode */);

    if (task_a_id == INVALID_TASK_ID) {
        kprint("Failed to create test task A\n");
        return -1;
    }

    kprint("Created Task A with ID ");
    kprint_decimal(task_a_id);
    kprint("\n");

    /* Create test task B */
    uint32_t task_b_id = task_create("TestTaskB", test_task_b, NULL,
                                    1,    /* Normal priority */
                                    0x02  /* Kernel mode */);

    if (task_b_id == INVALID_TASK_ID) {
        kprint("Failed to create test task B\n");
        return -1;
    }

    kprint("Created Task B with ID ");
    kprint_decimal(task_b_id);
    kprint("\n");

    /* Add tasks to scheduler */
    void *task_a_info, *task_b_info;

    if (task_get_info(task_a_id, &task_a_info) != 0) {
        kprint("Failed to get task A info\n");
        return -1;
    }

    if (task_get_info(task_b_id, &task_b_info) != 0) {
        kprint("Failed to get task B info\n");
        return -1;
    }

    if (schedule_task(task_a_info) != 0) {
        kprint("Failed to schedule task A\n");
        return -1;
    }

    if (schedule_task(task_b_info) != 0) {
        kprint("Failed to schedule task B\n");
        return -1;
    }

    kprint("Tasks scheduled, starting scheduler...\n");

    /* Start the scheduler - this will begin task execution */
    if (start_scheduler() != 0) {
        kprint("Failed to start scheduler\n");
        return -1;
    }

    /* If we reach here, scheduler is running tasks */
    kprint("Scheduler started successfully\n");

    return 0;
}

/*
 * Simple demonstration of task creation and yielding
 */
int demo_cooperative_scheduling(void) {
    kprint("=== Cooperative Scheduling Demo ===\n");

    /* This function demonstrates the basic concept */
    kprint("1. Tasks run voluntarily\n");
    kprint("2. Tasks must yield() to allow others to run\n");
    kprint("3. Round-robin scheduling gives fair CPU time\n");
    kprint("4. No preemption - tasks control their own execution\n");

    /* Run the actual test */
    return run_scheduler_test();
}

/* ========================================================================
 * SCHEDULER STATISTICS AND MONITORING
 * ======================================================================== */

/*
 * Print current scheduler statistics
 */
void print_scheduler_stats(void) {
    extern void get_scheduler_stats(uint64_t *context_switches, uint64_t *yields,
                                   uint32_t *ready_tasks, uint32_t *schedule_calls);
    extern void get_task_stats(uint32_t *total_tasks, uint32_t *active_tasks,
                              uint64_t *context_switches);

    uint64_t sched_switches, sched_yields;
    uint32_t ready_tasks, schedule_calls;
    uint32_t total_tasks, active_tasks;
    uint64_t task_switches;

    get_scheduler_stats(&sched_switches, &sched_yields, &ready_tasks, &schedule_calls);
    get_task_stats(&total_tasks, &active_tasks, &task_switches);

    kprint("\n=== Scheduler Statistics ===\n");
    kprint("Context switches: ");
    kprint_decimal(sched_switches);
    kprint("\n");

    kprint("Voluntary yields: ");
    kprint_decimal(sched_yields);
    kprint("\n");

    kprint("Schedule calls: ");
    kprint_decimal(schedule_calls);
    kprint("\n");

    kprint("Ready tasks: ");
    kprint_decimal(ready_tasks);
    kprint("\n");

    kprint("Total tasks created: ");
    kprint_decimal(total_tasks);
    kprint("\n");

    kprint("Active tasks: ");
    kprint_decimal(active_tasks);
    kprint("\n");
}

/*
 * Monitor scheduler performance
 */
void monitor_scheduler(uint32_t duration_seconds) {
    kprint("Monitoring scheduler for ");
    kprint_decimal(duration_seconds);
    kprint(" seconds...\n");

    /* Simple monitoring loop */
    for (uint32_t i = 0; i < duration_seconds; i++) {
        /* Wait roughly 1 second (crude delay) */
        for (volatile uint32_t j = 0; j < 1000000; j++) {
            /* Busy wait */
        }

        kprint("Monitor: ");
        kprint_decimal(i + 1);
        kprint("s elapsed\n");

        /* Print stats every 5 seconds */
        if ((i + 1) % 5 == 0) {
            print_scheduler_stats();
        }
    }

    kprint("Monitoring complete\n");
    print_scheduler_stats();
}