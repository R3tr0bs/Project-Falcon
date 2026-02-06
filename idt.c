#include "idt.h"
#include "ports.h"
#include "vga.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

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

extern void timer_wrapper(void);
extern void keyboard_wrapper(void);
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

void init_idt() {
    idt_ptr.limit = (sizeof(struct idt_entry_t) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt;
    asm volatile("lidt %0" : : "m" (idt_ptr));
}

void init_pic() {
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

void init_interrupts() {
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

    idt_set_gate(32, (uint32_t)timer_wrapper, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)keyboard_wrapper, 0x08, 0x8E);
}

void fault_handler(registers_t* regs) {
    if (regs->int_no < 32) {
        print_str("CPU Exception - System Halted.\n");
        asm volatile("cli; hlt");
    }
}

void timer_handler() {
    static uint32_t tick = 0;
    tick++;

    if ((tick % 18) < 9) {
        vga_buffer[VGA_WIDTH - 1] = make_vgaentry(3, make_color(12, 1));
    } else {
        vga_buffer[VGA_WIDTH - 1] = make_vgaentry(' ', make_color(0, 1));
    }

    outb(PIC1_COMMAND, 0x20);
}