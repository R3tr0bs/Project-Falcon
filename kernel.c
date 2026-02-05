/**
 * @file kernel.c
 * @brief Project Falcon's Simple Kernel
 *
 * This file contains the C part of the kernel, including the entry point `kmain`.
 * It handles interrupt setup, basic hardware drivers (VGA, keyboard), and a
 * simple command-line shell.
 *
 * Architecture:
 *  - kmain: Initializes all subsystems and enters an infinite loop.
 *  - Interrupts: CPU exceptions and hardware IRQs are handled.
 *    - The PIC is remapped to avoid conflicts with CPU exceptions.
 *    - The IDT is populated with ISRs (Interrupt Service Routines).
 *    - C handlers are implemented for CPU faults, the timer, and the keyboard.
 *  - Drivers:
 *    - VGA: Simple, direct-write text-mode output.
 *    - Keyboard: Reads scancodes, translates them, and buffers them for the shell.
 *  - Shell: A simple command interpreter for basic OS interaction.
 */

#include <stdint.h>

//==============================================================================
// SECTION: CONSTANTS AND GLOBALS
//==============================================================================

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

// --- CPU State (for fault handling) ---
// This struct maps to the stack layout created by 'isr_common_stub' in boot.s.
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


//==============================================================================
// SECTION: ASSEMBLY FUNCTION PROTOTYPES
//==============================================================================
// These wrappers are defined in boot.s. They form the bridge between
// a hardware/CPU interrupt and its C handler.

extern void timer_wrapper(void);
extern void keyboard_wrapper(void);

// CPU Exception ISRs
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();


//==============================================================================
// SECTION: FORWARD DECLARATIONS OF C FUNCTIONS
//==============================================================================
void process_command(char* command);
void print_newline();


//==============================================================================
// SECTION: UTILITY FUNCTIONS
//==============================================================================

/**
 * @brief Compares two null-terminated strings.
 * @return 0 if strings are identical, non-zero otherwise.
 */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/**
 * @brief Compares the first n bytes of two strings.
 * @return 0 if strings are identical up to n chars, non-zero otherwise.
 */
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


//==============================================================================
// SECTION: LOW-LEVEL I/O
//==============================================================================

/**
 * @brief Writes a byte to the specified hardware port.
 */
void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

/**
 * @brief Reads a byte from the specified hardware port.
 * @return The byte value read from the port.
 */
uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}


//==============================================================================
// SECTION: VGA TEXT MODE DRIVER
//==============================================================================

/**
 * @brief Creates a VGA color attribute byte.
 */
uint8_t make_color(uint8_t fg, uint8_t bg) {
    return (bg << 4) | fg;
}

/**
 * @brief Creates a 16-bit VGA buffer entry.
 */
uint16_t make_vgaentry(char c, uint8_t color) {
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | (color16 << 8);
}

/**
 * @brief Advances the cursor to the next line.
 */
void print_newline() {
    cursor_x = 0;
    cursor_y++;
    // TODO: Add scrolling when cursor_y >= VGA_HEIGHT
}

/**
 * @brief Prints a null-terminated string to the screen at the current cursor position.
 */
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

/**
 * @brief Clears the entire screen and resets the cursor to the top-left.
 */
void clear_screen() {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = make_vgaentry(' ', make_color(15, 4));
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}


//==============================================================================
// SECTION: INTERRUPT & PIC HANDLING
//==============================================================================

/**
 * @brief Sets up a gate (entry) in the Interrupt Descriptor Table (IDT).
 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

/**
 * @brief Initializes the Interrupt Descriptor Table (IDT).
 */
void init_idt() {
    idt_ptr.limit = (sizeof(struct idt_entry_t) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt;
    asm volatile("lidt %0" : : "m" (idt_ptr));
}

/**
 * @brief Initializes the Programmable Interrupt Controller (PIC).
 */
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

/**
 * @brief Populates the IDT with handlers for CPU exceptions and hardware IRQs.
 */
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

/**
 * @brief Generic C-level handler for CPU exceptions.
 */
void fault_handler(registers_t* regs) {
    if (regs->int_no < 32) {
        print_str("CPU Exception - System Halted.\n");
        asm volatile("cli; hlt");
    }
}

/**
 * @brief C-level handler for the timer interrupt (IRQ 0).
 */
void timer_handler() {
    static uint32_t tick = 0;
    tick++;

    // Visual "heartbeat" in the top-left corner
    if ((tick % 18) < 9) {
        vga_buffer[0] = make_vgaentry(3, make_color(12, 1)); // Red heart
    } else {
        vga_buffer[0] = make_vgaentry(' ', make_color(0, 1));
    }

    // CRITICAL: Send End-of-Interrupt (EOI) to the Master PIC.
    outb(PIC1_COMMAND, 0x20);
}


//==============================================================================
// SECTION: KEYBOARD DRIVER
//==============================================================================

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

/**
 * @brief C-level handler for the keyboard interrupt (IRQ 1).
 */
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


//==============================================================================
// SECTION: COMMAND SHELL
//==============================================================================

/**
 * @brief Processes a command received from the keyboard handler.
 */
void process_command(char* command) {
    if (strcmp(command, "help") == 0) {
        print_str("Project Falcon OS - Command List:\n");
        print_str("  help  - Display this message\n");
        print_str("  clear - Clear the terminal screen\n");
        print_str("  echo [text] - Print back the given text\n");
    } else if (strncmp(command, "echo ", 5) == 0) {
        print_str(command + 5);
        print_newline();
    } else if (strcmp(command, "clear") == 0) {
        clear_screen();
    } else if (command[0] != '\0') {
        print_str("Unknown command: '");
        print_str(command);
        print_str("\n");
    }
    print_str("> ");
}


//==============================================================================
// SECTION: KERNEL MAIN
//==============================================================================

/**
 * @brief The entry point for the C part of the kernel.
 */
void kmain(void) {
    init_idt();
    init_pic();
    init_interrupts();

    asm volatile("sti");

    print_str("Welcome to Project Falcon OS!\n");
    print_str("> ");

    for(;;);
}