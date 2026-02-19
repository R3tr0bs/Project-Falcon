#!/bin/bash
set -e # Stop script on error

# Clean up
rm -f *.o *.bin

# Compile Assembly
nasm -felf32 boot.s -o boot.o

# Compile C Sources
CFLAGS="-m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector"

gcc $CFLAGS -c kernel.c -o kernel.o
gcc $CFLAGS -c ports.c -o ports.o
gcc $CFLAGS -c utils.c -o utils.o
gcc $CFLAGS -c vga.c -o vga.o
gcc $CFLAGS -c idt.c -o idt.o
gcc $CFLAGS -c keyboard.c -o keyboard.o
gcc $CFLAGS -c pmm.c -o pmm.o
gcc $CFLAGS -c cmos.c -o cmos.o
gcc $CFLAGS -c pci.c -o pci.o
gcc $CFLAGS -c crypto.c -o crypto.o
gcc $CFLAGS -c cpu.c -o cpu.o
gcc $CFLAGS -c rng.c -o rng.o
gcc $CFLAGS -c heap.c -o heap.o
gcc $CFLAGS -c task.c -o task.o
gcc $CFLAGS -c fs.c -o fs.o
gcc $CFLAGS -c net.c -o net.o
gcc $CFLAGS -c gui.c -o gui.o
gcc $CFLAGS -c gui_desktop.c -o gui_desktop.o
gcc $CFLAGS -c mouse.c -o mouse.o
gcc $CFLAGS -c sound.c -o sound.o

# Link everything together
ld -m elf_i386 -T linker.ld -o falcon.bin boot.o kernel.o ports.o utils.o vga.o idt.o keyboard.o pmm.o cmos.o pci.o crypto.o cpu.o rng.o heap.o task.o fs.o net.o gui.o gui_desktop.o mouse.o sound.o
