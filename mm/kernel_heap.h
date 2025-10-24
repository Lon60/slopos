/*
 * SlopOS Memory Management - Kernel Heap Interface
 * Exposes kmalloc/kfree for kernel subsystems that need dynamic allocation
 */

#ifndef MM_KERNEL_HEAP_H
#define MM_KERNEL_HEAP_H

#include <stddef.h>

void *kmalloc(size_t size);
void kfree(void *ptr);

#endif /* MM_KERNEL_HEAP_H */

