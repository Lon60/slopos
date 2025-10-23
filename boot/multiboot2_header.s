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

    # Entry address tag removed - let GRUB use ELF entry point from link.ld
    # This avoids potential conflicts with GRUB's address calculations

    # Information request tag - Request UEFI memory map and framebuffer
    .align 8                        # Ensure 8-byte alignment
    .short 0x0001                   # Type: Information request
    .short 0                        # Flags: Required
    .long 16                        # Size of this tag (8 bytes header + 8 bytes data)
    .long 17                        # Request: EFI memory map
    .long 8                         # Request: Framebuffer info

    # Framebuffer preference tag - Request any graphics mode
    .align 8
    .short 0x0005                   # Type: Framebuffer tag
    .short 0                        # Flags: Optional
    .long 20                        # Size: 20 bytes
    .long 1024                      # Width: 1024 pixels
    .long 768                       # Height: 768 pixels
    .long 32                        # Depth: 32 bits per pixel

    # End tag - Required to terminate the header
    .align 8                        # Ensure 8-byte alignment
    .short 0x0000                   # Type: End tag
    .short 0                        # Flags: None
    .long 8                         # Size: Minimal size for end tag

multiboot2_header_end:

# Export symbols for other modules
.global multiboot2_header_start
.global multiboot2_header_end