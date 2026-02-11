// rotary_text_input.h - Text input using only rotary encoder
#ifndef ROTARY_TEXT_INPUT_H
#define ROTARY_TEXT_INPUT_H

#include <stdint.h>
#include <string.h>
#include "drivers/display.h"
#include "drivers/rotary.h"

// Character set for password input
static const char CHARSET[] = 
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!@#$%^&*()-_=+[]{}|;:,.<>?/ "
    "\x7F";  // DEL character at end

#define CHARSET_LEN (sizeof(CHARSET) - 1)
#define CHAR_DEL (CHARSET_LEN - 1)

// Text input state
typedef struct {
    char buffer[64];
    uint8_t length;
    uint8_t char_index;  // Current character in charset
    uint8_t mode;        // 0 = select char, 1 = confirm/next
} TextInput;

static TextInput text_input;

// Initialize text input
static inline void text_input_init(const char *initial) {
    memset(text_input.buffer, 0, sizeof(text_input.buffer));
    if (initial) {
        strncpy(text_input.buffer, initial, sizeof(text_input.buffer) - 1);
        text_input.length = strlen(text_input.buffer);
    } else {
        text_input.length = 0;
    }
    text_input.char_index = 0;
    text_input.mode = 0;
}

// Get current character being selected
static inline char text_input_current_char(void) {
    return CHARSET[text_input.char_index];
}

// Handle rotary encoder input
// Returns: 1 if done (user confirmed), 0 if still editing, -1 if cancelled
static inline int8_t text_input_update(Rotary *encoder) {
    int8_t dir = rotary_read(encoder);
    
    if (text_input.mode == 0) {
        // Mode 0: Selecting character
        if (dir > 0) {
            text_input.char_index = (text_input.char_index + 1) % CHARSET_LEN;
        } else if (dir < 0) {
            text_input.char_index = (text_input.char_index + CHARSET_LEN - 1) % CHARSET_LEN;
        }
        
        if (rotary_button_pressed(encoder)) {
            // Short press: confirm character
            text_input.mode = 1;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    } else {
        // Mode 1: Confirm action (add char, delete, or done)
        if (dir != 0) {
            // Any rotation: cancel and go back to char selection
            text_input.mode = 0;
        }
        
        if (rotary_button_pressed(encoder)) {
            // Short press: execute action
            char current = text_input_current_char();
            
            if (current == '\x7F') {
                // DEL: remove last character
                if (text_input.length > 0) {
                    text_input.length--;
                    text_input.buffer[text_input.length] = '\0';
                }
            } else if (current == ' ' && text_input.length == 0) {
                // Space at start = DONE
                return 1;
            } else {
                // Add character
                if (text_input.length < sizeof(text_input.buffer) - 1) {
                    text_input.buffer[text_input.length] = current;
                    text_input.length++;
                    text_input.buffer[text_input.length] = '\0';
                }
            }
            
            text_input.mode = 0;
            text_input.char_index = 0;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
    
    // Hold detection for cancel
    static uint32_t hold_start = 0;
    if (gpio_get_level((gpio_num_t)encoder->pin_sw) == 0) {
        if (hold_start == 0) {
            hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
        } else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - hold_start) > 1000) {
            return -1;  // Cancelled
        }
    } else {
        hold_start = 0;
    }
    
    return 0;  // Still editing
}

// Draw the text input UI
static inline void text_input_draw(const char *title) {
    display_clear();
    set_font(FONT_TOMTHUMB);
    
    // Title bar
    fill_rect(0, 0, WIDTH, 10, 1);
    set_cursor(2, 7);
    
    // Draw title inverted
    const char *t = title;
    int16_t tx = 2;
    while (*t) {
        if (*t >= TomThumb.first && *t <= TomThumb.last) {
            const GFXglyph *g = &TomThumb.glyph[*t - TomThumb.first];
            const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
            
            uint16_t bit_idx = 0;
            for (uint8_t yy = 0; yy < g->height; yy++) {
                for (uint8_t xx = 0; xx < g->width; xx++) {
                    if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                        int16_t px = tx + g->xOffset + xx;
                        int16_t py = 7 + g->yOffset + yy;
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
    
    draw_hline(0, 10, WIDTH, 1);
    
    // Current text (scrolling if needed)
    set_cursor(2, 20);
    uint8_t display_start = 0;
    if (text_input.length > 20) {
        display_start = text_input.length - 20;
    }
    
    char display_text[22];
    strncpy(display_text, text_input.buffer + display_start, 20);
    display_text[20] = '\0';
    print(display_text);
    
    // Cursor
    draw_vline(2 + (text_input.length - display_start) * 4, 21, 6, 1);
    
    draw_hline(0, 28, WIDTH, 1);
    
    // Current character selection (BIG)
    char current = text_input_current_char();
    
    if (text_input.mode == 0) {
        // Selecting mode - show character large
        set_cursor(WIDTH/2 - 6, 50);
        set_font(FONT_FREEMONO_9PT);
        
        if (current == '\x7F') {
            set_font(FONT_TOMTHUMB);
            set_cursor(WIDTH/2 - 8, 50);
            print("DEL");
        } else if (current == ' ' && text_input.length == 0) {
            set_font(FONT_TOMTHUMB);
            set_cursor(WIDTH/2 - 12, 50);
            print("DONE");
        } else {
            char str[2] = {current, '\0'};
            print(str);
        }
    } else {
        // Confirm mode - show with box
        draw_rect(WIDTH/2 - 15, 35, 30, 24, 1);
        set_cursor(WIDTH/2 - 6, 50);
        set_font(FONT_FREEMONO_9PT);
        
        if (current == '\x7F') {
            set_font(FONT_TOMTHUMB);
            set_cursor(WIDTH/2 - 8, 50);
            print("DEL");
        } else if (current == ' ' && text_input.length == 0) {
            set_font(FONT_TOMTHUMB);
            set_cursor(WIDTH/2 - 12, 50);
            print("DONE");
        } else {
            char str[2] = {current, '\0'};
            print(str);
        }
        
        set_font(FONT_TOMTHUMB);
        set_cursor(WIDTH/2 - 18, 68);
        print("Press=Add");
    }
    
    // Help text
    set_font(FONT_TOMTHUMB);
    set_cursor(2, 80);
    if (text_input.mode == 0) {
        print("Turn=Pick");
    } else {
        print("Turn=Cancel");
    }
    
    set_cursor(2, 88);
    print("Press=Select");
    
    set_cursor(2, 96);
    print("Hold=Cancel");
    
    // Show character position in set
    set_cursor(2, 104);
    char pos[16];
    snprintf(pos, sizeof(pos), "%d/%d", text_input.char_index + 1, CHARSET_LEN);
    print(pos);
    
    // Alphabet hint (show nearby chars)
    set_cursor(2, 112);
    char hint[32];
    uint8_t hint_pos = 0;
    for (int8_t i = -3; i <= 3; i++) {
        int16_t idx = text_input.char_index + i;
        if (idx < 0) idx += CHARSET_LEN;
        if (idx >= CHARSET_LEN) idx -= CHARSET_LEN;
        
        char c = CHARSET[idx];
        if (c == '\x7F') c = 'X';
        
        hint[hint_pos++] = (i == 0) ? '[' : ' ';
        hint[hint_pos++] = c;
        hint[hint_pos++] = (i == 0) ? ']' : ' ';
    }
    hint[hint_pos] = '\0';
    print(hint);
    
    display_show();
}

// Convenience function: get text input with title
// Returns: 1 if success (text in buffer), 0 if cancelled
static inline uint8_t text_input_get(Rotary *encoder, const char *title, 
                                      char *result, size_t result_size, 
                                      const char *initial) {
    text_input_init(initial);
    
    while (1) {
        text_input_draw(title);
        
        int8_t status = text_input_update(encoder);
        
        if (status == 1) {
            // Done
            strncpy(result, text_input.buffer, result_size - 1);
            result[result_size - 1] = '\0';
            return 1;
        } else if (status == -1) {
            // Cancelled
            return 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

#endif
