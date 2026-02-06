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

# Link everything together
ld -m elf_i386 -T linker.ld -o falcon.bin boot.o kernel.o ports.o utils.o vga.o idt.o keyboard.o