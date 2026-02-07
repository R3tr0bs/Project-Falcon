#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

extern volatile uint16_t* vga_buffer;
extern int cursor_x;
extern int cursor_y;

void clear_screen();
void print_str(const char* str);
void print_newline();
void print_dec(uint32_t n);
void print_hex(uint32_t n);
uint16_t make_vgaentry(char c, uint8_t color);
uint8_t make_color(uint8_t fg, uint8_t bg);

#endif