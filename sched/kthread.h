/*
 * SlopOS Kernel Thread Helpers
 * Convenience APIs for creating and managing in-kernel cooperative tasks
 */

#ifndef SCHED_KTHREAD_H
#define SCHED_KTHREAD_H

#include <stdint.h>
#include "task.h"

typedef uint32_t kthread_id_t;

/*
 * Spawn a kernel thread with default priority.
 * Returns INVALID_TASK_ID on failure.
 */
kthread_id_t kthread_spawn(const char *name, task_entry_t entry_point, void *arg);

/*
 * Spawn a kernel thread with explicit scheduling parameters.
 * The TASK_FLAG_KERNEL_MODE bit is enforced regardless of the supplied flags.
 */
kthread_id_t kthread_spawn_ex(const char *name, task_entry_t entry_point, void *arg,
                              uint8_t priority, uint16_t flags);

/*
 * Yield execution to allow other cooperative tasks to run.
 */
void kthread_yield(void);

/*
 * Block until the specified kernel thread has terminated.
 * Returns 0 on success, negative value on failure.
 */
int kthread_join(kthread_id_t thread_id);

/*
 * Terminate the calling kernel thread.
 * Does not return.
 */
void kthread_exit(void) __attribute__((noreturn));

#endif /* SCHED_KTHREAD_H */
