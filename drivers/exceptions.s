# SlopOS Exception Handler Assembly Stubs
# x86_64 assembly code for interrupt and exception handling
# Uses AT&T syntax

.section .text

# ========================================================================
# EXCEPTION HANDLER MACROS
# ========================================================================

# Macro for exception handlers that don't push an error code
.macro EXCEPTION_HANDLER_NO_ERROR num
.global exception_handler_\num
exception_handler_\num:
    # Push dummy error code for consistency
    pushq $0

    # Push exception vector number
    pushq $\num

    # Save all registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Set up kernel data segment
    movw $0x10, %ax         # Kernel data segment selector
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Call common exception handler
    movq $\num, %rdi        # Exception vector as first argument
    movq 120(%rsp), %rsi    # Error code as second argument (dummy 0)
    call common_exception_handler

    # Restore registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # Remove vector number and error code
    addq $16, %rsp

    # Return from interrupt
    iretq
.endm

# Macro for exception handlers that push an error code
.macro EXCEPTION_HANDLER_WITH_ERROR num
.global exception_handler_\num
exception_handler_\num:
    # Error code already pushed by CPU

    # Push exception vector number
    pushq $\num

    # Save all registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Set up kernel data segment
    movw $0x10, %ax         # Kernel data segment selector
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Call common exception handler
    movq $\num, %rdi        # Exception vector as first argument
    movq 128(%rsp), %rsi    # Error code as second argument (real error code)
    call common_exception_handler

    # Restore registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # Remove vector number and error code
    addq $16, %rsp

    # Return from interrupt
    iretq
.endm

# ========================================================================
# EXCEPTION HANDLERS WITHOUT ERROR CODE (0-31)
# ========================================================================

EXCEPTION_HANDLER_NO_ERROR 0    # Divide Error
EXCEPTION_HANDLER_NO_ERROR 1    # Debug
EXCEPTION_HANDLER_NO_ERROR 2    # NMI
EXCEPTION_HANDLER_NO_ERROR 3    # Breakpoint
EXCEPTION_HANDLER_NO_ERROR 4    # Overflow
EXCEPTION_HANDLER_NO_ERROR 5    # Bound Range
EXCEPTION_HANDLER_NO_ERROR 6    # Invalid Opcode
EXCEPTION_HANDLER_NO_ERROR 7    # Device Not Available
EXCEPTION_HANDLER_NO_ERROR 9    # Coprocessor Segment Overrun
EXCEPTION_HANDLER_NO_ERROR 15   # Reserved
EXCEPTION_HANDLER_NO_ERROR 16   # x87 FPU Error
EXCEPTION_HANDLER_NO_ERROR 18   # Machine Check
EXCEPTION_HANDLER_NO_ERROR 19   # SIMD FP Exception
EXCEPTION_HANDLER_NO_ERROR 20   # Virtualization Exception

# ========================================================================
# EXCEPTION HANDLERS WITH ERROR CODE
# ========================================================================

EXCEPTION_HANDLER_WITH_ERROR 8   # Double Fault
EXCEPTION_HANDLER_WITH_ERROR 10  # Invalid TSS
EXCEPTION_HANDLER_WITH_ERROR 11  # Segment Not Present
EXCEPTION_HANDLER_WITH_ERROR 12  # Stack Fault
EXCEPTION_HANDLER_WITH_ERROR 13  # General Protection
EXCEPTION_HANDLER_WITH_ERROR 14  # Page Fault
EXCEPTION_HANDLER_WITH_ERROR 17  # Alignment Check
EXCEPTION_HANDLER_WITH_ERROR 21  # Control Protection Exception

# ========================================================================
# DEFAULT EXCEPTION HANDLER
# ========================================================================

.global exception_handler_default
exception_handler_default:
    # Push dummy error code
    pushq $0

    # Push vector number 255 to indicate unknown
    pushq $255

    # Save all registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Set up kernel data segment
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Call common exception handler
    movq $255, %rdi         # Unknown vector
    movq 120(%rsp), %rsi    # Dummy error code
    call common_exception_handler

    # Restore registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # Remove vector and error code
    addq $16, %rsp

    # Return from interrupt
    iretq

# ========================================================================
# DEFAULT INTERRUPT HANDLER
# ========================================================================

.global interrupt_handler_default
interrupt_handler_default:
    # Push dummy error code
    pushq $0

    # Push vector number 254 to indicate generic interrupt
    pushq $254

    # Save minimal registers for interrupts
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11

    # Set up kernel data segment
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es

    # Call interrupt handler
    movq $254, %rdi         # Generic interrupt vector
    movq 80(%rsp), %rsi     # Dummy error code
    call common_interrupt_handler

    # Restore registers
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax

    # Remove vector and error code
    addq $16, %rsp

    # Return from interrupt
    iretq

# ========================================================================
# COMMON EXCEPTION AND INTERRUPT DISPATCH
# ========================================================================

common_exception_handler:
    # Arguments: RDI = vector number, RSI = error code

    # Dispatch to specific handlers based on vector
    cmpq $0, %rdi
    je call_divide_error

    cmpq $1, %rdi
    je call_debug

    cmpq $2, %rdi
    je call_nmi

    cmpq $3, %rdi
    je call_breakpoint

    cmpq $8, %rdi
    je call_double_fault

    cmpq $13, %rdi
    je call_general_protection

    cmpq $14, %rdi
    je call_page_fault

    # Default case for unknown exceptions
    movq %rdi, %rdi         # Vector number already in RDI
    call handle_unknown_exception
    ret

call_divide_error:
    call handle_divide_error
    ret

call_debug:
    call handle_debug_exception
    ret

call_nmi:
    call handle_nmi
    ret

call_breakpoint:
    call handle_breakpoint
    ret

call_double_fault:
    movq %rsi, %rdi         # Error code as first argument
    call handle_double_fault
    ret

call_general_protection:
    movq %rsi, %rdi         # Error code as first argument
    call handle_general_protection
    ret

call_page_fault:
    movq %rsi, %rdi         # Error code as first argument
    call handle_page_fault
    ret

common_interrupt_handler:
    # Arguments: RDI = vector number, RSI = error code

    # For now, all interrupts go to default handler
    movq %rdi, %rdi         # Vector number already in RDI
    call handle_unknown_interrupt
    ret

# ========================================================================
# UTILITY FUNCTIONS
# ========================================================================

# Get current instruction pointer
.global get_current_rip
get_current_rip:
    movq (%rsp), %rax       # Return address is current RIP
    ret

# Get current stack pointer
.global get_current_rsp
get_current_rsp:
    movq %rsp, %rax
    addq $8, %rax           # Adjust for return address
    ret

# Trigger a test exception (divide by zero)
.global trigger_divide_exception
trigger_divide_exception:
    movq $0, %rcx
    movq $1, %rax
    divq %rcx               # This will cause a divide error exception
    ret

# Trigger a test page fault
.global trigger_page_fault
trigger_page_fault:
    movq $0xDEADBEEF, %rax  # Invalid address
    movq (%rax), %rbx       # This will cause a page fault
    ret

# External function declarations
.extern handle_divide_error
.extern handle_debug_exception
.extern handle_nmi
.extern handle_breakpoint
.extern handle_double_fault
.extern handle_general_protection
.extern handle_page_fault
.extern handle_unknown_exception
.extern handle_unknown_interrupt