/*
 * SlopOS Limine Boot Protocol Support
 * Handles Limine bootloader protocol for framebuffer and memory information
 */

#include <stdint.h>
#include <stddef.h>
#include "limine_protocol.h"
#include "constants.h"
#include "log.h"
#include "../drivers/serial.h"

/* ========================================================================
 * LIMINE PROTOCOL REQUESTS
 * ======================================================================== */

/* Set the base revision to 1, the latest supported */
__attribute__((used, section(".limine_requests_start_marker")))
static volatile uint64_t limine_requests_start_marker[1] = {0};

/* Ensure base revision is set - must be in .limine_requests section */
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[3] = {
    0xf9562b2d5c95a6c8,
    0x6a7b384944536bdc,
    1  /* Base revision 1 */
};

/* Request framebuffer from Limine */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 1,
    .response = NULL
};

/* Request memory map from Limine */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL
};

/* Request HHDM (Higher Half Direct Mapping) from Limine */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL
};

/* Request bootloader info from Limine */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0,
    .response = NULL
};

/* Request kernel address from Limine */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL
};

/* Request kernel file for command line access */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_file_request kernel_file_request = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
    .revision = 0,
    .response = NULL
};

/* Mark end of requests */
__attribute__((used, section(".limine_requests_end_marker")))
static volatile uint64_t limine_requests_end_marker[1] = {0};

/* ========================================================================
 * GLOBAL SYSTEM INFORMATION
 * ======================================================================== */

static struct {
    uint64_t total_memory;
    uint64_t available_memory;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint8_t framebuffer_bpp;
    uint64_t hhdm_offset;
    uint64_t kernel_phys_base;
    uint64_t kernel_virt_base;
    int framebuffer_available;
    int memory_map_available;
    int hhdm_available;
    int kernel_cmdline_available;
    char kernel_cmdline[BOOT_CMDLINE_MAX_LEN];
} system_info = {0};

/* ========================================================================
 * LIMINE PROTOCOL PARSING
 * ======================================================================== */

/*
 * Initialize Limine boot protocol
 * Parse all Limine responses and populate system information
 */
int init_limine_protocol(void) {
    boot_log_debug("Limine Protocol: Initializing...");

    /* Check base revision */
    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        boot_log_info("ERROR: Limine base revision not supported!");
        return -1;
    }
    boot_log_debug("Limine Protocol: Base revision supported");

    /* Parse bootloader info */
    if (bootloader_info_request.response != NULL) {
        struct limine_bootloader_info_response *bi = 
            (struct limine_bootloader_info_response *)bootloader_info_request.response;

        BOOT_LOG_BLOCK(BOOT_LOG_LEVEL_DEBUG, {
            kprint("Bootloader: ");
            if (bi->name) kprint((const char *)bi->name);
            kprint(" version ");
            if (bi->version) kprint((const char *)bi->version);
            kprintln("");
        });
    }

    /* Parse HHDM (Higher Half Direct Mapping) */
    if (hhdm_request.response != NULL) {
        struct limine_hhdm_response *hhdm = 
            (struct limine_hhdm_response *)hhdm_request.response;
        
        system_info.hhdm_offset = hhdm->offset;
        system_info.hhdm_available = 1;
        
        BOOT_LOG_BLOCK(BOOT_LOG_LEVEL_DEBUG, {
            kprint("HHDM offset: ");
            kprint_hex(hhdm->offset);
            kprintln("");
        });
    }

    /* Parse kernel address */
    if (kernel_address_request.response != NULL) {
        struct limine_kernel_address_response *ka = 
            (struct limine_kernel_address_response *)kernel_address_request.response;
        
        system_info.kernel_phys_base = ka->physical_base;
        system_info.kernel_virt_base = ka->virtual_base;
        
        BOOT_LOG_BLOCK(BOOT_LOG_LEVEL_DEBUG, {
            kprint("Kernel physical base: ");
            kprint_hex(ka->physical_base);
            kprintln("");
            kprint("Kernel virtual base: ");
            kprint_hex(ka->virtual_base);
            kprintln("");
        });
    }

    /* Parse kernel command line */
    if (kernel_file_request.response != NULL) {
        struct limine_kernel_file_response *kf =
            (struct limine_kernel_file_response *)kernel_file_request.response;

        if (kf->kernel_file && kf->kernel_file->cmdline) {
            const char *cmdline = (const char *)kf->kernel_file->cmdline;
            size_t index = 0;

            while (cmdline[index] != '\0' && index < (BOOT_CMDLINE_MAX_LEN - 1)) {
                system_info.kernel_cmdline[index] = cmdline[index];
                index++;
            }
            system_info.kernel_cmdline[index] = '\0';
            system_info.kernel_cmdline_available = 1;

            if (index > 0) {
                BOOT_LOG_BLOCK(BOOT_LOG_LEVEL_DEBUG, {
                    kprint("Kernel command line: ");
                    kprintln(system_info.kernel_cmdline);
                });
            } else {
                boot_log_debug("Kernel command line: <empty>");
            }
        } else {
            boot_log_debug("Kernel command line: <not provided>");
        }
    } else {
        boot_log_debug("Kernel command line request unavailable");
    }

    /* Parse memory map */
    if (memmap_request.response != NULL) {
        struct limine_memmap_response *memmap = 
            (struct limine_memmap_response *)memmap_request.response;

        uint64_t total = 0;
        uint64_t available = 0;

        BOOT_LOG_BLOCK(BOOT_LOG_LEVEL_DEBUG, {
            kprint("Memory map: ");
            kprint_decimal(memmap->entry_count);
            kprintln(" entries");
        });

        for (uint64_t i = 0; i < memmap->entry_count; i++) {
            struct limine_memmap_entry *entry = 
                (struct limine_memmap_entry *)memmap->entries[i];
            
            total += entry->length;
            
            if (entry->type == LIMINE_MEMMAP_USABLE) {
                available += entry->length;
            }
        }
        
        system_info.total_memory = total;
        system_info.available_memory = available;
        system_info.memory_map_available = 1;
        
        BOOT_LOG_BLOCK(BOOT_LOG_LEVEL_DEBUG, {
            kprint("Total memory: ");
            kprint_decimal(total / (1024 * 1024));
            kprintln(" MB");
            kprint("Available memory: ");
            kprint_decimal(available / (1024 * 1024));
            kprintln(" MB");
        });
    } else {
        boot_log_info("WARNING: No memory map available from Limine");
    }

    /* Parse framebuffer */
    if (framebuffer_request.response != NULL) {
        struct limine_framebuffer_response *fb_resp = 
            (struct limine_framebuffer_response *)framebuffer_request.response;

        if (fb_resp->framebuffer_count > 0) {
            struct limine_framebuffer *fb = 
                (struct limine_framebuffer *)fb_resp->framebuffers[0];
            
            system_info.framebuffer_addr = (uint64_t)fb->address;
            system_info.framebuffer_width = (uint32_t)fb->width;
            system_info.framebuffer_height = (uint32_t)fb->height;
            system_info.framebuffer_pitch = (uint32_t)fb->pitch;
            system_info.framebuffer_bpp = (uint8_t)fb->bpp;
            system_info.framebuffer_available = 1;
            
            BOOT_LOG_BLOCK(BOOT_LOG_LEVEL_DEBUG, {
                kprint("Framebuffer: ");
                kprint_decimal(fb->width);
                kprint("x");
                kprint_decimal(fb->height);
                kprint(" @ ");
                kprint_decimal(fb->bpp);
                kprintln(" bpp");
                kprint("Framebuffer address: ");
                kprint_hex((uint64_t)fb->address);
                kprintln("");
                kprint("Framebuffer pitch: ");
                kprint_decimal(fb->pitch);
                kprintln(" bytes");
            });
        } else {
            boot_log_info("WARNING: No framebuffer provided by Limine");
            return -1;
        }
    } else {
        boot_log_info("ERROR: No framebuffer response from Limine");
        return -1;
    }

    boot_log_debug("Limine Protocol: Initialization complete");
    return 0;
}

/* ========================================================================
 * PUBLIC INTERFACE
 * ======================================================================== */

/*
 * Get framebuffer information
 * Compatible with existing Multiboot2 interface
 */
int get_framebuffer_info(uint64_t *addr, uint32_t *width, uint32_t *height,
                        uint32_t *pitch, uint8_t *bpp) {
    if (!system_info.framebuffer_available) {
        return 0;
    }

    if (addr) *addr = system_info.framebuffer_addr;
    if (width) *width = system_info.framebuffer_width;
    if (height) *height = system_info.framebuffer_height;
    if (pitch) *pitch = system_info.framebuffer_pitch;
    if (bpp) *bpp = system_info.framebuffer_bpp;

    return 1;
}

/*
 * Check if framebuffer is available
 */
int is_framebuffer_available(void) {
    return system_info.framebuffer_available;
}

/*
 * Get total system memory
 */
uint64_t get_total_memory(void) {
    return system_info.total_memory;
}

/*
 * Get available system memory
 */
uint64_t get_available_memory(void) {
    return system_info.available_memory;
}

/*
 * Get memory map availability
 */
int is_memory_map_available(void) {
    return system_info.memory_map_available;
}

/*
 * Get HHDM offset
 */
uint64_t get_hhdm_offset(void) {
    return system_info.hhdm_offset;
}

/*
 * Check if HHDM is available
 */
int is_hhdm_available(void) {
    return system_info.hhdm_available;
}

/*
 * Get kernel physical base
 */
uint64_t get_kernel_phys_base(void) {
    return system_info.kernel_phys_base;
}

/*
 * Get kernel virtual base
 */
uint64_t get_kernel_virt_base(void) {
    return system_info.kernel_virt_base;
}

const char *get_kernel_cmdline(void) {
    if (!system_info.kernel_cmdline_available) {
        return NULL;
    }
    return system_info.kernel_cmdline;
}

const struct limine_memmap_response *limine_get_memmap_response(void) {
    return (const struct limine_memmap_response *)memmap_request.response;
}

const struct limine_hhdm_response *limine_get_hhdm_response(void) {
    return (const struct limine_hhdm_response *)hhdm_request.response;
}
