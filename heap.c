#include "heap.h"
#include "utils.h"

#define HEAP_MAGIC 0x12345678
#define HEAP_ALIGN 8

typedef struct heap_block {
    uint32_t magic;
    uint32_t size;
    struct heap_block* next;
    struct heap_block* prev;
    uint8_t is_free;
} heap_block_t;

static heap_block_t* heap_start = NULL;
static heap_block_t* heap_end = NULL;
static uint32_t heap_size = 0;
static uint32_t heap_max_size = 0;

void heap_init(uint32_t start_addr, uint32_t end_addr) {
    start_addr = (start_addr + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
    
    heap_start = (heap_block_t*)start_addr;
    heap_end = (heap_block_t*)end_addr;
    heap_size = sizeof(heap_block_t);
    heap_max_size = end_addr - start_addr;
    
    heap_start->magic = HEAP_MAGIC;
    heap_start->size = heap_max_size - sizeof(heap_block_t);
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_start->is_free = 1;
}

static void split_block(heap_block_t* block, uint32_t size) {
    if (block->size <= size + sizeof(heap_block_t) + HEAP_ALIGN) return;
    
    heap_block_t* new_block = (heap_block_t*)((uint8_t*)block + sizeof(heap_block_t) + size);
    new_block->magic = HEAP_MAGIC;
    new_block->size = block->size - size - sizeof(heap_block_t);
    new_block->next = block->next;
    new_block->prev = block;
    new_block->is_free = 1;
    
    if (block->next) {
        block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = size;
}

static heap_block_t* find_free_block(uint32_t size) {
    heap_block_t* current = heap_start;
    while (current) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    size = (size + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
    
    heap_block_t* block = find_free_block(size);
    if (!block) return NULL;
    
    split_block(block, size);
    block->is_free = 0;
    
    return (uint8_t*)block + sizeof(heap_block_t);
}

void* kmalloc_aligned(size_t size, uint32_t alignment) {
    if (alignment <= HEAP_ALIGN) return kmalloc(size);
    
    uint32_t aligned_size = size + alignment;
    void* ptr = kmalloc(aligned_size);
    if (!ptr) return NULL;
    
    uint32_t addr = (uint32_t)ptr;
    uint32_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    
    if (addr == aligned_addr) return ptr;
    
    uint32_t offset = aligned_addr - addr;
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->size = offset - sizeof(heap_block_t);
    
    void* aligned_ptr = (void*)aligned_addr;
    heap_block_t* aligned_block = (heap_block_t*)((uint8_t*)aligned_ptr - sizeof(heap_block_t));
    aligned_block->magic = HEAP_MAGIC;
    aligned_block->size = size;
    aligned_block->next = block->next;
    aligned_block->prev = block;
    aligned_block->is_free = 0;
    
    if (block->next) {
        block->next->prev = aligned_block;
    }
    block->next = aligned_block;
    
    return aligned_ptr;
}

void* kmalloc_physical(size_t size, uint32_t* phys) {
    void* ptr = kmalloc_aligned(size, 0x1000);
    if (!ptr) return NULL;
    
    if (phys) {
        *phys = (uint32_t)ptr;
    }
    
    return ptr;
}

static void merge_blocks(heap_block_t* block) {
    if (block->next && block->next->is_free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    
    if (block->magic != HEAP_MAGIC) return;
    
    block->is_free = 1;
    merge_blocks(block);
    if (block->prev && block->prev->is_free) {
        merge_blocks(block->prev);
    }
}