; boot.s - Project Falcon Bootloader Stub
; Purpose: Define Multiboot Header for QEMU recognition and jump to C kernel

MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002        ; Magic number for Multiboot 1 standard
CHECKSUM equ -(MAGIC + FLAGS)   ; Header checksum

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
resb 16384 ; Reserve 16 KiB for Kernel Stack
stack_top:

section .text
; --- Interrupt Service Routines (ISRs) Wrappers ---

global timer_wrapper   ; Make accessible to C
extern timer_handler   ; The C function we will write
extern fault_handler ; This will be our C function
; --- CPU Exception Handling Macros ---

; Macro for exceptions that DO NOT push an error code automatically.
; We push a dummy '0' so the stack format remains consistent.
%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli             ; Disable interrupts immediately
    push 0          ; Push dummy error code
    push %1         ; Push the interrupt number
    jmp isr_common_stub
%endmacro

; Macro for exceptions that DO push an error code automatically.
%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli
    ; Error code is already on the stack
    push %1         ; Push the interrupt number
    jmp isr_common_stub
%endmacro

; --- Define the ISRs (0-31) ---
; These correspond to the Intel Manual specifications
ISR_NOERRCODE 0   ; Division by zero
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; Non-maskable interrupt
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound Range Exceeded
ISR_NOERRCODE 6   ; Invalid Opcode
ISR_NOERRCODE 7   ; Device Not Available
ISR_ERRCODE   8   ; Double Fault (CRITICAL)
ISR_NOERRCODE 9   ; Coprocessor Segment Overrun
ISR_ERRCODE   10  ; Invalid TSS
ISR_ERRCODE   11  ; Segment Not Present
ISR_ERRCODE   12  ; Stack-Segment Fault
ISR_ERRCODE   13  ; General Protection Fault (GPF)
ISR_ERRCODE   14  ; Page Fault (CRITICAL)
ISR_NOERRCODE 15  ; Reserved
; ... You can define the rest up to 31 similarly

; --- Common ISR Stub ---


isr_common_stub:
    pushad          ; 1. Pushes edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds      ; 2. Save the Data Segment descriptor
    push eax

    mov ax, 0x10    ; 3. Load the Kernel Data Segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call fault_handler ; 4. Call the high-level C code

    pop eax         ; 5. Restore the original Data Segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popad           ; 6. Restore general purpose registers

    add esp, 8      ; 7. Clean up the pushed error code and ISR number
                    ;    (We pushed 2 dwords = 8 bytes)
    
    iretd           ; 8. Return from interrupt

; This is the bridge between hardware and C code
timer_wrapper:
    pushad             ; 1. Save all general purpose registers
    cld                ; 2. Clear direction flag (standard practice)
    call timer_handler ; 3. Call the C kernel function
    popad              ; 4. Restore registers
    iretd              ; 5. Return from Interrupt (restores CS, EIP, EFLAGS)

global _start:function (_start.end - _start)

_start:
    mov esp, stack_top  ; Setup Stack Pointer (ESP)
    extern kmain
    call kmain          ; Transfer control to C Kernel Main
    cli                 ; Disable interrupts (just in case)


.hang: hlt              ; Halt the CPU
    jmp .hang           ; Infinite loop if HLT fails
.end: