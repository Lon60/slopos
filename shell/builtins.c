#include "builtins.h"

#include <stdint.h>

#include "../drivers/serial.h"
#include "../lib/string.h"
#include "../boot/shutdown.h"
#include "../mm/page_alloc.h"
#include "../sched/scheduler.h"

static int builtin_help(int argc, char **argv);
static int builtin_echo(int argc, char **argv);
static int builtin_clear(int argc, char **argv);
static int builtin_halt(int argc, char **argv);
static int builtin_info(int argc, char **argv);

static const shell_builtin_t builtin_table[] = {
    { "help",  builtin_help,  "List available commands" },
    { "echo",  builtin_echo,  "Print arguments back to the terminal" },
    { "clear", builtin_clear, "Clear the terminal display" },
    { "halt",  builtin_halt,  "Shut down the kernel" },
    { "info",  builtin_info,  "Show kernel memory and scheduler stats" }
};

static const size_t builtin_count = sizeof(builtin_table) / sizeof(builtin_table[0]);

const shell_builtin_t *shell_builtin_lookup(const char *name) {
    if (!name) {
        return NULL;
    }

    for (size_t i = 0; i < builtin_count; i++) {
        if (strcmp(builtin_table[i].name, name) == 0) {
            return &builtin_table[i];
        }
    }

    return NULL;
}

const shell_builtin_t *shell_builtin_list(size_t *count) {
    if (count) {
        *count = builtin_count;
    }
    return builtin_table;
}

static int builtin_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintln("Available commands:");

    for (size_t i = 0; i < builtin_count; i++) {
        kprint("  ");
        kprint(builtin_table[i].name);
        kprint(" - ");
        if (builtin_table[i].description) {
            kprintln(builtin_table[i].description);
        } else {
            kprintln("(no description)");
        }
    }

    return 0;
}

static int builtin_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i]) {
            kprint(argv[i]);
        }
        if (i + 1 < argc) {
            kprint(" ");
        }
    }

    kprintln("");
    return 0;
}

static int builtin_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* ANSI escape sequence: clear screen and move cursor home */
    kprint("\x1B[2J\x1B[H");
    return 0;
}

static int builtin_halt(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintln("Shell requested shutdown. Halting kernel...");
    kernel_shutdown("shell halt");

    return 0;  /* Not reached */
}

static int builtin_info(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint32_t total_pages = 0;
    uint32_t free_pages = 0;
    uint32_t allocated_pages = 0;
    get_page_allocator_stats(&total_pages, &free_pages, &allocated_pages);

    uint32_t total_tasks = 0;
    uint32_t active_tasks = 0;
    uint64_t task_context_switches = 0;
    get_task_stats(&total_tasks, &active_tasks, &task_context_switches);

    uint64_t scheduler_context_switches = 0;
    uint64_t scheduler_yields = 0;
    uint32_t ready_tasks = 0;
    uint32_t schedule_calls = 0;
    get_scheduler_stats(&scheduler_context_switches, &scheduler_yields,
                        &ready_tasks, &schedule_calls);

    kprintln("Kernel information:");

    kprint("  Memory: total pages=");
    kprint_decimal(total_pages);
    kprint(", free pages=");
    kprint_decimal(free_pages);
    kprint(", allocated pages=");
    kprint_decimal(allocated_pages);
    kprintln("");

    kprint("  Tasks: total=");
    kprint_decimal(total_tasks);
    kprint(", active=");
    kprint_decimal(active_tasks);
    kprint(", ctx switches=");
    kprint_decimal(task_context_switches);
    kprintln("");

    kprint("  Scheduler: switches=");
    kprint_decimal(scheduler_context_switches);
    kprint(", yields=");
    kprint_decimal(scheduler_yields);
    kprint(", ready=");
    kprint_decimal(ready_tasks);
    kprint(", schedule() calls=");
    kprint_decimal(schedule_calls);
    kprintln("");

    return 0;
}
