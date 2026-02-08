/*
 * kernel.c - Project Falcon Kernel
 * Handles interrupts, basic drivers (VGA, Keyboard), and shell.
 */

#include <stdint.h>
#include "ports.h"
#include "utils.h"
#include "vga.h"
#include "idt.h"
#include "keyboard.h"
#include "pmm.h"

// --- Multiboot Structures ---
typedef struct multiboot_memory_map {
    uint32_t size;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
    uint32_t type;
} multiboot_memory_map_t;

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} multiboot_info_t;

// Global pointer to the Multiboot info structure (initialized in kmain)
multiboot_info_t* global_mboot_info = 0;


// --- Command Shell ---

// Print memory map
void print_mmap() {
    if (global_mboot_info == 0 || !(global_mboot_info->flags & (1 << 6))) {
        print_str("Memory map not available.\n");
        return;
    }

    print_str("Physical Memory Map:\n");
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)global_mboot_info->mmap_addr;
    uint32_t mmap_end = global_mboot_info->mmap_addr + global_mboot_info->mmap_length;

    while ((uint32_t)mmap < mmap_end) {
        print_str("Addr: "); print_hex(mmap->addr_low);
        print_str(" Len: "); print_hex(mmap->len_low);
        print_str(" Type: "); print_dec(mmap->type);
        
        if (mmap->type == 1) print_str(" (RAM)");
        else print_str(" (Reserved)");
        
        print_str("\n");
        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(uint32_t));
    }
}

// Shutdown system
void shutdown() {
    print_str("Shutting down...\n");
    outw(0x604, 0x2000);  // QEMU shutdown command
    outw(0xB004, 0x2000); // Bochs shutdown command
    asm volatile("cli; hlt"); // Fallback: Halt CPU if shutdown fails
}

// Process command
void process_command(char* command) {
    if (strcmp(command, "help") == 0) {
        print_str("Project Falcon OS - Command List:\n");
        print_str("  help  - Display this message\n");
        print_str("  clear - Clear the terminal screen\n");
        print_str("  echo [text] - Print back the given text\n");
        print_str("  exit  - Shutdown the system\n");
        print_str("  mmap  - Show memory map\n");
    } else if (strncmp(command, "echo ", 5) == 0) {
        print_str(command + 5);
        print_newline();
    } else if (strcmp(command, "clear") == 0) {
        clear_screen();
    } else if (strcmp(command, "mmap") == 0) {
        print_mmap();
    } else if (strcmp(command, "exit") == 0) {
        shutdown();
    } else if (strcmp(command, "alloc") == 0) {
        void* ptr = pmm_alloc_block();
        if (ptr) {
            print_str("Allocated page at: ");
            print_hex((uint32_t)ptr);
            print_str("\n");
        } else {
            print_str("Out of memory!\n");
        }
    }else if(strcmp(command, "moshi") == 0) {
        print_str("Moshi THE KING! Welcome to Project Falcon OS!\n");
    }
    else if (command[0] != '\0') {
        print_str("Unknown command: '");
        print_str(command);
        print_str("\n");
    }
    print_str("> ");
}


// --- Kernel Main ---

// Kernel entry point
void kmain(uint32_t magic, multiboot_info_t* mboot_ptr) {
    init_idt();
    init_pic();
    init_interrupts();

    global_mboot_info = mboot_ptr;
    asm volatile("sti");
    clear_screen();

    print_str("Welcome to Project Falcon OS!\n");
    
    // Check Multiboot Magic Number
    if (magic != 0x2BADB002) {
        print_str("WARNING: Invalid Multiboot Magic Number!\n");
    } else {
        print_str("Multiboot Info Detected.\n");
        if (mboot_ptr->flags & 1) {
            print_str("Memory: ");
            print_dec(mboot_ptr->mem_lower + mboot_ptr->mem_upper);
            print_str(" KB\n");

            // Initialize PMM
            uint32_t mem_size = (mboot_ptr->mem_lower + mboot_ptr->mem_upper) * 1024;
            pmm_init(mem_size);

            // Parse memory map to mark free regions
            multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mboot_ptr->mmap_addr;
            uint32_t mmap_end = mboot_ptr->mmap_addr + mboot_ptr->mmap_length;

            while ((uint32_t)mmap < mmap_end) {
                if (mmap->type == 1) { // 1 = Available RAM
                    pmm_mark_region_free(mmap->addr_low, mmap->len_low);
                }
                mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(uint32_t));
            }

            // Reserve the first 2MB (Includes Kernel code, VGA buffer, BIOS data)
            // This prevents the PMM from allocating memory over our own kernel code.
            pmm_mark_region_used(0, 0x200000);

            print_str("PMM Initialized.\n");
        }
    }

    print_str("> ");

    for(;;);
}