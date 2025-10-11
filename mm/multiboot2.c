/*
 * SlopOS Memory Management - Multiboot2 Interface
 * Parse Multiboot2 information structure for UEFI memory map and system info
 */

#include <stdint.h>
#include <stddef.h>

// Multiboot2 information structure
#define MULTIBOOT2_MAGIC 0x36d76289

// Multiboot2 tag types
#define MULTIBOOT_TAG_TYPE_END               0
#define MULTIBOOT_TAG_TYPE_CMDLINE           1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME  2
#define MULTIBOOT_TAG_TYPE_MODULE            3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO     4
#define MULTIBOOT_TAG_TYPE_BOOTDEV           5
#define MULTIBOOT_TAG_TYPE_MMAP              6
#define MULTIBOOT_TAG_TYPE_VBE               7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER       8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS      9
#define MULTIBOOT_TAG_TYPE_APM               10
#define MULTIBOOT_TAG_TYPE_EFI32             11
#define MULTIBOOT_TAG_TYPE_EFI64             12
#define MULTIBOOT_TAG_TYPE_SMBIOS            13
#define MULTIBOOT_TAG_TYPE_ACPI_OLD          14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW          15
#define MULTIBOOT_TAG_TYPE_NETWORK           16
#define MULTIBOOT_TAG_TYPE_EFI_MMAP          17
#define MULTIBOOT_TAG_TYPE_EFI_BS            18

// Memory map entry types
#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5

// Basic structures
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct multiboot_tag_string {
    uint32_t type;
    uint32_t size;
    char string[0];
} __attribute__((packed));

struct multiboot_tag_basic_meminfo {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
} __attribute__((packed));

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[0];
} __attribute__((packed));

struct multiboot_tag_framebuffer_common {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
} __attribute__((packed));

struct multiboot_tag_efi64 {
    uint32_t type;
    uint32_t size;
    uint64_t pointer;
} __attribute__((packed));

struct multiboot_tag_efi_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t descr_size;
    uint32_t descr_vers;
    uint8_t efi_mmap[0];
} __attribute__((packed));

// EFI Memory Descriptor
typedef struct {
    uint32_t type;
    uint64_t phys_start;
    uint64_t virt_start;
    uint64_t num_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

// Global system information
static struct {
    uint64_t total_memory;
    uint64_t available_memory;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint8_t framebuffer_bpp;
    uint64_t efi_system_table;
    int efi_available;
    int framebuffer_available;
    int memory_map_available;
} system_info = {0};

/*
 * Align address up to next boundary
 */
static inline uint64_t align_up(uint64_t addr, uint64_t boundary) {
    return (addr + boundary - 1) & ~(boundary - 1);
}

/*
 * Parse basic memory information
 */
static void parse_basic_meminfo(struct multiboot_tag_basic_meminfo *tag) {
    system_info.total_memory = ((uint64_t)tag->mem_upper) * 1024 + 1024 * 1024;
    // mem_lower is memory below 1MB, mem_upper is memory above 1MB in KB
}

/*
 * Parse memory map
 */
static void parse_memory_map(struct multiboot_tag_mmap *tag) {
    struct multiboot_mmap_entry *entry;
    uint64_t available = 0;

    for (entry = tag->entries;
         (uint8_t*)entry < (uint8_t*)tag + tag->size;
         entry = (struct multiboot_mmap_entry*)((uint64_t)entry + tag->entry_size)) {

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            available += entry->len;
        }
    }

    system_info.available_memory = available;
    system_info.memory_map_available = 1;
}

/*
 * Parse EFI memory map
 */
static void parse_efi_memory_map(struct multiboot_tag_efi_mmap *tag) {
    efi_memory_descriptor_t *desc;
    uint64_t available = 0;
    uint32_t desc_count = (tag->size - sizeof(*tag)) / tag->descr_size;

    for (uint32_t i = 0; i < desc_count; i++) {
        desc = (efi_memory_descriptor_t*)((uint8_t*)tag->efi_mmap + i * tag->descr_size);

        // EFI memory types that are available for use
        if (desc->type == 7) { // EfiConventionalMemory
            available += desc->num_pages * 4096; // EFI pages are 4KB
        }
    }

    if (available > system_info.available_memory) {
        system_info.available_memory = available;
    }
}

/*
 * Parse framebuffer information
 */
static void parse_framebuffer(struct multiboot_tag_framebuffer_common *tag) {
    system_info.framebuffer_addr = tag->framebuffer_addr;
    system_info.framebuffer_width = tag->framebuffer_width;
    system_info.framebuffer_height = tag->framebuffer_height;
    system_info.framebuffer_pitch = tag->framebuffer_pitch;
    system_info.framebuffer_bpp = tag->framebuffer_bpp;
    system_info.framebuffer_available = 1;
}

/*
 * Parse EFI64 system table
 */
static void parse_efi64(struct multiboot_tag_efi64 *tag) {
    system_info.efi_system_table = tag->pointer;
    system_info.efi_available = 1;
}

/*
 * Parse Multiboot2 information structure
 */
void parse_multiboot2_info(uint64_t multiboot_info_addr) {
    // Verify magic number and basic structure
    if (multiboot_info_addr == 0) {
        return; // No multiboot info
    }

    uint32_t *info = (uint32_t*)multiboot_info_addr;
    uint32_t total_size = info[0];
    uint32_t reserved = info[1];

    if (total_size < 8) {
        return; // Invalid structure
    }

    // Parse tags
    struct multiboot_tag *tag = (struct multiboot_tag*)(info + 2);
    uint64_t addr = (uint64_t)tag;
    uint64_t end_addr = multiboot_info_addr + total_size;

    while (addr < end_addr) {
        tag = (struct multiboot_tag*)addr;

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                parse_basic_meminfo((struct multiboot_tag_basic_meminfo*)tag);
                break;

            case MULTIBOOT_TAG_TYPE_MMAP:
                parse_memory_map((struct multiboot_tag_mmap*)tag);
                break;

            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                parse_framebuffer((struct multiboot_tag_framebuffer_common*)tag);
                break;

            case MULTIBOOT_TAG_TYPE_EFI64:
                parse_efi64((struct multiboot_tag_efi64*)tag);
                break;

            case MULTIBOOT_TAG_TYPE_EFI_MMAP:
                parse_efi_memory_map((struct multiboot_tag_efi_mmap*)tag);
                break;

            default:
                // Unknown tag, skip
                break;
        }

        // Move to next tag (aligned to 8-byte boundary)
        addr = align_up(addr + tag->size, 8);
    }
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
 * Get framebuffer information
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
 * Get EFI system table
 */
uint64_t get_efi_system_table(void) {
    return system_info.efi_available ? system_info.efi_system_table : 0;
}

/*
 * Check if EFI is available
 */
int is_efi_available(void) {
    return system_info.efi_available;
}

/*
 * Check if framebuffer is available
 */
int is_framebuffer_available(void) {
    return system_info.framebuffer_available;
}

/*
 * Get memory map availability
 */
int is_memory_map_available(void) {
    return system_info.memory_map_available;
}

/*
 * Print system information summary (for debugging)
 * Note: This function assumes a basic debug output mechanism
 */
void print_system_info(void (*print_func)(const char*)) {
    if (!print_func) return;

    print_func("=== SlopOS System Information ===\n");

    if (system_info.total_memory > 0) {
        print_func("Memory detected\n");
    }

    if (system_info.framebuffer_available) {
        print_func("Framebuffer available\n");
    }

    if (system_info.efi_available) {
        print_func("EFI system table available\n");
    }

    if (system_info.memory_map_available) {
        print_func("Memory map available\n");
    }
}
