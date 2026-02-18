#ifndef PCI_H
#define PCI_H

#include <stdint.h>

typedef struct {
    uint16_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t prog_if;
} pci_device_t;

void pci_check_all_buses();
uint16_t pci_config_read_word(uint16_t bus, uint8_t slot, uint8_t func, uint8_t offset);

#endif
