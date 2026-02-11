// wifi_bridge.h - WiFi Bridge Mode Configuration
#ifndef WIFI_BRIDGE_H
#define WIFI_BRIDGE_H

#include <stdint.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *BRIDGE_TAG = "WiFi_Bridge";

typedef struct {
    char upstream_ssid[32];      // Network to connect to
    char upstream_password[64];  // Password for upstream
    char bridge_ssid[32];        // AP name to broadcast
    char bridge_password[64];    // AP password
    uint8_t enabled;             // Bridge mode enabled
} BridgeConfig;

static BridgeConfig bridge_config = {0};

// Save bridge config to NVS
static inline uint8_t bridge_save_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("bridge", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(BRIDGE_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return 0;
    }

    // Save upstream network credentials
    err = nvs_set_str(nvs_handle, "up_ssid", bridge_config.upstream_ssid);
    if (err != ESP_OK) {
        ESP_LOGE(BRIDGE_TAG, "Error saving upstream SSID");
        nvs_close(nvs_handle);
        return 0;
    }

    err = nvs_set_str(nvs_handle, "up_pass", bridge_config.upstream_password);
    if (err != ESP_OK) {
        ESP_LOGE(BRIDGE_TAG, "Error saving upstream password");
        nvs_close(nvs_handle);
        return 0;
    }

    // Save bridge AP credentials
    err = nvs_set_str(nvs_handle, "br_ssid", bridge_config.bridge_ssid);
    if (err != ESP_OK) {
        ESP_LOGE(BRIDGE_TAG, "Error saving bridge SSID");
        nvs_close(nvs_handle);
        return 0;
    }

    err = nvs_set_str(nvs_handle, "br_pass", bridge_config.bridge_password);
    if (err != ESP_OK) {
        ESP_LOGE(BRIDGE_TAG, "Error saving bridge password");
        nvs_close(nvs_handle);
        return 0;
    }

    // Save enabled flag
    err = nvs_set_u8(nvs_handle, "enabled", bridge_config.enabled);
    if (err != ESP_OK) {
        ESP_LOGE(BRIDGE_TAG, "Error saving enabled flag");
        nvs_close(nvs_handle);
        return 0;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(BRIDGE_TAG, "Bridge config saved to flash");
        return 1;
    }
    return 0;
}

// Load bridge config from NVS
static inline uint8_t bridge_load_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("bridge", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(BRIDGE_TAG, "No saved bridge config");
        return 0;
    }

    size_t required_size;

    // Load upstream SSID
    required_size = sizeof(bridge_config.upstream_ssid);
    err = nvs_get_str(nvs_handle, "up_ssid", bridge_config.upstream_ssid, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return 0;
    }

    // Load upstream password
    required_size = sizeof(bridge_config.upstream_password);
    err = nvs_get_str(nvs_handle, "up_pass", bridge_config.upstream_password, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return 0;
    }

    // Load bridge SSID
    required_size = sizeof(bridge_config.bridge_ssid);
    err = nvs_get_str(nvs_handle, "br_ssid", bridge_config.bridge_ssid, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return 0;
    }

    // Load bridge password
    required_size = sizeof(bridge_config.bridge_password);
    err = nvs_get_str(nvs_handle, "br_pass", bridge_config.bridge_password, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return 0;
    }

    // Load enabled flag
    err = nvs_get_u8(nvs_handle, "enabled", &bridge_config.enabled);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(BRIDGE_TAG, "Bridge config loaded from flash");
        ESP_LOGI(BRIDGE_TAG, "  Upstream: %s", bridge_config.upstream_ssid);
        ESP_LOGI(BRIDGE_TAG, "  Bridge AP: %s", bridge_config.bridge_ssid);
        ESP_LOGI(BRIDGE_TAG, "  Enabled: %d", bridge_config.enabled);
        return 1;
    }
    return 0;
}

// Clear bridge config from NVS
static inline uint8_t bridge_clear_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("bridge", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return 0;
    }

    nvs_erase_all(nvs_handle);
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    memset(&bridge_config, 0, sizeof(bridge_config));
    
    if (err == ESP_OK) {
        ESP_LOGI(BRIDGE_TAG, "Bridge config cleared");
        return 1;
    }
    return 0;
}

// Initialize bridge config (call at startup)
static inline void bridge_init(void) {
    memset(&bridge_config, 0, sizeof(bridge_config));
    
    // Try to load saved config
    if (!bridge_load_config()) {
        // Set defaults if no saved config
        strcpy(bridge_config.bridge_ssid, "Navi-Bridge");
        strcpy(bridge_config.bridge_password, "password123");
        bridge_config.enabled = 0;
        ESP_LOGI(BRIDGE_TAG, "Using default bridge config");
    }
}

// Get current bridge config
static inline BridgeConfig* bridge_get_config(void) {
    return &bridge_config;
}

// Set upstream network
static inline void bridge_set_upstream(const char *ssid, const char *password) {
    strncpy(bridge_config.upstream_ssid, ssid, sizeof(bridge_config.upstream_ssid) - 1);
    strncpy(bridge_config.upstream_password, password, sizeof(bridge_config.upstream_password) - 1);
}

// Set bridge AP
static inline void bridge_set_ap(const char *ssid, const char *password) {
    strncpy(bridge_config.bridge_ssid, ssid, sizeof(bridge_config.bridge_ssid) - 1);
    strncpy(bridge_config.bridge_password, password, sizeof(bridge_config.bridge_password) - 1);
}

// Enable/disable bridge
static inline void bridge_set_enabled(uint8_t enabled) {
    bridge_config.enabled = enabled;
}

#endif
