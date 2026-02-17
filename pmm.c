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
    if (bit >= pmm_total_blocks) {
        return 1;
    }
    return pmm_bitmap[bit / 32] & (1 << (bit % 32));
}

static int pmm_first_free_sized(uint32_t count) {
    if (count == 0 || count > pmm_total_blocks) {
        return -1;
    }
    uint32_t limit = pmm_total_blocks - count;
    for (uint32_t i = 0; i <= limit; i++) {
        if (pmm_test_bit(i)) {
            continue;
        }
        uint32_t j = 0;
        for (; j < count; j++) {
            if (pmm_test_bit(i + j)) {
                break;
            }
        }
        if (j == count) {
            return (int)i;
        }
        i += j;
    }
    return -1;
}

void pmm_init(uint32_t mem_size) {
    pmm_total_blocks = mem_size / PMM_BLOCK_SIZE;
    pmm_used_blocks = pmm_total_blocks; // Initially mark everything as used/unknown

    // Initialize bitmap to all 1s (used)
    // We will explicitly free the RAM regions reported by Multiboot later.
    memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
}

void* pmm_alloc_block() {
    return pmm_alloc_blocks(1);
}

void* pmm_alloc_blocks(uint32_t count) {
    if (count == 0 || pmm_get_free_blocks() < count) {
        return 0;
    }

    int frame = pmm_first_free_sized(count);
    if (frame == -1) {
        return 0;
    }

    for (uint32_t i = 0; i < count; i++) {
        pmm_set_bit((uint32_t)frame + i);
        pmm_used_blocks++;
    }

    uint32_t addr = (uint32_t)frame * PMM_BLOCK_SIZE;
    return (void*)addr;
}

void pmm_free_block(void* p) {
    uint32_t addr = (uint32_t)p;
    uint32_t frame = addr / PMM_BLOCK_SIZE;

    if (frame >= pmm_total_blocks) {
        return;
    }
    if (pmm_test_bit(frame)) {
        pmm_unset_bit(frame);
        pmm_used_blocks--;
    }
}

void pmm_free_blocks(void* p, uint32_t count) {
    if (count == 0) {
        return;
    }
    uint32_t addr = (uint32_t)p;
    uint32_t frame = addr / PMM_BLOCK_SIZE;
    if (frame >= pmm_total_blocks) {
        return;
    }
    uint32_t end = frame + count;
    if (end > pmm_total_blocks) {
        end = pmm_total_blocks;
    }
    for (uint32_t i = frame; i < end; i++) {
        if (pmm_test_bit(i)) {
            pmm_unset_bit(i);
            pmm_used_blocks--;
        }
    }
}

// Mark a specific memory region as used (e.g., Kernel code, hardware reserved)
void pmm_mark_region_used(uint32_t base, uint32_t size) {
    if (size == 0 || pmm_total_blocks == 0) {
        return;
    }
    uint32_t start = base / PMM_BLOCK_SIZE;
    uint32_t end = (base + size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;
    if (start >= pmm_total_blocks) {
        return;
    }
    if (end > pmm_total_blocks) {
        end = pmm_total_blocks;
    }
    for (uint32_t i = start; i < end; i++) {
        if (!pmm_test_bit(i)) {
            pmm_set_bit(i);
            pmm_used_blocks++;
        }
    }
}

// Mark a specific memory region as free (e.g., available RAM)
void pmm_mark_region_free(uint32_t base, uint32_t size) {
    if (size == 0 || pmm_total_blocks == 0) {
        return;
    }
    uint32_t start = base / PMM_BLOCK_SIZE;
    uint32_t end = (base + size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;
    if (start >= pmm_total_blocks) {
        return;
    }
    if (end > pmm_total_blocks) {
        end = pmm_total_blocks;
    }
    for (uint32_t i = start; i < end; i++) {
        if (pmm_test_bit(i)) {
            pmm_unset_bit(i);
            pmm_used_blocks--;
        }
    }
}

uint32_t pmm_get_total_blocks() {
    return pmm_total_blocks;
}

uint32_t pmm_get_used_blocks() {
    return pmm_used_blocks;
}

uint32_t pmm_get_free_blocks() {
    if (pmm_used_blocks > pmm_total_blocks) {
        return 0;
    }
    return pmm_total_blocks - pmm_used_blocks;
}
