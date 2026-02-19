#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H

#include <stdint.h>
#include "gui.h"
#include "mouse.h"

#define GUI_DESKTOP_COLOR 0x1F  // Light blue
#define GUI_TASKBAR_HEIGHT 24
#define GUI_WINDOW_MIN_WIDTH 100
#define GUI_WINDOW_MIN_HEIGHT 80
#define GUI_MAX_WINDOWS 32
#define GUI_MAX_ICONS 64

typedef enum {
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_DOWN,
    GUI_EVENT_MOUSE_UP,
    GUI_EVENT_KEY_DOWN,
    GUI_EVENT_KEY_UP,
    GUI_EVENT_WINDOW_CLOSE,
    GUI_EVENT_WINDOW_MINIMIZE,
    GUI_EVENT_WINDOW_MAXIMIZE
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    int x, y;
    uint8_t button;
    char key;
    void* target;
} gui_event_t;

typedef struct {
    int x, y;
    int width, height;
    char name[64];
    char icon[16];  // Icon character representation
    void (*onclick)(void);
    int visible;
} gui_desktop_icon_t;

typedef struct {
    char name[64];
    gui_window_t* window;
    int minimized;
    int order;
} gui_taskbar_item_t;

typedef struct {
    int x, y, width, height;
    uint8_t color;
    char text[128];
    int visible;
    void (*onclick)(void);
} gui_button_ex_t;

typedef struct {
    gui_desktop_icon_t icons[GUI_MAX_ICONS];
    gui_taskbar_item_t taskbar_items[GUI_MAX_WINDOWS];
    gui_button_ex_t start_button;
    gui_button_ex_t taskbar_buttons[GUI_MAX_WINDOWS];
    int icon_count;
    int taskbar_item_count;
    int taskbar_button_count;
    int desktop_color;
    int taskbar_color;
    int window_border_color;
    int window_title_color;
    gui_window_t* active_window;
    int mouse_x, mouse_y;
    int show_start_menu;
    gui_window_t* start_menu;
} gui_desktop_t;

// Desktop management
void gui_desktop_init(void);
void gui_desktop_render(void);
void gui_desktop_handle_mouse(int x, int y, uint8_t buttons);
void gui_desktop_handle_keyboard(char key);

// Window management
void gui_desktop_add_window(gui_window_t* window, const char* title);
void gui_desktop_remove_window(gui_window_t* window);
void gui_desktop_focus_window(gui_window_t* window);
gui_window_t* gui_desktop_get_active_window(void);

// Icon management
gui_desktop_icon_t* gui_desktop_add_icon(int x, int y, const char* name, const char* icon, void (*onclick)(void));
void gui_desktop_remove_icon(gui_desktop_icon_t* icon);

// Taskbar management
void gui_desktop_update_taskbar(void);
void gui_desktop_toggle_start_menu(void);

// Applications
void gui_app_calculator(void);
void gui_app_notepad(void);
void gui_app_file_manager(void);
void gui_app_terminal(void);
void gui_app_paint(void);
void gui_app_music_player(void);

// Advanced GUI elements
gui_button_ex_t* gui_create_button_ex(int x, int y, int width, int height, const char* text, uint8_t color, void (*onclick)(void));
void gui_draw_button_ex(gui_button_ex_t* button);

// Event system
void gui_handle_event(gui_event_t* event);
void gui_post_event(gui_event_t* event);

#endif