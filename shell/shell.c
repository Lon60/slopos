/*
 * SlopOS Shell Implementation
 * Main REPL (Read-Eval-Print Loop) for user interaction
 */

#include "shell.h"
#include "../drivers/tty.h"
#include "../drivers/serial.h"

#include <stddef.h>

/* ========================================================================
 * COMMAND EXECUTION (STUB)
 * ======================================================================== */

/*
 * Execute a command (placeholder for now)
 * Will be enhanced in Task 05 (parser) and Task 06 (commands)
 */
void shell_execute_command(const char *line) {
    if (!line) {
        return;
    }
    
    /* For now, just echo the command */
    kprint("Command: ");
    kprint(line);
    kprintln("");
}

/* ========================================================================
 * SHELL MAIN LOOP (REPL)
 * ======================================================================== */

/*
 * Main shell entry point
 * Called as task entry function
 */
void shell_main(void *arg) {
    (void)arg;  /* Unused parameter */
    
    /* Print welcome message (optional) */
    kprintln("");
    kprintln("SlopOS Shell v0.1");
    kprintln("");
    
    /* REPL loop */
    while (1) {
        /* Display prompt */
        kprint("$ ");
        
        /* Read line from keyboard */
        char line_buffer[256];
        size_t line_length = tty_read_line(line_buffer, sizeof(line_buffer));
        
        /* Handle empty lines */
        if (line_length == 0) {
            /* Empty line - just re-prompt */
            continue;
        }
        
        /* Execute command */
        shell_execute_command(line_buffer);
    }
    
    /* Should never reach here (infinite loop) */
}

