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

/* --- CPU State Structure --- */
/* This struct maps exactly to the stack layout created by 'isr_common_stub' */
typedef struct {
    uint32_t ds;                                     // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pushad
    uint32_t int_no;                                 // Interrupt number
    uint32_t err_code;                               // Error code (or dummy)
    uint32_t eip, cs, eflags, useresp, ss;           // Pushed by the processor automatically
} registers_t;


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

/* --- The "Blue Screen" Handler --- */
void fault_handler(registers_t regs) {
    // If interrupt is less than 32, it is a CPU Exception (Crash)
    if (regs.int_no < 32) {
        
        // 1. Clear screen or change background color to Red (Panic)
        // (Assuming you have a clear_screen function, otherwise just overwrite vga)
        volatile uint16_t* vga = (uint16_t*)0xB8000;
        for (int i = 0; i < 80*25; i++) {
             vga[i] = make_vgaentry(' ', make_color(15, 4)); // White on Red
        }

        // 2. Display Error Message (Basic implementation)
        const char* msg = "KERNEL PANIC: CPU EXCEPTION DETECTED!";
        for(int i=0; msg[i] != 0; i++) {
            vga[i] = make_vgaentry(msg[i], make_color(15, 4));
        }

        // TODO: Print the specific Interrupt Number (regs.int_no) to know WHAT happened
        // TODO: Print the EIP (regs.eip) to know WHERE it happened

        // 3. Halt the system completely to prevent further damage
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
    // Clear screen
    for (int i = 0; i < 80*25; i++) {
        vga_buffer[i] = make_vgaentry(' ', make_color(15, 1));
    }

    // Title
    vga_buffer[38] = make_vgaentry('F', make_color(10, 1));
    vga_buffer[39] = make_vgaentry('A', make_color(10, 1));
    vga_buffer[40] = make_vgaentry('L', make_color(10, 1));
    vga_buffer[41] = make_vgaentry('C', make_color(10, 1));
    vga_buffer[42] = make_vgaentry('O', make_color(10, 1));
    vga_buffer[43] = make_vgaentry('N', make_color(10, 1));

    // Setup Nervous System
    init_idt();
    init_interrupts();
    // Link the Assembly wrapper to Interrupt 32 (Timer)
    extern void timer_wrapper();
    idt_set_gate(32, (uint32_t)timer_wrapper, 0x08, 0x8E);

    // Initialize Hardware
    init_timer();

    // ENABLE INTERRUPTS (The moment of truth)
    // 'sti' = Set Interrupt Flag
    asm volatile("sti");
    asm volatile("int $0x1");
    // Infinite loop - The CPU will now jump to timer_handler automatically
    while(1);
}