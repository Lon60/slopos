/*
 * SlopOS Exception Handler C Functions
 * High-level exception handling and debugging functionality
 */

#include "idt.h"
#include "serial.h"
#include "../boot/debug.h"
#include <stdint.h>
#include <stddef.h>

/* ========================================================================
 * DEBUGGING AND UTILITY FUNCTIONS
 * ======================================================================== */

/*
 * Read CR2 register (page fault linear address)
 */
static uint64_t read_cr2(void) {
    uint64_t cr2;
    __asm__ volatile ("movq %%cr2, %0" : "=r" (cr2));
    return cr2;
}

/*
 * Read CR3 register (page directory base)
 */
static uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("movq %%cr3, %0" : "=r" (cr3));
    return cr3;
}

/*
 * Read RFLAGS register
 */
static uint64_t read_rflags(void) {
    uint64_t rflags;
    __asm__ volatile ("pushfq; popq %0" : "=r" (rflags));
    return rflags;
}

/*
 * Get current instruction pointer
 */
static uint64_t get_current_rip(void) {
    uint64_t rip;
    __asm__ volatile ("leaq (%%rip), %0" : "=r" (rip));
    return rip;
}

/*
 * Dump register state for debugging
 */
static void dump_registers(void) {
    uint64_t rsp, rbp;

    __asm__ volatile ("movq %%rsp, %0" : "=r" (rsp));
    __asm__ volatile ("movq %%rbp, %0" : "=r" (rbp));

    kprintln("=== REGISTER DUMP ===");
    kprint("RSP: "); kprint_hex(rsp); kprintln("");
    kprint("RBP: "); kprint_hex(rbp); kprintln("");
    kprint("CR2: "); kprint_hex(read_cr2()); kprintln("");
    kprint("CR3: "); kprint_hex(read_cr3()); kprintln("");
    kprint("RFLAGS: "); kprint_hex(read_rflags()); kprintln("");
    kprint("RIP: "); kprint_hex(get_current_rip()); kprintln("");
    kprintln("====================");
}

/*
 * Analyze page fault error code
 */
static void analyze_page_fault_error(uint64_t error_code) {
    kprintln("=== PAGE FAULT ERROR ANALYSIS ===");

    if (error_code & 0x1) {
        kprintln("Cause: Page protection violation");
    } else {
        kprintln("Cause: Page not present");
    }

    if (error_code & 0x2) {
        kprintln("Access: Write operation");
    } else {
        kprintln("Access: Read operation");
    }

    if (error_code & 0x4) {
        kprintln("Mode: User mode");
    } else {
        kprintln("Mode: Supervisor mode");
    }

    if (error_code & 0x8) {
        kprintln("Reserved bits: Set in page table entry");
    }

    if (error_code & 0x10) {
        kprintln("Cause: Instruction fetch");
    }

    if (error_code & 0x20) {
        kprintln("Protection Key: Violation detected");
    }

    if (error_code & 0x40) {
        kprintln("Shadow Stack: Access violation");
    }

    if (error_code & 0x8000) {
        kprintln("SGX: Violation detected");
    }

    kprintln("================================");
}

/*
 * Emergency kernel panic for critical exceptions
 */
static void emergency_panic(const char *message) {
    /* Disable interrupts to prevent further issues */
    __asm__ volatile ("cli");

    kprintln("");
    kprintln("!!! KERNEL PANIC !!!");
    kprintln(message);
    kprintln("System halted due to critical exception");

    dump_registers();

    kprintln("Halting system...");

    /* Halt the system */
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* ========================================================================
 * EXCEPTION HANDLERS WITHOUT ERROR CODE
 * ======================================================================== */

void handle_divide_error(void) {
    kprintln("EXCEPTION: Divide Error (#DE)");
    dump_registers();
    emergency_panic("Division by zero or overflow in division operation");
}

void handle_debug_exception(void) {
    kprintln("EXCEPTION: Debug (#DB)");
    dump_registers();
    kprintln("Debug exception occurred - continuing execution");
}

void handle_nmi(void) {
    kprintln("EXCEPTION: Non-Maskable Interrupt (NMI)");
    dump_registers();
    kprintln("NMI received - hardware issue possible");
}

void handle_breakpoint(void) {
    kprintln("EXCEPTION: Breakpoint (#BP)");
    dump_registers();
    kprintln("Breakpoint hit - continuing execution");
}

void handle_overflow(void) {
    kprintln("EXCEPTION: Overflow (#OF)");
    dump_registers();
    emergency_panic("Arithmetic overflow detected");
}

void handle_bound_range(void) {
    kprintln("EXCEPTION: Bound Range Exceeded (#BR)");
    dump_registers();
    emergency_panic("Array bounds check failed");
}

void handle_invalid_opcode(void) {
    kprintln("EXCEPTION: Invalid Opcode (#UD)");
    dump_registers();
    emergency_panic("Invalid or undefined opcode executed");
}

void handle_device_not_available(void) {
    kprintln("EXCEPTION: Device Not Available (#NM)");
    dump_registers();
    emergency_panic("FPU or other device not available");
}

void handle_coprocessor_overrun(void) {
    kprintln("EXCEPTION: Coprocessor Segment Overrun");
    dump_registers();
    emergency_panic("Legacy coprocessor segment overrun");
}

void handle_x87_fpu_error(void) {
    kprintln("EXCEPTION: x87 FPU Floating-Point Error (#MF)");
    dump_registers();
    emergency_panic("x87 FPU floating-point error");
}

void handle_machine_check(void) {
    kprintln("EXCEPTION: Machine Check (#MC)");
    dump_registers();
    emergency_panic("Hardware machine check error - system unstable");
}

void handle_simd_fp_exception(void) {
    kprintln("EXCEPTION: SIMD Floating-Point (#XM)");
    dump_registers();
    emergency_panic("SIMD floating-point exception");
}

void handle_virtualization_exception(void) {
    kprintln("EXCEPTION: Virtualization (#VE)");
    dump_registers();
    emergency_panic("Virtualization exception");
}

/* ========================================================================
 * EXCEPTION HANDLERS WITH ERROR CODE
 * ======================================================================== */

void handle_double_fault(uint64_t error_code) {
    kprintln("CRITICAL EXCEPTION: Double Fault (#DF)");
    kprint("Error Code: "); kprint_hex(error_code); kprintln("");
    dump_registers();
    emergency_panic("Double fault - critical system failure");
}

void handle_invalid_tss(uint64_t error_code) {
    kprintln("EXCEPTION: Invalid TSS (#TS)");
    kprint("Error Code: "); kprint_hex(error_code); kprintln("");
    dump_registers();
    emergency_panic("Invalid Task State Segment");
}

void handle_segment_not_present(uint64_t error_code) {
    kprintln("EXCEPTION: Segment Not Present (#NP)");
    kprint("Error Code: "); kprint_hex(error_code); kprintln("");
    dump_registers();
    emergency_panic("Required segment not present in memory");
}

void handle_stack_fault(uint64_t error_code) {
    kprintln("EXCEPTION: Stack-Segment Fault (#SS)");
    kprint("Error Code: "); kprint_hex(error_code); kprintln("");
    dump_registers();
    emergency_panic("Stack segment fault");
}

void handle_general_protection(uint64_t error_code) {
    kprintln("EXCEPTION: General Protection Fault (#GP)");
    kprint("Error Code: "); kprint_hex(error_code); kprintln("");
    dump_registers();

    if (error_code == 0) {
        kprintln("Cause: Protection violation not related to segment");
    } else {
        kprint("Segment selector index: "); kprint_hex(error_code >> 3); kprintln("");
        kprint("Table indicator: ");
        if (error_code & 0x4) {
            kprintln("LDT");
        } else {
            kprintln("GDT");
        }
        kprint("External event: ");
        if (error_code & 0x1) {
            kprintln("Yes");
        } else {
            kprintln("No");
        }
    }

    emergency_panic("General protection violation");
}

void handle_page_fault(uint64_t error_code) {
    uint64_t fault_addr = read_cr2();

    kprintln("EXCEPTION: Page Fault (#PF)");
    kprint("Faulting Address: "); kprint_hex(fault_addr); kprintln("");
    kprint("Error Code: "); kprint_hex(error_code); kprintln("");

    analyze_page_fault_error(error_code);
    dump_registers();

    /* Check if this is a kernel or user space fault */
    if (fault_addr >= 0xFFFF800000000000ULL) {
        kprintln("Fault in kernel space - critical error");
        emergency_panic("Kernel space page fault");
    } else {
        kprintln("Fault in user space");
        emergency_panic("User space page fault - no user space handler yet");
    }
}

void handle_alignment_check(uint64_t error_code) {
    kprintln("EXCEPTION: Alignment Check (#AC)");
    kprint("Error Code: "); kprint_hex(error_code); kprintln("");
    dump_registers();
    emergency_panic("Memory alignment check failed");
}

void handle_control_protection_exception(uint64_t error_code) {
    kprintln("EXCEPTION: Control Protection (#CP)");
    kprint("Error Code: "); kprint_hex(error_code); kprintln("");
    dump_registers();
    emergency_panic("Control protection exception");
}

/* ========================================================================
 * DEFAULT HANDLERS
 * ======================================================================== */

void handle_unknown_exception(uint8_t vector) {
    kprintln("EXCEPTION: Unknown Exception");
    kprint("Vector: "); kprint_hex(vector); kprintln("");
    dump_registers();
    emergency_panic("Unknown exception occurred");
}

void handle_unknown_interrupt(uint8_t vector) {
    kprintln("INTERRUPT: Unknown Interrupt");
    kprint("Vector: "); kprint_hex(vector); kprintln("");

    /* For interrupts, we can continue execution */
    kprintln("Ignoring unknown interrupt");
}

void handle_software_interrupt(uint8_t vector) {
    kprintln("SOFTWARE INTERRUPT: Test interrupt");
    kprint("Vector: "); kprint_hex(vector); kprintln("");
    kprintln("Software interrupt handled successfully");
}

/* ========================================================================
 * MEMORY MAPPING VERIFICATION AND DEBUG
 * ======================================================================== */

/*
 * Verify memory mapping for a given virtual address
 */
void verify_memory_mapping(uint64_t virtual_addr) {
    kprintln("=== MEMORY MAPPING VERIFICATION ===");
    kprint("Virtual Address: "); kprint_hex(virtual_addr); kprintln("");

    uint64_t cr3 = read_cr3();
    kprint("Page Directory Base (CR3): "); kprint_hex(cr3); kprintln("");

    /* Extract page table indices */
    uint64_t pml4_index = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdpt_index = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_addr >> 12) & 0x1FF;
    uint64_t offset = virtual_addr & 0xFFF;

    kprint("PML4 Index: "); kprint_hex(pml4_index); kprintln("");
    kprint("PDPT Index: "); kprint_hex(pdpt_index); kprintln("");
    kprint("PD Index: "); kprint_hex(pd_index); kprintln("");
    kprint("PT Index: "); kprint_hex(pt_index); kprintln("");
    kprint("Page Offset: "); kprint_hex(offset); kprintln("");

    kprintln("===================================");
}

/*
 * Analyze a page fault in detail
 */
void analyze_page_fault(uint64_t fault_addr, uint64_t error_code) {
    kprintln("=== DETAILED PAGE FAULT ANALYSIS ===");

    verify_memory_mapping(fault_addr);
    analyze_page_fault_error(error_code);

    /* Check common fault patterns */
    if (fault_addr == 0) {
        kprintln("Pattern: NULL pointer dereference");
    } else if (fault_addr < 0x1000) {
        kprintln("Pattern: Low memory access (likely NULL + offset)");
    } else if ((fault_addr & 0xFFF) == 0) {
        kprintln("Pattern: Page-aligned access");
    }

    /* Check if address is in common kernel regions */
    if (fault_addr >= 0xFFFFFFFF80000000ULL) {
        kprintln("Region: Higher-half kernel space");
    } else if (fault_addr >= 0xFFFF800000000000ULL) {
        kprintln("Region: Kernel space");
    } else if (fault_addr >= 0x400000) {
        kprintln("Region: User space");
    } else {
        kprintln("Region: Low memory");
    }

    kprintln("====================================");
}

/*
 * Dump stack trace (simplified version)
 */
void dump_stack_trace(uint64_t rbp, uint64_t rip) {
    kprintln("=== STACK TRACE ===");
    kprint("Start RIP: "); kprint_hex(rip); kprintln("");
    kprint("Start RBP: "); kprint_hex(rbp); kprintln("");

    int frame_index = 0;
    uint64_t current_rbp = rbp;

    while (current_rbp && frame_index < STACK_TRACE_DEPTH) {
        /* Validate that the frame pointer and return address are safe to read */
        if (!debug_is_valid_memory_address(current_rbp) ||
            !debug_is_valid_memory_address(current_rbp + sizeof(uint64_t))) {
            kprint("Frame "); kprint_decimal(frame_index);
            kprint(": invalid frame pointer ");
            kprint_hex(current_rbp); kprintln("");
            break;
        }

        uint64_t *frame = (uint64_t *)current_rbp;
        uint64_t next_rbp = frame[0];
        uint64_t return_rip = frame[1];

        kprint("Frame ");
        kprint_decimal(frame_index);
        kprint(": RBP=");
        kprint_hex(current_rbp);
        kprint(" RIP=");
        kprint_hex(return_rip);

        const char *symbol = debug_get_symbol_name(return_rip);
        if (symbol) {
            kprint(" (");
            kprint(symbol);
            kprint(")");
        }
        kprintln("");

        if (!next_rbp || next_rbp <= current_rbp) {
            kprintln("Frame: Non-increasing RBP detected, stopping trace");
            break;
        }

        current_rbp = next_rbp;
        frame_index++;
    }

    if (frame_index == 0) {
        kprintln("No stack frames walked");
    }

    kprintln("==================");
}
