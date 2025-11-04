#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdint.h>

/* ========================================================================
 * KEYBOARD DRIVER API
 * ======================================================================== */

/*
 * Initialize the keyboard driver
 * Must be called before using keyboard functions
 */
void keyboard_init(void);

/*
 * Handle a raw PS/2 scancode from the keyboard
 * Called from IRQ handler to process scancodes
 * Translates scancode to ASCII and stores in buffer if applicable
 */
void keyboard_handle_scancode(uint8_t scancode);

/*
 * Get next ASCII character from keyboard buffer
 * Returns ASCII character (1-255) if available
 * Returns 0 if no character available
 * Non-blocking function
 */
char keyboard_getchar(void);

/*
 * Check if a character is available in the keyboard buffer
 * Returns non-zero if character available, 0 otherwise
 * Non-blocking function
 */
int keyboard_has_input(void);

/*
 * Check if a character is waiting in the keyboard buffer without
 * modifying interrupt state. Callers must ensure interrupts are disabled.
 */
int keyboard_buffer_pending(void);

/*
 * Get raw scancode from buffer (for debugging)
 * Returns scancode if available, 0 otherwise
 * Non-blocking function
 */
uint8_t keyboard_get_scancode(void);

#endif /* DRIVERS_KEYBOARD_H */

