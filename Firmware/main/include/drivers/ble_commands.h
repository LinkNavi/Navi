// ble_commands.h - BLE command processor
#ifndef BLE_COMMANDS_H
#define BLE_COMMANDS_H

#include <string.h>
#include "drivers/ble.h"
#include "drivers/wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char *BLE_CMD_TAG = "BLE_CMD";

// Process BLE commands
static inline void ble_process_command(const char *cmd, uint16_t len) {
    ESP_LOGI(BLE_CMD_TAG, "Processing: %s", cmd);
    
    // WiFi Scan
    if (strncmp(cmd, "WIFI_SCAN", 9) == 0) {
        ESP_LOGI(BLE_CMD_TAG, "WiFi scan requested");
        
        wifi_ap_record_t ap_list[20];
        uint16_t ap_count = wifi_scan_networks(ap_list, 20);
        
        // Send results in format: WIFI:ssid,rssi,channel,encryption;
        for (uint16_t i = 0; i < ap_count; i++) {
            const char *enc_str = "Open";
            switch (ap_list[i].authmode) {
                case WIFI_AUTH_WEP: enc_str = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: enc_str = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: enc_str = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: enc_str = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA3_PSK: enc_str = "WPA3"; break;
                case WIFI_AUTH_WPA2_WPA3_PSK: enc_str = "WPA2/WPA3"; break;
                default: break;
            }
            
            ble_printf("WIFI:%s,%d,%d,%s;",
                      (char *)ap_list[i].ssid,
                      ap_list[i].rssi,
                      ap_list[i].primary,
                      enc_str);
            
            vTaskDelay(pdMS_TO_TICKS(50)); // Avoid overwhelming BLE
        }
        
        ble_send_string("WIFI_END");
        ESP_LOGI(BLE_CMD_TAG, "WiFi scan complete, sent %d networks", ap_count);
    }
    
    // WiFi Connect: WIFI_CONNECT:ssid:password
    else if (strncmp(cmd, "WIFI_CONNECT:", 13) == 0) {
        char ssid[33] = {0};
        char password[65] = {0};
        
        // Parse SSID and password
        const char *ptr = cmd + 13;
        const char *colon = strchr(ptr, ':');
        
        if (colon) {
            size_t ssid_len = colon - ptr;
            if (ssid_len < sizeof(ssid)) {
                strncpy(ssid, ptr, ssid_len);
                ssid[ssid_len] = '\0';
                
                strncpy(password, colon + 1, sizeof(password) - 1);
                password[sizeof(password) - 1] = '\0';
                
                ESP_LOGI(BLE_CMD_TAG, "Connecting to: %s", ssid);
                
                if (wifi_init_sta(ssid, password)) {
                    ble_send_string("WIFI_CONNECTED");
                    
                    char ip_str[32];
                    wifi_get_ip_string(ip_str, sizeof(ip_str));
                    ble_printf("WIFI_IP:%s", ip_str);
                } else {
                    ble_send_string("WIFI_FAILED");
                }
            }
        }
    }
    
    // WiFi Disconnect
    else if (strncmp(cmd, "WIFI_DISCONNECT", 15) == 0) {
        ESP_LOGI(BLE_CMD_TAG, "WiFi disconnect requested");
        wifi_disconnect();
        ble_send_string("WIFI_DISCONNECTED");
    }
    
    // WiFi Status
    else if (strncmp(cmd, "WIFI_STATUS", 11) == 0) {
        if (wifi_is_connected()) {
            char ip_str[32];
            wifi_get_ip_string(ip_str, sizeof(ip_str));
            ble_printf("WIFI_STATUS:CONNECTED:%s", ip_str);
        } else {
            ble_send_string("WIFI_STATUS:DISCONNECTED");
        }
    }
    
    // Bridge Start: BRIDGE_START:upstream_ssid:upstream_pass:bridge_ssid:bridge_pass
    else if (strncmp(cmd, "BRIDGE_START:", 13) == 0) {
        // TODO: Implement bridge mode
        ble_send_string("BRIDGE_NOT_IMPLEMENTED");
    }
    
    // Bridge Stop
    else if (strncmp(cmd, "BRIDGE_STOP", 11) == 0) {
        // TODO: Implement bridge mode
        ble_send_string("BRIDGE_STOPPED");
    }
    
    // Ping
    else if (strncmp(cmd, "PING", 4) == 0) {
        ble_send_string("PONG");
    }
    
    // Unknown command
    else {
        ESP_LOGW(BLE_CMD_TAG, "Unknown command: %s", cmd);
        ble_send_string("ERROR:UNKNOWN_COMMAND");
    }
}

#endif
