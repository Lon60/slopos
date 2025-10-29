/*
 * SlopOS Shutdown Orchestration
 * Provides a reusable shutdown sequence that is safe for panic handlers
 * and future power-management code.
 */

#ifndef BOOT_SHUTDOWN_H
#define BOOT_SHUTDOWN_H

#include <stddef.h>

/*
 * Disable interrupts and mask all interrupt controllers.
 * Safe to call multiple times; additional calls become no-ops.
 */
void kernel_quiesce_interrupts(void);

/*
 * Drain debug and serial output buffers to ensure logs reach the host.
 */
void kernel_drain_serial_output(void);

/*
 * Stop scheduling, tear down process state, quiesce hardware, and halt.
 * May be invoked from panic handlers or power-management paths.
 */
void kernel_shutdown(const char *reason);

#endif /* BOOT_SHUTDOWN_H */
