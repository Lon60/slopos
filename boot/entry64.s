# SlopOS 64-bit Mode Entry Point
# Handles transition to 64-bit mode and handoff to C kernel code
# CRITICAL: Implements SysV ABI compliance for EDI→RDI handoff

# Constants from constants.h
.set DATA_SEL, 0x10
.set BOOT_STACK_PHYS_ADDR, 0x20000
.set KERNEL_VIRTUAL_BASE, 0xFFFFFFFF80000000
.set BOOT_ERROR_INVALID_MULTIBOOT2, 0x33
.set PAGE_SIZE_4KB, 0x1000
.set BOOT_ERROR_PAGING_FAILED, 0x36
.set PAGE_SIZE_1GB, 0x40000000

.code64
.section .text

# Entry point for 64-bit mode (called from entry32.s via far jump)
.global entry_64bit
.type entry_64bit, @function

entry_64bit:
    # CRITICAL SysV ABI COMPLIANCE:
    # EDI from 32-bit mode contains multiboot2 info pointer
    # In 64-bit mode, this becomes RDI (first parameter register)
    # EDI was preserved through the transition, now RDI is ready for C call

    # We are now in 64-bit long mode
    # CS is already set to code segment by the far jump

    # Reload all segment registers with data segment selector
    movw $DATA_SEL, %ax             # Use named constant for data segment
    movw %ax, %ds                   # Data segment
    movw %ax, %es                   # Extra segment
    movw %ax, %fs                   # FS segment
    movw %ax, %gs                   # GS segment
    movw %ax, %ss                   # Stack segment

    # Set up 64-bit stack pointer using named constant
    # Use safe stack location in identity mapped space
    movq $BOOT_STACK_PHYS_ADDR, %rsp

    # Clear direction flag for string operations
    cld

    # Zero out 64-bit registers for clean state
    # PRESERVE RDI - it contains multiboot2 info for SysV ABI
    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    # RDI PRESERVED - contains multiboot2 info pointer
    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10
    xorq %r11, %r11
    xorq %r12, %r12
    xorq %r13, %r13
    xorq %r14, %r14
    xorq %r15, %r15

    # TEMPORARY: Skip higher-half verification for emergency boot
    # We're using GRUB's page tables which don't have higher-half mapping
    # call get_current_rip
    # movq $KERNEL_VIRTUAL_BASE, %rbx
    # cmpq %rbx, %rax
    # jb invalid_memory_layout

    # SysV ABI: RDI already contains multiboot2 info as first parameter
    # No conversion needed - EDI was preserved through transition
    # This ensures proper 64-bit function call convention

    # Set up proper stack frame for C code
    pushq %rbp                      # Save old base pointer (0 for boot)
    movq %rsp, %rbp                 # Set new base pointer

    # Call kernel_main with multiboot2 info in RDI (SysV ABI)
    # RDI already contains the multiboot2 info pointer from 32-bit mode
    call kernel_main

    # If kernel_main returns (it shouldn't), halt safely
    cli
kernel_return_halt:
    hlt
    jmp kernel_return_halt

# Get current instruction pointer for address verification
get_current_rip:
    movq (%rsp), %rax               # Return address is current RIP
    ret

# Handle invalid memory layout (not in higher-half)
invalid_memory_layout:
    # Set error code and halt using named constant
    movq $BOOT_ERROR_INVALID_MULTIBOOT2, %rax    # Use named error constant
    jmp error_halt_64

# 64-bit error halt routine
error_halt_64:
    # Error code is in RAX
    # Disable interrupts and halt
    cli
halt_loop_64:
    hlt
    jmp halt_loop_64

# Infinite halt for unexpected returns
infinite_halt:
    cli
    hlt
    jmp infinite_halt

# Stack verification routine
.global verify_64bit_stack
.type verify_64bit_stack, @function

verify_64bit_stack:
    # Get current stack pointer
    movq %rsp, %rax

    # Check if stack is in reasonable range using named constants
    # Should be above page size and below higher-half
    cmpq $PAGE_SIZE_4KB, %rax
    jb bad_stack

    # Should be in identity-mapped region or higher-half
    cmpq $KERNEL_VIRTUAL_BASE, %rax
    jae good_stack                  # In higher-half

    # Check if in identity-mapped region (below 1GB)
    cmpq $PAGE_SIZE_1GB, %rax
    jb good_stack                   # In identity-mapped region

bad_stack:
    movq $BOOT_ERROR_PAGING_FAILED, %rax    # Use named error constant
    jmp error_halt_64

good_stack:
    ret

# Set up final 64-bit execution environment
.global setup_64bit_environment
.type setup_64bit_environment, @function

setup_64bit_environment:
    # Verify stack is good
    call verify_64bit_stack

    # Clear any remaining 32-bit state
    # Clear high 32 bits of base pointer
    xorq %rbp, %rbp

    # Set up stack frame for C code
    pushq %rbp                      # Save old base pointer
    movq %rsp, %rbp                 # Set new base pointer

    ret

# Export symbols for other modules
.global entry_64bit
.global verify_64bit_stack
.global setup_64bit_environment

# Import symbols from other modules
.extern kernel_main                 # From C code