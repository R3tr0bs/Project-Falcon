#ifndef PMM_H
#define PMM_H

#include <stdint.h>

// Define page size as 4KB (standard for x86)
#define PMM_BLOCK_SIZE 4096

void pmm_init(uint32_t mem_size);
void* pmm_alloc_block();
void* pmm_alloc_blocks(uint32_t count);
void pmm_free_block(void* p);
void pmm_free_blocks(void* p, uint32_t count);
void pmm_mark_region_used(uint32_t base, uint32_t size);
void pmm_mark_region_free(uint32_t base, uint32_t size);
uint32_t pmm_get_total_blocks();
uint32_t pmm_get_used_blocks();
uint32_t pmm_get_free_blocks();

#endif
