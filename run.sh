#!/bin/bash
qemu-system-i386 -kernel falcon.bin -netdev user,id=net0 -device rtl8139,netdev=net0
