/* kernel.c - Project Falcon Phase 3: The Heartbeat */
#include <stdint.h>

/* --- Hardware Communication Ports --- */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/* --- VGA Text Mode Definitions --- */
volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;
extern void timer_wrapper(void);
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();

/* --- Helper: Print Hexadecimal Value --- */
/* Prints a 32-bit number in hex format (e.g., 0x0001F4A) at the current cursor location */
int cursor_x = 0;
int cursor_y = 0;

/* --- CPU State Structure --- */
/* This struct maps exactly to the stack layout created by 'isr_common_stub' */
typedef struct {
    uint32_t ds;                                     // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pushad
    uint32_t int_no;                                 // Interrupt number
    uint32_t err_code;                               // Error code (or dummy)
    uint32_t eip, cs, eflags, useresp, ss;           // Pushed by the processor automatically
} registers_t;


typedef struct {
    uint32_t v1;
    uint32_t v2;
    uint32_t v3;
} ProtectedInt;

/* --- IDT Structures (Same as before) --- */
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


uint8_t make_color(uint8_t fg, uint8_t bg) {
    return (bg << 4) | fg;
}

uint16_t make_vgaentry(char c, uint8_t color) {
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | (color16 << 8);
}

void write_safe(ProtectedInt* p, uint32_t value) {
    p->v1 = value;
    p->v2 = value;
    p->v3 = value;
}

uint32_t read_safe(ProtectedInt* p) {
    if (p->v1 == p->v2 && p->v2 == p->v3) return p->v1;
    if (p->v2 == p->v3) { p->v1 = p->v2; return p->v2; }
    if (p->v1 == p->v3) { p->v2 = p->v1; return p->v1; }
    if (p->v1 == p->v2) { p->v3 = p->v1; return p->v1; }
    return p->v1; 
}


void print_hex(uint32_t n) {
    const char *hex_chars = "0123456789ABCDEF";
    
    // Print "0x" prefix
    vga_buffer[cursor_y * 80 + cursor_x++] = make_vgaentry('0', make_color(15, 4));
    vga_buffer[cursor_y * 80 + cursor_x++] = make_vgaentry('x', make_color(15, 4));

    // Loop through 8 nibbles (4 bits each) because 32 bits / 4 = 8 chars
    for (int i = 28; i >= 0; i -= 4) {
        // Extract the nibble
        uint8_t nibble = (n >> i) & 0xF; 
        
        // Print the character
        vga_buffer[cursor_y * 80 + cursor_x++] = make_vgaentry(hex_chars[nibble], make_color(15, 4));
    }
    
    // Add a space after the number
    cursor_x++; 
}

/* Helper to move to next line */
void print_newline() {
    cursor_x = 0;
    cursor_y++;
}

/* Helper to print a string */
void print_str(const char* str) {
    for(int i=0; str[i] != 0; i++) {
        vga_buffer[cursor_y * 80 + cursor_x++] = make_vgaentry(str[i], make_color(15, 4));
    }
}

/* Update signature to take a POINTER */
void fault_handler(registers_t* regs) {
    
    // Tactic 1: Handle Divide by Zero (INT 0)
    if (regs->int_no == 0) {
        // 1. Notify (Optional - can be silent in production)
        print_str("[FALCON] Div-by-Zero detected! Patching...");
        print_newline();

        // 2. THE FIX: Skip the bad instruction.
        // Most 'div' instructions are 2 or 3 bytes long.
        // This is a heuristic. In a real OS, we would decode the instruction.
        // For 'div eax' (F7 F0) or similar, 2 bytes is a safe bet for this demo.
        regs->eip += 2; 

        // 3. THE SANITIZATION:
        // Since the division failed, EAX (the result) contains garbage.
        // Let's force it to 0 so the program logic usually continues safely.
        regs->eax = 0;

        // 4. Return immediately! Do not halt.
        return; 
    }

    // Tactic 2: Handle all other crashes
    if (regs->int_no < 32) {
        // ... (קוד המסך האדום הרגיל שלך כאן) ...
        print_str("FATAL UNRECOVERABLE ERROR");
        asm volatile("cli; hlt");
    }
}
/* --- Low Level I/O Functions --- */

/* Write a byte to a hardware port */
void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

/* Setup a gate in the IDT */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

/* Initialize IDT */
void init_idt() {
    idt_ptr.limit = (sizeof(struct idt_entry_t) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt;
    asm volatile("lidt %0" : : "m" (idt_ptr));
}

/* --- The Heart of Falcon --- */

volatile uint32_t tick = 0;

/* The Timer Handler (Called by hardware ~18 times per second by default) */
void timer_handler() {
    tick++;

    // Visual: A beating heart in the top-left corner
    // On every 18th tick (approx 1 second), flash a heart
    if (tick % 18 < 10) {
        // Draw Heart (ASCII 3) in Red
        vga_buffer[0] = make_vgaentry(3, make_color(4, 1)); 
    } else {
        // Draw empty space
        vga_buffer[0] = make_vgaentry(' ', make_color(1, 1));
    }

    // CRITICAL: Send End-of-Interrupt (EOI) to the Master PIC
    // If we don't do this, the PIC will never send another interrupt.
    outb(PIC1_COMMAND, 0x20);
}

/* Remap the PIC to avoid conflicts with CPU exceptions */
void init_timer() {
    // 1. Start initialization sequence (ICW1)
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    // 2. Remap offsets (ICW2)
    // Map Master PIC to interrupt 32 (0x20)
    outb(PIC1_DATA, 0x20); 
    // Map Slave PIC to interrupt 40 (0x28)
    outb(PIC2_DATA, 0x28); 

    // 3. Setup cascading (ICW3) - tell them how they are connected
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    // 4. Environment info (ICW4) - 8086 mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // 5. Unmask interrupts (Allow Timer)
    // 0xFE = 11111110 in binary. The zero means "Enable IRQ0 (Timer)"
    outb(PIC1_DATA, 0xFE);
    outb(PIC2_DATA, 0xFF);
}


void init_interrupts()
{
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
}


/* --- Main --- */
void kmain(void) {
    // ... init code ...
    init_idt();
    init_interrupts();
    asm volatile("sti");

    /* --- SCENARIO: THE INDESTRUCTIBLE SYSTEM --- */

    // 1. Setup Critical Data (TMR)
    ProtectedInt fuel_level;
    write_safe(&fuel_level, 100); // 100% Fuel

    print_str("System check... Fuel at 100%");
    print_newline();

    // 2. ATTACK 1: Memory Corruption
    // A cosmic ray hits memory!
    fuel_level.v2 = 99999; 
    
    // Validate TMR works
    uint32_t current_fuel = read_safe(&fuel_level);
    if (current_fuel == 100) {
        print_str("Memory Corruption Detected & Repaired automatically.");
        print_newline();
    } else {
        print_str("Memory Repair Failed!"); // Should not happen
    }

    // 3. ATTACK 2: Logic Crash (Divide by Zero)
    print_str("Attempting illegal calculation...");
    print_newline();
    
    int a = 10;
    int b = 0;
    int result;
    
    // This generates a 'div' instruction that normally kills the PC
    // But our new handler should catch it, print a message, and set result to 0.
    asm volatile (
        "div %2"
        : "=a"(result) 
        : "a"(a), "r"(b) // EAX=10, divisor=0
    );

    // If we get here, we survived the crash!
    print_str("I AM STILL ALIVE!");
    print_newline();
    print_str("Result fixed to: ");
    print_hex(result); // Should be 0 (our manual fix)

    while(1);
}