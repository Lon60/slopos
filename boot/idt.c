/*
 * SlopOS Interrupt Descriptor Table (IDT) Implementation
 * x86_64 IDT setup and exception handling
 */

#include "idt.h"
#include "constants.h"
#include "../drivers/serial.h"

// Global IDT and pointer
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_pointer;

// Custom exception handlers (can be overridden)
static exception_handler_t exception_handlers[32] = {0};

/*
 * Initialize the IDT with default exception handlers
 */
void idt_init(void) {
    kprintln("IDT: Initializing Interrupt Descriptor Table");

    // Clear the IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }

    // Set up the IDT pointer
    idt_pointer.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idt_pointer.base = (uint64_t)&idt;

    // Install exception handlers
    // Exceptions 0-19 are defined by Intel
    idt_set_gate(0, (uint64_t)isr0, 0x08, IDT_GATE_INTERRUPT);   // Divide Error
    idt_set_gate(1, (uint64_t)isr1, 0x08, IDT_GATE_INTERRUPT);   // Debug
    idt_set_gate(2, (uint64_t)isr2, 0x08, IDT_GATE_INTERRUPT);   // NMI
    idt_set_gate(3, (uint64_t)isr3, 0x08, IDT_GATE_TRAP);        // Breakpoint
    idt_set_gate(4, (uint64_t)isr4, 0x08, IDT_GATE_TRAP);        // Overflow
    idt_set_gate(5, (uint64_t)isr5, 0x08, IDT_GATE_INTERRUPT);   // Bound Range
    idt_set_gate(6, (uint64_t)isr6, 0x08, IDT_GATE_INTERRUPT);   // Invalid Opcode
    idt_set_gate(7, (uint64_t)isr7, 0x08, IDT_GATE_INTERRUPT);   // Device Not Available
    idt_set_gate(8, (uint64_t)isr8, 0x08, IDT_GATE_INTERRUPT);   // Double Fault
    // Vector 9 is reserved
    idt_set_gate(10, (uint64_t)isr10, 0x08, IDT_GATE_INTERRUPT); // Invalid TSS
    idt_set_gate(11, (uint64_t)isr11, 0x08, IDT_GATE_INTERRUPT); // Segment Not Present
    idt_set_gate(12, (uint64_t)isr12, 0x08, IDT_GATE_INTERRUPT); // Stack Fault
    idt_set_gate(13, (uint64_t)isr13, 0x08, IDT_GATE_INTERRUPT); // General Protection
    idt_set_gate(14, (uint64_t)isr14, 0x08, IDT_GATE_INTERRUPT); // Page Fault
    // Vector 15 is reserved
    idt_set_gate(16, (uint64_t)isr16, 0x08, IDT_GATE_INTERRUPT); // FPU Error
    idt_set_gate(17, (uint64_t)isr17, 0x08, IDT_GATE_INTERRUPT); // Alignment Check
    idt_set_gate(18, (uint64_t)isr18, 0x08, IDT_GATE_INTERRUPT); // Machine Check
    idt_set_gate(19, (uint64_t)isr19, 0x08, IDT_GATE_INTERRUPT); // SIMD FP Exception

    // Install IRQ handlers (vectors 32-47)
    idt_set_gate(32, (uint64_t)irq0, 0x08, IDT_GATE_INTERRUPT);  // Timer
    idt_set_gate(33, (uint64_t)irq1, 0x08, IDT_GATE_INTERRUPT);  // Keyboard
    idt_set_gate(34, (uint64_t)irq2, 0x08, IDT_GATE_INTERRUPT);  // Cascade
    idt_set_gate(35, (uint64_t)irq3, 0x08, IDT_GATE_INTERRUPT);  // COM2
    idt_set_gate(36, (uint64_t)irq4, 0x08, IDT_GATE_INTERRUPT);  // COM1
    idt_set_gate(37, (uint64_t)irq5, 0x08, IDT_GATE_INTERRUPT);  // LPT2
    idt_set_gate(38, (uint64_t)irq6, 0x08, IDT_GATE_INTERRUPT);  // Floppy
    idt_set_gate(39, (uint64_t)irq7, 0x08, IDT_GATE_INTERRUPT);  // LPT1
    idt_set_gate(40, (uint64_t)irq8, 0x08, IDT_GATE_INTERRUPT);  // RTC
    idt_set_gate(41, (uint64_t)irq9, 0x08, IDT_GATE_INTERRUPT);  // Free
    idt_set_gate(42, (uint64_t)irq10, 0x08, IDT_GATE_INTERRUPT); // Free
    idt_set_gate(43, (uint64_t)irq11, 0x08, IDT_GATE_INTERRUPT); // Free
    idt_set_gate(44, (uint64_t)irq12, 0x08, IDT_GATE_INTERRUPT); // Mouse
    idt_set_gate(45, (uint64_t)irq13, 0x08, IDT_GATE_INTERRUPT); // FPU
    idt_set_gate(46, (uint64_t)irq14, 0x08, IDT_GATE_INTERRUPT); // ATA Primary
    idt_set_gate(47, (uint64_t)irq15, 0x08, IDT_GATE_INTERRUPT); // ATA Secondary

    kprint("IDT: Configured ");
    kprint_dec(IDT_ENTRIES);
    kprintln(" interrupt vectors");
}

/*
 * Set an IDT gate
 */
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t type) {
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].ist = 0;  // No separate interrupt stacks for now
    idt[vector].type_attr = type | 0x60;  // DPL=3 for user access, Present=1
    idt[vector].offset_mid = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}

/*
 * Install a custom exception handler
 */
void idt_install_exception_handler(uint8_t vector, exception_handler_t handler) {
    if (vector < 32) {
        exception_handlers[vector] = handler;
        kprint("IDT: Installed custom handler for exception ");
        kprint_dec(vector);
        kprintln("");
    }
}

/*
 * Load the IDT
 */
void idt_load(void) {
    kprint("IDT: Loading IDT at address ");
    kprint_hex(idt_pointer.base);
    kprint(" with limit ");
    kprint_hex(idt_pointer.limit);
    kprintln("");

    // Load the IDT using assembly
    __asm__ volatile ("lidt %0" : : "m" (idt_pointer));

    kprintln("IDT: Successfully loaded");
}

/*
 * Common exception handler dispatcher
 */
void common_exception_handler(struct interrupt_frame *frame) {
    kprint("EXCEPTION: Vector ");
    kprint_dec(frame->vector);
    kprint(" (");
    kprint(get_exception_name(frame->vector));
    kprintln(")");

    // Call custom handler if installed
    if (frame->vector < 32 && exception_handlers[frame->vector] != NULL) {
        exception_handlers[frame->vector](frame);
        return;
    }

    // Default exception handling
    switch (frame->vector) {
        case EXCEPTION_DIVIDE_ERROR:
            exception_divide_error(frame);
            break;
        case EXCEPTION_DEBUG:
            exception_debug(frame);
            break;
        case EXCEPTION_NMI:
            exception_nmi(frame);
            break;
        case EXCEPTION_BREAKPOINT:
            exception_breakpoint(frame);
            break;
        case EXCEPTION_OVERFLOW:
            exception_overflow(frame);
            break;
        case EXCEPTION_BOUND_RANGE:
            exception_bound_range(frame);
            break;
        case EXCEPTION_INVALID_OPCODE:
            exception_invalid_opcode(frame);
            break;
        case EXCEPTION_DEVICE_NOT_AVAIL:
            exception_device_not_available(frame);
            break;
        case EXCEPTION_DOUBLE_FAULT:
            exception_double_fault(frame);
            break;
        case EXCEPTION_INVALID_TSS:
            exception_invalid_tss(frame);
            break;
        case EXCEPTION_SEGMENT_NOT_PRES:
            exception_segment_not_present(frame);
            break;
        case EXCEPTION_STACK_FAULT:
            exception_stack_fault(frame);
            break;
        case EXCEPTION_GENERAL_PROTECTION:
            exception_general_protection(frame);
            break;
        case EXCEPTION_PAGE_FAULT:
            exception_page_fault(frame);
            break;
        case EXCEPTION_FPU_ERROR:
            exception_fpu_error(frame);
            break;
        case EXCEPTION_ALIGNMENT_CHECK:
            exception_alignment_check(frame);
            break;
        case EXCEPTION_MACHINE_CHECK:
            exception_machine_check(frame);
            break;
        case EXCEPTION_SIMD_FP_EXCEPTION:
            exception_simd_fp_exception(frame);
            break;
        default:
            kprint("EXCEPTION: Unknown exception vector ");
            kprint_dec(frame->vector);
            kprintln("");
            dump_interrupt_frame(frame);
            break;
    }
}

/*
 * Get exception name string
 */
const char *get_exception_name(uint8_t vector) {
    static const char *exception_names[] = {
        "Divide Error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "Bound Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Segment Fault",
        "General Protection Fault",
        "Page Fault",
        "Reserved",
        "x87 FPU Error",
        "Alignment Check",
        "Machine Check",
        "SIMD Floating-Point Exception"
    };

    if (vector < 20) {
        return exception_names[vector];
    } else if (vector >= 32 && vector < 48) {
        return "Hardware IRQ";
    } else {
        return "Unknown";
    }
}

/*
 * Dump interrupt frame for debugging
 */
void dump_interrupt_frame(struct interrupt_frame *frame) {
    kprintln("=== INTERRUPT FRAME DUMP ===");

    kprint("Vector: ");
    kprint_dec(frame->vector);
    kprint(" Error Code: ");
    kprint_hex(frame->error_code);
    kprintln("");

    kprint("RIP: ");
    kprint_hex(frame->rip);
    kprint(" CS: ");
    kprint_hex(frame->cs);
    kprintln("");

    kprint("RFLAGS: ");
    kprint_hex(frame->rflags);
    kprint(" RSP: ");
    kprint_hex(frame->rsp);
    kprint(" SS: ");
    kprint_hex(frame->ss);
    kprintln("");

    kprint("RAX: ");
    kprint_hex(frame->rax);
    kprint(" RBX: ");
    kprint_hex(frame->rbx);
    kprint(" RCX: ");
    kprint_hex(frame->rcx);
    kprintln("");

    kprint("RDX: ");
    kprint_hex(frame->rdx);
    kprint(" RSI: ");
    kprint_hex(frame->rsi);
    kprint(" RDI: ");
    kprint_hex(frame->rdi);
    kprintln("");

    kprint("RBP: ");
    kprint_hex(frame->rbp);
    kprint(" R8: ");
    kprint_hex(frame->r8);
    kprint(" R9: ");
    kprint_hex(frame->r9);
    kprintln("");

    kprint("R10: ");
    kprint_hex(frame->r10);
    kprint(" R11: ");
    kprint_hex(frame->r11);
    kprint(" R12: ");
    kprint_hex(frame->r12);
    kprintln("");

    kprint("R13: ");
    kprint_hex(frame->r13);
    kprint(" R14: ");
    kprint_hex(frame->r14);
    kprint(" R15: ");
    kprint_hex(frame->r15);
    kprintln("");

    kprintln("=== END FRAME DUMP ===");
}

/*
 * Dump current CPU state
 */
void dump_cpu_state(void) {
    uint64_t rsp, rbp, rax, rbx, rcx, rdx;
    uint16_t cs, ds, es, ss;
    uint64_t cr0, cr2, cr3, cr4;
    uint64_t rflags;

    // Get general registers
    __asm__ volatile ("movq %%rsp, %0" : "=r" (rsp));
    __asm__ volatile ("movq %%rbp, %0" : "=r" (rbp));
    __asm__ volatile ("movq %%rax, %0" : "=r" (rax));
    __asm__ volatile ("movq %%rbx, %0" : "=r" (rbx));
    __asm__ volatile ("movq %%rcx, %0" : "=r" (rcx));
    __asm__ volatile ("movq %%rdx, %0" : "=r" (rdx));

    // Get segment registers
    __asm__ volatile ("movw %%cs, %0" : "=r" (cs));
    __asm__ volatile ("movw %%ds, %0" : "=r" (ds));
    __asm__ volatile ("movw %%es, %0" : "=r" (es));
    __asm__ volatile ("movw %%ss, %0" : "=r" (ss));

    // Get control registers
    __asm__ volatile ("movq %%cr0, %0" : "=r" (cr0));
    __asm__ volatile ("movq %%cr2, %0" : "=r" (cr2));
    __asm__ volatile ("movq %%cr3, %0" : "=r" (cr3));
    __asm__ volatile ("movq %%cr4, %0" : "=r" (cr4));

    // Get flags
    __asm__ volatile ("pushfq; popq %0" : "=r" (rflags));

    kprintln("=== CPU STATE DUMP ===");
    kprint("RSP: ");
    kprint_hex(rsp);
    kprint(" RBP: ");
    kprint_hex(rbp);
    kprintln("");

    kprint("RAX: ");
    kprint_hex(rax);
    kprint(" RBX: ");
    kprint_hex(rbx);
    kprint(" RCX: ");
    kprint_hex(rcx);
    kprint(" RDX: ");
    kprint_hex(rdx);
    kprintln("");

    kprint("CS: ");
    kprint_hex(cs);
    kprint(" DS: ");
    kprint_hex(ds);
    kprint(" ES: ");
    kprint_hex(es);
    kprint(" SS: ");
    kprint_hex(ss);
    kprintln("");

    kprint("CR0: ");
    kprint_hex(cr0);
    kprint(" CR2: ");
    kprint_hex(cr2);
    kprintln("");

    kprint("CR3: ");
    kprint_hex(cr3);
    kprint(" CR4: ");
    kprint_hex(cr4);
    kprintln("");

    kprint("RFLAGS: ");
    kprint_hex(rflags);
    kprintln("");
    kprintln("=== END CPU STATE DUMP ===");
}

/*
 * Default exception handlers
 */

void exception_divide_error(struct interrupt_frame *frame) {
    kprintln("FATAL: Divide by zero error");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Divide by zero error");
}

void exception_debug(struct interrupt_frame *frame) {
    kprintln("DEBUG: Debug exception occurred");
    dump_interrupt_frame(frame);
}

void exception_nmi(struct interrupt_frame *frame) {
    kprintln("FATAL: Non-maskable interrupt");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Non-maskable interrupt");
}

void exception_breakpoint(struct interrupt_frame *frame) {
    kprintln("DEBUG: Breakpoint exception");
    dump_interrupt_frame(frame);
}

void exception_overflow(struct interrupt_frame *frame) {
    kprintln("ERROR: Overflow exception");
    dump_interrupt_frame(frame);
}

void exception_bound_range(struct interrupt_frame *frame) {
    kprintln("ERROR: Bound range exceeded");
    dump_interrupt_frame(frame);
}

void exception_invalid_opcode(struct interrupt_frame *frame) {
    kprintln("FATAL: Invalid opcode");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Invalid opcode");
}

void exception_device_not_available(struct interrupt_frame *frame) {
    kprintln("ERROR: Device not available");
    dump_interrupt_frame(frame);
}

void exception_double_fault(struct interrupt_frame *frame) {
    kprintln("FATAL: Double fault");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Double fault");
}

void exception_invalid_tss(struct interrupt_frame *frame) {
    kprintln("FATAL: Invalid TSS");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Invalid TSS");
}

void exception_segment_not_present(struct interrupt_frame *frame) {
    kprintln("FATAL: Segment not present");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Segment not present");
}

void exception_stack_fault(struct interrupt_frame *frame) {
    kprintln("FATAL: Stack segment fault");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Stack segment fault");
}

void exception_general_protection(struct interrupt_frame *frame) {
    kprintln("FATAL: General protection fault");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("General protection fault");
}

void exception_page_fault(struct interrupt_frame *frame) {
    uint64_t fault_addr;
    __asm__ volatile ("movq %%cr2, %0" : "=r" (fault_addr));

    kprintln("FATAL: Page fault");
    kprint("Fault address: ");
    kprint_hex(fault_addr);
    kprintln("");

    kprint("Error code: ");
    kprint_hex(frame->error_code);
    if (frame->error_code & 1) kprint(" (Page present)");
    else kprint(" (Page not present)");
    if (frame->error_code & 2) kprint(" (Write)");
    else kprint(" (Read)");
    if (frame->error_code & 4) kprint(" (User)");
    else kprint(" (Supervisor)");
    kprintln("");

    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Page fault");
}

void exception_fpu_error(struct interrupt_frame *frame) {
    kprintln("ERROR: x87 FPU error");
    dump_interrupt_frame(frame);
}

void exception_alignment_check(struct interrupt_frame *frame) {
    kprintln("ERROR: Alignment check");
    dump_interrupt_frame(frame);
}

void exception_machine_check(struct interrupt_frame *frame) {
    kprintln("FATAL: Machine check");
    dump_interrupt_frame(frame);
    extern void kernel_panic(const char *message);
    kernel_panic("Machine check");
}

void exception_simd_fp_exception(struct interrupt_frame *frame) {
    kprintln("ERROR: SIMD floating-point exception");
    dump_interrupt_frame(frame);
}