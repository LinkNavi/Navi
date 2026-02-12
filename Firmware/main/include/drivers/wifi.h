// wifi.h - WiFi management for Navi (FIXED - no duplicate event loop)
#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MAX_RETRY 5

static const char *WIFI_TAG = "WiFi";
static EventGroupHandle_t wifi_event_group;
static uint8_t wifi_retry_num = 0;
static uint8_t wifi_connected = 0;
static uint8_t wifi_initialized = 0;  // Track if WiFi is already initialized

typedef struct {
    char ssid[32];
    char password[64];
} wifi_credentials_t;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(WIFI_TAG, "Retry connecting to AP (%d/%d)", wifi_retry_num, MAX_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        wifi_connected = 0;
        ESP_LOGI(WIFI_TAG, "Connect to AP failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        wifi_connected = 1;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static inline void wifi_init_system(void) {
    // Check if STA netif already exists (created by BLE or another module)
    esp_netif_t *existing = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (existing) {
        wifi_initialized = 1;
        return;
    }
    if (wifi_initialized) return;
    
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    wifi_initialized = 1;
    ESP_LOGI(WIFI_TAG, "WiFi system initialized");
}

static inline uint8_t wifi_init_sta(const char *ssid, const char *password) {
    // Initialize WiFi system if not already done
    wifi_init_system();
    
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    // Support all security types - set threshold to OPEN if no password
    if (strlen(password) == 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        // Accept any secure auth mode (WEP, WPA, WPA2, WPA3)
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WEP;
    }
    
    // PMF (Protected Management Frames) config for WPA3
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;  // Optional, not mandatory
    
    // Enable scanning of all auth modes
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Restart WiFi to apply new config
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "Connected to AP SSID:%s", ssid);
        return 1;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s", ssid);
        return 0;
    } else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
        return 0;
    }
}

static inline uint8_t wifi_is_connected(void) {
    return wifi_connected;
}

static inline void wifi_disconnect(void) {
    if (wifi_connected) {
        esp_wifi_disconnect();
        wifi_connected = 0;
        ESP_LOGI(WIFI_TAG, "Disconnected");
    }
}

static inline uint8_t wifi_save_credentials(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return 0;
    }

    err = nvs_set_str(nvs_handle, "wifi_ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error saving SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return 0;
    }

    err = nvs_set_str(nvs_handle, "wifi_pass", password);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error saving password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return 0;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Credentials saved");
        return 1;
    }
    return 0;
}

static inline uint8_t wifi_load_credentials(char *ssid, size_t ssid_len, 
                                             char *password, size_t pass_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return 0;
    }

    size_t required_size = ssid_len;
    err = nvs_get_str(nvs_handle, "wifi_ssid", ssid, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(WIFI_TAG, "No saved SSID found");
        nvs_close(nvs_handle);
        return 0;
    }

    required_size = pass_len;
    err = nvs_get_str(nvs_handle, "wifi_pass", password, &required_size);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGW(WIFI_TAG, "No saved password found");
        return 0;
    }

    ESP_LOGI(WIFI_TAG, "Loaded credentials for SSID: %s", ssid);
    return 1;
}

static inline void wifi_get_ip_string(char *ip_str, size_t len) {
    if (!wifi_connected) {
        strncpy(ip_str, "Not connected", len);
        return;
    }

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(netif, &ip_info);
        snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    } else {
        strncpy(ip_str, "Error", len);
    }
}

// FIXED: Use wifi_init_system() instead of reinitializing everything
static inline uint16_t wifi_scan_networks(wifi_ap_record_t *ap_list, uint16_t max_aps) {
    wifi_init_system();  // Only initializes if not already done
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    esp_wifi_scan_start(&scan_config, true);
    
    uint16_t ap_count = max_aps;
    esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    
    ESP_LOGI(WIFI_TAG, "Found %d networks", ap_count);
    return ap_count;
}

#endif
