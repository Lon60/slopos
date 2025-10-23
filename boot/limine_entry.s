# SlopOS Limine Entry Point
# Limine bootloader jumps directly to 64-bit mode with paging enabled
# No 32-bit entry code needed - Limine handles the transition

.code64
.section .text
.global _start

_start:
    # Limine provides 64-bit long mode with paging enabled
    # Set up our own kernel stack for safety
    
    # Send 'L' IMMEDIATELY to verify entry (before doing ANYTHING else)
    movw $0x3F8, %dx        # COM1 port
    movb $'L', %al          # Character 'L' for Limine  
    outb %al, %dx
    
    # Load kernel stack pointer (use absolute address in higher half)
    movabs $kernel_stack_top, %rsp
    
    # Ensure 16-byte stack alignment (required by System V ABI)
    andq $-16, %rsp
    
    # Clear direction flag for string operations
    cld

    # Zero out base pointer for clean stack traces
    xorq %rbp, %rbp

    # Initialize COM1 properly
    call early_serial_init
    
    # Send another character to show we survived
    movw $0x3F8, %dx        # COM1 port
    movb $'S', %al          # Character 'S' for Stack OK
    outb %al, %dx
    
    # Zero out registers for clean state
    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    xorq %rdi, %rdi
    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10
    xorq %r11, %r11
    xorq %r12, %r12
    xorq %r13, %r13
    xorq %r14, %r14
    xorq %r15, %r15

    # Call kernel_main with no parameters
    # Boot information is available via static Limine request structures
    call kernel_main

    # If kernel_main returns (it shouldn't), halt
    cli
.halt_loop:
    hlt
    jmp .halt_loop

# Minimal serial port initialization
# Initializes COM1 for 115200 baud, 8N1
early_serial_init:
    pushq %rax
    pushq %rdx
    
    # Disable interrupts on COM1
    movw $0x3F9, %dx        # COM1 + 1 (IER)
    xorb %al, %al
    outb %al, %dx
    
    # Enable DLAB (Divisor Latch Access Bit)
    movw $0x3FB, %dx        # COM1 + 3 (LCR)
    movb $0x80, %al
    outb %al, %dx
    
    # Set divisor to 1 (115200 baud)
    movw $0x3F8, %dx        # COM1 + 0 (DLL)
    movb $0x01, %al
    outb %al, %dx
    
    movw $0x3F9, %dx        # COM1 + 1 (DLH)
    xorb %al, %al
    outb %al, %dx
    
    # 8 bits, no parity, one stop bit (8N1)
    movw $0x3FB, %dx        # COM1 + 3 (LCR)
    movb $0x03, %al
    outb %al, %dx
    
    # Enable FIFO, clear TX/RX queues, 14-byte threshold
    movw $0x3FA, %dx        # COM1 + 2 (FCR)
    movb $0xC7, %al
    outb %al, %dx
    
    # Mark data terminal ready, request to send, auxiliary output 2
    movw $0x3FC, %dx        # COM1 + 4 (MCR)
    movb $0x0B, %al
    outb %al, %dx
    
    popq %rdx
    popq %rax
    ret

.size _start, . - _start

# Kernel stack in BSS section
# 64KB stack should be plenty for early boot
.section .bss
.align 16
.global kernel_stack_bottom
kernel_stack_bottom:
    .skip 65536             # 64KB stack
.global kernel_stack_top
kernel_stack_top:

