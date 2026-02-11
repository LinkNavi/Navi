// ir_blaster.h
#ifndef IR_BLASTER_H
#define IR_BLASTER_H

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#define IR_CARRIER_FREQ 38000  // 38kHz carrier
#define IR_DUTY_CYCLE 33       // 33% duty cycle

// NEC Protocol timings (in microseconds)
#define NEC_HEADER_HIGH  9000
#define NEC_HEADER_LOW   4500
#define NEC_BIT_HIGH     560
#define NEC_BIT_ONE_LOW  1690
#define NEC_BIT_ZERO_LOW 560
#define NEC_END_HIGH     560

typedef struct {
    uint8_t pin;
} IR_Blaster;

static IR_Blaster ir_blaster;

// Generate 38kHz carrier for given microseconds
static inline void ir_mark(uint16_t us) {
    uint32_t cycles = (us * IR_CARRIER_FREQ) / 1000;
    uint32_t high_time = (1000000 / IR_CARRIER_FREQ) * IR_DUTY_CYCLE / 100;
    uint32_t low_time = (1000000 / IR_CARRIER_FREQ) - high_time;
    
    for (uint32_t i = 0; i < cycles; i++) {
        gpio_set_level((gpio_num_t)ir_blaster.pin, 1);
        esp_rom_delay_us(high_time);
        gpio_set_level((gpio_num_t)ir_blaster.pin, 0);
        esp_rom_delay_us(low_time);
    }
}

// Space (no carrier)
static inline void ir_space(uint16_t us) {
    gpio_set_level((gpio_num_t)ir_blaster.pin, 0);
    esp_rom_delay_us(us);
}

static inline void ir_init(uint8_t pin) {
    ir_blaster.pin = pin;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)pin, 0);
}

// Send NEC protocol (address, command)
static inline void ir_send_nec(uint8_t address, uint8_t command) {
    // Header
    ir_mark(NEC_HEADER_HIGH);
    ir_space(NEC_HEADER_LOW);
    
    // Address
    for (int i = 0; i < 8; i++) {
        ir_mark(NEC_BIT_HIGH);
        ir_space((address & (1 << i)) ? NEC_BIT_ONE_LOW : NEC_BIT_ZERO_LOW);
    }
    
    // Address inverse
    for (int i = 0; i < 8; i++) {
        ir_mark(NEC_BIT_HIGH);
        ir_space((~address & (1 << i)) ? NEC_BIT_ONE_LOW : NEC_BIT_ZERO_LOW);
    }
    
    // Command
    for (int i = 0; i < 8; i++) {
        ir_mark(NEC_BIT_HIGH);
        ir_space((command & (1 << i)) ? NEC_BIT_ONE_LOW : NEC_BIT_ZERO_LOW);
    }
    
    // Command inverse
    for (int i = 0; i < 8; i++) {
        ir_mark(NEC_BIT_HIGH);
        ir_space((~command & (1 << i)) ? NEC_BIT_ONE_LOW : NEC_BIT_ZERO_LOW);
    }
    
    // End bit
    ir_mark(NEC_END_HIGH);
    ir_space(0);
}

// Send raw pulse/space sequence
static inline void ir_send_raw(const uint16_t *data, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        if (i % 2 == 0) {
            ir_mark(data[i]);
        } else {
            ir_space(data[i]);
        }
    }
}

#endif
