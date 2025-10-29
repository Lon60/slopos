#
# SlopOS Context Switching Assembly
# Low-level task context switching for x86_64
# AT&T syntax for cooperative task switching
#

.section .text
.global context_switch

#
# context_switch(void *old_context, void *new_context)
#
# Switches from current task context to new task context
#
# Parameters:
#   rdi - Pointer to old task context (may be NULL for first switch)
#   rsi - Pointer to new task context (must not be NULL)
#
# Context structure layout (matches task_context_t in task.c):
#   Offset 0x00: rax
#   Offset 0x08: rbx
#   Offset 0x10: rcx
#   Offset 0x18: rdx
#   Offset 0x20: rsi
#   Offset 0x28: rdi
#   Offset 0x30: rbp
#   Offset 0x38: rsp
#   Offset 0x40: r8
#   Offset 0x48: r9
#   Offset 0x50: r10
#   Offset 0x58: r11
#   Offset 0x60: r12
#   Offset 0x68: r13
#   Offset 0x70: r14
#   Offset 0x78: r15
#   Offset 0x80: rip
#   Offset 0x88: rflags
#   Offset 0x90: cs
#   Offset 0x98: ds
#   Offset 0xA0: es
#   Offset 0xA8: fs
#   Offset 0xB0: gs
#   Offset 0xB8: ss
#   Offset 0xC0: cr3
#

context_switch:
    # Check if we need to save old context
    test    %rdi, %rdi              # Test if old_context is NULL
    jz      load_new_context        # Skip save if NULL (first task switch)

    # Save current CPU state to old context
    movq    %rax, 0x00(%rdi)        # Save rax
    movq    %rbx, 0x08(%rdi)        # Save rbx
    movq    %rcx, 0x10(%rdi)        # Save rcx
    movq    %rdx, 0x18(%rdi)        # Save rdx
    movq    %rsi, 0x20(%rdi)        # Save rsi
    # Note: rdi will be saved after we're done using it
    movq    %rbp, 0x30(%rdi)        # Save rbp
    movq    %rsp, 0x38(%rdi)        # Save current stack pointer
    movq    %r8,  0x40(%rdi)        # Save r8
    movq    %r9,  0x48(%rdi)        # Save r9
    movq    %r10, 0x50(%rdi)        # Save r10
    movq    %r11, 0x58(%rdi)        # Save r11
    movq    %r12, 0x60(%rdi)        # Save r12
    movq    %r13, 0x68(%rdi)        # Save r13
    movq    %r14, 0x70(%rdi)        # Save r14
    movq    %r15, 0x78(%rdi)        # Save r15

    # Save return address as instruction pointer
    # The return address is on top of stack
    movq    (%rsp), %rax            # Get return address from stack
    movq    %rax, 0x80(%rdi)        # Save as rip in context

    # Save flags register
    pushfq                          # Push flags onto stack
    popq    %rax                    # Pop flags into rax
    movq    %rax, 0x88(%rdi)        # Save rflags

    # Save segment registers
    movw    %cs, %ax                # Get code segment
    movq    %rax, 0x90(%rdi)        # Save cs (zero-extended)
    movw    %ds, %ax                # Get data segment
    movq    %rax, 0x98(%rdi)        # Save ds (zero-extended)
    movw    %es, %ax                # Get extra segment
    movq    %rax, 0xA0(%rdi)        # Save es (zero-extended)
    movw    %fs, %ax                # Get fs segment
    movq    %rax, 0xA8(%rdi)        # Save fs (zero-extended)
    movw    %gs, %ax                # Get gs segment
    movq    %rax, 0xB0(%rdi)        # Save gs (zero-extended)
    movw    %ss, %ax                # Get stack segment
    movq    %rax, 0xB8(%rdi)        # Save ss (zero-extended)

    # Save page directory (CR3)
    movq    %cr3, %rax              # Get current page directory
    movq    %rax, 0xC0(%rdi)        # Save cr3

    # Now save rdi (we're done using old_context pointer)
    movq    %rdi, 0x28(%rdi)        # Save rdi

load_new_context:
    # Load new context from new_context pointer (in rsi)

    # Load page directory first (if it's different)
    movq    0xC0(%rsi), %rax        # Load new cr3
    movq    %cr3, %rdx              # Get current cr3
    cmpq    %rax, %rdx              # Compare with new cr3
    je      skip_cr3_load           # Skip if same page directory
    movq    %rax, %cr3              # Load new page directory
skip_cr3_load:

    # Load segment registers
    movq    0x98(%rsi), %rax        # Load ds
    movw    %ax, %ds                # Set data segment
    movq    0xA0(%rsi), %rax        # Load es
    movw    %ax, %es                # Set extra segment
    movq    0xA8(%rsi), %rax        # Load fs
    movw    %ax, %fs                # Set fs segment
    movq    0xB0(%rsi), %rax        # Load gs
    movw    %ax, %gs                # Set gs segment
    # Note: cs and ss will be loaded with iretq

    # Load flags register
    movq    0x88(%rsi), %rax        # Load rflags
    pushq   %rax                    # Push onto stack
    popfq                           # Pop into flags register

    # Load general purpose registers (except rsp and rip)
    movq    0x00(%rsi), %rax        # Load rax
    movq    0x08(%rsi), %rbx        # Load rbx
    movq    0x10(%rsi), %rcx        # Load rcx
    movq    0x18(%rsi), %rdx        # Load rdx
    # rsi will be loaded last since we're using it
    movq    0x28(%rsi), %rdi        # Load rdi
    movq    0x30(%rsi), %rbp        # Load rbp
    movq    0x40(%rsi), %r8         # Load r8
    movq    0x48(%rsi), %r9         # Load r9
    movq    0x50(%rsi), %r10        # Load r10
    movq    0x58(%rsi), %r11        # Load r11
    movq    0x60(%rsi), %r12        # Load r12
    movq    0x68(%rsi), %r13        # Load r13
    movq    0x70(%rsi), %r14        # Load r14
    movq    0x78(%rsi), %r15        # Load r15

    # Prepare stack for iretq instruction
    # iretq expects: [rsp] = rip, [rsp+8] = cs, [rsp+16] = rflags,
    #               [rsp+24] = rsp, [rsp+32] = ss

    # Get the new stack pointer and create iret frame
    movq    0x38(%rsi), %rsp        # Load new stack pointer

    # Push iret frame onto new stack
    movq    0xB8(%rsi), %r11        # Load ss
    pushq   %r11                    # Push ss

    pushq   0x38(%rsi)              # Push target rsp (same as current)

    movq    0x88(%rsi), %r11        # Load rflags
    pushq   %r11                    # Push rflags

    movq    0x90(%rsi), %r11        # Load cs
    pushq   %r11                    # Push cs

    pushq   0x80(%rsi)              # Push rip (instruction pointer)

    # Load rsi last (we were using it to access new_context)
    movq    0x20(%rsi), %rsi        # Load rsi

    # Jump to new task using iretq
    # This will pop rip, cs, rflags, rsp, ss from stack
    # and continue execution at the new task's instruction pointer
    iretq

#
# Alternative simplified context switch for debugging
# Uses simple jmp instead of full iret mechanism
#
.global simple_context_switch
simple_context_switch:
    # Check if we need to save old context
    test    %rdi, %rdi              # Test if old_context is NULL
    jz      simple_load_new         # Skip save if NULL

    # Save essential registers only
    movq    %rsp, 0x38(%rdi)        # Save stack pointer
    movq    %rbp, 0x30(%rdi)        # Save base pointer
    movq    %rbx, 0x08(%rdi)        # Save rbx (callee-saved)
    movq    %r12, 0x60(%rdi)        # Save r12 (callee-saved)
    movq    %r13, 0x68(%rdi)        # Save r13 (callee-saved)
    movq    %r14, 0x70(%rdi)        # Save r14 (callee-saved)
    movq    %r15, 0x78(%rdi)        # Save r15 (callee-saved)

    # Save return address
    movq    (%rsp), %rax            # Get return address
    movq    %rax, 0x80(%rdi)        # Save as rip

simple_load_new:
    # Load new context
    movq    0x38(%rsi), %rsp        # Load stack pointer
    movq    0x30(%rsi), %rbp        # Load base pointer
    movq    0x08(%rsi), %rbx        # Load rbx
    movq    0x60(%rsi), %r12        # Load r12
    movq    0x68(%rsi), %r13        # Load r13
    movq    0x70(%rsi), %r14        # Load r14
    movq    0x78(%rsi), %r15        # Load r15

    # Jump to new instruction pointer
    jmpq    *0x80(%rsi)             # Jump to new rip

#
# Task entry point wrapper
# This is called when a new task starts execution for the first time
#
.global task_entry_wrapper
task_entry_wrapper:
    # At this point, the task entry point is in %rdi (from context setup)
    # and the task argument is already in %rsi

    # Preserve entry point and move argument into ABI position
    movq    %rdi, %rax              # Save entry function pointer
    movq    %rsi, %rdi              # Move argument into first parameter register

    # Call the task entry function
    callq   *%rax

    # If task returns, hand control back to the scheduler to terminate
    callq   scheduler_task_exit

    # Should never reach here, but halt defensively
    hlt

#
# Initialize first task context for kernel
# Used when transitioning from kernel boot to first task
#
.global init_kernel_context
init_kernel_context:
    # rdi points to kernel context structure to initialize
    # This saves current kernel state as a "task" context

    # Save current kernel registers
    movq    %rax, 0x00(%rdi)        # Save rax
    movq    %rbx, 0x08(%rdi)        # Save rbx
    movq    %rcx, 0x10(%rdi)        # Save rcx
    movq    %rdx, 0x18(%rdi)        # Save rdx
    movq    %rsi, 0x20(%rdi)        # Save rsi
    movq    %rdi, 0x28(%rdi)        # Save rdi
    movq    %rbp, 0x30(%rdi)        # Save rbp
    movq    %rsp, 0x38(%rdi)        # Save rsp
    movq    %r8,  0x40(%rdi)        # Save r8
    movq    %r9,  0x48(%rdi)        # Save r9
    movq    %r10, 0x50(%rdi)        # Save r10
    movq    %r11, 0x58(%rdi)        # Save r11
    movq    %r12, 0x60(%rdi)        # Save r12
    movq    %r13, 0x68(%rdi)        # Save r13
    movq    %r14, 0x70(%rdi)        # Save r14
    movq    %r15, 0x78(%rdi)        # Save r15

    # Save return address as rip
    movq    (%rsp), %rax            # Get return address
    movq    %rax, 0x80(%rdi)        # Save as rip

    # Save current flags
    pushfq                          # Push flags
    popq    %rax                    # Pop to rax
    movq    %rax, 0x88(%rdi)        # Save rflags

    # Save current segments
    movw    %cs, %ax
    movq    %rax, 0x90(%rdi)        # Save cs
    movw    %ds, %ax
    movq    %rax, 0x98(%rdi)        # Save ds
    movw    %es, %ax
    movq    %rax, 0xA0(%rdi)        # Save es
    movw    %fs, %ax
    movq    %rax, 0xA8(%rdi)        # Save fs
    movw    %gs, %ax
    movq    %rax, 0xB0(%rdi)        # Save gs
    movw    %ss, %ax
    movq    %rax, 0xB8(%rdi)        # Save ss

    # Save current page directory
    movq    %cr3, %rax
    movq    %rax, 0xC0(%rdi)        # Save cr3

    ret
