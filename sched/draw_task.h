/*
 * SlopOS Framebuffer Draw Task
 * Cooperative task that renders animated graphics to the framebuffer
 */

#ifndef SCHED_DRAW_TASK_H
#define SCHED_DRAW_TASK_H

/*
 * Spawn the framebuffer draw task.
 * Returns 0 on success, non-zero on failure.
 *
 * The task assumes the framebuffer subsystem has been initialized and will
 * continuously render animated primitives until descheduled or shutdown.
 */
int spawn_framebuffer_draw_task(void);

#endif /* SCHED_DRAW_TASK_H */

