// pin_config.h - Configurable pin assignments with NVS storage
#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include <stdint.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *PIN_CONFIG_TAG = "PinConfig";

typedef struct {
    // I2C
    uint8_t i2c_sda;
    uint8_t i2c_scl;
    
    // Rotary Encoder
    uint8_t rotary_clk;
    uint8_t rotary_dt;
    uint8_t rotary_sw;
    
    // IR Blaster
    uint8_t ir_pin;
    
    // SD Card
    uint8_t sd_mosi;
    uint8_t sd_miso;
    uint8_t sd_clk;
    uint8_t sd_cs;
} PinConfig;

// Default pin configuration for ESP32-S3
static PinConfig default_pins = {
    .i2c_sda = 16,
    .i2c_scl = 17,
    .rotary_clk = 5,
    .rotary_dt = 6,
    .rotary_sw = 7,
    .ir_pin = 4,
    .sd_mosi = 11,
    .sd_miso = 13,
    .sd_clk = 12,
    .sd_cs = 10,
};

static PinConfig current_pins;

// Initialize pin config (call at startup)
static inline void pin_config_init(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    // Try to load from NVS
    err = nvs_open("pins", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(PIN_CONFIG_TAG, "No saved config, using defaults");
        current_pins = default_pins;
        return;
    }
    
    size_t required_size = sizeof(PinConfig);
    err = nvs_get_blob(nvs_handle, "config", &current_pins, &required_size);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(PIN_CONFIG_TAG, "Loaded pin config from flash");
        ESP_LOGI(PIN_CONFIG_TAG, "  I2C: SDA=%d SCL=%d", current_pins.i2c_sda, current_pins.i2c_scl);
        ESP_LOGI(PIN_CONFIG_TAG, "  Rotary: CLK=%d DT=%d SW=%d", 
                 current_pins.rotary_clk, current_pins.rotary_dt, current_pins.rotary_sw);
        ESP_LOGI(PIN_CONFIG_TAG, "  IR: %d", current_pins.ir_pin);
        ESP_LOGI(PIN_CONFIG_TAG, "  SD: MOSI=%d MISO=%d CLK=%d CS=%d",
                 current_pins.sd_mosi, current_pins.sd_miso, current_pins.sd_clk, current_pins.sd_cs);
    } else {
        ESP_LOGW(PIN_CONFIG_TAG, "Failed to load config, using defaults");
        current_pins = default_pins;
    }
}

// Save pin config to NVS
static inline uint8_t pin_config_save(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open("pins", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(PIN_CONFIG_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return 0;
    }
    
    err = nvs_set_blob(nvs_handle, "config", &current_pins, sizeof(PinConfig));
    if (err != ESP_OK) {
        ESP_LOGE(PIN_CONFIG_TAG, "Error saving config");
        nvs_close(nvs_handle);
        return 0;
    }
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(PIN_CONFIG_TAG, "Pin config saved to flash");
        return 1;
    }
    return 0;
}

// Reset to defaults
static inline void pin_config_reset(void) {
    current_pins = default_pins;
    ESP_LOGI(PIN_CONFIG_TAG, "Reset to default pins");
}

// Get current config
static inline PinConfig* pin_config_get(void) {
    return &current_pins;
}

// Validate pin number (ESP32-S3 specific)
static inline uint8_t pin_is_valid(uint8_t pin) {
    // ESP32-S3 has GPIO 0-48, but some are restricted
    if (pin > 48) return 0;
    
    // Avoid strapping pins and flash pins
    if (pin == 0) return 0;   // Boot button
    if (pin >= 26 && pin <= 32) return 0;  // Flash/PSRAM (usually)
    if (pin == 45 || pin == 46) return 0;  // Strapping
    
    return 1;
}

// Check for pin conflicts
static inline uint8_t pin_has_conflict(uint8_t pin, const char *exclude_category) {
    PinConfig *cfg = &current_pins;
    
    if (strcmp(exclude_category, "i2c") != 0) {
        if (pin == cfg->i2c_sda || pin == cfg->i2c_scl) return 1;
    }
    if (strcmp(exclude_category, "rotary") != 0) {
        if (pin == cfg->rotary_clk || pin == cfg->rotary_dt || pin == cfg->rotary_sw) return 1;
    }
    if (strcmp(exclude_category, "ir") != 0) {
        if (pin == cfg->ir_pin) return 1;
    }
    if (strcmp(exclude_category, "sd") != 0) {
        if (pin == cfg->sd_mosi || pin == cfg->sd_miso || 
            pin == cfg->sd_clk || pin == cfg->sd_cs) return 1;
    }
    
    return 0;
}

#endif
