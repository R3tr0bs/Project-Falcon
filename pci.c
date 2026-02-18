#include "pci.h"
#include "ports.h"
#include "vga.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

uint32_t pci_config_read_dword(uint16_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint32_t tmp = 0;

    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    tmp = (uint32_t)(inl(PCI_CONFIG_DATA));
    return tmp;
}

uint16_t pci_config_read_word(uint16_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_config_read_dword(bus, slot, func, offset);
    uint16_t word = (uint16_t)((dword >> ((offset & 2) * 8)) & 0xffff);
    return word;
}

void check_device(uint16_t bus, uint8_t device) {
    uint16_t vendor = pci_config_read_word(bus, device, 0, 0);
    if (vendor == 0xFFFF) return; // Device doesn't exist

    uint16_t device_id = pci_config_read_word(bus, device, 0, 2);
    uint16_t class_code = pci_config_read_word(bus, device, 0, 0x0A);
    uint8_t class_id = (class_code >> 8) & 0xFF;
    uint8_t subclass_id = class_code & 0xFF;

    print_str("PCI [");
    print_dec(bus);
    print_str(":");
    print_dec(device);
    print_str("] Vendor: ");
    print_hex(vendor);
    print_str(" Device: ");
    print_hex(device_id);
    print_str(" Class: ");
    print_hex(class_id);
    print_str(" Subclass: ");
    print_hex(subclass_id);
    print_str("\n");
}

void pci_check_all_buses() {
    print_str("Scanning PCI Bus...\n");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            check_device(bus, device);
        }
    }
    print_str("PCI Scan Complete.\n");
}
