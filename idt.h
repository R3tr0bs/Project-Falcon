#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no;
    uint32_t err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

void init_idt();
void init_pic();
void init_interrupts();

extern volatile uint32_t timer_ticks;

// Handlers called from Assembly
void fault_handler(registers_t* regs);
void timer_handler();

#endif