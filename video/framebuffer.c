/*
 * SlopOS Framebuffer Driver - UEFI GOP Framebuffer Management
 * Handles initialization and management of the system framebuffer
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"

/* Forward declarations */
void kernel_panic(const char *message);
int get_framebuffer_info(uint64_t *addr, uint32_t *width, uint32_t *height,
                        uint32_t *pitch, uint8_t *bpp);

/* ========================================================================
 * FRAMEBUFFER CONSTANTS AND STRUCTURES
 * ======================================================================== */

/* Supported pixel formats */
#define PIXEL_FORMAT_RGB    0x01   /* Red-Green-Blue */
#define PIXEL_FORMAT_BGR    0x02   /* Blue-Green-Red */
#define PIXEL_FORMAT_RGBA   0x03   /* Red-Green-Blue-Alpha */
#define PIXEL_FORMAT_BGRA   0x04   /* Blue-Green-Red-Alpha */

/* Maximum framebuffer dimensions */
#define MAX_FRAMEBUFFER_WIDTH   4096
#define MAX_FRAMEBUFFER_HEIGHT  4096
#define MIN_FRAMEBUFFER_WIDTH   320
#define MIN_FRAMEBUFFER_HEIGHT  240

/* Color depths */
#define COLOR_DEPTH_16    16   /* 16-bit color */
#define COLOR_DEPTH_24    24   /* 24-bit color */
#define COLOR_DEPTH_32    32   /* 32-bit color */

/* Common colors (32-bit RGBA format) */
#define COLOR_BLACK       0x00000000
#define COLOR_WHITE       0xFFFFFFFF
#define COLOR_RED         0xFF0000FF
#define COLOR_GREEN       0x00FF00FF
#define COLOR_BLUE        0x0000FFFF
#define COLOR_YELLOW      0xFFFF00FF
#define COLOR_CYAN        0x00FFFFFF
#define COLOR_MAGENTA     0xFF00FFFF
#define COLOR_GRAY        0x808080FF
#define COLOR_DARK_GRAY   0x404040FF
#define COLOR_LIGHT_GRAY  0xC0C0C0FF

/* Framebuffer information structure */
typedef struct {
    uint64_t physical_addr;     /* Physical address of framebuffer */
    void *virtual_addr;         /* Virtual address of framebuffer */
    uint32_t width;             /* Width in pixels */
    uint32_t height;            /* Height in pixels */
    uint32_t pitch;             /* Bytes per scanline */
    uint8_t bpp;                /* Bits per pixel */
    uint8_t pixel_format;       /* Pixel format */
    uint32_t buffer_size;       /* Total buffer size in bytes */
    uint8_t initialized;        /* Initialization status */
} framebuffer_info_t;

/* Global framebuffer state */
static framebuffer_info_t fb_info = {0};

/* ========================================================================
 * PIXEL FORMAT UTILITIES
 * ======================================================================== */

/*
 * Determine pixel format from bits per pixel
 */
static uint8_t determine_pixel_format(uint8_t bpp) {
    switch (bpp) {
        case 16:
            return PIXEL_FORMAT_RGB;    /* Assume RGB565 */
        case 24:
            return PIXEL_FORMAT_RGB;    /* RGB888 */
        case 32:
            return PIXEL_FORMAT_RGBA;   /* RGBA8888 or RGBX8888 */
        default:
            return PIXEL_FORMAT_RGB;    /* Default fallback */
    }
}

/*
 * Calculate bytes per pixel from bits per pixel
 */
static uint32_t bytes_per_pixel(uint8_t bpp) {
    return (bpp + 7) / 8;  /* Round up to nearest byte */
}

/*
 * Validate framebuffer dimensions
 */
static int validate_dimensions(uint32_t width, uint32_t height) {
    if (width < MIN_FRAMEBUFFER_WIDTH || width > MAX_FRAMEBUFFER_WIDTH) {
        return 0;
    }
    if (height < MIN_FRAMEBUFFER_HEIGHT || height > MAX_FRAMEBUFFER_HEIGHT) {
        return 0;
    }
    return 1;
}

/* ========================================================================
 * FRAMEBUFFER INITIALIZATION
 * ======================================================================== */

/*
 * Initialize framebuffer from Multiboot2 information
 */
int framebuffer_init(void) {
    uint64_t phys_addr;
    uint32_t width, height, pitch;
    uint8_t bpp;

    kprintln("Initializing framebuffer...");

    /* Get framebuffer info from Multiboot2 */
    if (!get_framebuffer_info(&phys_addr, &width, &height, &pitch, &bpp)) {
        kprintln("ERROR: No framebuffer available from bootloader");
        return -1;
    }

    kprint("Framebuffer found at physical address: ");
    kprint_hex(phys_addr);
    kprintln("");

    /* Validate parameters */
    if (phys_addr == 0) {
        kprintln("ERROR: Invalid framebuffer address");
        return -1;
    }

    if (!validate_dimensions(width, height)) {
        kprintln("ERROR: Invalid framebuffer dimensions");
        return -1;
    }

    if (bpp != 16 && bpp != 24 && bpp != 32) {
        kprintln("ERROR: Unsupported color depth");
        return -1;
    }

    /* Calculate buffer size */
    uint32_t buffer_size = pitch * height;
    if (buffer_size == 0 || buffer_size > 64 * 1024 * 1024) {  /* Max 64MB */
        kprintln("ERROR: Invalid framebuffer size");
        return -1;
    }

    /* For now, use direct physical mapping in higher half */
    /* TODO: Map through proper virtual memory management */
    void *virtual_addr = (void*)(KERNEL_VIRTUAL_BASE + phys_addr);

    /* Initialize framebuffer info */
    fb_info.physical_addr = phys_addr;
    fb_info.virtual_addr = virtual_addr;
    fb_info.width = width;
    fb_info.height = height;
    fb_info.pitch = pitch;
    fb_info.bpp = bpp;
    fb_info.pixel_format = determine_pixel_format(bpp);
    fb_info.buffer_size = buffer_size;
    fb_info.initialized = 1;

    kprint("Framebuffer initialized: ");
    kprint_decimal(width);
    kprint("x");
    kprint_decimal(height);
    kprint(" @ ");
    kprint_decimal(bpp);
    kprintln(" bpp");

    return 0;
}

/*
 * Get framebuffer information
 */
framebuffer_info_t *framebuffer_get_info(void) {
    if (!fb_info.initialized) {
        return NULL;
    }
    return &fb_info;
}

/*
 * Check if framebuffer is initialized
 */
int framebuffer_is_initialized(void) {
    return fb_info.initialized;
}

/* ========================================================================
 * BASIC FRAMEBUFFER OPERATIONS
 * ======================================================================== */

/*
 * Clear framebuffer to specified color
 */
void framebuffer_clear(uint32_t color) {
    if (!fb_info.initialized) {
        return;
    }

    uint8_t *buffer = (uint8_t*)fb_info.virtual_addr;
    uint32_t bytes_pp = bytes_per_pixel(fb_info.bpp);

    /* Convert color based on pixel format */
    uint32_t pixel_value = color;
    if (fb_info.pixel_format == PIXEL_FORMAT_BGR ||
        fb_info.pixel_format == PIXEL_FORMAT_BGRA) {
        /* Swap R and B components */
        pixel_value = ((color & 0xFF0000) >> 16) |
                     (color & 0x00FF00) |
                     ((color & 0x0000FF) << 16) |
                     (color & 0xFF000000);
    }

    /* Fill buffer */
    for (uint32_t y = 0; y < fb_info.height; y++) {
        uint8_t *row = buffer + y * fb_info.pitch;

        for (uint32_t x = 0; x < fb_info.width; x++) {
            uint8_t *pixel = row + x * bytes_pp;

            switch (bytes_pp) {
                case 2: /* 16-bit */
                    *(uint16_t*)pixel = (uint16_t)pixel_value;
                    break;
                case 3: /* 24-bit */
                    pixel[0] = (pixel_value >> 16) & 0xFF;
                    pixel[1] = (pixel_value >> 8) & 0xFF;
                    pixel[2] = pixel_value & 0xFF;
                    break;
                case 4: /* 32-bit */
                    *(uint32_t*)pixel = pixel_value;
                    break;
            }
        }
    }
}

/*
 * Set pixel at coordinates
 */
void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_info.initialized) {
        return;
    }

    /* Bounds checking */
    if (x >= fb_info.width || y >= fb_info.height) {
        return;
    }

    uint8_t *buffer = (uint8_t*)fb_info.virtual_addr;
    uint32_t bytes_pp = bytes_per_pixel(fb_info.bpp);
    uint8_t *pixel = buffer + y * fb_info.pitch + x * bytes_pp;

    /* Convert color based on pixel format */
    uint32_t pixel_value = color;
    if (fb_info.pixel_format == PIXEL_FORMAT_BGR ||
        fb_info.pixel_format == PIXEL_FORMAT_BGRA) {
        /* Swap R and B components */
        pixel_value = ((color & 0xFF0000) >> 16) |
                     (color & 0x00FF00) |
                     ((color & 0x0000FF) << 16) |
                     (color & 0xFF000000);
    }

    /* Set pixel */
    switch (bytes_pp) {
        case 2: /* 16-bit */
            *(uint16_t*)pixel = (uint16_t)pixel_value;
            break;
        case 3: /* 24-bit */
            pixel[0] = (pixel_value >> 16) & 0xFF;
            pixel[1] = (pixel_value >> 8) & 0xFF;
            pixel[2] = pixel_value & 0xFF;
            break;
        case 4: /* 32-bit */
            *(uint32_t*)pixel = pixel_value;
            break;
    }
}

/*
 * Get pixel at coordinates
 */
uint32_t framebuffer_get_pixel(uint32_t x, uint32_t y) {
    if (!fb_info.initialized) {
        return 0;
    }

    /* Bounds checking */
    if (x >= fb_info.width || y >= fb_info.height) {
        return 0;
    }

    uint8_t *buffer = (uint8_t*)fb_info.virtual_addr;
    uint32_t bytes_pp = bytes_per_pixel(fb_info.bpp);
    uint8_t *pixel = buffer + y * fb_info.pitch + x * bytes_pp;

    uint32_t color = 0;

    /* Get pixel */
    switch (bytes_pp) {
        case 2: /* 16-bit */
            color = *(uint16_t*)pixel;
            break;
        case 3: /* 24-bit */
            color = (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];
            break;
        case 4: /* 32-bit */
            color = *(uint32_t*)pixel;
            break;
    }

    /* Convert color based on pixel format */
    if (fb_info.pixel_format == PIXEL_FORMAT_BGR ||
        fb_info.pixel_format == PIXEL_FORMAT_BGRA) {
        /* Swap R and B components */
        color = ((color & 0xFF0000) >> 16) |
               (color & 0x00FF00) |
               ((color & 0x0000FF) << 16) |
               (color & 0xFF000000);
    }

    return color;
}

/* ========================================================================
 * FRAMEBUFFER UTILITY FUNCTIONS
 * ======================================================================== */

/*
 * Get framebuffer width
 */
uint32_t framebuffer_get_width(void) {
    return fb_info.initialized ? fb_info.width : 0;
}

/*
 * Get framebuffer height
 */
uint32_t framebuffer_get_height(void) {
    return fb_info.initialized ? fb_info.height : 0;
}

/*
 * Get color depth
 */
uint8_t framebuffer_get_bpp(void) {
    return fb_info.initialized ? fb_info.bpp : 0;
}

/*
 * Create RGBA color value
 */
uint32_t framebuffer_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b | ((uint32_t)a << 24);
}

/*
 * Create RGB color value (with full alpha)
 */
uint32_t framebuffer_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return framebuffer_rgba(r, g, b, 0xFF);
}