#include "tty.h"
#include "keyboard.h"
#include "serial.h"

#include <stddef.h>
#include <stdint.h>

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

/*
 * Check if character is printable (can be echoed)
 */
static inline int is_printable(char c) {
    return (c >= 0x20 && c <= 0x7E) || c == '\t';
}

/*
 * Check if character is a control character that needs special handling
 */
static inline int is_control_char(char c) {
    return (c >= 0x00 && c <= 0x1F) || c == 0x7F;
}

/*
 * Attempt to fetch a character from any interactive input source.
 * Prefers PS/2 keyboard scancodes but falls back to the serial console
 * when running without a graphical window.
 */
static int tty_poll_input_char(char *out_char) {
    if (!out_char) {
        return 0;
    }

    if (keyboard_has_input()) {
        *out_char = keyboard_getchar();
        return 1;
    }

    if (serial_data_available(SERIAL_COM1_PORT)) {
        char c = serial_getc(SERIAL_COM1_PORT);

        if (c == '\r') {
            c = '\n';
        } else if (c == 0x7F) {
            c = '\b';
        }

        *out_char = c;
        return 1;
    }

    return 0;
}

/* ========================================================================
 * TTY READLINE IMPLEMENTATION
 * ======================================================================== */

size_t tty_read_line(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    
    /* Ensure we have at least space for null terminator */
    if (buffer_size < 2) {
        buffer[0] = '\0';
        return 0;
    }
    
    size_t pos = 0;  /* Current position in buffer */
    size_t max_pos = buffer_size - 1;  /* Maximum position (leave room for null terminator) */
    
    /* Read characters until Enter is pressed */
    while (1) {
        /* Block until character is available from keyboard or serial */
        char c = 0;
        while (!tty_poll_input_char(&c)) {
            /* Busy wait - could be enhanced with task scheduling later */
        }
        
        /* Handle Enter key - finish line input */
        if (c == '\n' || c == '\r') {
            buffer[pos] = '\0';
            kprint_char('\n');  /* Echo newline */
            return pos;
        }
        
        /* Handle Backspace */
        if (c == '\b') {
            if (pos > 0) {
                /* Remove character from buffer */
                pos--;
                
                /* Erase character visually: backspace, space, backspace */
                kprint_char('\b');
                kprint_char(' ');
                kprint_char('\b');
            }
            /* If buffer is empty, ignore backspace (no character to delete) */
            continue;
        }
        
        /* Handle buffer overflow */
        if (pos >= max_pos) {
            /* Buffer full - ignore new characters (or could beep/alert) */
            continue;
        }
        
        /* Handle printable characters */
        if (is_printable(c)) {
            buffer[pos++] = c;
            kprint_char(c);  /* Echo character */
            continue;
        }
        
        /* Handle other control characters (ignore by default) */
        if (is_control_char(c)) {
            /* Don't echo control characters */
            continue;
        }
        
        /* For any other character, store and echo if it's in printable range */
        if (pos < max_pos) {
            buffer[pos++] = c;
            kprint_char(c);
        }
    }
}
