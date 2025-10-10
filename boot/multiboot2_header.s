# SlopOS Multiboot2 Header
# Contains ONLY the Multiboot2 header structure using named constants
# Must be first in the binary as defined by link.ld

.section .multiboot2_header
.align 8

# Multiboot2 Header Structure with actual constants
multiboot2_header_start:
    .long 0xe85250d6                # Magic number (MULTIBOOT2_HEADER_MAGIC)
    .long 0x00000000                # Architecture: i386 protected mode
    .long multiboot2_header_end - multiboot2_header_start  # Header length
    .long -(0xe85250d6 + 0x00000000 + (multiboot2_header_end - multiboot2_header_start))  # Checksum

    # Information request tag - Request UEFI memory map
    .short 0x0001                   # Type: Information request
    .short 0                        # Flags: Required
    .long 12                        # Size of this tag
    .long 17                        # Request: EFI memory map

    # Framebuffer tag - Request specific framebuffer configuration
    .short 0x0005                   # Type: Framebuffer
    .short 0                        # Flags: Required
    .long 20                        # Size of this tag
    .long 1024                      # Preferred width
    .long 768                       # Preferred height
    .long 32                        # Preferred depth (bits per pixel)

    # End tag - Required to terminate the header
    .short 0x0000                   # Type: End tag
    .short 0                        # Flags: None
    .long 8                         # Size: Minimal size for end tag

multiboot2_header_end:

# Export symbols for other modules
.global multiboot2_header_start
.global multiboot2_header_end