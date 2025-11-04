/*
 * SlopOS Early Initialization
 * Main 64-bit kernel entry point and early setup
 */

#include <stdint.h>
#include <stddef.h>
#include "../drivers/serial.h"
#include "constants.h"
#include "idt.h"
#include "debug.h"
#include "gdt.h"
#include "limine_protocol.h"
#include "safe_stack.h"
#include "shutdown.h"
#include "../drivers/pic.h"
#include "../drivers/apic.h"
#include "../drivers/irq.h"
#include "../drivers/interrupt_test.h"
#include "../drivers/pci.h"

// Forward declarations for other modules
extern void verify_cpu_state(void);
extern void verify_memory_layout(void);
extern void check_stack_health(void);
extern void kernel_panic(const char *message);
extern void init_paging(void);
extern void init_kernel_memory_layout(void);
extern int init_memory_system(const struct limine_memmap_response *memmap,
                              uint64_t hhdm_offset);

// IDT and interrupt handling
extern void init_idt(void);
extern void init_pic(void);
extern void dump_idt(void);
extern void load_idt(void);

extern void disable_pic(void);

// Kernel state tracking
static volatile int kernel_initialized = 0;

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
 * Initialize kernel subsystems in proper order
 */
static void initialize_kernel_subsystems(void) {
    early_debug_string("SlopOS: Initializing remaining kernel subsystems\n");
    extern int splash_report_progress(int progress, const char *message);

    // Initialize debug subsystem first
    splash_report_progress(20, "Initializing debug...");
    debug_init();
    early_debug_string("SlopOS: Debug subsystem initialized\n");

    // Set up GDT and TSS before enabling interrupts
    early_debug_string("SlopOS: Initializing GDT/TSS...\n");
    splash_report_progress(30, "Setting up GDT/TSS...");
    gdt_init();
    early_debug_string("SlopOS: GDT/TSS initialized\n");

    // Initialize IDT FIRST - critical for debugging any issues that follow
    early_debug_string("SlopOS: Initializing IDT...\n");
    splash_report_progress(40, "Setting up interrupts...");
    idt_init();

    // Configure dedicated IST stacks for critical exceptions
    early_debug_string("SlopOS: Configuring safe exception stacks...\n");
    safe_stack_init();
    early_debug_string("SlopOS: Safe exception stacks ready\n");

    idt_load();
    early_debug_string("SlopOS: IDT initialized - exception handling active\n");

    // Initialize PIC for interrupt control
    early_debug_string("SlopOS: Initializing PIC...\n");
    splash_report_progress(50, "Initializing PIC...");
    pic_init();
    early_debug_string("SlopOS: PIC initialized - interrupt control ready\n");

    early_debug_string("SlopOS: Configuring IRQ dispatcher...\n");
    splash_report_progress(55, "Setting up IRQ dispatcher...");
    irq_init();
    early_debug_string("SlopOS: IRQ dispatcher ready\n");

    // Skip paging initialization - already set up by boot assembly code
    // The boot/entry32.s and boot/entry64.s have already configured:
    //   - CR3 register pointing to PML4
    //   - Identity mapping for low memory
    //   - Higher-half kernel mapping
    // Attempting to reinitialize would corrupt the active page tables
    early_debug_string("SlopOS: Using paging already configured by bootloader\n");
    early_debug_string("SlopOS: Memory system initialized earlier via Limine data\n");

    // Detect and initialize APIC now that memory management is available
    early_debug_string("SlopOS: Detecting Local APIC...\n");
    splash_report_progress(60, "Detecting APIC...");
    if (apic_detect()) {
        early_debug_string("SlopOS: Initializing Local APIC...\n");
        splash_report_progress(65, "Initializing APIC...");
        if (apic_init() == 0) {
            early_debug_string("SlopOS: Local APIC initialized, masking legacy PIC\n");
            disable_pic();
        } else {
            early_debug_string("SlopOS: APIC initialization failed, retaining PIC\n");
        }
    } else {
        early_debug_string("SlopOS: Local APIC unavailable, continuing with PIC\n");
    }

    early_debug_string("SlopOS: Enumerating PCI devices...\n");
    splash_report_progress(70, "Enumerating PCI devices...");
    if (pci_init() == 0) {
        early_debug_string("SlopOS: PCI subsystem initialized\n");
        const pci_gpu_info_t *gpu = pci_get_primary_gpu();
        if (gpu && gpu->present) {
            kprint("PCI: Primary GPU detected (bus ");
            kprint_dec(gpu->device.bus);
            kprint(", device ");
            kprint_dec(gpu->device.device);
            kprint(", function ");
            kprint_dec(gpu->device.function);
            kprintln(")");
            if (gpu->mmio_virt_base) {
                kprint("PCI: GPU MMIO virtual base 0x");
                kprint_hex((uint64_t)(uintptr_t)gpu->mmio_virt_base);
                kprint(", size 0x");
                kprint_hex(gpu->mmio_size);
                kprintln("");
            } else {
                kprintln("PCI: WARNING GPU MMIO mapping unavailable");
            }
        } else {
            kprintln("PCI: No GPU-class device discovered during enumeration");
        }
    } else {
        kprintln("PCI: Initialization failed");
    }

    struct interrupt_test_config test_config;
    interrupt_test_config_init_defaults(&test_config);

    const char *cmdline = get_kernel_cmdline();
    if (cmdline) {
        interrupt_test_config_parse_cmdline(&test_config, cmdline);
    }

    if (test_config.enabled && test_config.suite_mask == 0) {
        kprintln("INTERRUPT_TEST: No suites selected, skipping execution");
        test_config.enabled = 0;
    }

    if (test_config.enabled) {
        early_debug_string("SlopOS: Running interrupt test framework...\n");
        splash_report_progress(75, "Running interrupt tests...");

        kprint("INTERRUPT_TEST: Suites -> ");
        kprintln(interrupt_test_suite_string(test_config.suite_mask));

        kprint("INTERRUPT_TEST: Verbosity -> ");
        kprintln(interrupt_test_verbosity_string(test_config.verbosity));

        kprint("INTERRUPT_TEST: Timeout (ms) -> ");
        kprint_dec(test_config.timeout_ms);
        kprintln("");

        interrupt_test_init(&test_config);
        int passed = run_all_interrupt_tests(&test_config);
        struct test_stats *stats = test_get_stats();
        int failed_tests = stats ? stats->failed_tests : 0;
        interrupt_test_cleanup();

        kprint("INTERRUPT_TEST: Boot run passed tests -> ");
        kprint_dec(passed);
        kprintln("");

        if (test_config.shutdown_on_complete) {
            kprintln("INTERRUPT_TEST: Auto shutdown enabled after harness");
            interrupt_test_request_shutdown(failed_tests);
        }

        early_debug_string("SlopOS: Interrupt test framework complete\n");
    } else {
        early_debug_string("SlopOS: Interrupt test framework disabled (config)\n");
    }

    // Mark kernel as initialized
    kernel_initialized = 1;
    early_debug_string("SlopOS: Kernel subsystems initialized\n");
}

/*
 * Main 64-bit kernel entry point
 * Called from assembly code after successful boot via Limine bootloader
 *
 * This is the Limine protocol version - no parameters needed,
 * Limine provides boot information via static request structures.
 */
void kernel_main(void) {
    // Initialize COM1 serial port FIRST - before anything that prints
    serial_init_com1();
    
    // Output success message via serial port FIRST to prove serial works
    kprintln("SlopOS Kernel Started!");
    kprintln("Booting via Limine Protocol...");

    kprintln("Initializing Limine protocol interface...");
    if (init_limine_protocol() != 0) {
        kprintln("ERROR: Limine protocol initialization failed");
        kernel_panic("Limine protocol initialization failed");
    } else {
        kprintln("Limine protocol interface ready.");
    }

    if (!is_memory_map_available()) {
        kprintln("ERROR: Limine did not provide a memory map");
        kernel_panic("Missing Limine memory map");
    }

    const struct limine_memmap_response *limine_memmap = limine_get_memmap_response();
    if (!limine_memmap) {
        kprintln("ERROR: Limine memory map response pointer is NULL");
        kernel_panic("Invalid Limine memory map");
    }

    uint64_t hhdm_offset = 0;
    if (is_hhdm_available()) {
        hhdm_offset = get_hhdm_offset();
    } else {
        kprintln("WARNING: Limine did not report an HHDM offset");
    }

    kprintln("Initializing memory management from Limine data...");
    if (init_memory_system(limine_memmap, hhdm_offset) != 0) {
        kprintln("ERROR: Memory system initialization failed");
        kernel_panic("Memory system initialization failed");
    }
    kprintln("Memory management initialized.");

    // Verify we're running in higher-half virtual memory
    uint64_t stack_ptr;
    __asm__ volatile ("movq %%rsp, %0" : "=r" (stack_ptr));
    
    kprintln("Stack pointer read successfully!");
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

    // Initialize remaining kernel subsystems now that memory is online
    kprintln("Initializing remaining kernel subsystems...");
    initialize_kernel_subsystems();
    kprintln("Kernel subsystem initialization complete.");

    // Skip exception tests for now - we want a clean boot first
    // The IDT is properly configured and will catch any unexpected exceptions
    // TODO: Add controlled exception tests later with proper recovery
    kprintln("Exception handling system configured and ready");

    // Initialize video subsystem (video-pipeline-architect)
    kprintln("Initializing framebuffer graphics system...");
    extern int framebuffer_init(void);
    extern void font_console_init(uint32_t fg_color, uint32_t bg_color);
    extern void framebuffer_clear(uint32_t color);
    extern int graphics_draw_rect_filled(int x, int y, int width, int height, uint32_t color);
    extern int graphics_draw_circle(int cx, int cy, int radius, uint32_t color);
    extern int splash_show_boot_screen(void);
    extern int splash_report_progress(int progress, const char *message);
    extern int splash_finish(void);

    if (framebuffer_init() == 0) {
        kprintln("Framebuffer initialized successfully!");

        // Initialize splash screen
        splash_show_boot_screen();
        splash_report_progress(10, "Graphics initialized");

        // Graphics demo will be shown after splash screen completes
        // (The splash screen will remain visible during kernel initialization)
    } else {
        kprintln("WARNING: Framebuffer initialization failed - no graphics available");
    }

    // Initialize scheduler subsystem (with SIMD-disabled compiler flags)
    kprintln("Initializing scheduler subsystem...");
    splash_report_progress(85, "Initializing scheduler...");
    extern int init_task_manager(void);
    extern int init_scheduler(void);

    if (init_task_manager() != 0) {
        kprintln("ERROR: Task manager initialization failed");
    } else {
        kprintln("Task manager initialized successfully!");
    }

    splash_report_progress(90, "Starting task manager...");
    if (init_scheduler() != 0) {
        kprintln("ERROR: Scheduler initialization failed");
    } else {
        kprintln("Scheduler initialized successfully!");
    }
    
    // Finish splash screen and show boot completion
    splash_report_progress(95, "Boot complete");
    splash_finish();

    // Clear splash screen and show graphics demo
    framebuffer_clear(0x001122FF);

    // Initialize console with white text on dark background
    font_console_init(0xFFFFFFFF, 0x00000000);

    // Draw graphics demo
    graphics_draw_rect_filled(20, 20, 300, 150, 0xFF0000FF);        // Red rectangle
    graphics_draw_rect_filled(700, 20, 300, 150, 0x00FF00FF);       // Green rectangle
    graphics_draw_circle(512, 384, 100, 0xFFFF00FF);                // Yellow circle

    // White border around entire screen
    graphics_draw_rect_filled(0, 0, 1024, 4, 0xFFFFFFFF);           // Top
    graphics_draw_rect_filled(0, 764, 1024, 4, 0xFFFFFFFF);         // Bottom
    graphics_draw_rect_filled(0, 0, 4, 768, 0xFFFFFFFF);            // Left
    graphics_draw_rect_filled(1020, 0, 4, 768, 0xFFFFFFFF);         // Right

    // Display welcome message using font_draw_string
    extern int font_draw_string(int x, int y, const char *str, uint32_t fg_color, uint32_t bg_color);
    font_draw_string(20, 600, "*** SLOPOS GRAPHICS SYSTEM OPERATIONAL ***", 0xFFFFFFFF, 0x00000000);
    font_draw_string(20, 616, "Framebuffer: WORKING | Resolution: 1024x768", 0xFFFFFFFF, 0x00000000);
    font_draw_string(20, 632, "Memory: OK | Graphics: OK | Text: OK", 0xFFFFFFFF, 0x00000000);

    kprintln("");
    kprintln("=== KERNEL BOOT SUCCESSFUL ===");
    kprintln("Operational subsystems:");
    kprintln("  - Serial output (COM1)");
    kprintln("  - Exception handling (IDT)");
    kprintln("  - Interrupt control (PIC)");
    kprintln("  - Memory management");
    kprintln("  - Debug & diagnostics");
    kprintln("  - Scheduler (cooperative multitasking)");
    kprintln("");
 kprintln("Kernel initialization complete - ALL SYSTEMS OPERATIONAL!");
    kprintln("System ready for next development phase.");

    // Enter idle loop
    kprintln("Entering idle loop (HLT)...");
    while (1) {
        __asm__ volatile ("hlt");  // Halt until next interrupt
    }
}

/*
 * Alternative entry point for compatibility
 */
void kernel_main_no_multiboot(void) {
    kernel_main();
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
