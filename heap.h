#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

void heap_init(uint32_t start_addr, uint32_t end_addr);
void* kmalloc(size_t size);
void* kmalloc_aligned(size_t size, uint32_t alignment);
void* kmalloc_physical(size_t size, uint32_t* phys);
void kfree(void* ptr);

#endif