#include "mouse.h"
#include "ports.h"
#include "vga.h"

#define NULL ((void*)0)

static mouse_state_t mouse_state;
static mouse_handler_t mouse_handler = NULL;
static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[4];
static int mouse_x = 0, mouse_y = 0;

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(MOUSE_PORT_STATUS) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(MOUSE_PORT_STATUS) & 2) == 0) return;
        }
    }
}

uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(MOUSE_PORT_DATA);
}

void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(MOUSE_PORT_COMMAND, 0xD4);
    mouse_wait(1);
    outb(MOUSE_PORT_DATA, data);
}

void mouse_install_handler(mouse_handler_t handler) {
    mouse_handler = handler;
}

void mouse_remove_handler(void) {
    mouse_handler = NULL;
}

mouse_state_t* mouse_get_state(void) {
    return &mouse_state;
}

static void mouse_handle_packet(void) {
    if (mouse_cycle == 3) {
        uint8_t status = mouse_packet[0];
        int8_t dx = mouse_packet[1];
        int8_t dy = mouse_packet[2];
        
        // Check for overflow
        if (status & 0x80 || status & 0x40) return;
        
        // Handle sign extension
        if (status & 0x10) dx |= 0xFFFFFF00;
        if (status & 0x20) dy |= 0xFFFFFF00;
        
        // Update mouse position
        mouse_x += dx;
        mouse_y -= dy; // Mouse Y is inverted
        
        // Clamp to screen bounds
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= VGA_WIDTH) mouse_x = VGA_WIDTH - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= VGA_HEIGHT) mouse_y = VGA_HEIGHT - 1;
        
        // Update state
        mouse_state.x = mouse_x;
        mouse_state.y = mouse_y;
        mouse_state.delta_x = dx;
        mouse_state.delta_y = dy;
        mouse_state.buttons = status & 0x07;
        
        // Call handler if installed
        if (mouse_handler) {
            mouse_handler(mouse_x, mouse_y, mouse_state.buttons);
        }
        
        mouse_cycle = 0;
    }
}

void mouse_irq_handler(void) {
    uint8_t status = inb(MOUSE_PORT_STATUS);
    
    if (status & 0x20) {
        mouse_packet[mouse_cycle] = inb(MOUSE_PORT_DATA);
        mouse_cycle = (mouse_cycle + 1) % 3;
        mouse_handle_packet();
    }
}

void mouse_enable(void) {
    uint8_t status;
    
    // Enable mouse
    mouse_wait(1);
    outb(MOUSE_PORT_COMMAND, 0xA8);
    
    // Enable interrupts
    mouse_wait(1);
    outb(MOUSE_PORT_COMMAND, 0x20);
    mouse_wait(0);
    status = (inb(MOUSE_PORT_DATA) | 2);
    mouse_wait(1);
    outb(MOUSE_PORT_COMMAND, 0x60);
    mouse_wait(1);
    outb(MOUSE_PORT_DATA, status);
    
    // Set default settings
    mouse_write(0xF6);
    mouse_read(); // Acknowledge
    
    // Enable mouse
    mouse_write(0xF4);
    mouse_read(); // Acknowledge
    
    mouse_state.enabled = 1;
}

void mouse_disable(void) {
    mouse_write(0xF5);
    mouse_read(); // Acknowledge
    mouse_state.enabled = 0;
}

void mouse_init(void) {
    mouse_state.x = VGA_WIDTH / 2;
    mouse_state.y = VGA_HEIGHT / 2;
    mouse_state.delta_x = 0;
    mouse_state.delta_y = 0;
    mouse_state.buttons = 0;
    mouse_state.scroll = 0;
    mouse_state.present = 1;
    mouse_state.enabled = 0;
    
    mouse_x = VGA_WIDTH / 2;
    mouse_y = VGA_HEIGHT / 2;
    mouse_cycle = 0;
    
    mouse_enable();
}