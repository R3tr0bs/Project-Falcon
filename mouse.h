#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#define MOUSE_PORT_DATA    0x60
#define MOUSE_PORT_STATUS  0x64
#define MOUSE_PORT_COMMAND 0x64

#define MOUSE_ACK           0xFA
#define MOUSE_NACK          0xFE
#define MOUSE_ERROR         0xFC

#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

typedef struct {
    int x, y;
    int delta_x, delta_y;
    uint8_t buttons;
    uint8_t scroll;
    int present;
    int enabled;
} mouse_state_t;

typedef void (*mouse_handler_t)(int x, int y, uint8_t buttons);

void mouse_init(void);
void mouse_enable(void);
void mouse_disable(void);
void mouse_install_handler(mouse_handler_t handler);
void mouse_remove_handler(void);
mouse_state_t* mouse_get_state(void);
void mouse_wait(uint8_t type);
uint8_t mouse_read(void);
void mouse_write(uint8_t data);
void mouse_irq_handler(void);

#endif