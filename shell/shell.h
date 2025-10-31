#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

/* ========================================================================
 * SHELL API
 * ======================================================================== */

/*
 * Main shell entry point
 * Called as task entry function
 */
void shell_main(void *arg);

/*
 * Execute a command (placeholder for now)
 * Will be enhanced in Task 05 (parser) and Task 06 (commands)
 */
void shell_execute_command(const char *line);

#endif /* SHELL_SHELL_H */

