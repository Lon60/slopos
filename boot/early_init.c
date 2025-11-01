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
#include "init.h"
#include "log.h"
#include "safe_stack.h"
#include "shutdown.h"
#include "../drivers/pic.h"
#include "../drivers/apic.h"
#include "../drivers/irq.h"
#include "../drivers/interrupt_test.h"
#include "../sched/task.h"
#include "../sched/scheduler.h"
#include "../shell/shell.h"
#include "../fs/ramfs.h"
#include "../video/framebuffer.h"
#include "../video/graphics.h"
#include "../video/font.h"
#include <string.h>

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

struct boot_runtime_context {
    const struct limine_memmap_response *memmap;
    uint64_t hhdm_offset;
    const char *cmdline;
};

static struct boot_runtime_context boot_ctx = {0};

static int optional_steps_enabled = 1;

static void boot_info(const char *text) {
    boot_log_info(text);
}

static void boot_debug(const char *text) {
    boot_log_debug(text);
}

void boot_init_set_optional_enabled(int enabled) {
    optional_steps_enabled = enabled ? 1 : 0;
}

int boot_init_optional_enabled(void) {
    return optional_steps_enabled;
}

struct boot_init_phase_desc {
    const char *name;
    const struct boot_init_step *start;
    const struct boot_init_step *end;
};

#define DECLARE_PHASE_BOUNDS(phase) \
    extern const struct boot_init_step __start_boot_init_##phase[]; \
    extern const struct boot_init_step __stop_boot_init_##phase[];

BOOT_INIT_PHASES(DECLARE_PHASE_BOUNDS)
#undef DECLARE_PHASE_BOUNDS

static const struct boot_init_phase_desc boot_phase_table[BOOT_INIT_PHASE_COUNT] = {
#define PHASE_ENTRY(phase) \
    [BOOT_INIT_PHASE_##phase] = { #phase, __start_boot_init_##phase, __stop_boot_init_##phase },
    BOOT_INIT_PHASES(PHASE_ENTRY)
#undef PHASE_ENTRY
};

static void boot_init_report_phase(enum boot_log_level level,
                                   const char *prefix,
                                   const char *value) {
    if (!boot_log_is_enabled(level)) {
        return;
    }
    boot_log_raw(level, "[boot:init] ");
    boot_log_raw(level, prefix);
    if (value) {
        boot_log_raw(level, value);
    }
    boot_log_newline();
}

static void boot_init_report_step(enum boot_log_level level,
                                  const char *label,
                                  const char *value) {
    if (!boot_log_is_enabled(level)) {
        return;
    }
    boot_log_raw(level, "    ");
    boot_log_raw(level, label);
    boot_log_raw(level, ": ");
    boot_log_raw(level, value ? value : "(unnamed)");
    boot_log_newline();
}

static void boot_init_report_skip(const char *value) {
    if (!boot_log_is_enabled(BOOT_LOG_LEVEL_DEBUG)) {
        return;
    }
    boot_log_raw(BOOT_LOG_LEVEL_DEBUG, "    skip -> ");
    boot_log_raw(BOOT_LOG_LEVEL_DEBUG, value ? value : "(unnamed)");
    boot_log_newline();
}

static void boot_init_report_failure(const char *phase, const char *step_name) {
    boot_log_raw(BOOT_LOG_LEVEL_INFO, "[boot:init] FAILURE in ");
    boot_log_raw(BOOT_LOG_LEVEL_INFO, phase ? phase : "(unknown)");
    boot_log_raw(BOOT_LOG_LEVEL_INFO, " -> ");
    boot_log_raw(BOOT_LOG_LEVEL_INFO, step_name ? step_name : "(unnamed)");
    boot_log_newline();
}

static int boot_run_step(const char *phase_name, const struct boot_init_step *step) {
    if (!step || !step->fn) {
        return 0;
    }

    if ((step->flags & BOOT_INIT_FLAG_OPTIONAL) && !boot_init_optional_enabled()) {
        boot_init_report_skip(step->name);
        return 0;
    }

    boot_init_report_step(BOOT_LOG_LEVEL_DEBUG, "step", step->name);
    int rc = step->fn();
    if (rc != 0) {
        boot_init_report_failure(phase_name, step->name);
        kernel_panic("Boot init step failed");
    }
    return rc;
}

int boot_init_run_phase(enum boot_init_phase phase) {
    if (phase < 0 || phase >= BOOT_INIT_PHASE_COUNT) {
        return -1;
    }

    const struct boot_init_phase_desc *desc = &boot_phase_table[phase];
    if (!desc->start || !desc->end) {
        return 0;
    }

    boot_init_report_phase(BOOT_LOG_LEVEL_DEBUG, "phase start -> ", desc->name);
    const struct boot_init_step *cursor = desc->start;
    while (cursor < desc->end) {
        boot_run_step(desc->name, cursor);
        cursor++;
    }
    boot_init_report_phase(BOOT_LOG_LEVEL_INFO, "phase complete -> ", desc->name);
    return 0;
}

int boot_init_run_all(void) {
    for (int phase = 0; phase < BOOT_INIT_PHASE_COUNT; phase++) {
        int rc = boot_init_run_phase((enum boot_init_phase)phase);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

static int command_line_has_token(const char *cmdline, const char *token) {
    if (!cmdline || !token) {
        return 0;
    }

    size_t token_len = strlen(token);
    if (token_len == 0) {
        return 0;
    }

    const char *cursor = cmdline;
    while (*cursor) {
        while (*cursor == ' ') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        const char *start = cursor;
        while (*cursor && *cursor != ' ') {
            cursor++;
        }

        size_t len = (size_t)(cursor - start);
        if (len == token_len && strncmp(start, token, token_len) == 0) {
            return 1;
        }
    }

    return 0;
}

/* Early hardware phase --------------------------------------------------- */
static int boot_step_serial_init(void) {
    if (serial_init_com1() != 0) {
        boot_info("ERROR: Serial initialization failed");
        return -1;
    }
    boot_log_attach_serial();
    boot_debug("Serial console ready on COM1");
    return 0;
}

static int boot_step_boot_banner(void) {
    boot_info("SlopOS Kernel Started!");
    boot_info("Booting via Limine Protocol...");
    return 0;
}

static int boot_step_limine_protocol(void) {
    boot_debug("Initializing Limine protocol interface...");
    if (init_limine_protocol() != 0) {
        boot_info("ERROR: Limine protocol initialization failed");
        return -1;
    }
    boot_info("Limine protocol interface ready.");

    if (!is_memory_map_available()) {
        boot_info("ERROR: Limine did not provide a memory map");
        return -1;
    }

    const struct limine_memmap_response *limine_memmap = limine_get_memmap_response();
    if (!limine_memmap) {
        boot_info("ERROR: Limine memory map response pointer is NULL");
        return -1;
    }

    boot_ctx.memmap = limine_memmap;

    if (is_hhdm_available()) {
        boot_ctx.hhdm_offset = get_hhdm_offset();
    } else {
        boot_ctx.hhdm_offset = 0;
        boot_info("WARNING: Limine did not report an HHDM offset");
    }

    boot_ctx.cmdline = get_kernel_cmdline();
    if (boot_ctx.cmdline) {
        boot_debug("Boot command line detected");
    } else {
        boot_debug("Boot command line unavailable");
    }

    return 0;
}

static int boot_step_boot_config(void) {
    if (!boot_ctx.cmdline) {
        return 0;
    }

    if (command_line_has_token(boot_ctx.cmdline, "boot.debug=on") ||
        command_line_has_token(boot_ctx.cmdline, "boot.debug=1") ||
        command_line_has_token(boot_ctx.cmdline, "boot.debug=true") ||
        command_line_has_token(boot_ctx.cmdline, "bootdebug=on")) {
        boot_log_set_level(BOOT_LOG_LEVEL_DEBUG);
        boot_info("Boot option: debug logging enabled");
    } else if (command_line_has_token(boot_ctx.cmdline, "boot.debug=off") ||
               command_line_has_token(boot_ctx.cmdline, "boot.debug=0") ||
               command_line_has_token(boot_ctx.cmdline, "boot.debug=false") ||
               command_line_has_token(boot_ctx.cmdline, "bootdebug=off")) {
        boot_log_set_level(BOOT_LOG_LEVEL_INFO);
        boot_debug("Boot option: debug logging disabled");
    }

    if (command_line_has_token(boot_ctx.cmdline, "demo=off") ||
        command_line_has_token(boot_ctx.cmdline, "demo=disabled") ||
        command_line_has_token(boot_ctx.cmdline, "video=off") ||
        command_line_has_token(boot_ctx.cmdline, "no-demo")) {
        boot_init_set_optional_enabled(0);
        boot_info("Boot option: framebuffer demo disabled");
    } else if (command_line_has_token(boot_ctx.cmdline, "demo=on") ||
               command_line_has_token(boot_ctx.cmdline, "demo=enabled")) {
        boot_init_set_optional_enabled(1);
        boot_info("Boot option: framebuffer demo enabled");
    }

    return 0;
}

BOOT_INIT_STEP(early_hw, "serial", boot_step_serial_init);
BOOT_INIT_STEP(early_hw, "boot banner", boot_step_boot_banner);
BOOT_INIT_STEP(early_hw, "limine", boot_step_limine_protocol);
BOOT_INIT_STEP(early_hw, "boot config", boot_step_boot_config);

/* Memory phase ----------------------------------------------------------- */
static int boot_step_memory_init(void) {
    if (!boot_ctx.memmap) {
        boot_info("ERROR: Memory map not available");
        return -1;
    }

    boot_debug("Initializing memory management from Limine data...");
    if (init_memory_system(boot_ctx.memmap, boot_ctx.hhdm_offset) != 0) {
        boot_info("ERROR: Memory system initialization failed");
        return -1;
    }
    boot_info("Memory management initialized.");
    return 0;
}

static int boot_step_memory_verify(void) {
    uint64_t stack_ptr;
    __asm__ volatile ("movq %%rsp, %0" : "=r" (stack_ptr));

    if (boot_log_is_enabled(BOOT_LOG_LEVEL_DEBUG)) {
        boot_debug("Stack pointer read successfully!");
        kprint("Current Stack Pointer: ");
        kprint_hex(stack_ptr);
        kprintln("");

        void *current_ip = __builtin_return_address(0);
        kprint("Kernel Code Address: ");
        kprint_hex((uint64_t)current_ip);
        kprintln("");

        if ((uint64_t)current_ip >= KERNEL_VIRTUAL_BASE) {
            boot_debug("Running in higher-half virtual memory - CORRECT");
        } else {
            boot_info("WARNING: Not running in higher-half virtual memory");
        }
    }

    return 0;
}

BOOT_INIT_STEP(memory, "memory init", boot_step_memory_init);
BOOT_INIT_STEP(memory, "address verification", boot_step_memory_verify);

/* Driver phase ----------------------------------------------------------- */
static int boot_step_debug_subsystem(void) {
    debug_init();
    boot_debug("Debug subsystem initialized.");
    return 0;
}

static int boot_step_gdt_setup(void) {
    boot_debug("Initializing GDT/TSS...");
    gdt_init();
    boot_debug("GDT/TSS initialized.");
    return 0;
}

static int boot_step_idt_setup(void) {
    boot_debug("Initializing IDT...");
    idt_init();
    safe_stack_init();
    idt_load();
    boot_debug("IDT initialized and loaded.");
    return 0;
}

static int boot_step_pic_setup(void) {
    boot_debug("Initializing PIC for interrupt control...");
    pic_init();
    boot_debug("PIC initialized.");
    return 0;
}

static int boot_step_irq_setup(void) {
    boot_debug("Configuring IRQ dispatcher...");
    irq_init();
    boot_debug("IRQ dispatcher ready.");
    return 0;
}

static int boot_step_apic_setup(void) {
    boot_debug("Detecting Local APIC...");
    if (apic_detect()) {
        boot_debug("Initializing Local APIC...");
        if (apic_init() == 0) {
            boot_debug("Local APIC initialized, masking legacy PIC.");
            disable_pic();
        } else {
            boot_info("WARNING: APIC initialization failed, retaining PIC.");
        }
    } else {
        boot_debug("Local APIC unavailable, continuing with PIC.");
    }
    return 0;
}

static int boot_step_interrupt_tests(void) {
    struct interrupt_test_config test_config;
    interrupt_test_config_init_defaults(&test_config);

    if (boot_ctx.cmdline) {
        interrupt_test_config_parse_cmdline(&test_config, boot_ctx.cmdline);
    }

    if (test_config.enabled && test_config.suite_mask == 0) {
        boot_info("INTERRUPT_TEST: No suites selected, skipping execution");
        test_config.enabled = 0;
        test_config.shutdown_on_complete = 0;
    }

    if (!test_config.enabled) {
        boot_debug("INTERRUPT_TEST: Harness disabled");
        return 0;
    }

    boot_info("INTERRUPT_TEST: Running interrupt harness");

    if (boot_log_is_enabled(BOOT_LOG_LEVEL_DEBUG)) {
        kprint("INTERRUPT_TEST: Suites -> ");
        kprintln(interrupt_test_suite_string(test_config.suite_mask));

        kprint("INTERRUPT_TEST: Verbosity -> ");
        kprintln(interrupt_test_verbosity_string(test_config.verbosity));

        kprint("INTERRUPT_TEST: Timeout (ms) -> ");
        kprint_dec(test_config.timeout_ms);
        kprintln("");
    }

    interrupt_test_init(&test_config);
    int passed = run_all_interrupt_tests(&test_config);
    struct test_stats *stats = test_get_stats();
    int failed_tests = stats ? stats->failed_tests : 0;
    interrupt_test_cleanup();

    if (boot_log_is_enabled(BOOT_LOG_LEVEL_DEBUG)) {
        kprint("INTERRUPT_TEST: Boot run passed tests -> ");
        kprint_dec(passed);
        kprintln("");
    }

    if (test_config.shutdown_on_complete) {
        boot_debug("INTERRUPT_TEST: Auto shutdown enabled after harness");
        interrupt_test_request_shutdown(failed_tests);
    }

    if (failed_tests > 0) {
        boot_info("INTERRUPT_TEST: Failures detected");
    } else {
        boot_info("INTERRUPT_TEST: Completed successfully");
    }
    return 0;
}

BOOT_INIT_STEP(drivers, "debug", boot_step_debug_subsystem);
BOOT_INIT_STEP(drivers, "gdt/tss", boot_step_gdt_setup);
BOOT_INIT_STEP(drivers, "idt", boot_step_idt_setup);
BOOT_INIT_STEP(drivers, "pic", boot_step_pic_setup);
BOOT_INIT_STEP(drivers, "irq dispatcher", boot_step_irq_setup);
BOOT_INIT_STEP(drivers, "apic", boot_step_apic_setup);
BOOT_INIT_STEP(drivers, "interrupt tests", boot_step_interrupt_tests);

/* Services phase --------------------------------------------------------- */
static int boot_step_ramfs_init(void) {
    if (ramfs_init() != 0) {
        boot_info("ERROR: RamFS initialization failed");
        return -1;
    }
    boot_debug("RamFS initialized.");
    return 0;
}

static int boot_step_task_manager_init(void) {
    boot_debug("Initializing task manager...");
    if (init_task_manager() != 0) {
        boot_info("ERROR: Task manager initialization failed");
        return -1;
    }
    boot_debug("Task manager initialized.");
    return 0;
}

static int boot_step_scheduler_init(void) {
    boot_debug("Initializing scheduler subsystem...");
    if (init_scheduler() != 0) {
        boot_info("ERROR: Scheduler initialization failed");
        return -1;
    }
    boot_debug("Scheduler initialized.");
    return 0;
}

static int boot_step_shell_task(void) {
    boot_debug("Creating shell task...");
    uint32_t shell_task_id = task_create("shell", shell_main, NULL, 5, 0x02);
    if (shell_task_id == INVALID_TASK_ID) {
        boot_info("ERROR: Failed to create shell task");
        return -1;
    }

    task_t *shell_task_info;
    if (task_get_info(shell_task_id, &shell_task_info) != 0) {
        boot_info("ERROR: Failed to get shell task info");
        return -1;
    }

    if (schedule_task(shell_task_info) != 0) {
        boot_info("ERROR: Failed to schedule shell task");
        return -1;
    }

    boot_debug("Shell task created and scheduled successfully!");
    return 0;
}

static int boot_step_idle_task(void) {
    boot_debug("Creating idle task...");
    if (create_idle_task() != 0) {
        boot_info("ERROR: Failed to create idle task");
        return -1;
    }
    boot_debug("Idle task ready.");
    return 0;
}

static int boot_step_mark_kernel_ready(void) {
    kernel_initialized = 1;
    boot_info("Kernel core services initialized.");
    return 0;
}

BOOT_INIT_STEP(services, "ramfs", boot_step_ramfs_init);
BOOT_INIT_STEP(services, "task manager", boot_step_task_manager_init);
BOOT_INIT_STEP(services, "scheduler", boot_step_scheduler_init);
BOOT_INIT_STEP(services, "shell task", boot_step_shell_task);
BOOT_INIT_STEP(services, "idle task", boot_step_idle_task);
BOOT_INIT_STEP(services, "mark ready", boot_step_mark_kernel_ready);

/* Optional/demo phase ---------------------------------------------------- */
static int boot_step_framebuffer_demo(void) {
    boot_log_debug("Graphics demo: initializing framebuffer");
    if (framebuffer_init() != 0) {
        boot_log_debug("WARNING: Framebuffer initialization failed - no graphics available");
        return 0;
    }

    framebuffer_info_t *fb_info = framebuffer_get_info();
    if (fb_info && fb_info->virtual_addr && fb_info->virtual_addr != (void*)fb_info->physical_addr) {
        if (boot_log_is_enabled(BOOT_LOG_LEVEL_DEBUG)) {
            kprint("Graphics: Framebuffer using translated virtual address ");
            kprint_hex((uint64_t)fb_info->virtual_addr);
            kprintln(" (translation verified)");
        }
    }

    framebuffer_clear(0x001122FF);
    font_console_init(0xFFFFFFFF, 0x00000000);

    graphics_draw_rect_filled(20, 20, 300, 150, 0xFF0000FF);
    graphics_draw_rect_filled(700, 20, 300, 150, 0x00FF00FF);
    graphics_draw_circle(512, 384, 100, 0xFFFF00FF);
    graphics_draw_rect_filled(0, 0, 1024, 4, 0xFFFFFFFF);
    graphics_draw_rect_filled(0, 764, 1024, 4, 0xFFFFFFFF);
    graphics_draw_rect_filled(0, 0, 4, 768, 0xFFFFFFFF);
    graphics_draw_rect_filled(1020, 0, 4, 768, 0xFFFFFFFF);

    font_draw_string(20, 600, "*** SLOPOS GRAPHICS SYSTEM OPERATIONAL ***", 0xFFFFFFFF, 0x00000000);
    font_draw_string(20, 616, "Framebuffer: WORKING | Resolution: 1024x768", 0xFFFFFFFF, 0x00000000);
    font_draw_string(20, 632, "Memory: OK | Graphics: OK | Text: OK", 0xFFFFFFFF, 0x00000000);

    boot_log_debug("Graphics demo: draw complete");
    return 0;
}

BOOT_INIT_OPTIONAL_STEP(optional, "framebuffer demo", boot_step_framebuffer_demo);

/*
 * Main 64-bit kernel entry point
 * Called from assembly code after successful boot via Limine bootloader
 *
 * This is the Limine protocol version - no parameters needed,
 * Limine provides boot information via static request structures.
 */
void kernel_main(void) {
    if (boot_init_run_all() != 0) {
        kernel_panic("Boot initialization failed");
    }

    if (boot_log_is_enabled(BOOT_LOG_LEVEL_INFO)) {
        boot_log_newline();
    }
    boot_info("=== KERNEL BOOT SUCCESSFUL ===");
    boot_info("Operational subsystems: serial, interrupts, memory, scheduler, shell");
    if (!boot_init_optional_enabled()) {
        boot_info("Optional graphics demo: skipped");
    }
    boot_info("Kernel initialization complete - ALL SYSTEMS OPERATIONAL!");
    boot_info("Starting scheduler...");
    if (boot_log_is_enabled(BOOT_LOG_LEVEL_INFO)) {
        boot_log_newline();
    }
    
    // Start scheduler (this will switch to shell task and run it)
    if (start_scheduler() != 0) {
        kprintln("ERROR: Scheduler startup failed");
        kernel_panic("Scheduler startup failed");
    }
    
    // If we get here, scheduler has exited (shouldn't happen in normal operation)
    kprintln("WARNING: Scheduler exited unexpectedly");
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
        boot_log_info("SlopOS: Kernel status - INITIALIZED");
    } else {
        boot_log_info("SlopOS: Kernel status - INITIALIZING");
    }
}
