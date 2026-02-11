// display.h
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <string.h>
#include "driver/i2c.h"
#include "font.h"

// Display type selection
#define DISPLAY_SSD1306 0
#define DISPLAY_SH1107  1

#ifndef DISPLAY_TYPE
#define DISPLAY_TYPE DISPLAY_SH1107
#endif

#define DISPLAY_ADDR 0x3C
#define DISPLAY_CMD  0x00
#define DISPLAY_DATA 0x40

#if DISPLAY_TYPE == DISPLAY_SSD1306
#define WIDTH 128
#define HEIGHT 64
#else
#define WIDTH 128
#define HEIGHT 128
#endif

// Framebuffer and state
static uint8_t framebuffer[WIDTH * HEIGHT / 8];
static int16_t cursor_x = 0;
static int16_t cursor_y = 0;
static FontType current_font = FONT_TOMTHUMB;

// Dirty region tracking
static uint8_t dirty_x0 = 0, dirty_y0 = 0;
static uint8_t dirty_x1 = WIDTH-1, dirty_y1 = HEIGHT-1;
static uint8_t is_dirty = 0;

// Mark region as dirty
static inline void mark_dirty(int16_t x, int16_t y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    if (!is_dirty) {
        dirty_x0 = dirty_x1 = x;
        dirty_y0 = dirty_y1 = y;
        is_dirty = 1;
    } else {
        if (x < dirty_x0) dirty_x0 = x;
        if (x > dirty_x1) dirty_x1 = x;
        if (y < dirty_y0) dirty_y0 = y;
        if (y > dirty_y1) dirty_y1 = y;
    }
}

static inline void reset_dirty(void) {
    is_dirty = 0;
}

// Hardware communication
static inline void display_write_cmd(uint8_t cmd) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (DISPLAY_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, DISPLAY_CMD, true);
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(I2C_NUM_0, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
}

static inline void display_init(void) {
#if DISPLAY_TYPE == DISPLAY_SSD1306
    // SSD1306 initialization
    display_write_cmd(0xAE);
    display_write_cmd(0xD5); display_write_cmd(0x80);
    display_write_cmd(0xA8); display_write_cmd(0x3F);
    display_write_cmd(0xD3); display_write_cmd(0x00);
    display_write_cmd(0x40);
    display_write_cmd(0x8D); display_write_cmd(0x14);
    display_write_cmd(0x20); display_write_cmd(0x00);
    display_write_cmd(0xA1);
    display_write_cmd(0xC8);
    display_write_cmd(0xDA); display_write_cmd(0x12);
    display_write_cmd(0x81); display_write_cmd(0xCF);
    display_write_cmd(0xD9); display_write_cmd(0xF1);
    display_write_cmd(0xDB); display_write_cmd(0x40);
    display_write_cmd(0xA4);
    display_write_cmd(0xA6);
    display_write_cmd(0xAF);
#else
    display_write_cmd(0xAE);
display_write_cmd(0xDC); display_write_cmd(0x00);
display_write_cmd(0x81); display_write_cmd(0x2F);
display_write_cmd(0x20); // Page addressing mode
display_write_cmd(0xA0); // Segment remap normal
display_write_cmd(0xC0); // COM normal
display_write_cmd(0xA8); display_write_cmd(0x7F);
display_write_cmd(0xD3); display_write_cmd(0x00); // offset 0
display_write_cmd(0xD5); display_write_cmd(0x51);
display_write_cmd(0xD9); display_write_cmd(0x22);
display_write_cmd(0xDB); display_write_cmd(0x35);
display_write_cmd(0xA4);
display_write_cmd(0xA6);
display_write_cmd(0xAF);
#endif
}

// Full display update
static inline void display_show(void) {
#if DISPLAY_TYPE == DISPLAY_SSD1306
    display_write_cmd(0x21); display_write_cmd(0); display_write_cmd(127);
    display_write_cmd(0x22); display_write_cmd(0); display_write_cmd(7);
    
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (DISPLAY_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, DISPLAY_DATA, true);
    i2c_master_write(handle, framebuffer, sizeof(framebuffer), true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(I2C_NUM_0, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
#else
    // SH1107 page-by-page update (FIXED column offset)
   for (uint8_t page = 0; page < 16; page++) {
    display_write_cmd(0xB0 + page);  // Set page
    display_write_cmd(0x00);          // Lower column start = 0
    display_write_cmd(0x10);          // Upper column start = 0
    
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (DISPLAY_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, DISPLAY_DATA, true);
    
    for (uint8_t chunk = 0; chunk < 4; chunk++) {
        i2c_master_write(handle, &framebuffer[page * WIDTH + chunk * 32], 32, true);
    }
    
    i2c_master_stop(handle);
    i2c_master_cmd_begin(I2C_NUM_0, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
}
#endif
    reset_dirty();
}

// Partial display update (fast!)
static inline void display_show_partial(void) {
    if (!is_dirty) return;
    
    uint8_t col_start = dirty_x0;
    uint8_t col_end = dirty_x1;
    uint8_t page_start = dirty_y0 / 8;
    uint8_t page_end = dirty_y1 / 8;
    
#if DISPLAY_TYPE == DISPLAY_SSD1306
    display_write_cmd(0x21); 
    display_write_cmd(col_start); 
    display_write_cmd(col_end);
    display_write_cmd(0x22); 
    display_write_cmd(page_start); 
    display_write_cmd(page_end);
    
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (DISPLAY_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, DISPLAY_DATA, true);
    
    for (uint8_t page = page_start; page <= page_end; page++) {
        i2c_master_write(handle, &framebuffer[page * WIDTH + col_start], 
                        col_end - col_start + 1, true);
    }
    
    i2c_master_stop(handle);
    i2c_master_cmd_begin(I2C_NUM_0, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
#else
    // SH1107 partial update
   for (uint8_t page = 0; page < 16; page++) {
    display_write_cmd(0xB0 + page);
    display_write_cmd(0x00);
    display_write_cmd(0x10);
    
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (DISPLAY_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, DISPLAY_DATA, true);
    i2c_master_write(handle, &framebuffer[page * WIDTH], 128, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(I2C_NUM_0, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
}
#endif
    reset_dirty();
}

// Core drawing
static inline void display_pixel(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    mark_dirty(x, y);
    if (color) framebuffer[x + (y/8)*WIDTH] |= (1 << (y&7));
    else framebuffer[x + (y/8)*WIDTH] &= ~(1 << (y&7));
}

static inline void display_clear(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
    dirty_x0 = 0; dirty_y0 = 0;
    dirty_x1 = WIDTH-1; dirty_y1 = HEIGHT-1;
    is_dirty = 1;
}

// Fast primitives
static inline void draw_hline(int16_t x, int16_t y, int16_t w, uint8_t color) {
    if (y < 0 || y >= HEIGHT || x >= WIDTH) return;
    int16_t x_end = x + w;
    if (x < 0) x = 0;
    if (x_end > WIDTH) x_end = WIDTH;
    
    mark_dirty(x, y);
    mark_dirty(x_end - 1, y);
    
    uint16_t page = (y / 8) * WIDTH;
    uint8_t mask = 1 << (y & 7);
    
    if (color) {
        for (int16_t i = x; i < x_end; i++) {
            framebuffer[page + i] |= mask;
        }
    } else {
        mask = ~mask;
        for (int16_t i = x; i < x_end; i++) {
            framebuffer[page + i] &= mask;
        }
    }
}

static inline void draw_vline(int16_t x, int16_t y, int16_t h, uint8_t color) {
    for (int16_t i = y; i < y + h; i++) {
        display_pixel(x, i, color);
    }
}

static inline void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    if (y0 == y1) {
        if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
        draw_hline(x0, y0, x1 - x0 + 1, color);
        return;
    }
    if (x0 == x1) {
        if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
        draw_vline(x0, y0, y1 - y0 + 1, color);
        return;
    }
    
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t sx = dx > 0 ? 1 : -1;
    int16_t sy = dy > 0 ? 1 : -1;
    dx = dx > 0 ? dx : -dx;
    dy = dy > 0 ? dy : -dy;
    int16_t err = dx - dy;
    
    while (1) {
        display_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static inline void draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    draw_hline(x, y, w, color);
    draw_hline(x, y + h - 1, w, color);
    draw_vline(x, y, h, color);
    draw_vline(x + w - 1, y, h, color);
}

static inline void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WIDTH) w = WIDTH - x;
    if (y + h > HEIGHT) h = HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    
    mark_dirty(x, y);
    mark_dirty(x + w - 1, y + h - 1);
    
    uint8_t page_start = y / 8;
    uint8_t page_end = (y + h - 1) / 8;
    
    if ((y & 7) == 0 && ((y + h) & 7) == 0) {
        uint8_t fill = color ? 0xFF : 0x00;
        for (uint8_t page = page_start; page <= page_end; page++) {
            memset(&framebuffer[page * WIDTH + x], fill, w);
        }
    } else {
        for (int16_t i = x; i < x + w; i++) {
            for (int16_t j = y; j < y + h; j++) {
                uint16_t idx = i + (j / 8) * WIDTH;
                uint8_t mask = 1 << (j & 7);
                if (color) framebuffer[idx] |= mask;
                else framebuffer[idx] &= ~mask;
            }
        }
    }
}

static inline void draw_circle(int16_t x0, int16_t y0, int16_t r, uint8_t color) {
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        display_pixel(x0 + x, y0 + y, color);
        display_pixel(x0 + y, y0 + x, color);
        display_pixel(x0 - y, y0 + x, color);
        display_pixel(x0 - x, y0 + y, color);
        display_pixel(x0 - x, y0 - y, color);
        display_pixel(x0 - y, y0 - x, color);
        display_pixel(x0 + y, y0 - x, color);
        display_pixel(x0 + x, y0 - y, color);
        
        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static inline void fill_circle(int16_t x0, int16_t y0, int16_t r, uint8_t color) {
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        draw_hline(x0 - x, y0 + y, 2 * x + 1, color);
        draw_hline(x0 - x, y0 - y, 2 * x + 1, color);
        draw_hline(x0 - y, y0 + x, 2 * y + 1, color);
        draw_hline(x0 - y, y0 - x, 2 * y + 1, color);
        
        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static inline void invert_display(void) {
    for (uint16_t i = 0; i < sizeof(framebuffer); i++) {
        framebuffer[i] = ~framebuffer[i];
    }
    dirty_x0 = 0; dirty_y0 = 0;
    dirty_x1 = WIDTH-1; dirty_y1 = HEIGHT-1;
    is_dirty = 1;
}

static inline void set_contrast(uint8_t contrast) {
    display_write_cmd(0x81);
    display_write_cmd(contrast);
}

static inline void draw_bitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h) {
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (bitmap[i + (j / 8) * w] & (1 << (j & 7))) {
                display_pixel(x + i, y + j, 1);
            }
        }
    }
}

static inline void draw_char_gfx(int16_t x, int16_t y, char c, const GFXfont *font) {
    if (c < font->first || c > font->last) return;
    
    const GFXglyph *glyph = &font->glyph[c - font->first];
    const uint8_t *bitmap = font->bitmap + glyph->bitmapOffset;
    
    mark_dirty(x + glyph->xOffset, y + glyph->yOffset);
    mark_dirty(x + glyph->xOffset + glyph->width - 1, y + glyph->yOffset + glyph->height - 1);
    
    uint16_t bit_idx = 0;
    for (uint8_t yy = 0; yy < glyph->height; yy++) {
        for (uint8_t xx = 0; xx < glyph->width; xx++) {
            if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                int16_t px = x + glyph->xOffset + xx;
                int16_t py = y + glyph->yOffset + yy;
                if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                    framebuffer[px + (py/8)*WIDTH] |= (1 << (py&7));
                }
            }
            bit_idx++;
        }
    }
}

static inline void draw_char(int16_t x, int16_t y, char c, FontType font_type) {
    switch(font_type) {
        case FONT_TOMTHUMB:
            draw_char_gfx(x, y, c, &TomThumb);
            break;
        case FONT_FREEMONO_9PT:
            draw_char_gfx(x, y, c, &FreeMono9pt7b);
            break;
    }
}

static inline void draw_string(int16_t x, int16_t y, const char *str, FontType font_type) {
    int16_t cursor_x = x;
    const GFXfont *font = NULL;
    
    switch(font_type) {
        case FONT_TOMTHUMB:
            font = &TomThumb;
            break;
        case FONT_FREEMONO_9PT:
            font = &FreeMono9pt7b;
            break;
        default:
            return;
    }
    
    while (*str) {
        if (*str >= font->first && *str <= font->last) {
            const GFXglyph *glyph = &font->glyph[*str - font->first];
            draw_char(cursor_x, y, *str, font_type);
            cursor_x += glyph->xAdvance;
        }
        str++;
    }
}

static inline void set_cursor(int16_t x, int16_t y) {
    cursor_x = x;
    cursor_y = y;
}

static inline void set_font(FontType font) {
    current_font = font;
}

static inline void print(const char *str) {
    const GFXfont *font = NULL;
    
    switch(current_font) {
        case FONT_TOMTHUMB:
            font = &TomThumb;
            break;
        case FONT_FREEMONO_9PT:
            font = &FreeMono9pt7b;
            break;
        default:
            return;
    }
    
    while (*str) {
        if (*str >= font->first && *str <= font->last) {
            const GFXglyph *glyph = &font->glyph[*str - font->first];
            draw_char(cursor_x, cursor_y, *str, current_font);
            cursor_x += glyph->xAdvance;
        }
        str++;
    }
}

static inline void println(const char *str) {
    print(str);
    cursor_x = 0;
    cursor_y += current_font == FONT_TOMTHUMB ? 8 : 20;
}

// Legacy aliases for compatibility
#define ssd1306_init display_init
#define ssd1306_display display_show
#define ssd1306_display_partial display_show_partial
#define ssd1306_pixel display_pixel
#define ssd1306_clear display_clear

#endif
