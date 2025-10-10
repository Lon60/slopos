# SlopOS 32-bit Entry Point and CPU Verification
# Handles initial 32-bit protected mode entry and CPU capability checks

.code32

.section .bss
.align 16
# Boot stack for 32-bit and early 64-bit execution
boot_stack_bottom:
    .skip 16384                     # 16KB stack space
boot_stack_top:

# Page table locations in low memory (physical addresses)
# We'll place them at fixed addresses to avoid relocation issues
# early_pml4 = 0x30000 (192KB)
# early_pdpt = 0x31000 (196KB)
# early_pd   = 0x32000 (200KB)

.section .text
.global _start
.type _start, @function

# Main entry point from bootloader
_start:
    # Entry state: 32-bit protected mode
    # EAX = Multiboot2 magic number (0x36d76289)
    # EBX = Physical address of Multiboot2 information structure

    # Disable interrupts immediately
    cli

    # Save Multiboot2 information for later use
    movl %ebx, %edi                 # Store multiboot2 info pointer in EDI

    # Initialize stack pointer (use physical address)
    # Use absolute address to avoid relocation issues
    movl $0x20000, %esp              # Stack at 128KB physical address

    # Clear EFLAGS register
    pushl $0
    popfl

    # Verify CPU capabilities before proceeding
    call check_cpuid_support
    call check_long_mode_support

    # CPU verification passed - proceed with boot process
    # Set up basic paging structures
    call setup_early_page_tables

    # Jump to GDT setup (implemented in separate file)
    call setup_64bit_gdt

    # Enable paging and transition to long mode
    call enable_long_mode_paging

    # Perform far jump to 64-bit code segment
    # Use absolute address for the far jump
    ljmpl $0x08, $long_mode_entry

# Check if CPUID instruction is supported
check_cpuid_support:
    # Test if we can modify the ID flag in EFLAGS
    pushfl                          # Save current EFLAGS
    pushfl                          # Save EFLAGS again for modification
    xorl $0x00200000, (%esp)       # Flip the ID bit (bit 21)
    popfl                           # Load modified EFLAGS
    pushfl                          # Push EFLAGS again
    popl %eax                       # Pop into EAX
    xorl (%esp), %eax               # Compare with original
    popfl                           # Restore original EFLAGS
    andl $0x00200000, %eax          # Isolate ID bit
    jz cpuid_not_supported          # Jump if ID bit unchanged
    ret

cpuid_not_supported:
    movl $0x31, %eax                # Error code: No CPUID
    jmp boot_error_halt

# Check if 64-bit long mode is supported
check_long_mode_support:
    # Check if extended CPUID functions are available
    movl $0x80000000, %eax          # Get highest extended function
    cpuid
    cmpl $0x80000001, %eax          # Must support 0x80000001
    jb long_mode_not_supported      # Jump if not supported

    # Check for long mode support in extended features
    movl $0x80000001, %eax          # Extended feature information
    cpuid
    testl $0x20000000, %edx         # Test LM bit (bit 29)
    jz long_mode_not_supported      # Jump if long mode not supported
    ret

long_mode_not_supported:
    movl $0x32, %eax                # Error code: No long mode
    jmp boot_error_halt

# Set up basic page table structures for early boot
setup_early_page_tables:
    # Clear all page tables first (3 pages starting at 0x30000)
    movl $0x30000, %edi             # PML4 at 0x30000
    xorl %eax, %eax                 # Clear EAX
    movl $3072, %ecx                # 3 pages * 1024 dwords each
    rep stosl                       # Clear memory

    # Set up PML4 entry for identity mapping (first 1GB)
    movl $0x31003, %eax             # PDPT address + flags (Present + Writable)
    movl %eax, 0x30000              # PML4[0] = PDPT

    # Set up PML4 entry for higher-half kernel mapping
    # Higher-half address 0xFFFFFFFF80000000 -> PML4[511]
    movl %eax, 0x30000 + 511 * 8    # PML4[511] = same PDPT

    # Set up PDPT entry for identity mapping
    movl $0x32003, %eax             # PD address + flags (Present + Writable)
    movl %eax, 0x31000              # PDPT[0] = PD

    # Set up PDPT entry for higher-half mapping
    # 0x80000000 part -> PDPT[510]
    movl %eax, 0x31000 + 510 * 8    # PDPT[510] = same PD

    # Fill PD with 2MB pages for first 1GB (identity mapping)
    movl $0x32000, %edi             # PD at 0x32000
    movl $0x00000083, %eax          # Present + Writable + Page Size (2MB)
    movl $512, %ecx                 # 512 entries = 1GB total

fill_page_directory:
    movl %eax, (%edi)               # Store page entry
    addl $0x200000, %eax            # Next 2MB page
    addl $8, %edi                   # Next PD entry
    loop fill_page_directory

    ret

# Enable paging and transition to long mode
enable_long_mode_paging:
    # Load page table base address into CR3
    movl $0x30000, %eax             # PML4 physical address
    movl %eax, %cr3

    # Enable Physical Address Extension (PAE) in CR4
    movl %cr4, %eax
    orl $0x20, %eax                 # Set PAE bit (bit 5)
    movl %eax, %cr4

    # Enable long mode in EFER MSR
    movl $0xC0000080, %ecx          # EFER MSR number
    rdmsr                           # Read current EFER value
    orl $0x100, %eax                # Set LME bit (bit 8)
    wrmsr                           # Write back to EFER

    # Enable paging in CR0
    movl %cr0, %eax
    orl $0x80000000, %eax           # Set PG bit (bit 31)
    movl %eax, %cr0

    ret

# Error handling - halt system with error code in EAX
boot_error_halt:
    # Error code is in EAX
    # In a real implementation, this could output to serial or screen
    cli                             # Ensure interrupts are disabled
halt_loop:
    hlt                             # Halt processor
    jmp halt_loop                   # Loop forever

# Transition point to 64-bit mode - this will be called from the far jump
.code64
long_mode_entry:
    # Now in 64-bit long mode
    # Call the 64-bit entry point
    call entry_64bit

.code32  # Return to 32-bit for remaining symbols

# Export symbols for linking with other boot modules
.global long_mode_entry