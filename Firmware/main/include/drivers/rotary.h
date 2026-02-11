// rotary.h
#ifndef ROTARY_H
#define ROTARY_H
#include <stdint.h>
#include "driver/gpio.h"

typedef struct {
    uint8_t pin_clk;
    uint8_t pin_dt;
    uint8_t pin_sw;
    uint8_t last_clk;
    uint8_t last_dt;
    uint8_t last_sw;
    int32_t position;
} Rotary;

static inline void rotary_init(Rotary *rot, uint8_t clk, uint8_t dt, uint8_t sw) {
    rot->pin_clk = clk;
    rot->pin_dt = dt;
    rot->pin_sw = sw;
    rot->position = 0;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << clk) | (1ULL << dt) | (1ULL << sw),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    rot->last_clk = gpio_get_level((gpio_num_t)clk);
    rot->last_dt = gpio_get_level((gpio_num_t)dt);
    rot->last_sw = gpio_get_level((gpio_num_t)sw);
}

static inline int8_t rotary_read(Rotary *rot) {
    uint8_t clk = gpio_get_level((gpio_num_t)rot->pin_clk);
    uint8_t dt = gpio_get_level((gpio_num_t)rot->pin_dt);
    int8_t direction = 0;
    
    // Detect falling edge on CLK
    if (clk != rot->last_clk && clk == 0) {
        // Check DT state to determine direction
        if (dt == 1) {
            rot->position++;
            direction = 1;
        } else {
            rot->position--;
            direction = -1;
        }
    }
    
    rot->last_clk = clk;
    rot->last_dt = dt;
    
    return direction;
}

static inline uint8_t rotary_button_pressed(Rotary *rot) {
    uint8_t sw = gpio_get_level((gpio_num_t)rot->pin_sw);
    uint8_t pressed = 0;
    
    if (sw == 0 && rot->last_sw == 1) {
        pressed = 1;
    }
    rot->last_sw = sw;
    
    return pressed;
}

static inline int32_t rotary_get_position(Rotary *rot) {
    return rot->position;
}

static inline void rotary_reset_position(Rotary *rot) {
    rot->position = 0;
}

#endif
