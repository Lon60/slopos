/*
 * SlopOS Splash Screen Implementation
 * Displays boot splash screen with logo and loading progress
 */

#include <stdint.h>
#include <stddef.h>
#include "splash.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"
#include "../drivers/serial.h"

/* ========================================================================
 * SPLASH SCREEN IMPLEMENTATION
 * ======================================================================== */

/*
 * Simple delay function for splash screen timing
 * Note: This is a busy-wait delay, suitable for early boot
 */
static void splash_delay_ms(uint32_t milliseconds) {
    // Simple busy-wait delay (approximately 1ms per 1000000 iterations on typical hardware)
    // This is rough timing but sufficient for splash screen display
    // TODO: Replace with PIT-backed timing once the timer driver is available this early.
    volatile uint64_t cycles = (uint64_t)milliseconds * 1000000;
    for (volatile uint64_t i = 0; i < cycles; i++) {
        __asm__ volatile ("nop");
    }
}

/*
 * Draw SlopOS logo as ASCII art using graphics primitives
 */
static int splash_draw_logo(int center_x, int center_y) {
    if (!framebuffer_is_initialized()) {
        return -1;
    }

    // Get framebuffer dimensions
    uint32_t width = framebuffer_get_width();
    uint32_t height = framebuffer_get_height();

    // Calculate logo dimensions and position
    int logo_width = 300;
    int logo_height = 150;
    int logo_x = center_x - logo_width / 2;
    int logo_y = center_y - logo_height / 2;

    // Draw main logo rectangle with gradient effect
    for (int y = 0; y < logo_height; y++) {
        uint32_t gradient_intensity = 0x40 + (y * 0x80 / logo_height);
        uint32_t gradient_color = (gradient_intensity << 24) | (gradient_intensity << 16) | 0xFF;
        graphics_draw_hline(logo_x, logo_x + logo_width, logo_y + y, gradient_color);
    }

    // Draw logo border
    graphics_draw_rect(logo_x - 2, logo_y - 2, logo_width + 4, logo_height + 4, SPLASH_LOGO_COLOR);

    // Draw stylized "SLOP" letters using geometric shapes
    int letter_spacing = 60;
    int letter_start_x = logo_x + 30;
    int letter_y = logo_y + 40;
    int letter_height = 70;

    // Letter S - curves approximated with rectangles
    graphics_draw_rect_filled(letter_start_x, letter_y, 40, 15, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x, letter_y + 25, 40, 15, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x, letter_y + 55, 40, 15, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x, letter_y, 15, 40, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x + 25, letter_y + 30, 15, 40, SPLASH_LOGO_COLOR);

    // Letter L
    letter_start_x += letter_spacing;
    graphics_draw_rect_filled(letter_start_x, letter_y, 15, letter_height, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x, letter_y + letter_height - 15, 40, 15, SPLASH_LOGO_COLOR);

    // Letter O
    letter_start_x += letter_spacing;
    graphics_draw_rect_filled(letter_start_x, letter_y, 40, 15, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x, letter_y + letter_height - 15, 40, 15, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x, letter_y, 15, letter_height, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x + 25, letter_y, 15, letter_height, SPLASH_LOGO_COLOR);

    // Letter P
    letter_start_x += letter_spacing;
    graphics_draw_rect_filled(letter_start_x, letter_y, 15, letter_height, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x, letter_y, 40, 15, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x, letter_y + 25, 40, 15, SPLASH_LOGO_COLOR);
    graphics_draw_rect_filled(letter_start_x + 25, letter_y, 15, 40, SPLASH_LOGO_COLOR);

    return 0;
}

/*
 * Draw progress bar
 */
static int splash_draw_progress_bar(int x, int y, int width, int height, int progress) {
    if (!framebuffer_is_initialized()) {
        return -1;
    }

    // Draw progress bar background
    graphics_draw_rect_filled(x, y, width, height, 0x333333FF);

    // Draw progress bar border
    graphics_draw_rect(x - 1, y - 1, width + 2, height + 2, SPLASH_LOGO_COLOR);

    // Draw progress fill
    if (progress > 0) {
        int fill_width = (width * progress) / 100;
        graphics_draw_rect_filled(x, y, fill_width, height, SPLASH_PROGRESS_COLOR);
    }

    return 0;
}

// Global splash screen state
static int splash_active = 0;
static int current_progress = 0;

/*
 * Initialize splash screen (without fake animation)
 */
int splash_show_boot_screen(void) {
    if (!framebuffer_is_initialized()) {
        kprintln("SPLASH: Framebuffer not initialized");
        return -1;
    }

    kprintln("SPLASH: Displaying boot splash screen...");

    // Clear screen with splash background color
    framebuffer_clear(SPLASH_BG_COLOR);

    // Get screen dimensions
    uint32_t width = framebuffer_get_width();
    uint32_t height = framebuffer_get_height();
    int center_x = width / 2;
    int center_y = height / 2;

    // Draw logo
    splash_draw_logo(center_x, center_y - 80);

    // Draw title text
    font_draw_string(center_x - 80, center_y + 100, "SlopOS v0.000069", SPLASH_TEXT_COLOR, 0x00000000);
    font_draw_string(center_x - 120, center_y + 120, "the ultimate vibe slop experience", SPLASH_TEXT_COLOR, 0x00000000);

    // Draw loading message
    font_draw_string(center_x - 40, center_y + 160, "Initializing...", SPLASH_TEXT_COLOR, 0x00000000);

    // Draw initial progress bar at 0%
    int progress_bar_width = 300;
    int progress_bar_height = 20;
    int progress_bar_x = center_x - progress_bar_width / 2;
    int progress_bar_y = center_y + 200;

    splash_draw_progress_bar(progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, 0);

    // Mark splash as active and reset progress
    splash_active = 1;
    current_progress = 0;

    kprintln("SPLASH: Boot splash screen initialized");
    return 0;
}

/*
 * Report progress during boot (called from kernel initialization)
 */
int splash_report_progress(int progress, const char *message) {
    if (!splash_active || !framebuffer_is_initialized()) {
        return -1;
    }

    // Update progress
    current_progress = progress;
    if (current_progress > 100) current_progress = 100;

    kprint("SPLASH: Progress ");
    kprint_dec(current_progress);
    kprint("% - ");
    if (message) {
        kprintln(message);
    } else {
        kprintln("...");
    }

    // Update the visual progress bar and message
    int result = splash_update_progress(current_progress, message);

    // Add realistic delay between steps (300-600ms)
    // Different delays for different types of operations to simulate realistic loading
    uint32_t delay_ms = 300; // Base delay

    // Vary delay based on the type of operation (simulated complexity)
    if (current_progress <= 20) {
        delay_ms = 400; // Graphics initialization takes a bit longer
    } else if (current_progress <= 50) {
        delay_ms = 350; // System setup operations
    } else if (current_progress <= 70) {
        delay_ms = 500; // Hardware detection/PCI enumeration takes longer
    } else if (current_progress <= 90) {
        delay_ms = 450; // Scheduler setup
    } else {
        delay_ms = 600; // Final completion steps
    }

    // Apply the delay
    splash_delay_ms(delay_ms);

    return result;
}

/*
 * Mark splash screen as complete
 */
int splash_finish(void) {
    if (splash_active) {
        splash_report_progress(100, "Boot complete");
        splash_active = 0;
        kprintln("SPLASH: Boot splash screen complete");
    }
    return 0;
}

/*
 * Update splash screen with loading progress
 */
int splash_update_progress(int progress, const char *message) {
    if (!framebuffer_is_initialized()) {
        return -1;
    }

    // Get screen dimensions
    uint32_t width = framebuffer_get_width();
    uint32_t height = framebuffer_get_height();
    int center_x = width / 2;
    int center_y = height / 2;

    // Clear previous message area
    graphics_draw_rect_filled(center_x - 150, center_y + 155, 300, 20, SPLASH_BG_COLOR);

    // Draw new message
    if (message) {
        font_draw_string(center_x - 70, center_y + 160, message, SPLASH_TEXT_COLOR, 0x00000000);
    }

    // Update progress bar
    int progress_bar_width = 300;
    int progress_bar_height = 20;
    int progress_bar_x = center_x - progress_bar_width / 2;
    int progress_bar_y = center_y + 200;

    splash_draw_progress_bar(progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, progress);

    return 0;
}

/*
 * Clear splash screen
 */
int splash_clear(void) {
    if (!framebuffer_is_initialized()) {
        return -1;
    }

    // Clear screen to black
    framebuffer_clear(0x00000000);
    return 0;
}

/*
 * Show splash screen with simple delay
 */
int splash_show_with_delay(void) {
    int result = splash_show_boot_screen();
    if (result == 0) {
        splash_delay_ms(SPLASH_DISPLAY_TIME_MS);
    }
    return result;
}
