#include "vga.h"

volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

uint8_t make_color(uint8_t fg, uint8_t bg) {
    return (bg << 4) | fg;
}

uint16_t make_vgaentry(char c, uint8_t color) {
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | (color16 << 8);
}

void terminal_scroll() {
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_vgaentry(' ', make_color(15, 4));
    }
}

void print_newline() {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= VGA_HEIGHT) {
        terminal_scroll();
        cursor_y = VGA_HEIGHT - 1;
    }
}

void print_str(const char* str) {
    for(int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            print_newline();
        } else {
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_vgaentry(str[i], make_color(15, 4));
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
            vga_buffer[y * VGA_WIDTH + x] = make_vgaentry(' ', make_color(15, 4));
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