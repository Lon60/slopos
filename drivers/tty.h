#ifndef DRIVERS_TTY_H
#define DRIVERS_TTY_H

#include <stddef.h>
#include <stdint.h>

/* ========================================================================
 * TERMINAL/TTY API
 * ======================================================================== */

/*
 * Read a complete line from keyboard input
 * Blocks until Enter is pressed
 *
 * Parameters:
 *   buffer - Buffer to store the line (must be at least buffer_size bytes)
 *   buffer_size - Maximum size of buffer (including null terminator)
 *
 * Returns:
 *   Number of characters read (excluding null terminator)
 *   0 for empty line (just Enter) or error
 *
 * Behavior:
 *   - Echoes printable characters as typed
 *   - Handles backspace (deletes last character, erases visually)
 *   - Enter finishes input and returns line
 *   - Buffer overflow is prevented (no memory corruption)
 *   - Line is always null-terminated
 */
size_t tty_read_line(char *buffer, size_t buffer_size);

#endif /* DRIVERS_TTY_H */

