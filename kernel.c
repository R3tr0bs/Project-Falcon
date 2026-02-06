/*
 * kernel.c - Project Falcon Kernel
 * Handles interrupts, basic drivers (VGA, Keyboard), and shell.
 */

#include <stdint.h>

// --- Constants and Globals ---

// --- Hardware I/O Ports ---
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define KBD_DATA_PORT 0x60

// --- Kernel Configuration ---
#define CMD_BUFFER_SIZE 256
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// --- Global Variables ---
// VGA buffer starts at this memory address in text mode.
volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;
// Current cursor position.
int cursor_x = 0;
int cursor_y = 0;
// Buffer to hold command line input.
char cmd_buffer[CMD_BUFFER_SIZE];
int cmd_buffer_idx = 0;

// --- CPU State ---
typedef struct {
    uint32_t ds;                                     // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pushad
    uint32_t int_no;                                 // Interrupt number
    uint32_t err_code;                               // Error code (or dummy value)
    uint32_t eip, cs, eflags, useresp, ss;           // Pushed by the processor automatically
} registers_t;

// --- IDT Structures ---
struct idt_entry_t {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr_t {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_entry_t idt[256];
struct idt_ptr_t idt_ptr;

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

// --- Assembly Function Prototypes ---

extern void timer_wrapper(void);
extern void keyboard_wrapper(void);

// CPU Exception ISRs
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();


// --- Forward Declarations ---
void process_command(char* command);
void print_newline();


// --- Utility Functions ---

// Compares two strings
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Compares first n bytes of two strings
int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) {
        --n;
        s1++;
        s2++;
    }
    if (n == 0) {
        return 0;
    } else {
        return *(const unsigned char*)s1 - *(const unsigned char*)s2;
    }
}

// Copies n bytes from src to dest
void* memcpy(void* dest, const void* src, int n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

// Fills first n bytes of s with c
void* memset(void* s, int c, int n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void print_dec(uint32_t n);
void print_hex(uint32_t n);

// --- Low-Level I/O ---

// Write byte to port
void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// Read byte from port
uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Write word to port
void outw(uint16_t port, uint16_t val) {
    asm volatile ( "outw %0, %1" : : "a"(val), "Nd"(port) );
}


// --- VGA Text Mode Driver ---

// Create color attribute
uint8_t make_color(uint8_t fg, uint8_t bg) {
    return (bg << 4) | fg;
}

// Create VGA entry
uint16_t make_vgaentry(char c, uint8_t color) {
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | (color16 << 8);
}

// Scroll screen up
void terminal_scroll() {
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_vgaentry(' ', make_color(15, 4));
    }
}

// Advance cursor to next line
void print_newline() {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= VGA_HEIGHT) {
        terminal_scroll();
        cursor_y = VGA_HEIGHT - 1;
    }
}

// Print string
void print_str(const char* str) {
    for(int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            print_newline();
        } else {
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_vgaentry(str[i], make_color(15, 4));
            cursor_x++;
            if (cursor_x >= VGA_WIDTH) {
                print_newline();
            }
        }
    }
}

// Clear screen
void clear_screen() {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = make_vgaentry(' ', make_color(15, 4));
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

// Print decimal number
void print_dec(uint32_t n) {
    if (n == 0) {
        print_str("0");
        return;
    }
    char buf[32];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    // Reverse buffer
    for (int j = 0; j < i / 2; j++) {
        char temp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = temp;
    }
    buf[i] = '\0';
    print_str(buf);
}

// Print hex number
void print_hex(uint32_t n) {
    print_str("0x");
    char hex_chars[] = "0123456789ABCDEF";
    char buf[9];
    buf[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        buf[i] = hex_chars[n & 0xF];
        n >>= 4;
    }
    // Skip leading zeros (optional, but looks nicer)
    char* p = buf;
    while (*p == '0' && *(p+1) != '\0') p++;
    print_str(p);
}

// --- Interrupt & PIC Handling ---

// Set IDT gate
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

// Initialize IDT
void init_idt() {
    idt_ptr.limit = (sizeof(struct idt_entry_t) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt;
    asm volatile("lidt %0" : : "m" (idt_ptr));
}

// Initialize PIC
void init_pic() {
    // Start initialization sequence
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    // Remap offsets: Master PIC to 32 (0x20), Slave PIC to 40 (0x28)
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    // Setup cascading
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    // Set 8086 mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // Unmask interrupts: Enable IRQ0 (Timer) and IRQ1 (Keyboard)
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

// Populate IDT
void init_interrupts() {
    // CPU Exceptions
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);

    // Hardware (PIC) Interrupts
    idt_set_gate(32, (uint32_t)timer_wrapper, 0x08, 0x8E);   // IRQ 0: Timer
    idt_set_gate(33, (uint32_t)keyboard_wrapper, 0x08, 0x8E); // IRQ 1: Keyboard
}

// Generic CPU exception handler
void fault_handler(registers_t* regs) {
    if (regs->int_no < 32) {
        print_str("CPU Exception - System Halted.\n");
        asm volatile("cli; hlt");
    }
}

// Timer interrupt handler (IRQ 0)
void timer_handler() {
    static uint32_t tick = 0;
    tick++;

    // Visual "heartbeat" in the top-left corner
    if ((tick % 18) < 9) {
        vga_buffer[VGA_WIDTH - 1] = make_vgaentry(3, make_color(12, 1)); // Red heart
    } else {
        vga_buffer[VGA_WIDTH - 1] = make_vgaentry(' ', make_color(0, 1));
    }

    // CRITICAL: Send End-of-Interrupt (EOI) to the Master PIC.
    outb(PIC1_COMMAND, 0x20);
}

// --- Keyboard Driver ---

// Scancode to ASCII map for a standard US keyboard layout.
unsigned char kbdus[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// Keyboard interrupt handler (IRQ 1)
void keyboard_handler() {
    uint8_t scancode = inb(KBD_DATA_PORT);

    // We only handle key presses (scancode bit 7 is 0).
    if (!(scancode & 0x80)) {
        char c = kbdus[scancode];

        if (c == '\n') {
            print_newline();
            cmd_buffer[cmd_buffer_idx] = '\0';
            process_command(cmd_buffer);
            cmd_buffer_idx = 0;
        } else if (c == '\b') {
            if (cmd_buffer_idx > 0) {
                cmd_buffer_idx--;
                if (cursor_x > 0) {
                    cursor_x--;
                    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_vgaentry(' ', make_color(15, 4));
                }
            }
        } else if (c && cmd_buffer_idx < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_buffer_idx++] = c;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x++] = make_vgaentry(c, make_color(15, 4));
        }

        if (cursor_x >= VGA_WIDTH) {
            print_newline();
        }
    }

    outb(PIC1_COMMAND, 0x20);
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
    } else if (command[0] != '\0') {
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
        }
    }

    print_str("> ");

    for(;;);
}