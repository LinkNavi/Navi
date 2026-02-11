// keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "driver/i2c.h"
#include "bytes.h"

#define CARDKB_ADDR 0x5F
#define CARDKB_BUFFER_SIZE 32

// Special keys
#define KEY_NONE 0x00
#define KEY_ENTER 0x0D
#define KEY_TAB 0x09
#define KEY_BACKSPACE 0x08
#define KEY_ESC 0x1B
#define KEY_UP 0xB4
#define KEY_DOWN 0xB5
#define KEY_LEFT 0xB6
#define KEY_RIGHT 0xB7
#define KEY_DEL 0x7F

// Keyboard state
typedef struct {
    ByteBuffer buffer;
    uint8_t buffer_data[CARDKB_BUFFER_SIZE];
    uint8_t last_key;
    uint32_t last_press_time;
    uint8_t repeat_enabled;
    uint16_t repeat_delay;
    uint16_t repeat_rate;
} Keyboard;

static Keyboard kbd;

// Initialize keyboard
static inline void keyboard_init(void) {
    byte_buffer_init(&kbd.buffer, kbd.buffer_data, CARDKB_BUFFER_SIZE);
    kbd.last_key = KEY_NONE;
    kbd.last_press_time = 0;
    kbd.repeat_enabled = 1;
    kbd.repeat_delay = 500;  // ms before repeat starts
    kbd.repeat_rate = 100;   // ms between repeats
}

// Read raw key from CardKB
static inline uint8_t cardkb_read_raw(void) {
    uint8_t key = KEY_NONE;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CARDKB_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &key, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        return KEY_NONE;
    }
    
    return key;
}

// Check if keyboard is connected
static inline uint8_t keyboard_is_connected(void) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CARDKB_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(cmd);
    
    return ret == ESP_OK;
}

// Update keyboard (call in loop)
static inline void keyboard_update(void) {
    uint8_t key = cardkb_read_raw();
    uint32_t now = millis();
    
    if (key != KEY_NONE) {
        // New key press
        if (key != kbd.last_key) {
            byte_buffer_push(&kbd.buffer, key);
            kbd.last_key = key;
            kbd.last_press_time = now;
        } 
        // Key repeat
        else if (kbd.repeat_enabled) {
            uint32_t hold_time = now - kbd.last_press_time;
            if (hold_time > kbd.repeat_delay) {
                uint32_t repeat_time = hold_time - kbd.repeat_delay;
                if (repeat_time % kbd.repeat_rate < 20) {  // Debounce
                    byte_buffer_push(&kbd.buffer, key);
                }
            }
        }
    } else {
        // Key released
        kbd.last_key = KEY_NONE;
    }
}

// Get next key from buffer
static inline uint8_t keyboard_get_key(void) {
    uint8_t key;
    if (byte_buffer_pop(&kbd.buffer, &key)) {
        return key;
    }
    return KEY_NONE;
}

// Peek at next key without removing
static inline uint8_t keyboard_peek_key(void) {
    uint8_t key;
    if (byte_buffer_peek(&kbd.buffer, &key)) {
        return key;
    }
    return KEY_NONE;
}

// Check if keys available
static inline uint16_t keyboard_available(void) {
    return byte_buffer_available(&kbd.buffer);
}

// Clear keyboard buffer
static inline void keyboard_clear(void) {
    byte_buffer_clear(&kbd.buffer);
    kbd.last_key = KEY_NONE;
}

// Configure key repeat
static inline void keyboard_set_repeat(uint8_t enabled, uint16_t delay_ms, uint16_t rate_ms) {
    kbd.repeat_enabled = enabled;
    kbd.repeat_delay = delay_ms;
    kbd.repeat_rate = rate_ms;
}

// Character classification helpers
static inline uint8_t is_printable(uint8_t c) {
    return c >= 0x20 && c <= 0x7E;
}

static inline uint8_t is_digit(uint8_t c) {
    return c >= '0' && c <= '9';
}

static inline uint8_t is_alpha(uint8_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline uint8_t is_alnum(uint8_t c) {
    return is_alpha(c) || is_digit(c);
}

static inline uint8_t is_whitespace(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Convert to uppercase/lowercase
static inline uint8_t to_upper(uint8_t c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

static inline uint8_t to_lower(uint8_t c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

#endif
