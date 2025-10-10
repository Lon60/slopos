/*
 * SlopOS Early Initialization
 * Main 64-bit kernel entry point and early setup
 */

#include <stdint.h>
#include <stddef.h>

// Forward declarations for other modules
extern void verify_cpu_state(void);
extern void verify_memory_layout(void);
extern void check_stack_health(void);
extern void kernel_panic(const char *message);
extern void init_paging(void);
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

    // Initialize paging system
    init_paging();
    early_debug_string("SlopOS: Paging system initialized\n");

    // Parse Multiboot2 information if available
    if (multiboot2_info_addr != 0) {
        parse_multiboot2_info(multiboot2_info_addr);
        early_debug_string("SlopOS: Multiboot2 info parsed\n");
    } else {
        early_debug_string("SlopOS: No multiboot2 info available\n");
    }

    // Mark kernel as initialized
    kernel_initialized = 1;
    early_debug_string("SlopOS: Kernel subsystems initialized\n");
}

/*
 * Main 64-bit kernel entry point
 * Called from assembly code after successful transition to long mode
 *
 * Parameters:
 *   multiboot_info - Physical address of Multiboot2 information structure
 *
 * At entry:
 *   - CPU is in 64-bit long mode
 *   - Basic paging is established (identity + higher-half)
 *   - Stack is available
 *   - Interrupts are disabled
 */
void kernel_main(uint64_t multiboot_info) {
    // Store multiboot2 info for later use
    set_multiboot2_info(multiboot_info);

    // Verify system state is correct
    verify_cpu_state();
    verify_memory_layout();
    check_stack_health();

    // Signal successful 64-bit mode entry
    early_debug_string("SlopOS: 64-bit kernel started successfully\n");

    // Initialize kernel subsystems
    initialize_kernel_subsystems();

    early_debug_string("SlopOS: Early kernel initialization complete\n");
    early_debug_string("SlopOS: Entering main kernel loop\n");

    // TODO: Transition to main kernel initialization
    // For now, enter a safe infinite loop
    while (1) {
        __asm__ volatile ("hlt");
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