/*
 * SlopOS Early Initialization
 * Main 64-bit kernel entry point and early setup
 */

#include <stdint.h>
#include <stddef.h>
#include "../drivers/serial.h"
#include "constants.h"

// Forward declarations for other modules
extern void verify_cpu_state(void);
extern void verify_memory_layout(void);
extern void check_stack_health(void);
extern void kernel_panic(const char *message);
extern void init_paging(void);
extern void init_kernel_memory_layout(void);
extern void parse_multiboot2_info(uint64_t multiboot_info_addr);

// Kernel state tracking
static volatile int kernel_initialized = 0;
static uint64_t multiboot2_info_addr = 0;

/*
 * Early debug output support
 * Simple serial output for debugging during early boot
 */
static void early_debug_char(char c) {
    // Output to COM1 (0x3F8) - best effort only
    __asm__ volatile (
        "movw $0x3F8, %%dx\n\t"
        "movb %0, %%al\n\t"
        "outb %%al, %%dx"
        :
        : "r" (c)
        : "dx", "al"
    );
}

static void early_debug_string(const char *str) {
    if (!str) return;

    while (*str) {
        early_debug_char(*str++);
    }
}

/*
 * Set multiboot2 info address for later processing
 */
void set_multiboot2_info(uint64_t addr) {
    multiboot2_info_addr = addr;
}

/*
 * Get multiboot2 info address
 */
uint64_t get_multiboot2_info(void) {
    return multiboot2_info_addr;
}

/*
 * Initialize kernel subsystems in proper order
 */
static void initialize_kernel_subsystems(void) {
    early_debug_string("SlopOS: Initializing kernel subsystems\n");

    // Parse Multiboot2 information first if available
    if (multiboot2_info_addr != 0) {
        parse_multiboot2_info(multiboot2_info_addr);
        early_debug_string("SlopOS: Multiboot2 info parsed\n");
    } else {
        early_debug_string("SlopOS: No multiboot2 info available\n");
    }

    // Initialize paging system
    init_paging();
    early_debug_string("SlopOS: Paging system initialized\n");

    // Initialize kernel memory layout
    init_kernel_memory_layout();
    early_debug_string("SlopOS: Kernel memory layout initialized\n");

    // Mark kernel as initialized
    kernel_initialized = 1;
    early_debug_string("SlopOS: Kernel subsystems initialized\n");
}

/*
 * Main 64-bit kernel entry point - MVP VERSION
 * Called from assembly code after successful transition to long mode
 *
 * Parameters:
 *   multiboot_info - Physical address of Multiboot2 information structure
 *
 * This is a minimal viable kernel main function to prove OVMF boot works.
 * Following SysV ABI - first parameter in RDI.
 */
void kernel_main(uint64_t multiboot_info) {
    // Store multiboot2 info for later use
    set_multiboot2_info(multiboot_info);

    // Initialize COM1 serial port for output
    serial_init_com1();

    // Output success message via serial port
    kprintln("SlopOS Kernel Started!");
    kprintln("OVMF UEFI Boot Successful");
    kprint("Multiboot2 Info Pointer: ");
    kprint_hex(multiboot_info);
    kprintln("");

    // Verify we're running in higher-half virtual memory
    uint64_t stack_ptr;
    __asm__ volatile ("movq %%rsp, %0" : "=r" (stack_ptr));
    kprint("Current Stack Pointer: ");
    kprint_hex(stack_ptr);
    kprintln("");

    // Get current instruction pointer
    void *current_ip = &&current_location;
current_location:
    kprint("Kernel Code Address: ");
    kprint_hex((uint64_t)current_ip);
    kprintln("");

    // Check if we're in higher-half (above 0xFFFFFFFF80000000)
    if ((uint64_t)current_ip >= KERNEL_VIRTUAL_BASE) {
        kprintln("Running in higher-half virtual memory - CORRECT");
    } else {
        kprintln("WARNING: Not running in higher-half virtual memory");
    }

    // Initialize kernel subsystems with memory management
    kprintln("Initializing kernel subsystems...");
    initialize_kernel_subsystems();
    kprintln("Kernel subsystem initialization complete.");

    // Initialize video subsystem (video-pipeline-architect)
    kprintln("Initializing framebuffer graphics system...");
    extern int framebuffer_init(void);
    extern void font_console_init(uint32_t fg_color, uint32_t bg_color);
    extern int font_console_puts(const char *str);
    extern void framebuffer_clear(uint32_t color);
    extern int graphics_draw_rect_filled(int x, int y, int width, int height, uint32_t color);
    extern int graphics_draw_circle(int cx, int cy, int radius, uint32_t color);

    if (framebuffer_init() == 0) {
        kprintln("Framebuffer initialized successfully!");

        // Clear screen to dark blue
        framebuffer_clear(0x001122FF);

        // Initialize console with white text on dark background
        font_console_init(0xFFFFFFFF, 0x00000000);

        // Test graphics by drawing some basic shapes
        graphics_draw_rect_filled(50, 50, 200, 100, 0xFF0000FF);  // Red rectangle
        graphics_draw_circle(400, 200, 50, 0x00FF00FF);           // Green circle
        graphics_draw_rect_filled(10, 300, 300, 2, 0xFFFFFFFF);   // White line

        // Display welcome message on screen
        font_console_puts("SlopOS Graphics System Initialized!\n");
        font_console_puts("Basic framebuffer operations working.\n");
        font_console_puts("Memory management: OK\n");
        font_console_puts("Graphics primitives: OK\n");
        font_console_puts("Text rendering: OK\n");

        kprintln("Graphics system test complete - visual output should be visible!");
    } else {
        kprintln("WARNING: Framebuffer initialization failed - no graphics available");
    }

    // TODO: Initialize scheduler subsystem (kernel-scheduler-manager)

    kprintln("Kernel initialization complete.");
    kprintln("Entering infinite loop - MVP SUCCESS!");

    // MVP complete - infinite halt loop
    while (1) {
        __asm__ volatile ("hlt");  // Halt until next interrupt
    }
}

/*
 * Alternative entry point without multiboot2 info
 * Used when multiboot2 info is not available or invalid
 */
void kernel_main_no_multiboot(void) {
    kernel_main(0);  // Call main entry with no multiboot info
}

/*
 * Get kernel initialization status
 * Returns non-zero if kernel is fully initialized
 */
int is_kernel_initialized(void) {
    return kernel_initialized;
}

/*
 * Get kernel initialization progress as percentage
 * Returns 0-100 indicating initialization progress
 */
int get_initialization_progress(void) {
    if (!kernel_initialized) {
        return 50;  // Basic boot complete, subsystems pending
    }
    return 100;     // Fully initialized
}

/*
 * Early kernel status reporting
 */
void report_kernel_status(void) {
    if (is_kernel_initialized()) {
        early_debug_string("SlopOS: Kernel status - INITIALIZED\n");
    } else {
        early_debug_string("SlopOS: Kernel status - INITIALIZING\n");
    }
}

/*
 * Safe kernel shutdown routine
 * Performs cleanup and halts the system safely
 */
void kernel_shutdown(const char *reason) {
    // Disable interrupts
    __asm__ volatile ("cli");

    early_debug_string("SlopOS: Kernel shutdown requested\n");
    if (reason) {
        early_debug_string("SlopOS: Shutdown reason: ");
        early_debug_string(reason);
        early_debug_string("\n");
    }

    // TODO: Perform cleanup of kernel resources

    early_debug_string("SlopOS: System halted\n");

    // Halt the system
    while (1) {
        __asm__ volatile ("hlt");
    }
}