// rotary_pcnt.h - Hardware PCNT-based rotary encoder driver
#ifndef ROTARY_PCNT_H
#define ROTARY_PCNT_H

#include <stdint.h>
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *ROTARY_PCNT_TAG = "RotaryPCNT";

typedef struct {
    pcnt_unit_handle_t unit;
    pcnt_channel_handle_t chan_a;
    pcnt_channel_handle_t chan_b;
    uint8_t pin_clk;
    uint8_t pin_dt;
    uint8_t pin_sw;
    int last_count;
    uint8_t last_sw;
    int32_t position;
} RotaryPCNT;

// Initialize PCNT-based rotary encoder
static inline void rotary_pcnt_init(RotaryPCNT *rot, uint8_t clk, uint8_t dt, uint8_t sw) {
    rot->pin_clk = clk;
    rot->pin_dt = dt;
    rot->pin_sw = sw;
    rot->last_count = 0;
    rot->position = 0;
    
    ESP_LOGI(ROTARY_PCNT_TAG, "Initializing PCNT rotary encoder on CLK=%d, DT=%d", clk, dt);
    
    // Create PCNT unit
    pcnt_unit_config_t unit_config = {
        .high_limit = 32767,
        .low_limit = -32768,
        .flags.accum_count = 0,  // Clear on overflow
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &rot->unit));
    
    // Channel A (CLK pin)
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = clk,
        .level_gpio_num = dt,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(rot->unit, &chan_a_config, &rot->chan_a));
    
    // Channel B (DT pin)
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = dt,
        .level_gpio_num = clk,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(rot->unit, &chan_b_config, &rot->chan_b));
    
    // Configure quadrature decoding
    // When CLK rises and DT is low: increment
    // When CLK rises and DT is high: decrement
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(rot->chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,  // Rising edge, DT high
        PCNT_CHANNEL_EDGE_ACTION_INCREASE)); // Rising edge, DT low
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(rot->chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,      // DT low: keep action
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE)); // DT high: invert action
    
    // Same for channel B (DT pin edges)
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(rot->chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,   // Rising edge, CLK high
        PCNT_CHANNEL_EDGE_ACTION_DECREASE)); // Rising edge, CLK low
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(rot->chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,      // CLK low: keep action
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE)); // CLK high: invert action
    
    // Enable glitch filter (1 microsecond)
    // This filters out electrical noise and bounce
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,  // 1µs glitch filter
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(rot->unit, &filter_config));
    
    // Enable and start the unit
    ESP_ERROR_CHECK(pcnt_unit_enable(rot->unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(rot->unit));
    ESP_ERROR_CHECK(pcnt_unit_start(rot->unit));
    
    // Configure button pin (standard GPIO)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sw),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    rot->last_sw = gpio_get_level((gpio_num_t)sw);
    
    ESP_LOGI(ROTARY_PCNT_TAG, "PCNT encoder initialized successfully");
}

// Read rotation delta (returns -1, 0, or 1)
static inline int8_t rotary_pcnt_read(RotaryPCNT *rot) {
    int count;
    ESP_ERROR_CHECK(pcnt_unit_get_count(rot->unit, &count));
    
    // Calculate difference since last read
    int diff = count - rot->last_count;
    
    if (diff != 0) {
        rot->last_count = count;
        rot->position += diff;
        
        // Return simplified direction
        if (diff > 0) return 1;   // Clockwise
        if (diff < 0) return -1;  // Counter-clockwise
    }
    
    return 0;  // No change
}

// Check if button was pressed (edge detection)
static inline uint8_t rotary_pcnt_button_pressed(RotaryPCNT *rot) {
    uint8_t sw = gpio_get_level((gpio_num_t)rot->pin_sw);
    uint8_t pressed = 0;
    
    // Detect falling edge (button press)
    if (sw == 0 && rot->last_sw == 1) {
        pressed = 1;
    }
    rot->last_sw = sw;
    
    return pressed;
}

// Get absolute position
static inline int32_t rotary_pcnt_get_position(RotaryPCNT *rot) {
    return rot->position;
}

// Reset position to zero
static inline void rotary_pcnt_reset_position(RotaryPCNT *rot) {
    rot->position = 0;
    pcnt_unit_clear_count(rot->unit);
    rot->last_count = 0;
}

// Cleanup (call before freeing)
static inline void rotary_pcnt_deinit(RotaryPCNT *rot) {
    pcnt_unit_stop(rot->unit);
    pcnt_unit_disable(rot->unit);
    pcnt_del_channel(rot->chan_a);
    pcnt_del_channel(rot->chan_b);
    pcnt_unit_remove_glitch_filter(rot->unit);
    pcnt_del_unit(rot->unit);
    
    ESP_LOGI(ROTARY_PCNT_TAG, "PCNT encoder deinitialized");
}

#endif
