/*
 * SlopOS Shutdown Orchestration
 * Provides a reusable shutdown sequence that quiesces the scheduler,
 * terminates process state, and halts hardware in a defined order.
 *
 * The helpers in this file are written to be re-entrant so that panic
 * handlers or future power-management code can safely invoke them even
 * if a shutdown is already in progress.
 */

#include "shutdown.h"
#include "debug.h"
#include "constants.h"
#include "../sched/scheduler.h"
#include "../drivers/serial.h"
#include "../drivers/apic.h"

#include <stdint.h>

/* Legacy PIC helpers exposed without a public header */
extern void disable_pic(void);
extern void pic_send_eoi(uint8_t irq);

/* Track shutdown progress so re-entrant callers can short-circuit safely */
static volatile int shutdown_in_progress = 0;
static volatile int interrupts_quiesced = 0;
static volatile int serial_drained = 0;

/*
 * Disable interrupts, flush pending requests, and mask interrupt sources.
 */
void kernel_quiesce_interrupts(void) {
    __asm__ volatile ("cli");

    if (interrupts_quiesced) {
        return;
    }

    kprintln("Kernel shutdown: quiescing interrupt controllers");

    /* Drain any in-flight legacy PIC interrupts */
    for (uint8_t irq = 0; irq < 16; irq++) {
        pic_send_eoi(irq);
    }

    disable_pic();

    if (apic_is_available()) {
        apic_send_eoi();
        apic_timer_stop();
        apic_disable();
    }

    interrupts_quiesced = 1;
}

/*
 * Ensure serial buffers are empty so shutdown logs reach the host.
 */
void kernel_drain_serial_output(void) {
    if (serial_drained) {
        return;
    }

    kprintln("Kernel shutdown: draining serial output");

    debug_flush();

    uint16_t kernel_port = serial_get_kernel_output();
    if (kernel_port != COM1_BASE) {
        serial_flush(COM1_BASE);
    }

    serial_drained = 1;
}

/*
 * Execute the full shutdown sequence and halt the CPUs.
 */
void kernel_shutdown(const char *reason) {
    __asm__ volatile ("cli");

    if (shutdown_in_progress) {
        kernel_quiesce_interrupts();
        kernel_drain_serial_output();
        goto halt;
    }

    shutdown_in_progress = 1;

    kprintln("=== Kernel Shutdown Requested ===");
    if (reason) {
        kprint("Reason: ");
        kprintln(reason);
    }

    scheduler_shutdown();

    if (task_shutdown_all() != 0) {
        kprintln("Warning: Failed to terminate one or more tasks");
    }

    task_set_current(NULL);

    kernel_quiesce_interrupts();
    kernel_drain_serial_output();

    kprintln("Kernel shutdown complete. Halting processors.");

halt:
    while (1) {
        __asm__ volatile ("hlt");
    }
}
