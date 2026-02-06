;
; boot.s - Project Falcon Bootloader

; --- Constants for Multiboot Header ---
MBALIGN  equ  1 << 0  ; Align loaded modules on page boundaries
MEMINFO  equ  1 << 1  ; Provide memory map
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002 ; Multiboot 1 magic number
CHECKSUM equ -(MAGIC + FLAGS) ; Checksum to make the header valid

; --- Multiboot Header ---
section .multiboot
align 4
    dd MAGIC      ; Magic number
    dd FLAGS      ; Flags
    dd CHECKSUM   ; Checksum

; --- BSS (Stack) ---
section .bss
align 16
stack_bottom:
    resb 16384 ; Reserve 16 KiB for the stack
stack_top:
    ; The stack grows downwards, so stack_top is the starting address for ESP.

; --- Text (Code) ---
section .text
global _start:function (_start.end - _start)

; --- Declare external C functions that we will call from assembly ---
extern kmain            ; The main C kernel entry point
extern fault_handler    ; The C handler for all CPU exceptions
extern timer_handler    ; The C handler for the timer (IRQ 0)
extern keyboard_handler ; The C handler for the keyboard (IRQ 1)

; --- Make assembly functions visible to the C code ---
global timer_wrapper
global keyboard_wrapper

; --- ISR Wrappers ---

%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli             ; Disable interrupts
    push 0          ; Push a dummy error code
    push %1         ; Push the interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli             ; Disable interrupts
    ; The hardware already pushed an error code
    push %1         ; Push the interrupt number
    jmp isr_common_stub
%endmacro

; Define the first 16 CPU exception handlers using the macros.
ISR_NOERRCODE 0   ; 0: Division by zero
ISR_NOERRCODE 1   ; 1: Debug
ISR_NOERRCODE 2   ; 2: Non-maskable interrupt
ISR_NOERRCODE 3   ; 3: Breakpoint
ISR_NOERRCODE 4   ; 4: Overflow
ISR_NOERRCODE 5   ; 5: Bound Range Exceeded
ISR_NOERRCODE 6   ; 6: Invalid Opcode
ISR_NOERRCODE 7   ; 7: Device Not Available
ISR_ERRCODE   8   ; 8: Double Fault
ISR_NOERRCODE 9   ; 9: Coprocessor Segment Overrun
ISR_ERRCODE   10  ; 10: Invalid TSS
ISR_ERRCODE   11  ; 11: Segment Not Present
ISR_ERRCODE   12  ; 12: Stack-Segment Fault
ISR_ERRCODE   13  ; 13: General Protection Fault (GPF)
ISR_ERRCODE   14  ; 14: Page Fault
ISR_NOERRCODE 15  ; 15: Reserved by Intel

; --- Common ISR Stub ---
isr_common_stub:
    pushad          ; Save all general-purpose registers (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP)

    mov ax, ds      ; Save the current data segment
    push eax

    mov ax, 0x10    ; Load the kernel data segment selector (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp           ; Pass a POINTER to the registers_t struct as an argument to the C handler
    call fault_handler ; Call the generic C fault handler
    add esp, 4         ; Clean up the pointer argument from the stack

    pop eax         ; Restore the original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popad           ; Restore all general-purpose registers
    add esp, 8      ; Clean up the error code and ISR number from the stack
    iretd           ; Return from interrupt (restores EIP, CS, EFLAGS)

; --- Hardware Interrupt Wrappers ---

; Wrapper for IRQ 0 (Timer)
timer_wrapper:
    pushad             ; Save registers
    cld                ; Clear direction flag (standard practice)
    call timer_handler ; Call the C handler
    popad              ; Restore registers
    iretd              ; Return from interrupt

; Wrapper for IRQ 1 (Keyboard)
keyboard_wrapper:
    pushad             ; Save registers
    cld                ; Clear direction flag
    call keyboard_handler ; Call the C handler
    popad              ; Restore registers
    iretd              ; Return from interrupt

; --- Entry Point ---
_start:
    mov esp, stack_top  ; Set the stack pointer to the top of our reserved stack area
    push ebx            ; Push pointer to Multiboot information structure
    push eax            ; Push Multiboot magic number
    call kmain          ; Call C kernel
    cli                 ; Disable interrupts

.hang:
    hlt                 ; Halt the CPU
    jmp .hang           ; Infinite loop if HLT fails
.end:
