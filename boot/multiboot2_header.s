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

    # Information request tag - Request UEFI memory map
    .align 8                        # Ensure 8-byte alignment
    .short 0x0001                   # Type: Information request
    .short 0                        # Flags: Required
    .long 12                        # Size of this tag
    .long 17                        # Request: EFI memory map

    # Framebuffer tag removed - GRUB was crashing when trying to set video mode
    # The kernel will detect and use whatever framebuffer GRUB/UEFI provides

    # End tag - Required to terminate the header
    .align 8                        # Ensure 8-byte alignment
    .short 0x0000                   # Type: End tag
    .short 0                        # Flags: None
    .long 8                         # Size: Minimal size for end tag

multiboot2_header_end:

# Export symbols for other modules
.global multiboot2_header_start
.global multiboot2_header_end