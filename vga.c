#include "vga.h"

volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;
uint8_t current_color = 0x4F; // Default: White on Red (wait, 15 is White, 4 is Red? No, 4 is Red, 15 is White)
// In original code: make_color(15, 4) -> 15 (White) on 4 (Red) -> 0x4F.
// Wait, make_color(fg, bg) is (bg << 4) | fg.
// make_color(15, 4) = (4 << 4) | 15 = 0x4F.
// Background 4 (Red), Foreground 15 (White).

uint8_t make_color(uint8_t fg, uint8_t bg) {
    return (bg << 4) | fg;
}

void set_color(uint8_t fg, uint8_t bg) {
    current_color = make_color(fg, bg);
}

uint16_t make_vgaentry(char c, uint8_t color) {
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | (color16 << 8);
}

void put_char(char c) {
    if (c == '\n') {
        print_newline();
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_vgaentry(c, current_color);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            print_newline();
        }
    }
}

void erase_last_char() {
    if (cursor_x > 0) {
        cursor_x--;
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_vgaentry(' ', current_color);
    } else if (cursor_y > 0) {
        cursor_y--;
        cursor_x = VGA_WIDTH - 1;
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_vgaentry(' ', current_color);
    }
}

void terminal_scroll() {
    for (int y = 0; y < VGA_HEIGHT - 2; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 2) * VGA_WIDTH + x] = make_vgaentry(' ', current_color);
    }
}

void print_newline() {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= VGA_HEIGHT - 1) {
        terminal_scroll();
        cursor_y = VGA_HEIGHT - 2;
    }
}

void print_str(const char* str) {
    for(int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            print_newline();
        } else {
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_vgaentry(str[i], current_color);
            cursor_x++;
            if (cursor_x >= VGA_WIDTH) {
                print_newline();
            }
        }
    }
}

void clear_screen() {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = make_vgaentry(' ', current_color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

void print_dec(uint32_t n) {
    if (n == 0) {
        print_str("0");
        return;
    }
    char buf[32];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    for (int j = 0; j < i / 2; j++) {
        char temp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = temp;
    }
    buf[i] = '\0';
    print_str(buf);
}

void print_hex(uint32_t n) {
    print_str("0x");
    char hex_chars[] = "0123456789ABCDEF";
    char buf[9];
    buf[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        buf[i] = hex_chars[n & 0xF];
        n >>= 4;
    }
    char* p = buf;
    while (*p == '0' && *(p+1) != '\0') p++;
    print_str(p);
}
