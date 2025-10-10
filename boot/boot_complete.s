# SlopOS Complete Boot Sequence
# Consolidated boot assembly to avoid relocation issues
# Contains: Multiboot2 header, 32-bit entry, GDT setup, 64-bit transition

# ============================================================================
# Multiboot2 Header Section
# ============================================================================
.section .multiboot2_header
.align 8

multiboot2_header_start:
    .long 0xe85250d6                # Magic number
    .long 0                         # Architecture: i386
    .long multiboot2_header_end - multiboot2_header_start  # Header length
    .long -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header_start))  # Checksum

    # Information request tag
    .short 1                        # Type: Information request
    .short 0                        # Flags
    .long 12                        # Size
    .long 4                         # Request UEFI memory map

    # Framebuffer tag
    .short 5                        # Type: Framebuffer
    .short 0                        # Flags
    .long 20                        # Size
    .long 1024                      # Width
    .long 768                       # Height
    .long 32                        # Depth

    # End tag
    .short 0                        # Type: End
    .short 0                        # Flags
    .long 8                         # Size

multiboot2_header_end:

# ============================================================================
# GDT for 64-bit Mode
# ============================================================================
.section .rodata
.align 8

gdt_64_start:
    .quad 0x0000000000000000        # Null descriptor
    .quad 0x00AF9A000000FFFF        # Code segment (64-bit)
    .quad 0x00AF92000000FFFF        # Data segment (64-bit)
gdt_64_end:

gdt_64_descriptor:
    .word gdt_64_end - gdt_64_start - 1    # GDT limit
    .long gdt_64_start                     # GDT base

# ============================================================================
# Bootstrap Data Section
# ============================================================================
.section .bss
.align 16
boot_stack_bottom:
    .skip 16384                     # 16KB stack
boot_stack_top:

# ============================================================================
# 32-bit Boot Code
# ============================================================================
.section .text
.code32

.global _start
.type _start, @function

_start:
    # Entry: 32-bit protected mode
    # EAX = Multiboot2 magic
    # EBX = Multiboot2 info address

    cli                             # Disable interrupts
    movl %ebx, %edi                 # Save multiboot2 info
    movl $0x20000, %esp             # Set stack at 128KB

    # Clear EFLAGS
    pushl $0
    popfl

    # Check CPU capabilities
    call check_cpuid
    call check_long_mode

    # Set up paging
    call setup_page_tables
    call setup_gdt
    call enable_paging

    # Jump to 64-bit mode directly
    # Use relative offset calculation
    call get_eip
    addl $(start_64 - get_eip_ret), %eax
    pushl $0x08                     # Push code segment
    pushl %eax                      # Push absolute address
    lretl                           # Far return = far jump

get_eip:
    movl (%esp), %eax               # Get return address (current EIP)
get_eip_ret:
    ret

# Check CPUID support
check_cpuid:
    pushfl
    pushfl
    xorl $0x00200000, (%esp)
    popfl
    pushfl
    popl %eax
    xorl (%esp), %eax
    popfl
    andl $0x00200000, %eax
    jz error_halt
    ret

# Check long mode support
check_long_mode:
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb error_halt

    movl $0x80000001, %eax
    cpuid
    testl $0x20000000, %edx
    jz error_halt
    ret

# Set up page tables at fixed addresses
setup_page_tables:
    # Clear page tables (PML4 at 0x30000, PDPT at 0x31000, PD at 0x32000)
    movl $0x30000, %edi
    xorl %eax, %eax
    movl $3072, %ecx                # 3 pages
    rep stosl

    # Set up PML4
    movl $0x31003, %eax             # PDPT + flags
    movl %eax, 0x30000              # Identity mapping PML4[0]
    movl %eax, 0x30000 + 511 * 8    # Higher-half PML4[511]

    # Set up PDPT
    movl $0x32003, %eax             # PD + flags
    movl %eax, 0x31000              # Identity PDPT[0]
    movl %eax, 0x31000 + 510 * 8    # Higher-half PDPT[510]

    # Set up PD with 2MB pages
    movl $0x32000, %edi
    movl $0x00000083, %eax          # 2MB page + flags
    movl $512, %ecx

fill_pd:
    movl %eax, (%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop fill_pd

    ret

# Set up GDT
setup_gdt:
    lgdt gdt_64_descriptor
    ret

# Enable paging and long mode
enable_paging:
    # Load PML4
    movl $0x30000, %eax
    movl %eax, %cr3

    # Enable PAE
    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4

    # Enable long mode
    movl $0xC0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr

    # Enable paging
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    ret

error_halt:
    cli
    hlt
    jmp error_halt

# ============================================================================
# 64-bit Code
# ============================================================================
.code64

start_64:
    # Set segment registers
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # Set 64-bit stack
    movq $0x20000, %rsp

    # Clear registers
    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    # RDI contains multiboot2 info - preserve it

    # Jump to higher-half C code
    movabs $kernel_main, %rax
    callq *%rax

    # Should never return
    cli
    hlt
    jmp .-2

# Export required symbols
.global start_64
.extern kernel_main