# SlopOS Simple Boot Sequence
# Minimal boot code using only absolute addresses to avoid relocations

# ============================================================================
# Multiboot2 Header - MUST be first
# ============================================================================
.section .multiboot2_header
.align 8

multiboot2_header_start:
    .long 0xe85250d6                # Magic number
    .long 0                         # Architecture: i386
    .long multiboot2_header_end - multiboot2_header_start
    .long -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header_start))

    # Information request tag
    .short 1, 0, 12, 4

    # Framebuffer tag
    .short 5, 0, 20
    .long 1024, 768, 32

    # End tag
    .short 0, 0, 8

multiboot2_header_end:

# ============================================================================
# Text Section - All executable code
# ============================================================================
.section .text
.code32

.global _start
_start:
    # Entry point: 32-bit protected mode
    cli
    movl %ebx, %edi                 # Save multiboot2 info
    movl $0x20000, %esp             # Stack at 128KB

    # Verify CPU support
    call 1f                         # Check CPUID
1:  pushfl
    pushfl
    xorl $0x00200000, (%esp)
    popfl
    pushfl
    popl %eax
    xorl (%esp), %eax
    popfl
    testl $0x00200000, %eax
    jz halt

    # Check long mode
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb halt
    movl $0x80000001, %eax
    cpuid
    testl $0x20000000, %edx
    jz halt

    # Set up page tables at fixed addresses
    # Clear page tables (3 pages starting at 0x40000)
    movl $0x40000, %edi
    xorl %eax, %eax
    movl $3072, %ecx
    rep stosl

    # PML4 at 0x40000
    movl $0x41003, 0x40000          # PML4[0] = PDPT + flags
    movl $0x41003, 0x40000 + 511*8  # PML4[511] = PDPT + flags

    # PDPT at 0x41000
    movl $0x42003, 0x41000          # PDPT[0] = PD + flags
    movl $0x42003, 0x41000 + 510*8  # PDPT[510] = PD + flags

    # PD at 0x42000 - identity map first 1GB with 2MB pages
    movl $0x42000, %edi
    movl $0x83, %eax                # 2MB page + flags
    movl $512, %ecx
2:  movl %eax, (%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop 2b

    # Load minimal GDT
    lgdt gdt_desc

    # Enable long mode
    movl $0x40000, %eax             # Load PML4
    movl %eax, %cr3

    movl %cr4, %eax                 # Enable PAE
    orl $0x20, %eax
    movl %eax, %cr4

    movl $0xC0000080, %ecx          # Enable long mode in EFER
    rdmsr
    orl $0x100, %eax
    wrmsr

    movl %cr0, %eax                 # Enable paging
    orl $0x80000000, %eax
    movl %eax, %cr0

    # Jump to 64-bit mode - calculate address at runtime
    call get_current_addr           # Get current position
get_current_addr:
    popl %eax                       # EAX = address of get_current_addr
    addl $(start64 - get_current_addr), %eax  # Calculate start64 address
    pushl $0x08                     # Code segment
    pushl %eax                      # Target address
    lretl

halt:
    cli
    hlt
    jmp halt

# 64-bit entry point
.code64
start64:
    # Set up 64-bit segments
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # Set 64-bit stack
    movq $0x20000, %rsp

    # Convert multiboot2 info from EDI to RDI (zero-extend)
    movl %edi, %edi

    # Call C kernel main
    movabs $kernel_main, %rax
    callq *%rax

    # Halt if kernel_main returns
    cli
    hlt
    jmp .-2

# Minimal GDT in the same section to avoid relocations
.align 8
gdt_start:
    .quad 0                         # Null descriptor
    .quad 0x00AF9A000000FFFF        # Code segment
    .quad 0x00AF92000000FFFF        # Data segment
gdt_end:

gdt_desc:
    .word gdt_end - gdt_start - 1
    .long gdt_start

# External symbol
.extern kernel_main