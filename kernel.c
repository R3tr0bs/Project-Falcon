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

#define MAX_FILES 16
#define MAX_FILENAME 24

typedef struct {
    int used;
    char name[MAX_FILENAME];
    uint32_t size;
    uint32_t capacity;
    void* data;
} ram_file_t;

static ram_file_t ram_files[MAX_FILES];

static char* skip_spaces(char* s) {
    while (s && (*s == ' ' || *s == '\t')) {
        s++;
    }
    return s;
}

static char* next_token(char** s) {
    char* p = skip_spaces(*s);
    if (!p || *p == '\0') {
        *s = p;
        return 0;
    }
    char* start = p;
    while (*p && *p != ' ' && *p != '\t') {
        p++;
    }
    if (*p) {
        *p = '\0';
        p++;
    }
    *s = p;
    return start;
}

static int parse_u32(const char* s, uint32_t* out) {
    if (!s || !*s) {
        return 0;
    }
    uint32_t base = 10;
    uint32_t value = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (!*s) {
        return 0;
    }
    while (*s) {
        char c = *s;
        uint32_t digit;
        if (c >= '0' && c <= '9') {
            digit = (uint32_t)(c - '0');
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = (uint32_t)(c - 'a' + 10);
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = (uint32_t)(c - 'A' + 10);
        } else {
            return 0;
        }
        value = value * base + digit;
        s++;
    }
    *out = value;
    return 1;
}

static void copy_name(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int find_file_index(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (ram_files[i].used && strcmp(ram_files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_free_file_slot() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!ram_files[i].used) {
            return i;
        }
    }
    return -1;
}

static void list_files() {
    int any = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (ram_files[i].used) {
            print_str(ram_files[i].name);
            print_str(" ");
            print_dec(ram_files[i].size);
            print_str(" bytes\n");
            any = 1;
        }
    }
    if (!any) {
        print_str("No files.\n");
    }
}

static void file_stat(const char* name) {
    int idx = find_file_index(name);
    if (idx < 0) {
        print_str("File not found.\n");
        return;
    }
    ram_file_t* f = &ram_files[idx];
    print_str("Name: ");
    print_str(f->name);
    print_str(" Size: ");
    print_dec(f->size);
    print_str(" Capacity: ");
    print_dec(f->capacity);
    print_str("\n");
}

static void file_touch(const char* name) {
    if (!name || !*name) {
        print_str("Missing filename.\n");
        return;
    }
    if (find_file_index(name) >= 0) {
        print_str("File already exists.\n");
        return;
    }
    int slot = find_free_file_slot();
    if (slot < 0) {
        print_str("File table full.\n");
        return;
    }
    ram_files[slot].used = 1;
    copy_name(ram_files[slot].name, name, MAX_FILENAME);
    ram_files[slot].size = 0;
    ram_files[slot].capacity = 0;
    ram_files[slot].data = 0;
    print_str("File created.\n");
}

static void file_rm(const char* name) {
    int idx = find_file_index(name);
    if (idx < 0) {
        print_str("File not found.\n");
        return;
    }
    ram_file_t* f = &ram_files[idx];
    if (f->data && f->capacity) {
        pmm_free_blocks(f->data, f->capacity / PMM_BLOCK_SIZE);
    }
    f->used = 0;
    f->name[0] = '\0';
    f->size = 0;
    f->capacity = 0;
    f->data = 0;
    print_str("File removed.\n");
}

static void file_cat(const char* name) {
    int idx = find_file_index(name);
    if (idx < 0) {
        print_str("File not found.\n");
        return;
    }
    ram_file_t* f = &ram_files[idx];
    if (!f->data || f->size == 0) {
        print_str("\n");
        return;
    }
    print_str((const char*)f->data);
    print_str("\n");
}

static void file_write(const char* name, const char* content) {
    if (!name || !*name) {
        print_str("Missing filename.\n");
        return;
    }
    int idx = find_file_index(name);
    if (idx < 0) {
        int slot = find_free_file_slot();
        if (slot < 0) {
            print_str("File table full.\n");
            return;
        }
        idx = slot;
        ram_files[idx].used = 1;
        copy_name(ram_files[idx].name, name, MAX_FILENAME);
        ram_files[idx].size = 0;
        ram_files[idx].capacity = 0;
        ram_files[idx].data = 0;
    }
    ram_file_t* f = &ram_files[idx];
    uint32_t size = content ? (uint32_t)strlen(content) : 0;
    uint32_t bytes_needed = size + 1;
    uint32_t blocks = (bytes_needed + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;
    void* new_data = 0;
    if (blocks > 0) {
        new_data = pmm_alloc_blocks(blocks);
        if (!new_data) {
            print_str("Out of memory.\n");
            return;
        }
    }
    if (f->data && f->capacity) {
        pmm_free_blocks(f->data, f->capacity / PMM_BLOCK_SIZE);
    }
    f->data = new_data;
    f->capacity = blocks * PMM_BLOCK_SIZE;
    f->size = size;
    if (f->data) {
        memcpy(f->data, content, (int)size);
        ((char*)f->data)[size] = '\0';
    }
    print_str("File written.\n");
}

static void print_mmap_type(uint32_t type) {
    if (type == 1) {
        print_str("RAM");
    } else if (type == 2) {
        print_str("Reserved");
    } else if (type == 3) {
        print_str("ACPI");
    } else if (type == 4) {
        print_str("NVS");
    } else if (type == 5) {
        print_str("Bad");
    } else {
        print_str("Unknown");
    }
}


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
    uint32_t entry = 0;
    uint32_t avail_kb = 0;
    uint32_t reserved_kb = 0;

    while ((uint32_t)mmap < mmap_end) {
        print_str("#"); print_dec(entry);
        print_str(" Addr: ");
        if (mmap->addr_high) {
            print_hex(mmap->addr_high);
            print_str(":");
        }
        print_hex(mmap->addr_low);
        print_str(" Len: ");
        if (mmap->len_high) {
            print_hex(mmap->len_high);
            print_str(":");
        }
        print_hex(mmap->len_low);
        print_str(" Type: ");
        print_dec(mmap->type);
        print_str(" (");
        print_mmap_type(mmap->type);
        print_str(")");
        print_str("\n");
        if (mmap->type == 1) {
            avail_kb += mmap->len_low / 1024;
        } else {
            reserved_kb += mmap->len_low / 1024;
        }
        entry++;
        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(uint32_t));
    }
    print_str("Available KB: ");
    print_dec(avail_kb);
    print_str(" Reserved KB: ");
    print_dec(reserved_kb);
    print_str("\n");
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
    char* cursor = command;
    char* cmd = next_token(&cursor);
    if (!cmd || cmd[0] == '\0') {
        print_str("> ");
        return;
    }
    if (strcmp(cmd, "help") == 0) {
        print_str("Project Falcon OS - Command List:\n");
        print_str("  help  - Display this message\n");
        print_str("  clear - Clear the terminal screen\n");
        print_str("  echo [text] - Print back the given text\n");
        print_str("  exit  - Shutdown the system\n");
        print_str("  mmap  - Show memory map\n");
        print_str("  mem  - Show memory usage\n");
        print_str("  alloc [blocks] - Allocate memory blocks\n");
        print_str("  free <addr> [blocks] - Free memory blocks\n");
        print_str("  files/ls - List files\n");
        print_str("  touch <name> - Create empty file\n");
        print_str("  write <name> <text> - Write text to file\n");
        print_str("  cat <name> - Print file contents\n");
        print_str("  rm <name> - Delete file\n");
        print_str("  stat <name> - Show file details\n");
    } else if (strcmp(cmd, "echo") == 0) {
        char* msg = skip_spaces(cursor);
        if (msg && *msg) {
            print_str(msg);
        }
        print_newline();
    } else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } else if (strcmp(cmd, "mmap") == 0) {
        print_mmap();
    } else if (strcmp(cmd, "mem") == 0) {
        uint32_t total = pmm_get_total_blocks();
        uint32_t used = pmm_get_used_blocks();
        uint32_t free = pmm_get_free_blocks();
        print_str("Total blocks: ");
        print_dec(total);
        print_str(" Used: ");
        print_dec(used);
        print_str(" Free: ");
        print_dec(free);
        print_str("\n");
    } else if (strcmp(cmd, "exit") == 0) {
        shutdown();
    } else if (strcmp(cmd, "alloc") == 0) {
        char* arg = next_token(&cursor);
        uint32_t blocks = 1;
        if (arg && *arg) {
            if (!parse_u32(arg, &blocks) || blocks == 0) {
                print_str("Invalid block count.\n");
                print_str("> ");
                return;
            }
        }
        void* ptr = pmm_alloc_blocks(blocks);
        if (ptr) {
            print_str("Allocated at: ");
            print_hex((uint32_t)ptr);
            print_str(" Blocks: ");
            print_dec(blocks);
            print_str("\n");
        } else {
            print_str("Out of memory!\n");
        }
    } else if (strcmp(cmd, "free") == 0) {
        char* addr_str = next_token(&cursor);
        char* count_str = next_token(&cursor);
        uint32_t addr = 0;
        uint32_t blocks = 1;
        if (!addr_str || !parse_u32(addr_str, &addr)) {
            print_str("Invalid address.\n");
        } else {
            if (count_str && *count_str) {
                if (!parse_u32(count_str, &blocks) || blocks == 0) {
                    print_str("Invalid block count.\n");
                    print_str("> ");
                    return;
                }
            }
            pmm_free_blocks((void*)addr, blocks);
            print_str("Freed.\n");
        }
    } else if (strcmp(cmd, "files") == 0 || strcmp(cmd, "ls") == 0) {
        list_files();
    } else if (strcmp(cmd, "touch") == 0) {
        char* name = next_token(&cursor);
        if (!name) {
            print_str("Missing filename.\n");
        } else {
            file_touch(name);
        }
    } else if (strcmp(cmd, "write") == 0) {
        char* name = next_token(&cursor);
        char* content = skip_spaces(cursor);
        if (!name) {
            print_str("Missing filename.\n");
        } else {
            file_write(name, content);
        }
    } else if (strcmp(cmd, "cat") == 0) {
        char* name = next_token(&cursor);
        if (!name) {
            print_str("Missing filename.\n");
        } else {
            file_cat(name);
        }
    } else if (strcmp(cmd, "rm") == 0) {
        char* name = next_token(&cursor);
        if (!name) {
            print_str("Missing filename.\n");
        } else {
            file_rm(name);
        }
    } else if (strcmp(cmd, "stat") == 0) {
        char* name = next_token(&cursor);
        if (!name) {
            print_str("Missing filename.\n");
        } else {
            file_stat(name);
        }
    } else if(strcmp(cmd, "moshi") == 0) {
        print_str("Moshi THE KING! Welcome to Project Falcon OS!\n");
    } else if (cmd[0] != '\0') {
        print_str("Unknown command: '");
        print_str(cmd);
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
