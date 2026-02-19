#include "keyboard.h"
#include "ports.h"
#include "vga.h"
#include "utils.h"

#define KBD_DATA_PORT 0x60
#define PIC1_COMMAND 0x20

char cmd_buffer[CMD_BUFFER_SIZE];
int cmd_buffer_idx = 0;

// Defined in kernel.c
extern void process_command(char* command);
extern void add_history(const char* command);

unsigned char kbdus[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void keyboard_handler() {
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (!(scancode & 0x80)) {
        char c = kbdus[scancode];
        if (c == '\n') {
            print_newline();
            cmd_buffer[cmd_buffer_idx] = '\0';
            add_history(cmd_buffer);
            process_command(cmd_buffer);
            cmd_buffer_idx = 0;
        } else if (c == '\b') {
            if (cmd_buffer_idx > 0) {
                cmd_buffer_idx--;
                erase_last_char();
            }
        } else if (c && cmd_buffer_idx < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_buffer_idx++] = c;
            put_char(c);
        }
        if (cursor_x >= VGA_WIDTH) {
            print_newline();
        }
    }
    outb(PIC1_COMMAND, 0x20);
}
