/*
 * SlopOS PIC (Programmable Interrupt Controller) Driver
 * 8259 PIC management for legacy interrupt handling
 */

#ifndef PIC_H
#define PIC_H

#include <stdint.h>

// PIC ports
#define PIC1_COMMAND_PORT    0x20  // Master PIC command port
#define PIC1_DATA_PORT       0x21  // Master PIC data port
#define PIC2_COMMAND_PORT    0xA0  // Slave PIC command port
#define PIC2_DATA_PORT       0xA1  // Slave PIC data port

// PIC commands
#define PIC_EOI              0x20  // End of Interrupt command
#define PIC_INIT             0x11  // Initialize PIC
#define PIC_ICW4_8086        0x01  // 8086 mode

// Default PIC IRQ mappings (before remapping)
#define PIC_DEFAULT_MASTER_BASE  0x08  // IRQ 0-7 mapped to interrupts 8-15
#define PIC_DEFAULT_SLAVE_BASE   0x70  // IRQ 8-15 mapped to interrupts 112-119

// Our remapped IRQ base (to avoid conflicts with CPU exceptions)
#define PIC_MASTER_BASE      0x20  // IRQ 0-7 mapped to interrupts 32-39
#define PIC_SLAVE_BASE       0x28  // IRQ 8-15 mapped to interrupts 40-47

// IRQ line numbers
#define IRQ_TIMER            0
#define IRQ_KEYBOARD         1
#define IRQ_CASCADE          2   // Used internally by PICs
#define IRQ_COM2             3
#define IRQ_COM1             4
#define IRQ_LPT2             5
#define IRQ_FLOPPY           6
#define IRQ_LPT1             7
#define IRQ_RTC              8
#define IRQ_FREE1            9
#define IRQ_FREE2            10
#define IRQ_FREE3            11
#define IRQ_MOUSE            12
#define IRQ_FPU              13
#define IRQ_ATA_PRIMARY      14
#define IRQ_ATA_SECONDARY    15

// PIC management functions
void pic_init(void);
void pic_remap(uint8_t master_base, uint8_t slave_base);
void pic_disable(void);
void pic_enable_irq(uint8_t irq);
void pic_disable_irq(uint8_t irq);
void pic_send_eoi(uint8_t irq);
uint16_t pic_get_irr(void);  // Get Interrupt Request Register
uint16_t pic_get_isr(void);  // Get In-Service Register
uint8_t pic_read_master_mask(void);
uint8_t pic_read_slave_mask(void);
void pic_set_master_mask(uint8_t mask);
void pic_set_slave_mask(uint8_t mask);

// Utility functions
int pic_irq_is_spurious(uint8_t irq);
void pic_dump_state(void);

#endif // PIC_H