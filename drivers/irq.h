#ifndef DRIVERS_IRQ_H
#define DRIVERS_IRQ_H

#include <stdint.h>

struct interrupt_frame;

typedef void (*irq_handler_t)(uint8_t irq, struct interrupt_frame *frame, void *context);

struct irq_stats {
    uint64_t count;
    uint64_t last_timestamp;
};

void irq_init(void);
int irq_register_handler(uint8_t irq, irq_handler_t handler, void *context, const char *name);
void irq_unregister_handler(uint8_t irq);
void irq_enable_line(uint8_t irq);
void irq_disable_line(uint8_t irq);
void irq_dispatch(struct interrupt_frame *frame);
int irq_get_stats(uint8_t irq, struct irq_stats *out_stats);

#endif /* DRIVERS_IRQ_H */
