# SlopOS GDT Setup for 64-bit Mode Transition
# Configures Global Descriptor Table for long mode operation using named constants

# Constants from constants.h
.set GDT_NULL_DESCRIPTOR, 0x0000000000000000
.set GDT_CODE_DESCRIPTOR_64, 0x00AF9A000000FFFF
.set GDT_DATA_DESCRIPTOR_64, 0x00AF92000000FFFF
.set GDT_DATA_SELECTOR, 0x10
.set GDT_CODE_SELECTOR, 0x08

.section .rodata
.align 8

# Global Descriptor Table for 64-bit mode
# Contains minimal segments required for long mode operation
gdt_64_start:
    # Null descriptor (required first entry)
    .quad GDT_NULL_DESCRIPTOR

    # Code segment descriptor for 64-bit mode
    # Detailed breakdown in constants.h
    .quad GDT_CODE_DESCRIPTOR_64

    # Data segment descriptor for 64-bit mode
    # Detailed breakdown in constants.h
    .quad GDT_DATA_DESCRIPTOR_64

gdt_64_end:

# GDT descriptor structure for LGDT instruction
gdt_64_descriptor:
    .word gdt_64_end - gdt_64_start - 1    # GDT size (limit)
    .long gdt_64_start                     # GDT base address (32-bit for now)

# Extended GDT descriptor for 64-bit mode (when needed)
.align 8
gdt_64_descriptor_long:
    .word gdt_64_end - gdt_64_start - 1    # GDT size (limit)
    .quad gdt_64_start                     # GDT base address (64-bit)

.section .text

# Set up 64-bit GDT for long mode transition
.global setup_64bit_gdt
.type setup_64bit_gdt, @function

setup_64bit_gdt:
    # Load the 64-bit GDT
    lgdt gdt_64_descriptor
    ret

# Reload segment registers after entering 64-bit mode
# This function should be called from 64-bit code
.global reload_segments_64
.type reload_segments_64, @function

.code64
reload_segments_64:
    # Set data segment selectors using named constant
    movw $GDT_DATA_SELECTOR, %ax    # Data segment selector
    movw %ax, %ds                   # Data segment
    movw %ax, %es                   # Extra segment
    movw %ax, %fs                   # FS segment
    movw %ax, %gs                   # GS segment
    movw %ax, %ss                   # Stack segment

    # Code segment is already set by the far jump in entry32.s
    # No need to reload CS here
    ret

.code32  # Return to 32-bit mode for remaining boot code

# Get GDT information for other modules
.global get_gdt_base
.type get_gdt_base, @function

get_gdt_base:
    movl $gdt_64_start, %eax        # Return GDT base address
    ret

.global get_gdt_limit
.type get_gdt_limit, @function

get_gdt_limit:
    movl $(gdt_64_end - gdt_64_start - 1), %eax  # Return GDT limit
    ret

# Segment selector constants (for use by other modules)
.global CODE_SEGMENT_SELECTOR
.global DATA_SEGMENT_SELECTOR

.section .rodata
CODE_SEGMENT_SELECTOR: .long GDT_CODE_SELECTOR  # Code segment selector
DATA_SEGMENT_SELECTOR: .long GDT_DATA_SELECTOR  # Data segment selector

# Export GDT symbols for debugging and other modules
.global gdt_64_start
.global gdt_64_end
.global gdt_64_descriptor