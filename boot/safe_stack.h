/*
 * SlopOS Safe Exception Stack Management
 * Allocates IST stacks with guard pages and tracks usage diagnostics
 */

#ifndef SAFE_STACK_H
#define SAFE_STACK_H

#include <stdint.h>

void safe_stack_init(void);
void safe_stack_record_usage(uint8_t vector, uint64_t frame_ptr);
int safe_stack_guard_fault(uint64_t fault_addr, const char **stack_name);

#endif /* SAFE_STACK_H */
