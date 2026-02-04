rm -f *.o *.bin
nasm -felf32 boot.s -o boot.o
gcc -m32 -c kernel.c -o kernel.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
ld -m elf_i386 -T linker.ld -o falcon.bin boot.o kernel.o