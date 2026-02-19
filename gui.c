#include "gui.h"
#include "gui_desktop.h"
#include "heap.h"
#include "mouse.h"
#include "vga.h"

// Windows array definition
gui_window_t windows[GUI_MAX_WINDOWS];

// Simple strlen implementation for freestanding environment
static int gui_strlen(const char* str) {
    int len = 0;
    while (str && *str++) len++;
    return len;
}
static int window_count = 0;
static gui_window_t* focused_window = NULL;

// Window decoration buttons
#define WINDOW_CLOSE_BTN_X  -20
#define WINDOW_CLOSE_BTN_Y  -15
#define WINDOW_CLOSE_BTN_W   16
#define WINDOW_CLOSE_BTN_H   16

#define WINDOW_MIN_BTN_X    -40
#define WINDOW_MIN_BTN_Y    -15
#define WINDOW_MIN_BTN_W    16
#define WINDOW_MIN_BTN_H    16

#define WINDOW_MAX_BTN_X    -60
#define WINDOW_MAX_BTN_Y    -15
#define WINDOW_MAX_BTN_W    16
#define WINDOW_MAX_BTN_H    16

void gui_init(void) {
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        windows[i].buffer = NULL;
        windows[i].visible = 0;
        windows[i].focused = 0;
        windows[i].dragging = 0;
        windows[i].minimized = 0;
        windows[i].maximized = 0;
    }
    window_count = 0;
    focused_window = NULL;
}

gui_window_t* gui_create_window(int x, int y, int width, int height, const char* title) {
    if (window_count >= GUI_MAX_WINDOWS) return NULL;
    
    gui_window_t* window = &windows[window_count++];
    
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->visible = 1;
    window->focused = 0;
    window->dragging = 0;
    window->minimized = 0;
    window->maximized = 0;
    
    int title_len = 0;
    while (title[title_len] && title_len < 63) {
        window->title[title_len] = title[title_len];
        title_len++;
    }
    window->title[title_len] = '\0';
    
    window->buffer = (uint8_t*)kmalloc(width * height * 2);
    if (!window->buffer) {
        window_count--;
        return NULL;
    }
    
    // Clear window buffer
    for (int i = 0; i < width * height; i++) {
        ((uint16_t*)window->buffer)[i] = make_vgaentry(' ', make_color(0, 7));
    }
    
    // Add to desktop
    gui_desktop_add_window(window, title);
    
    return window;
}

void gui_destroy_window(gui_window_t* window) {
    if (!window) return;
    
    if (window->buffer) {
        kfree(window->buffer);
        window->buffer = NULL;
    }
    
    window->visible = 0;
    
    if (focused_window == window) {
        focused_window = NULL;
    }
    
    // Remove from desktop
    gui_desktop_remove_window(window);
}

static void gui_draw_pixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT - 1) return;
    vga_buffer[y * VGA_WIDTH + x] = make_vgaentry(' ', color);
}

static void gui_draw_rect(int x, int y, int width, int height, uint8_t color) {
    for (int py = y; py < y + height; py++) {
        for (int px = x; px < x + width; px++) {
            gui_draw_pixel(px, py, color);
        }
    }
}

static void gui_draw_text(int x, int y, const char* text, uint8_t color) {
    int px = x;
    int py = y;
    
    for (int i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            py++;
            px = x;
            continue;
        }
        
        if (px < VGA_WIDTH && py < VGA_HEIGHT - 1) {
            vga_buffer[py * VGA_WIDTH + px] = make_vgaentry(text[i], color);
        }
        px++;
    }
}

static void gui_draw_window_buttons(gui_window_t* window) {
    if (!window->focused) return;
    
    // Close button (red)
    int close_x = window->x + window->width + WINDOW_CLOSE_BTN_X;
    int close_y = window->y + WINDOW_CLOSE_BTN_Y;
    gui_draw_rect(close_x, close_y, WINDOW_CLOSE_BTN_W, WINDOW_CLOSE_BTN_H, make_color(COLOR_WHITE, COLOR_RED));
    gui_draw_text(close_x + 4, close_y + 4, "X", make_color(COLOR_WHITE, COLOR_RED));
    
    // Maximize button (green)
    int max_x = window->x + window->width + WINDOW_MAX_BTN_X;
    int max_y = window->y + WINDOW_MAX_BTN_Y;
    gui_draw_rect(max_x, max_y, WINDOW_MAX_BTN_W, WINDOW_MAX_BTN_H, make_color(COLOR_WHITE, COLOR_GREEN));
    gui_draw_text(max_x + 4, max_y + 4, "[]", make_color(COLOR_WHITE, COLOR_GREEN));
    
    // Minimize button (yellow)
    int min_x = window->x + window->width + WINDOW_MIN_BTN_X;
    int min_y = window->y + WINDOW_MIN_BTN_Y;
    gui_draw_rect(min_x, min_y, WINDOW_MIN_BTN_W, WINDOW_MIN_BTN_H, make_color(COLOR_BLACK, COLOR_BROWN));
    gui_draw_text(min_x + 4, min_y + 4, "_", make_color(COLOR_BLACK, COLOR_BROWN));
}

void gui_draw_window(gui_window_t* window) {
    if (!window || !window->visible || window->minimized) return;
    
    // Draw window border
    uint8_t border_color = make_color(COLOR_DARK_GREY, COLOR_BLACK);
    uint8_t title_color = make_color(COLOR_WHITE, COLOR_BLUE);
    uint8_t bg_color = make_color(COLOR_BLACK, COLOR_WHITE);
    
    if (window->focused) {
        border_color = make_color(COLOR_WHITE, COLOR_RED);
        title_color = make_color(COLOR_WHITE, COLOR_BLUE);
    }
    
    // Draw border
    gui_draw_rect(window->x - GUI_WINDOW_BORDER, window->y - GUI_WINDOW_BORDER - GUI_WINDOW_TITLE_HEIGHT,
                  window->width + 2 * GUI_WINDOW_BORDER,
                  window->height + 2 * GUI_WINDOW_BORDER + GUI_WINDOW_TITLE_HEIGHT, border_color);
    
    // Draw title bar
    gui_draw_rect(window->x, window->y - GUI_WINDOW_TITLE_HEIGHT,
                  window->width, GUI_WINDOW_TITLE_HEIGHT, title_color);
    
    // Draw title text
    gui_draw_text(window->x + 2, window->y - GUI_WINDOW_TITLE_HEIGHT + 5, window->title, title_color);
    
    // Draw window buttons
    gui_draw_window_buttons(window);
    
    // Draw window content
    gui_draw_rect(window->x, window->y, window->width, window->height, bg_color);
    
    // Copy window buffer to screen
    for (int py = 0; py < window->height && py < VGA_HEIGHT - 1; py++) {
        for (int px = 0; px < window->width && px < VGA_WIDTH; px++) {
            int screen_x = window->x + px;
            int screen_y = window->y + py;
            
            if (screen_x < VGA_WIDTH && screen_y < VGA_HEIGHT - 1) {
                vga_buffer[screen_y * VGA_WIDTH + screen_x] = ((uint16_t*)window->buffer)[py * window->width + px];
            }
        }
    }
}

void gui_handle_mouse(int x, int y, int button) {
    static int dragging = 0;
    
    for (int i = window_count - 1; i >= 0; i--) {
        gui_window_t* window = &windows[i];
        
        if (!window->visible || window->minimized) continue;
        
        // Check window control buttons
        if (window->focused) {
            // Close button
            int close_x = window->x + window->width + WINDOW_CLOSE_BTN_X;
            int close_y = window->y + WINDOW_CLOSE_BTN_Y;
            if (x >= close_x && x < close_x + WINDOW_CLOSE_BTN_W &&
                y >= close_y && y < close_y + WINDOW_CLOSE_BTN_H && button) {
                gui_destroy_window(window);
                return;
            }
            
            // Minimize button
            int min_x = window->x + window->width + WINDOW_MIN_BTN_X;
            int min_y = window->y + WINDOW_MIN_BTN_Y;
            if (x >= min_x && x < min_x + WINDOW_MIN_BTN_W &&
                y >= min_y && y < min_y + WINDOW_MIN_BTN_H && button) {
                window->minimized = 1;
                return;
            }
            
            // Maximize button
            int max_x = window->x + window->width + WINDOW_MAX_BTN_X;
            int max_y = window->y + WINDOW_MAX_BTN_Y;
            if (x >= max_x && x < max_x + WINDOW_MAX_BTN_W &&
                y >= max_y && y < max_y + WINDOW_MAX_BTN_H && button) {
                window->maximized = !window->maximized;
                if (window->maximized) {
                    // Maximize window to full screen
                    window->x = 0;
                    window->y = GUI_TASKBAR_HEIGHT;
                    window->width = VGA_WIDTH;
                    window->height = VGA_HEIGHT - GUI_TASKBAR_HEIGHT - GUI_TASKBAR_HEIGHT;
                }
                return;
            }
        }
        
        // Check if click is in title bar
        if (x >= window->x && x < window->x + window->width &&
            y >= window->y - GUI_WINDOW_TITLE_HEIGHT && y < window->y) {
            
            // Focus window
            if (focused_window != window) {
                if (focused_window) focused_window->focused = 0;
                focused_window = window;
                window->focused = 1;
            }
            
            // Start dragging
            if (button) {
                window->dragging = 1;
                window->drag_x = x - window->x;
                window->drag_y = y - window->y;
                dragging = 1;
            } else {
                window->dragging = 0;
                dragging = 0;
            }
            
            return;
        }
        
        // Handle window dragging
        if (window->dragging && dragging) {
            window->x = x - window->drag_x;
            window->y = y - window->drag_y;
            
            // Keep window on screen
            if (window->x < 0) window->x = 0;
            if (window->y < GUI_WINDOW_TITLE_HEIGHT) window->y = GUI_WINDOW_TITLE_HEIGHT;
            if (window->x + window->width > VGA_WIDTH) window->x = VGA_WIDTH - window->width;
            if (window->y + window->height > VGA_HEIGHT - GUI_TASKBAR_HEIGHT) {
                window->y = VGA_HEIGHT - GUI_TASKBAR_HEIGHT - window->height;
            }
            
            return;
        }
        
        // Check if click is in window content
        if (x >= window->x && x < window->x + window->width &&
            y >= window->y && y < window->y + window->height) {
            
            // Focus window
            if (focused_window != window) {
                if (focused_window) focused_window->focused = 0;
                focused_window = window;
                window->focused = 1;
            }
            
            // Handle window content clicks
            // This would be extended for specific applications
            
            return;
        }
    }
    
    // If no window handled the click, clear focus
    if (focused_window) {
        focused_window->focused = 0;
        focused_window = NULL;
    }
}

void gui_handle_keyboard(char key) {
    if (focused_window && focused_window->visible && !focused_window->minimized) {
        // Handle keyboard input for focused window
        // This would be extended for specific applications
        
        // For now, just echo to window buffer if it's a textbox
        if (key >= 32 && key <= 126) { // Printable characters
            // This is a simplified implementation
            // Real implementation would depend on the active control
        }
    }
}

gui_button_t* gui_create_button(gui_window_t* window, int x, int y, int width, int height, const char* text, void (*onclick)(void)) {
    if (!window || !window->buffer) return NULL;
    
    gui_button_t* button = (gui_button_t*)kmalloc(sizeof(gui_button_t));
    if (!button) return NULL;
    
    button->x = x;
    button->y = y;
    button->width = width;
    button->height = height;
    button->onclick = onclick;
    
    // Copy text
    int i = 0;
    while (text[i] && i < 127) {
        button->text[i] = text[i];
        i++;
    }
    button->text[i] = '\0';
    
    // Draw button in window buffer
    uint8_t button_color = make_color(COLOR_BLACK, COLOR_LIGHT_GREY);
    for (int py = 0; py < height && py < window->height; py++) {
        for (int px = 0; px < width && px < window->width; px++) {
            if (x + px < window->width && y + py < window->height) {
                ((uint16_t*)window->buffer)[(y + py) * window->width + (x + px)] = make_vgaentry(' ', button_color);
            }
        }
    }
    
    // Draw button text (centered)
    int text_len = gui_strlen(text);
    int text_x = (width - text_len) / 2;
    int text_y = height / 2;
    
    for (int j = 0; text[j]; j++) {
        if (text_x + j < width && text_y < height) {
            ((uint16_t*)window->buffer)[(y + text_y) * window->width + (x + text_x + j)] = make_vgaentry(text[j], button_color);
        }
    }
    
    return button;
}

gui_textbox_t* gui_create_textbox(gui_window_t* window, int x, int y, int width, int height, const char* text, int max_length) {
    if (!window || !window->buffer) return NULL;
    
    gui_textbox_t* textbox = (gui_textbox_t*)kmalloc(sizeof(gui_textbox_t));
    if (!textbox) return NULL;
    
    textbox->x = x;
    textbox->y = y;
    textbox->width = width;
    textbox->height = height;
    textbox->cursor_pos = 0;
    textbox->max_length = max_length;
    
    // Copy initial text
    int i = 0;
    while (text[i] && i < 255) {
        textbox->text[i] = text[i];
        i++;
    }
    textbox->text[i] = '\0';
    
    // Draw textbox in window buffer
    uint8_t textbox_color = make_color(COLOR_BLACK, COLOR_WHITE);
    uint8_t border_color = make_color(COLOR_BLACK, COLOR_DARK_GREY);
    
    // Draw border
    for (int py = 0; py < height + 2 && py < window->height; py++) {
        for (int px = 0; px < width + 2 && px < window->width; px++) {
            if (x + px < window->width && y + py < window->height) {
                ((uint16_t*)window->buffer)[(y + py) * window->width + (x + px)] = make_vgaentry(' ', border_color);
            }
        }
    }
    
    // Draw text area
    for (int py = 0; py < height && py < window->height; py++) {
        for (int px = 0; px < width && px < window->width; px++) {
            if (x + px + 1 < window->width && y + py + 1 < window->height) {
                ((uint16_t*)window->buffer)[(y + py + 1) * window->width + (x + px + 1)] = make_vgaentry(' ', textbox_color);
            }
        }
    }
    
    // Draw initial text
    for (int j = 0; text[j]; j++) {
        if (j < width - 2 && 1 < height - 1) {
            ((uint16_t*)window->buffer)[(y + 1) * window->width + (x + 1 + j)] = make_vgaentry(text[j], textbox_color);
        }
    }
    
    return textbox;
}

void gui_render(void) {
    // This function is now handled by gui_desktop_render()
    // It's kept for compatibility but the desktop handles all rendering
}