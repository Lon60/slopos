/*
 * SlopOS Kernel Panic Handler
 * Emergency error handling for critical kernel failures
 * Uses serial driver for reliable output during panic situations
 */

#include <stdint.h>
#include "constants.h"
#include "shutdown.h"
#include "../drivers/serial.h"

/*
 * Emergency serial output for panic messages
 * Uses emergency serial functions that bypass normal initialization
 */
static void panic_output_char(char c) {
    serial_emergency_putc(c);
}

static void panic_output_string(const char *str) {
    serial_emergency_puts(str);
}

/*
 * Output hexadecimal number for debugging
 */
static void panic_output_hex(uint64_t value) {
    serial_emergency_put_hex(value);
}

/*
 * Get current instruction pointer for debugging
 */
static uint64_t get_current_rip(void) {
    uint64_t rip;
    __asm__ volatile ("leaq (%%rip), %0" : "=r" (rip));
    return rip;
}

/*
 * Get current stack pointer for debugging
 */
static uint64_t get_current_rsp(void) {
    uint64_t rsp;
    __asm__ volatile ("movq %%rsp, %0" : "=r" (rsp));
    return rsp;
}

/*
 * Main kernel panic routine
 * Displays error information and halts the system
 */
void kernel_panic(const char *message) {
    // Disable interrupts immediately
    __asm__ volatile ("cli");

    // Output panic header
    panic_output_string("\n\n");
    panic_output_string("=== KERNEL PANIC ===\n");

    // Output panic message
    if (message) {
        panic_output_string("PANIC: ");
        panic_output_string(message);
        panic_output_string("\n");
    } else {
        panic_output_string("PANIC: No message provided\n");
    }

    // Output debugging information
    panic_output_string("RIP: ");
    panic_output_hex(get_current_rip());
    panic_output_string("\n");

    panic_output_string("RSP: ");
    panic_output_hex(get_current_rsp());
    panic_output_string("\n");

    // Output CPU state information
    uint64_t cr0, cr3, cr4;
    __asm__ volatile ("movq %%cr0, %0" : "=r" (cr0));
    __asm__ volatile ("movq %%cr3, %0" : "=r" (cr3));
    __asm__ volatile ("movq %%cr4, %0" : "=r" (cr4));

    panic_output_string("CR0: ");
    panic_output_hex(cr0);
    panic_output_string("\n");

    panic_output_string("CR3: ");
    panic_output_hex(cr3);
    panic_output_string("\n");

    panic_output_string("CR4: ");
    panic_output_hex(cr4);
    panic_output_string("\n");

    panic_output_string("===================\n");
    panic_output_string("System halted.\n");

    kernel_shutdown(message ? message : "panic");
}

/*
 * Kernel panic with additional context information
 */
void kernel_panic_with_context(const char *message, const char *function,
                              const char *file, int line) {
    // Disable interrupts immediately
    __asm__ volatile ("cli");

    panic_output_string("\n\n");
    panic_output_string("=== KERNEL PANIC ===\n");

    if (message) {
        panic_output_string("PANIC: ");
        panic_output_string(message);
        panic_output_string("\n");
    }

    if (function) {
        panic_output_string("Function: ");
        panic_output_string(function);
        panic_output_string("\n");
    }

    if (file) {
        panic_output_string("File: ");
        panic_output_string(file);
        if (line > 0) {
            panic_output_string(":");
            // Simple line number output (assuming line < 10000)
            if (line >= 1000) panic_output_char('0' + (line / 1000) % 10);
            if (line >= 100) panic_output_char('0' + (line / 100) % 10);
            if (line >= 10) panic_output_char('0' + (line / 10) % 10);
            panic_output_char('0' + line % 10);
        }
        panic_output_string("\n");
    }

    // Continue with standard panic procedure
    panic_output_string("RIP: ");
    panic_output_hex(get_current_rip());
    panic_output_string("\n");

    panic_output_string("RSP: ");
    panic_output_hex(get_current_rsp());
    panic_output_string("\n");

    panic_output_string("===================\n");
    panic_output_string("System halted.\n");

    kernel_shutdown(message ? message : "panic");
}

/*
 * Assert function for kernel debugging
 */
void kernel_assert(int condition, const char *message) {
    if (!condition) {
        kernel_panic(message ? message : "Assertion failed");
    }
}
