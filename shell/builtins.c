#include "builtins.h"

#include <stdint.h>

#include "../drivers/serial.h"
#include "../fs/fileio.h"
#include "../fs/ramfs.h"
#include "../lib/string.h"
#include "../boot/shutdown.h"
#include "../mm/kernel_heap.h"
#include "../mm/page_alloc.h"
#include "../sched/scheduler.h"

static const shell_builtin_t builtin_table[] = {
    { "help",  builtin_help,  "List available commands" },
    { "echo",  builtin_echo,  "Print arguments back to the terminal" },
    { "clear", builtin_clear, "Clear the terminal display" },
    { "halt",  builtin_halt,  "Shut down the kernel" },
    { "info",  builtin_info,  "Show kernel memory and scheduler stats" },
    { "ls",    builtin_ls,    "List directory contents" },
    { "cat",   builtin_cat,   "Display file contents" },
    { "write", builtin_write, "Write text to a file" },
    { "mkdir", builtin_mkdir, "Create a directory" },
    { "rm",    builtin_rm,    "Remove a file" }
};

static const size_t builtin_count = sizeof(builtin_table) / sizeof(builtin_table[0]);

static const char *shell_normalize_path(const char *input, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return NULL;
    }

    if (!input || input[0] == '\0') {
        buffer[0] = '/';
        if (buffer_size > 1) {
            buffer[1] = '\0';
        }
        return buffer;
    }

    if (input[0] == '/') {
        return input;
    }

    size_t length = strlen(input);
    if ((length + 2) > buffer_size) {
        return NULL;
    }

    buffer[0] = '/';
    for (size_t i = 0; i < length; i++) {
        buffer[i + 1] = input[i];
    }
    buffer[length + 1] = '\0';
    return buffer;
}

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

int builtin_help(int argc, char **argv) {
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

int builtin_echo(int argc, char **argv) {
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

int builtin_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* ANSI escape sequence: clear screen and move cursor home */
    kprint("\x1B[2J\x1B[H");
    return 0;
}

int builtin_halt(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintln("Shell requested shutdown. Halting kernel...");
    kernel_shutdown("shell halt");

    return 0;  /* Not reached */
}

int builtin_info(int argc, char **argv) {
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

int builtin_ls(int argc, char **argv) {
    if (argc > 2) {
        kprintln("ls: too many arguments");
        return 1;
    }

    const char *path = "/";
    char path_buffer[128];

    if (argc == 2) {
        const char *normalized = shell_normalize_path(argv[1], path_buffer, sizeof(path_buffer));
        if (!normalized) {
            kprintln("ls: path too long");
            return 1;
        }
        path = normalized;
    }

    ramfs_node_t *node = ramfs_find_node(path);
    if (!node) {
        kprint("ls: cannot access '");
        kprint(path);
        kprintln("': No such file or directory");
        return 1;
    }

    if (node->type == RAMFS_TYPE_FILE) {
        kprint(node->name);
        kprint(" (");
        kprint_decimal((uint64_t)node->size);
        kprintln(" bytes)");
        return 0;
    }

    if (node->type != RAMFS_TYPE_DIRECTORY) {
        kprint("ls: cannot access '");
        kprint(path);
        kprintln("': Not a directory");
        return 1;
    }

    ramfs_node_t **entries = NULL;
    int count = 0;
    if (ramfs_list_directory(path, &entries, &count) != 0) {
        kprint("ls: cannot access '");
        kprint(path);
        kprintln("': Failed to list directory");
        return 1;
    }

    for (int i = 0; i < count; i++) {
        ramfs_node_t *entry = entries[i];
        if (!entry) {
            continue;
        }

        if (entry->type == RAMFS_TYPE_DIRECTORY) {
            kprint("[");
            kprint(entry->name);
            kprintln("]");
        } else if (entry->type == RAMFS_TYPE_FILE) {
            kprint(entry->name);
            kprint(" (");
            kprint_decimal((uint64_t)entry->size);
            kprintln(" bytes)");
        } else {
            kprintln(entry->name);
        }
    }

    if (entries) {
        kfree(entries);
    }

    return 0;
}

int builtin_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintln("cat: missing file operand");
        return 1;
    }
    if (argc > 2) {
        kprintln("cat: too many arguments");
        return 1;
    }

    char path_buffer[128];
    const char *path = shell_normalize_path(argv[1], path_buffer, sizeof(path_buffer));
    if (!path) {
        kprintln("cat: path too long");
        return 1;
    }

    ramfs_node_t *node = ramfs_find_node(path);
    if (!node) {
        kprint("cat: '");
        kprint(path);
        kprintln("': No such file or directory");
        return 1;
    }

    if (node->type != RAMFS_TYPE_FILE) {
        kprint("cat: '");
        kprint(path);
        kprintln("': Is a directory");
        return 1;
    }

    int fd = file_open(path, FILE_OPEN_READ);
    if (fd < 0) {
        kprint("cat: cannot open '");
        kprint(path);
        kprintln("'");
        return 1;
    }

    char buffer[128];
    int saw_data = 0;
    int last_was_newline = 0;
    uint16_t port = serial_get_kernel_output();

    while (1) {
        ssize_t bytes_read = file_read(fd, buffer, sizeof(buffer));
        if (bytes_read < 0) {
            file_close(fd);
            kprint("cat: error reading '");
            kprint(path);
            kprintln("'");
            return 1;
        }
        if (bytes_read == 0) {
            break;
        }

        serial_write(port, buffer, (size_t)bytes_read);
        saw_data = 1;
        last_was_newline = (buffer[bytes_read - 1] == '\n');
    }

    file_close(fd);

    if (!saw_data || !last_was_newline) {
        kprintln("");
    }

    return 0;
}

int builtin_write(int argc, char **argv) {
    if (argc < 2) {
        kprintln("write: missing file operand");
        return 1;
    }
    if (argc < 3) {
        kprintln("write: missing text operand");
        return 1;
    }
    if (argc > 3) {
        kprintln("write: too many arguments");
        return 1;
    }

    char path_buffer[128];
    const char *path = shell_normalize_path(argv[1], path_buffer, sizeof(path_buffer));
    if (!path) {
        kprintln("write: path too long");
        return 1;
    }

    const char *text = argv[2];
    size_t length = strlen(text);

    int fd = file_open(path, FILE_OPEN_WRITE | FILE_OPEN_CREAT);
    if (fd < 0) {
        kprint("write: cannot open '");
        kprint(path);
        kprintln("'");
        return 1;
    }

    if (length > 0) {
        ssize_t written = file_write(fd, text, length);
        if (written < 0 || (size_t)written != length) {
            file_close(fd);
            kprint("write: failed to write to '");
            kprint(path);
            kprintln("'");
            return 1;
        }
    } else {
        file_close(fd);
        if (ramfs_write_file(path, NULL, 0) != 0) {
            kprint("write: failed to truncate '");
            kprint(path);
            kprintln("'");
            return 1;
        }
        return 0;
    }

    file_close(fd);
    return 0;
}

int builtin_mkdir(int argc, char **argv) {
    if (argc < 2) {
        kprintln("mkdir: missing operand");
        return 1;
    }
    if (argc > 2) {
        kprintln("mkdir: too many arguments");
        return 1;
    }

    char path_buffer[128];
    const char *path = shell_normalize_path(argv[1], path_buffer, sizeof(path_buffer));
    if (!path) {
        kprintln("mkdir: path too long");
        return 1;
    }

    ramfs_node_t *created = ramfs_create_directory(path);
    if (!created) {
        ramfs_node_t *existing = ramfs_find_node(path);
        kprint("mkdir: cannot create directory '");
        kprint(path);
        kprint("': ");
        if (existing && existing->type == RAMFS_TYPE_FILE) {
            kprintln("File exists");
        } else {
            kprintln("Failed");
        }
        return 1;
    }

    return 0;
}

int builtin_rm(int argc, char **argv) {
    if (argc < 2) {
        kprintln("rm: missing operand");
        return 1;
    }
    if (argc > 2) {
        kprintln("rm: too many arguments");
        return 1;
    }

    char path_buffer[128];
    const char *path = shell_normalize_path(argv[1], path_buffer, sizeof(path_buffer));
    if (!path) {
        kprintln("rm: path too long");
        return 1;
    }

    ramfs_node_t *node = ramfs_find_node(path);
    if (!node) {
        kprint("rm: cannot remove '");
        kprint(path);
        kprintln("': No such file or directory");
        return 1;
    }

    if (node->type != RAMFS_TYPE_FILE) {
        kprint("rm: cannot remove '");
        kprint(path);
        kprintln("': Is a directory");
        return 1;
    }

    if (file_unlink(path) != 0) {
        kprint("rm: cannot remove '");
        kprint(path);
        kprintln("'");
        return 1;
    }

    return 0;
}
