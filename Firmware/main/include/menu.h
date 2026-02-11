// menu.h - enhanced version
#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "drivers/display.h"

#define MAX_MENU_ITEMS 8
#define MENU_ITEM_HEIGHT 12
#define MENU_SCROLL_MARGIN 2
#define TITLE_BAR_HEIGHT 12
#define STATUS_BAR_HEIGHT 10

typedef struct {
    const char *label;
    const char *icon;  // Single char icon/symbol
    void (*action)(void);
} MenuItem;

typedef struct {
    const char *title;
    MenuItem items[MAX_MENU_ITEMS];
    uint8_t item_count;
    uint8_t selected;
    uint8_t scroll_offset;
    int16_t anim_offset;  // For smooth scrolling
} Menu;

extern Menu *current_menu;
extern char status_text[32];

static inline void menu_init(Menu *menu, const char *title) {
    menu->title = title;
    menu->item_count = 0;
    menu->selected = 0;
    menu->scroll_offset = 0;
    menu->anim_offset = 0;
}

static inline void menu_add_item(Menu *menu, const char *label, void (*action)(void)) {
    if (menu->item_count >= MAX_MENU_ITEMS) return;
    menu->items[menu->item_count].label = label;
    menu->items[menu->item_count].icon = ">"; // Default icon
    menu->items[menu->item_count].action = action;
    menu->item_count++;
}

static inline void menu_add_item_icon(Menu *menu, const char *icon, const char *label, void (*action)(void)) {
    if (menu->item_count >= MAX_MENU_ITEMS) return;
    menu->items[menu->item_count].label = label;
    menu->items[menu->item_count].icon = icon;
    menu->items[menu->item_count].action = action;
    menu->item_count++;
}

static inline void menu_set_status(const char *text) {
    strncpy(status_text, text, sizeof(status_text) - 1);
    status_text[sizeof(status_text) - 1] = '\0';
}

static inline void menu_set_active(Menu *menu) {
    current_menu = menu;
}

static inline void menu_next(void) {
    if (!current_menu) return;
    if (current_menu->selected < current_menu->item_count - 1) {
        current_menu->selected++;
        
        uint8_t visible_items = (HEIGHT - TITLE_BAR_HEIGHT - STATUS_BAR_HEIGHT - 2) / MENU_ITEM_HEIGHT;
        if (current_menu->selected >= current_menu->scroll_offset + visible_items - MENU_SCROLL_MARGIN) {
            current_menu->scroll_offset++;
        }
    }
}

static inline void menu_prev(void) {
    if (!current_menu) return;
    if (current_menu->selected > 0) {
        current_menu->selected--;
        
        if (current_menu->selected < current_menu->scroll_offset + MENU_SCROLL_MARGIN) {
            if (current_menu->scroll_offset > 0) current_menu->scroll_offset--;
        }
    }
}

static inline void menu_select(void) {
    if (!current_menu) return;
    if (current_menu->items[current_menu->selected].action) {
        current_menu->items[current_menu->selected].action();
    }
}

static inline uint8_t get_text_width(const char *text, FontType font) {
    uint8_t width = 0;
    const GFXfont *gfx_font = (font == FONT_TOMTHUMB) ? &TomThumb : &FreeMono9pt7b;
    
    while (*text) {
        if (*text >= gfx_font->first && *text <= gfx_font->last) {
            const GFXglyph *glyph = &gfx_font->glyph[*text - gfx_font->first];
            width += glyph->xAdvance;
        }
        text++;
    }
    return width;
}

static inline void menu_draw(void) {
    if (!current_menu) return;
    
    display_clear();
    
    // ===== Title bar with shadow effect =====
    fill_rect(0, 0, WIDTH, TITLE_BAR_HEIGHT, 1);
    
    // Draw title centered
    set_font(FONT_TOMTHUMB);
    uint8_t title_width = get_text_width(current_menu->title, FONT_TOMTHUMB);
    uint8_t title_x = (WIDTH - title_width) / 2;
    
    const char *t = current_menu->title;
    int16_t tx = title_x;
    while (*t) {
        if (*t >= TomThumb.first && *t <= TomThumb.last) {
            const GFXglyph *g = &TomThumb.glyph[*t - TomThumb.first];
            const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
            
            uint16_t bit_idx = 0;
            for (uint8_t yy = 0; yy < g->height; yy++) {
                for (uint8_t xx = 0; xx < g->width; xx++) {
                    if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                        int16_t px = tx + g->xOffset + xx;
                        int16_t py = 8 + g->yOffset + yy;
                        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                            framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                        }
                    }
                    bit_idx++;
                }
            }
            tx += g->xAdvance;
        }
        t++;
    }
    
    // Divider line under title
    draw_hline(0, TITLE_BAR_HEIGHT, WIDTH, 1);
    
    // ===== Menu items =====
    uint8_t visible_items = (HEIGHT - TITLE_BAR_HEIGHT - STATUS_BAR_HEIGHT - 2) / MENU_ITEM_HEIGHT;
    uint8_t y = TITLE_BAR_HEIGHT + 2;
    
    for (uint8_t i = current_menu->scroll_offset; 
         i < current_menu->item_count && i < current_menu->scroll_offset + visible_items; 
         i++) {
        
        if (i == current_menu->selected) {
            // Draw selection with rounded corners effect
            fill_rect(2, y, WIDTH - 4, MENU_ITEM_HEIGHT, 1);
            
            // Draw icon
            set_cursor(6, y + 8);
            const char *icon_str = current_menu->items[i].icon;
            while (*icon_str) {
                if (*icon_str >= TomThumb.first && *icon_str <= TomThumb.last) {
                    const GFXglyph *g = &TomThumb.glyph[*icon_str - TomThumb.first];
                    const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
                    
                    uint16_t bit_idx = 0;
                    for (uint8_t yy = 0; yy < g->height; yy++) {
                        for (uint8_t xx = 0; xx < g->width; xx++) {
                            if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                                int16_t px = cursor_x + g->xOffset + xx;
                                int16_t py = cursor_y + g->yOffset + yy;
                                if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                                    framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                                }
                            }
                            bit_idx++;
                        }
                    }
                    cursor_x += g->xAdvance;
                }
                icon_str++;
            }
            
            // Draw label (inverted)
            set_cursor(cursor_x + 2, y + 8);
            const char *s = current_menu->items[i].label;
            while (*s) {
                if (*s >= TomThumb.first && *s <= TomThumb.last) {
                    const GFXglyph *g = &TomThumb.glyph[*s - TomThumb.first];
                    const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
                    
                    uint16_t bit_idx = 0;
                    for (uint8_t yy = 0; yy < g->height; yy++) {
                        for (uint8_t xx = 0; xx < g->width; xx++) {
                            if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                                int16_t px = cursor_x + g->xOffset + xx;
                                int16_t py = cursor_y + g->yOffset + yy;
                                if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                                    framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                                }
                            }
                            bit_idx++;
                        }
                    }
                    cursor_x += g->xAdvance;
                }
                s++;
            }
        } else {
            // Normal item
            set_cursor(6, y + 8);
            print(current_menu->items[i].icon);
            set_cursor(cursor_x + 2, y + 8);
            print(current_menu->items[i].label);
        }
        
        y += MENU_ITEM_HEIGHT;
    }
    
    // ===== Scroll indicators =====
    if (current_menu->scroll_offset > 0) {
        // Up arrow
        fill_rect(WIDTH - 8, TITLE_BAR_HEIGHT + 2, 6, 6, 1);
        display_pixel(WIDTH - 5, TITLE_BAR_HEIGHT + 3, 0);
        display_pixel(WIDTH - 6, TITLE_BAR_HEIGHT + 4, 0);
        display_pixel(WIDTH - 4, TITLE_BAR_HEIGHT + 4, 0);
        display_pixel(WIDTH - 7, TITLE_BAR_HEIGHT + 5, 0);
        display_pixel(WIDTH - 3, TITLE_BAR_HEIGHT + 5, 0);
    }
    if (current_menu->scroll_offset + visible_items < current_menu->item_count) {
        // Down arrow
        fill_rect(WIDTH - 8, HEIGHT - STATUS_BAR_HEIGHT - 8, 6, 6, 1);
        display_pixel(WIDTH - 7, HEIGHT - STATUS_BAR_HEIGHT - 7, 0);
        display_pixel(WIDTH - 3, HEIGHT - STATUS_BAR_HEIGHT - 7, 0);
        display_pixel(WIDTH - 6, HEIGHT - STATUS_BAR_HEIGHT - 6, 0);
        display_pixel(WIDTH - 4, HEIGHT - STATUS_BAR_HEIGHT - 6, 0);
        display_pixel(WIDTH - 5, HEIGHT - STATUS_BAR_HEIGHT - 5, 0);
    }
    
    // ===== Status bar =====
    draw_hline(0, HEIGHT - STATUS_BAR_HEIGHT, WIDTH, 1);
    
    // Item counter (left)
    set_cursor(2, HEIGHT - 3);
    char counter[16];
    snprintf(counter, sizeof(counter), "%d/%d", current_menu->selected + 1, current_menu->item_count);
    print(counter);
    
    // Status text (right aligned)
    if (status_text[0]) {
        uint8_t status_width = get_text_width(status_text, FONT_TOMTHUMB);
        set_cursor(WIDTH - status_width - 2, HEIGHT - 3);
        print(status_text);
    }
    
    display_show();
}

#endif
