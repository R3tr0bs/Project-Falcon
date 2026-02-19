#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

extern volatile uint16_t* vga_buffer;
extern int cursor_x;
extern int cursor_y;

// VGA Colors
enum vga_color {
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_GRAY = 8,
	COLOR_DARK_GRAY = 8,
	COLOR_DARK_GREY = 8,
	COLOR_ORANGE = 14,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN = 14,
	COLOR_WHITE = 15,
};

void clear_screen();
void put_char(char c);
void erase_last_char();
void set_color(uint8_t fg, uint8_t bg);
void print_str(const char* str);
void print_newline();
void print_dec(uint32_t n);
void print_hex(uint32_t n);
uint16_t make_vgaentry(char c, uint8_t color);
uint8_t make_color(uint8_t fg, uint8_t bg);

#endif
