#include "pmm.h"
#include "utils.h"
#include "vga.h"

// Bitmap to track memory usage.
// 1024 * 32 * 32 bits covers 4GB of RAM (max for 32-bit).
// Each bit represents one 4KB block.
static uint32_t pmm_bitmap[32768];
static uint32_t pmm_total_blocks = 0;
static uint32_t pmm_used_blocks = 0;

// Helper: Set a bit in the bitmap (mark as used)
static void pmm_set_bit(uint32_t bit) {
    pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

// Helper: Unset a bit in the bitmap (mark as free)
static void pmm_unset_bit(uint32_t bit) {
    pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

// Helper: Check if a bit is set
static int pmm_test_bit(uint32_t bit) {
    return pmm_bitmap[bit / 32] & (1 << (bit % 32));
}

// Helper: Find the first free block (first bit that is 0)
static int pmm_first_free() {
    for (uint32_t i = 0; i < 32768; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) { // If not all bits are set
            for (int j = 0; j < 32; j++) {
                if (!(pmm_bitmap[i] & (1 << j))) {
                    return i * 32 + j;
                }
            }
        }
    }
    return -1; // Out of memory
}

void pmm_init(uint32_t mem_size) {
    pmm_total_blocks = mem_size / PMM_BLOCK_SIZE;
    pmm_used_blocks = pmm_total_blocks; // Initially mark everything as used/unknown

    // Initialize bitmap to all 1s (used)
    // We will explicitly free the RAM regions reported by Multiboot later.
    memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
}

void* pmm_alloc_block() {
    if (pmm_total_blocks - pmm_used_blocks <= 0) {
        return 0; // Out of memory
    }

    int frame = pmm_first_free();
    if (frame == -1) {
        return 0;
    }

    pmm_set_bit(frame);
    pmm_used_blocks++;

    uint32_t addr = frame * PMM_BLOCK_SIZE;
    return (void*)addr;
}

void pmm_free_block(void* p) {
    uint32_t addr = (uint32_t)p;
    int frame = addr / PMM_BLOCK_SIZE;

    pmm_unset_bit(frame);
    pmm_used_blocks--;
}

// Mark a specific memory region as used (e.g., Kernel code, hardware reserved)
void pmm_mark_region_used(uint32_t base, uint32_t size) {
    int align = base / PMM_BLOCK_SIZE;
    int blocks = size / PMM_BLOCK_SIZE;

    for (; blocks > 0; blocks--) {
        pmm_set_bit(align++);
        pmm_used_blocks++;
    }
}

// Mark a specific memory region as free (e.g., available RAM)
void pmm_mark_region_free(uint32_t base, uint32_t size) {
    int align = base / PMM_BLOCK_SIZE;
    int blocks = size / PMM_BLOCK_SIZE;

    for (; blocks > 0; blocks--) {
        pmm_unset_bit(align++);
        pmm_used_blocks--;
    }
}