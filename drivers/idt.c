/*
 * SlopOS Interrupt Descriptor Table (IDT) Infrastructure
 * Implementation of IDT management and initialization
 */

#include "serial.h"
#include <stdint.h>
#include <stddef.h>

/* Use the canonical IDT definitions from boot/ so we share vectors, gate
 * constants and the assembly ISR/IRQ stubs. This header also provides the
 * struct definitions for `idt_entry` and `idt_ptr` used below. */
#include "../boot/idt.h"
#include "../boot/constants.h"


/* If a project-local alias for the kernel code selector is not defined,
 * map it to the GDT selector defined in boot/constants.h */
#ifndef KERNEL_CODE_SELECTOR
#define KERNEL_CODE_SELECTOR GDT_CODE_SELECTOR
#endif

/* Forward declaration for kernel_panic */
extern void kernel_panic(const char *message);

/* ========================================================================
 * IDT DATA STRUCTURES
 * ======================================================================== */

/* IDT entries - 256 entries for complete interrupt vector coverage */
/* IDT entries and pointer (use types from ../boot/idt.h) */
static struct idt_entry idt[IDT_ENTRIES] __attribute__((aligned(16)));
static struct idt_ptr idt_ptr;

/* IDT initialization status */
static volatile int idt_initialized = 0;

/* ========================================================================
 * IDT ENTRY MANAGEMENT
 * ======================================================================== */

/*
 * Set a specific IDT entry
 * vector: Interrupt vector number (0-255)
 * handler: Address of the handler function
 * selector: Code segment selector (usually KERNEL_CODE_SELECTOR)
 * type_attr: Type and attribute flags
 */
void set_idt_entry(unsigned int vector, uint64_t handler, uint16_t selector, uint8_t type_attr) {
    if (vector >= IDT_ENTRIES) {
        kprintln("ERROR: Invalid IDT vector");
        return;
    }

    struct idt_entry *entry = &idt[vector];

    /* Set handler address (split across multiple fields) */
    entry->offset_low = handler & 0xFFFF;
    entry->offset_mid = (handler >> 16) & 0xFFFF;
    entry->offset_high = (handler >> 32) & 0xFFFFFFFF;

    /* Set segment selector */
    entry->selector = selector;

    /* Set type and attributes */
    entry->type_attr = type_attr;

    /* IST (Interrupt Stack Table) - not used for now */
    entry->ist = 0;

    /* Reserved/zero field must be 0 (name from ../boot/idt.h is `zero`) */
    entry->zero = 0;
}
/* Initialize a default IDT entry with the matching assembly ISR/IRQ stub.
 * We map exception vectors to `isrN` stubs and IRQs (32-47) to `irqN` stubs
 * as declared in ../boot/idt.h. For other vectors we fall back to isr0.
 */
static void set_default_idt_entry(unsigned int vector) {
    uint64_t handler = 0;

    if (vector < 20) {
        switch (vector) {
            case 0: handler = (uint64_t)isr0; break;
            case 1: handler = (uint64_t)isr1; break;
            case 2: handler = (uint64_t)isr2; break;
            case 3: handler = (uint64_t)isr3; break;
            case 4: handler = (uint64_t)isr4; break;
            case 5: handler = (uint64_t)isr5; break;
            case 6: handler = (uint64_t)isr6; break;
            case 7: handler = (uint64_t)isr7; break;
            case 8: handler = (uint64_t)isr8; break;
            case 10: handler = (uint64_t)isr10; break;
            case 11: handler = (uint64_t)isr11; break;
            case 12: handler = (uint64_t)isr12; break;
            case 13: handler = (uint64_t)isr13; break;
            case 14: handler = (uint64_t)isr14; break;
            case 16: handler = (uint64_t)isr16; break;
            case 17: handler = (uint64_t)isr17; break;
            case 18: handler = (uint64_t)isr18; break;
            case 19: handler = (uint64_t)isr19; break;
            default: handler = (uint64_t)isr0; break; /* reserved/unknown */
        }
        set_idt_entry(vector, handler, KERNEL_CODE_SELECTOR, IDT_GATE_INTERRUPT);
    } else if (vector >= 32 && vector < 48) {
        /* Map IRQs 32..47 to irq0..irq15 */
        switch (vector) {
            case 32: handler = (uint64_t)irq0; break;
            case 33: handler = (uint64_t)irq1; break;
            case 34: handler = (uint64_t)irq2; break;
            case 35: handler = (uint64_t)irq3; break;
            case 36: handler = (uint64_t)irq4; break;
            case 37: handler = (uint64_t)irq5; break;
            case 38: handler = (uint64_t)irq6; break;
            case 39: handler = (uint64_t)irq7; break;
            case 40: handler = (uint64_t)irq8; break;
            case 41: handler = (uint64_t)irq9; break;
            case 42: handler = (uint64_t)irq10; break;
            case 43: handler = (uint64_t)irq11; break;
            case 44: handler = (uint64_t)irq12; break;
            case 45: handler = (uint64_t)irq13; break;
            case 46: handler = (uint64_t)irq14; break;
            case 47: handler = (uint64_t)irq15; break;
            default: handler = (uint64_t)irq0; break;
        }
        set_idt_entry(vector, handler, KERNEL_CODE_SELECTOR, IDT_GATE_INTERRUPT);
    } else {
        /* For vectors without a dedicated stub, fall back to isr0 */
        set_idt_entry(vector, (uint64_t)isr0, KERNEL_CODE_SELECTOR, IDT_GATE_INTERRUPT);
    }
}

/* ========================================================================
 * IDT INITIALIZATION
 * ======================================================================== */

/*
 * Initialize all IDT entries with default handlers
 */
static void setup_default_handlers(void) {

    kprintln("IDT: Setting up default handlers for all 256 vectors");

    /* Initialize all entries with default handlers first */
    for (unsigned int i = 0; i < IDT_ENTRIES; i++) {
        set_default_idt_entry(i);
    }

    kprintln("IDT: Default handlers installed");
}

/* Setup any project-specific IDT overrides. We already installed default
 * ISR/IRQ stubs for all core vectors earlier; here we only set a software
 * interrupt handler if a C handler exists in this compilation unit.
 */
static void setup_exception_handlers(void) {
    kprintln("IDT: Installing specific exception handlers (none required)");

#ifdef software_interrupt_handler
    set_idt_entry(0x80, (uint64_t)software_interrupt_handler,
                 KERNEL_CODE_SELECTOR, IDT_GATE_INTERRUPT);
#endif

    kprintln("IDT: Exception handlers installed");
}

/*
 * Initialize the IDT
 */
void init_idt(void) {
    kprintln("IDT: Initializing Interrupt Descriptor Table");

    /* Clear the IDT first */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }

    /* Setup IDT pointer */
    idt_ptr.limit = (IDT_ENTRIES * sizeof(struct idt_entry)) - 1;
    idt_ptr.base = (uint64_t)&idt[0];

    kprint("IDT: Base address: ");
    kprint_hex(idt_ptr.base);
    kprintln("");
    kprint("IDT: Limit: ");
    kprint_hex(idt_ptr.limit);
    kprintln("");

    /* Install default handlers for all vectors */
    setup_default_handlers();

    /* Install specific exception handlers */
    setup_exception_handlers();

    /* Mark as initialized */
    idt_initialized = 1;

    kprintln("IDT: Initialization complete");
}

/*
 * Load the IDT into the processor
 */
void load_idt(void) {
    if (!idt_initialized) {
        kprintln("ERROR: IDT not initialized before loading");
        return;
    }

    kprintln("IDT: Loading IDT into processor");

    /* Load IDT using LIDT instruction */
    __asm__ volatile (
        "lidt %0"
        :
        : "m" (idt_ptr)
        : "memory"
    );

    kprintln("IDT: IDT loaded successfully");
}

/* ========================================================================
 * IDT QUERY AND DEBUG FUNCTIONS
 * ======================================================================== */

/*
 * Get the current IDT base address
 */
uint64_t get_idt_base(void) {
    return idt_ptr.base;
}

/*
 * Check if IDT is initialized
 */
int is_idt_initialized(void) {
    return idt_initialized;
}

/*
 * Dump IDT contents for debugging
 */
void dump_idt(void) {
    kprintln("IDT: Dumping IDT contents");
    kprint("IDT Base: ");
    kprint_hex(idt_ptr.base);
    kprintln("");
    kprint("IDT Limit: ");
    kprint_hex(idt_ptr.limit);
    kprintln("");

    /* Dump first 32 entries (exceptions) */
    kprintln("IDT: Exception vectors (0-31):");
    for (int i = 0; i < 32; i++) {
        struct idt_entry *entry = &idt[i];
        uint64_t handler = entry->offset_low |
                          ((uint64_t)entry->offset_mid << 16) |
                          ((uint64_t)entry->offset_high << 32);

        kprint("Vector ");
        kprint_hex(i);
        kprint(": Handler=");
        kprint_hex(handler);
        kprint(" Selector=");
        kprint_hex(entry->selector);
        kprint(" Type=");
        kprint_hex(entry->type_attr);
        kprintln("");
    }

    kprintln("IDT: Dump complete");
}

/* ========================================================================
 * IDT VERIFICATION AND TESTING
 * ======================================================================== */

/*
 * Verify IDT integrity
 */
int verify_idt_integrity(void) {
    if (!idt_initialized) {
        kprintln("IDT: Not initialized");
        return 0;
    }

    /* Check IDT pointer */
    if (idt_ptr.base != (uint64_t)&idt[0]) {
        kprintln("IDT: Base address mismatch");
        return 0;
    }

    if (idt_ptr.limit != (IDT_ENTRIES * sizeof(struct idt_entry)) - 1) {
        kprintln("IDT: Limit mismatch");
        return 0;
    }

    /* Check that critical exception handlers are set */
    for (int i = 0; i < 32; i++) {
        struct idt_entry *entry = &idt[i];
        uint64_t handler = entry->offset_low |
                          ((uint64_t)entry->offset_mid << 16) |
                          ((uint64_t)entry->offset_high << 32);

        if (handler == 0) {
            kprint("IDT: Exception vector ");
            kprint_hex(i);
            kprintln(" has null handler");
            return 0;
        }

        if (entry->selector != KERNEL_CODE_SELECTOR) {
            kprint("IDT: Exception vector ");
            kprint_hex(i);
            kprintln(" has wrong selector");
            return 0;
        }
    }

    kprintln("IDT: Integrity check passed");
    return 1;
}

/*
 * Test IDT by triggering a software interrupt
 */
void test_idt(void) {
    if (!idt_initialized) {
        kprintln("IDT: Cannot test - not initialized");
        return;
    }

    kprintln("IDT: Testing with software interrupt...");

    /* Trigger interrupt 0x80 (software interrupt) */
    __asm__ volatile ("int $0x80");

    kprintln("IDT: Software interrupt test completed");
}
