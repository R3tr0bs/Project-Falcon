#include "gui_desktop.h"
#include "mouse.h"
#include "vga.h"
#include "utils.h"
#include "heap.h"

// Simple strcpy implementation for freestanding environment
static void gui_strcpy(char* dst, const char* src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

// Simple strcpy implementation for button text
static void gui_strcpy_button(char* dst, const char* src) {
    while (*src && dst - ((gui_button_t*)0)->text < 128) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

static gui_desktop_t desktop;

// Mouse cursor shape (8x8 bitmap)
static const uint8_t mouse_cursor_bitmap[8] = {
    0b11110000,
    0b10001000,
    0b10001000,
    0b10001000,
    0b11110000,
    0b00000000,
    0b00000000,
    0b00000000
};

void gui_desktop_init(void) {
    desktop.icon_count = 0;
    desktop.taskbar_item_count = 0;
    desktop.taskbar_button_count = 0;
    desktop.active_window = NULL;
    desktop.show_start_menu = 0;
    desktop.start_menu = NULL;
    desktop.mouse_x = VGA_WIDTH / 2;
    desktop.mouse_y = VGA_HEIGHT / 2;
    
    // Set default colors
    desktop.desktop_color = make_color(COLOR_LIGHT_BLUE, COLOR_BLACK);
    desktop.taskbar_color = make_color(COLOR_GRAY, COLOR_BLACK);
    desktop.window_border_color = make_color(COLOR_DARK_GRAY, COLOR_BLACK);
    desktop.window_title_color = make_color(COLOR_BLUE, COLOR_WHITE);
    
    // Create start button
    desktop.start_button.x = 0;
    desktop.start_button.y = VGA_HEIGHT - GUI_TASKBAR_HEIGHT;
    desktop.start_button.width = 60;
    desktop.start_button.height = GUI_TASKBAR_HEIGHT;
    desktop.start_button.color = make_color(COLOR_GREEN, COLOR_WHITE);
    gui_strcpy(desktop.start_button.text, "Start");
    desktop.start_button.visible = 1;
    desktop.start_button.onclick = gui_desktop_toggle_start_menu;
    
    // Add default desktop icons
    gui_desktop_add_icon(10, 10, "Calculator", "C", gui_app_calculator);
    gui_desktop_add_icon(10, 50, "Notepad", "N", gui_app_notepad);
    gui_desktop_add_icon(10, 90, "File Manager", "F", gui_app_file_manager);
    gui_desktop_add_icon(10, 130, "Terminal", "T", gui_app_terminal);
    gui_desktop_add_icon(10, 170, "Paint", "P", gui_app_paint);
    gui_desktop_add_icon(10, 210, "Music", "M", gui_app_music_player);
}

void gui_draw_mouse_cursor(int x, int y) {
    // Draw mouse cursor at current position
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (mouse_cursor_bitmap[row] & (1 << (7 - col))) {
                int screen_x = x + col;
                int screen_y = y + row;
                if (screen_x < VGA_WIDTH && screen_y < VGA_HEIGHT) {
                    vga_buffer[screen_y * VGA_WIDTH + screen_x] = make_vgaentry(' ', make_color(COLOR_WHITE, COLOR_BLACK));
                }
            }
        }
    }
}

void gui_desktop_render(void) {
    // Clear desktop background
    for (int y = 0; y < VGA_HEIGHT - GUI_TASKBAR_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = make_vgaentry(' ', desktop.desktop_color);
        }
    }
    
    // Draw desktop icons
    for (int i = 0; i < desktop.icon_count; i++) {
        gui_desktop_icon_t* icon = &desktop.icons[i];
        if (icon->visible) {
            // Draw icon background
            for (int dy = 0; dy < 32; dy++) {
                for (int dx = 0; dx < 48; dx++) {
                    int screen_x = icon->x + dx;
                    int screen_y = icon->y + dy;
                    if (screen_x < VGA_WIDTH && screen_y < VGA_HEIGHT - GUI_TASKBAR_HEIGHT) {
                        vga_buffer[screen_y * VGA_WIDTH + screen_x] = make_vgaentry(' ', make_color(COLOR_LIGHT_GREY, COLOR_BLACK));
                    }
                }
            }
            
            // Draw icon text
            int text_x = icon->x + 24 - (strlen(icon->icon) * 4);
            int text_y = icon->y + 8;
            for (int j = 0; icon->icon[j]; j++) {
                if (text_x + j < VGA_WIDTH && text_y < VGA_HEIGHT) {
                    vga_buffer[text_y * VGA_WIDTH + text_x + j] = make_vgaentry(icon->icon[j], make_color(COLOR_BLACK, COLOR_LIGHT_GREY));
                }
            }
            
            // Draw icon name
            int name_x = icon->x + 24 - (strlen(icon->name) * 4);
            int name_y = icon->y + 24;
            for (int j = 0; icon->name[j]; j++) {
                if (name_x + j < VGA_WIDTH && name_y < VGA_HEIGHT) {
                    vga_buffer[name_y * VGA_WIDTH + name_x + j] = make_vgaentry(icon->name[j], make_color(COLOR_BLACK, COLOR_LIGHT_GREY));
                }
            }
        }
    }
    
    // Draw taskbar
    int taskbar_y = VGA_HEIGHT - GUI_TASKBAR_HEIGHT;
    for (int y = taskbar_y; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = make_vgaentry(' ', desktop.taskbar_color);
        }
    }
    
    // Draw start button
    if (desktop.start_button.visible) {
        gui_draw_button_ex(&desktop.start_button);
    }
    
    // Draw taskbar buttons
    for (int i = 0; i < desktop.taskbar_button_count; i++) {
        gui_draw_button_ex(&desktop.taskbar_buttons[i]);
    }
    
    // Draw active windows
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (windows[i].visible && windows[i].buffer) {
            gui_draw_window(&windows[i]);
        }
    }
    
    // Draw start menu if visible
    if (desktop.show_start_menu && desktop.start_menu) {
        gui_draw_window(desktop.start_menu);
    }
    
    // Draw mouse cursor last (on top of everything)
    gui_draw_mouse_cursor(desktop.mouse_x, desktop.mouse_y);
}

void gui_desktop_handle_mouse(int x, int y, uint8_t buttons) {
    // Update mouse position
    desktop.mouse_x = x;
    desktop.mouse_y = y;
    
    // Handle icon clicks
    if (buttons & MOUSE_LEFT_BUTTON) {
        for (int i = 0; i < desktop.icon_count; i++) {
            gui_desktop_icon_t* icon = &desktop.icons[i];
            if (icon->visible && 
                x >= icon->x && x < icon->x + 48 &&
                y >= icon->y && y < icon->y + 32) {
                if (icon->onclick) {
                    icon->onclick();
                }
                return;
            }
        }
        
        // Handle start button click
        if (desktop.start_button.visible &&
            x >= desktop.start_button.x && x < desktop.start_button.x + desktop.start_button.width &&
            y >= desktop.start_button.y && y < desktop.start_button.y + desktop.start_button.height) {
            if (desktop.start_button.onclick) {
                desktop.start_button.onclick();
            }
            return;
        }
    }
    
    // Handle window interactions
    gui_handle_mouse(x, y, buttons);
}

void gui_desktop_handle_keyboard(char key) {
    // Handle keyboard shortcuts
    if (key == 's' && desktop.show_start_menu) {
        gui_desktop_toggle_start_menu();
    }
    
    // Handle active window keyboard input
    if (desktop.active_window) {
        gui_handle_keyboard(key);
    }
}

gui_desktop_icon_t* gui_desktop_add_icon(int x, int y, const char* name, const char* icon, void (*onclick)(void)) {
    if (desktop.icon_count >= GUI_MAX_ICONS) return NULL;
    
    gui_desktop_icon_t* desktop_icon = &desktop.icons[desktop.icon_count++];
    desktop_icon->x = x;
    desktop_icon->y = y;
    desktop_icon->width = 48;
    desktop_icon->height = 32;
    desktop_icon->visible = 1;
    desktop_icon->onclick = onclick;
    
    // Copy name
    int i = 0;
    while (name[i] && i < 63) {
        desktop_icon->name[i] = name[i];
        i++;
    }
    desktop_icon->name[i] = '\0';
    
    // Copy icon
    i = 0;
    while (icon[i] && i < 15) {
        desktop_icon->icon[i] = icon[i];
        i++;
    }
    desktop_icon->icon[i] = '\0';
    
    return desktop_icon;
}

void gui_desktop_add_window(gui_window_t* window, const char* title) {
    if (desktop.taskbar_item_count >= GUI_MAX_WINDOWS) return;
    
    gui_taskbar_item_t* item = &desktop.taskbar_items[desktop.taskbar_item_count++];
    item->window = window;
    item->minimized = 0;
    item->order = desktop.taskbar_item_count;
    
    // Copy title
    int i = 0;
    while (title[i] && i < 63) {
        item->name[i] = title[i];
        i++;
    }
    item->name[i] = '\0';
    
    // Create taskbar button
    gui_button_ex_t* button = &desktop.taskbar_buttons[desktop.taskbar_button_count++];
    button->x = 60 + (desktop.taskbar_button_count - 1) * 80; // Start after start button
    button->y = VGA_HEIGHT - GUI_TASKBAR_HEIGHT;
    button->width = 75;
    button->height = GUI_TASKBAR_HEIGHT;
    button->color = make_color(COLOR_GRAY, COLOR_BLACK);
    gui_strcpy_button(button->text, title);
    button->visible = 1;
    button->onclick = NULL; // Will be set later
    
    desktop.active_window = window;
}

void gui_desktop_focus_window(gui_window_t* window) {
    desktop.active_window = window;
    if (window) {
        window->focused = 1;
    }
}

void gui_desktop_remove_window(gui_window_t* window) {
    if (!window) return;
    
    // Remove from taskbar
    for (int i = 0; i < desktop.taskbar_item_count; i++) {
        if (desktop.taskbar_items[i].window == window) {
            // Shift remaining items
            for (int j = i; j < desktop.taskbar_item_count - 1; j++) {
                desktop.taskbar_items[j] = desktop.taskbar_items[j + 1];
            }
            desktop.taskbar_item_count--;
            break;
        }
    }
    
    // Remove from taskbar buttons
    for (int i = 0; i < desktop.taskbar_button_count; i++) {
        if (desktop.taskbar_buttons[i].visible && 
            desktop.taskbar_items[i].window == window) {
            desktop.taskbar_buttons[i].visible = 0;
            // Shift remaining buttons
            for (int j = i; j < desktop.taskbar_button_count - 1; j++) {
                desktop.taskbar_buttons[j] = desktop.taskbar_buttons[j + 1];
                desktop.taskbar_items[j] = desktop.taskbar_items[j + 1];
            }
            desktop.taskbar_button_count--;
            break;
        }
    }
    
    // Update active window if this was the active one
    if (desktop.active_window == window) {
        desktop.active_window = NULL;
        // Find another window to focus
        for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
            if (windows[i].visible && &windows[i] != window) {
                desktop.active_window = &windows[i];
                windows[i].focused = 1;
                break;
            }
        }
    }
}

void gui_desktop_toggle_start_menu(void) {
    desktop.show_start_menu = !desktop.show_start_menu;
    
    if (desktop.show_start_menu && !desktop.start_menu) {
        // Create start menu window
        desktop.start_menu = gui_create_window(0, VGA_HEIGHT - GUI_TASKBAR_HEIGHT - 200, 200, 200, "Start Menu");
        if (desktop.start_menu) {
            desktop.start_menu->visible = 1;
        }
    }
}

gui_button_ex_t* gui_create_button_ex(int x, int y, int width, int height, const char* text, uint8_t color, void (*onclick)(void)) {
    gui_button_ex_t* button = (gui_button_ex_t*)kmalloc(sizeof(gui_button_ex_t));
    if (!button) return NULL;
    
    button->x = x;
    button->y = y;
    button->width = width;
    button->height = height;
    button->color = color;
    button->onclick = onclick;
    button->visible = 1;
    
    // Copy text
    int i = 0;
    while (text[i] && i < 127) {
        button->text[i] = text[i];
        i++;
    }
    button->text[i] = '\0';
    
    return button;
}

void gui_draw_button_ex(gui_button_ex_t* button) {
    if (!button->visible) return;
    
    // Draw button background
    for (int y = 0; y < button->height; y++) {
        for (int x = 0; x < button->width; x++) {
            int screen_x = button->x + x;
            int screen_y = button->y + y;
            if (screen_x < VGA_WIDTH && screen_y < VGA_HEIGHT) {
                vga_buffer[screen_y * VGA_WIDTH + screen_x] = make_vgaentry(' ', button->color);
            }
        }
    }
    
    // Draw button text (centered)
    int text_len = strlen(button->text);
    int text_x = button->x + (button->width - text_len) / 2;
    int text_y = button->y + button->height / 2;
    
    for (int i = 0; button->text[i]; i++) {
        if (text_x + i < VGA_WIDTH && text_y < VGA_HEIGHT) {
            vga_buffer[text_y * VGA_WIDTH + text_x + i] = make_vgaentry(button->text[i], button->color);
        }
    }
}

// Application implementations
void gui_app_calculator(void) {
    gui_window_t* calc = gui_create_window(100, 100, 300, 200, "Calculator");
    if (calc) {
        gui_desktop_add_window(calc, "Calculator");
        // Add calculator buttons and display
        gui_create_button_ex(50, 50, 40, 30, "7", make_color(COLOR_GRAY, COLOR_BLACK), NULL);
        gui_create_button_ex(100, 50, 40, 30, "8", make_color(COLOR_GRAY, COLOR_BLACK), NULL);
        gui_create_button_ex(150, 50, 40, 30, "9", make_color(COLOR_GRAY, COLOR_BLACK), NULL);
        gui_create_button_ex(200, 50, 40, 30, "/", make_color(COLOR_ORANGE, COLOR_BLACK), NULL);
    }
}

void gui_app_notepad(void) {
    gui_window_t* notepad = gui_create_window(150, 150, 400, 300, "Notepad");
    if (notepad) {
        gui_desktop_add_window(notepad, "Notepad");
        // Add text editing area
        gui_create_textbox(notepad, 10, 30, 380, 250, "", 1024);
    }
}

void gui_app_file_manager(void) {
    gui_window_t* fileman = gui_create_window(200, 200, 500, 400, "File Manager");
    if (fileman) {
        gui_desktop_add_window(fileman, "File Manager");
        // Add file list and navigation
        // This would show filesystem contents
    }
}

void gui_app_terminal(void) {
    gui_window_t* terminal = gui_create_window(250, 250, 600, 400, "Terminal");
    if (terminal) {
        gui_desktop_add_window(terminal, "Terminal");
        // Terminal emulation would go here
    }
}

void gui_app_paint(void) {
    gui_window_t* paint = gui_create_window(300, 300, 400, 300, "Paint");
    if (paint) {
        gui_desktop_add_window(paint, "Paint");
        // Drawing canvas and tools
    }
}

void gui_app_music_player(void) {
    gui_window_t* music = gui_create_window(350, 350, 300, 150, "Music Player");
    if (music) {
        gui_desktop_add_window(music, "Music Player");
        // Playback controls
        gui_create_button_ex(50, 50, 40, 30, "Play", make_color(COLOR_GREEN, COLOR_BLACK), NULL);
        gui_create_button_ex(100, 50, 40, 30, "Stop", make_color(COLOR_RED, COLOR_BLACK), NULL);
        gui_create_button_ex(150, 50, 40, 30, "Next", make_color(COLOR_BLUE, COLOR_BLACK), NULL);
    }
}