#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "vga.h"

#define GUI_MAX_WINDOWS 32
#define GUI_WINDOW_TITLE_HEIGHT 20
#define GUI_WINDOW_BORDER 2

typedef struct {
    int x, y;
    int width, height;
    char title[64];
    uint8_t* buffer;
    int visible;
    int focused;
    int dragging;
    int drag_x, drag_y;
    int minimized;
    int maximized;
} gui_window_t;

typedef struct {
    int x, y;
    int width, height;
    char text[128];
    void (*onclick)(void);
} gui_button_t;

typedef struct {
    int x, y;
    int width, height;
    char text[256];
    int cursor_pos;
    int max_length;
} gui_textbox_t;

void gui_init(void);
extern gui_window_t windows[GUI_MAX_WINDOWS];
gui_window_t* gui_create_window(int x, int y, int width, int height, const char* title);
void gui_destroy_window(gui_window_t* window);
void gui_draw_window(gui_window_t* window);
void gui_handle_mouse(int x, int y, int button);
void gui_handle_keyboard(char key);
void gui_render(void);

gui_button_t* gui_create_button(gui_window_t* window, int x, int y, int width, int height, const char* text, void (*onclick)(void));
gui_textbox_t* gui_create_textbox(gui_window_t* window, int x, int y, int width, int height, const char* text, int max_length);

#endif