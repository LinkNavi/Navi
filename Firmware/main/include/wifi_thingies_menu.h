// wifi_bridge_runtime.h - WiFi Bridge Runtime (AP + STA mode)
#ifndef WIFI_BRIDGE_RUNTIME_H
#define WIFI_BRIDGE_RUNTIME_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/lwip_napt.h"
#include "wifi_bridge.h"
#include <arpa/inet.h>
#include "lwip/lwip_napt.h"
#define AP_NETIF_FLAG 1
static const char *BRIDGE_RUNTIME_TAG = "Bridge_Runtime";
void wifi_thingies_init(void);
void wifi_thingies_open(void);
static uint8_t bridge_running = 0;
static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

// Event handler for bridge mode
static void bridge_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
   //   ESP_LOGI(BRIDGE_RUNTIME_TAG, "Station "MACSTR" joined AP", MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
 //   ESP_LOGI(BRIDGE_RUNTIME_TAG, "Station "MACSTR" left AP", MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(BRIDGE_RUNTIME_TAG, "Connected to upstream WiFi");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(BRIDGE_RUNTIME_TAG, "Disconnected from upstream WiFi, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(BRIDGE_RUNTIME_TAG, "Got IP from upstream: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // Enable NAT (Network Address Translation) for routing
        ip_napt_enable(AP_NETIF_FLAG, 1);
        ESP_LOGI(BRIDGE_RUNTIME_TAG, "NAT enabled - bridge is active!");
    }
}

// Start WiFi bridge
static inline uint8_t bridge_start(void) {
    BridgeConfig *cfg = bridge_get_config();
    
    if (!cfg->enabled) {
        ESP_LOGW(BRIDGE_RUNTIME_TAG, "Bridge mode disabled in config");
        return 0;
    }
    
    if (strlen(cfg->upstream_ssid) == 0) {
        ESP_LOGE(BRIDGE_RUNTIME_TAG, "No upstream SSID configured");
        return 0;
    }
    
    if (strlen(cfg->bridge_ssid) == 0) {
        ESP_LOGE(BRIDGE_RUNTIME_TAG, "No bridge SSID configured");
        return 0;
    }
    
    ESP_LOGI(BRIDGE_RUNTIME_TAG, "Starting WiFi bridge...");
    ESP_LOGI(BRIDGE_RUNTIME_TAG, "  Upstream: %s", cfg->upstream_ssid);
    ESP_LOGI(BRIDGE_RUNTIME_TAG, "  Bridge AP: %s", cfg->bridge_ssid);
    
    // Initialize network interfaces
    { esp_err_t __e = esp_netif_init(); if (__e != ESP_OK && __e != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(__e); }
    { esp_err_t __e = esp_event_loop_create_default(); if (__e != ESP_OK && __e != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(__e); }
    
    // Create AP and STA interfaces
if (!ap_netif) ap_netif = esp_netif_create_default_wifi_ap();
if (!sta_netif) sta_netif = esp_netif_create_default_wifi_sta();
    
    // Configure AP IP
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_ip_info_t ap_ip;
    memset(&ap_ip, 0, sizeof(esp_netif_ip_info_t));
   inet_aton("192.168.4.1", (ip4_addr_t*)&ap_ip.ip);
inet_aton("192.168.4.1", (ip4_addr_t*)&ap_ip.gw);
inet_aton("255.255.255.0", (ip4_addr_t*)&ap_ip.netmask); 
	esp_netif_set_ip_info(ap_netif, &ap_ip);
    esp_netif_dhcps_start(ap_netif);
    
    // Initialize WiFi
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &bridge_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &bridge_event_handler,
                                                        NULL,
                                                        NULL));
    
    // Configure WiFi for AP+STA mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    
    // Configure STA (upstream connection)
    wifi_config_t sta_config = {0};
    strncpy((char *)sta_config.sta.ssid, cfg->upstream_ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char *)sta_config.sta.password, cfg->upstream_password, sizeof(sta_config.sta.password) - 1);
    
    if (strlen(cfg->upstream_password) == 0) {
        sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        sta_config.sta.threshold.authmode = WIFI_AUTH_WEP;
    }
    
    sta_config.sta.pmf_cfg.capable = true;
    sta_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    
    // Configure AP (bridge access point)
    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, cfg->bridge_ssid, sizeof(ap_config.ap.ssid) - 1);
    strncpy((char *)ap_config.ap.password, cfg->bridge_password, sizeof(ap_config.ap.password) - 1);
    ap_config.ap.ssid_len = strlen(cfg->bridge_ssid);
    ap_config.ap.channel = 6;
    ap_config.ap.max_connection = 4;
    
    if (strlen(cfg->bridge_password) < 8) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGW(BRIDGE_RUNTIME_TAG, "Bridge AP is OPEN (password too short)");
    } else {
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Connect to upstream
    esp_wifi_connect();
    
    bridge_running = 1;
    ESP_LOGI(BRIDGE_RUNTIME_TAG, "Bridge started successfully!");
    ESP_LOGI(BRIDGE_RUNTIME_TAG, "AP SSID: %s", cfg->bridge_ssid);
    ESP_LOGI(BRIDGE_RUNTIME_TAG, "AP IP: 192.168.4.1");
    
    return 1;
}

// Stop WiFi bridge
static inline void bridge_stop(void) {
    if (!bridge_running) return;
    
    ESP_LOGI(BRIDGE_RUNTIME_TAG, "Stopping bridge...");
    
    // Disable NAT
    ip_napt_enable(AP_NETIF_FLAG, 0);
    
    // Stop WiFi
    esp_wifi_stop();
    esp_wifi_deinit();
    
    bridge_running = 0;
    ESP_LOGI(BRIDGE_RUNTIME_TAG, "Bridge stopped");
}

// Check if bridge is running
static inline uint8_t bridge_is_running(void) {
    return bridge_running;
}

// Get bridge status string
static inline void bridge_get_status(char *status, size_t len) {
    if (!bridge_running) {
        strncpy(status, "Stopped", len);
        return;
    }
    
    BridgeConfig *cfg = bridge_get_config();
    snprintf(status, len, "Running: %s", cfg->bridge_ssid);
}

#endif
