// wifi_spam.h - Beacon Spam Attack (Configurable)
#ifndef WIFI_SPAM_H
#define WIFI_SPAM_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_CUSTOM_SSIDS 500

static const char *SPAM_TAG = "WiFi_Spam";
static uint8_t spam_running = 0;
static TaskHandle_t spam_task_handle = NULL;

// Configuration structure
typedef struct {
    uint8_t tx_power;               // 8-84 (2dBm - 21dBm)
    uint16_t beacon_interval;       // ms between beacons
    uint8_t random_macs;            // 1=random, 0=fixed
    uint8_t use_custom_list;        // 1=custom, 0=default
    uint8_t burst_count;            // Beacons per SSID (1-5)
    uint8_t simultaneous_networks;  // Networks to show at once (0=all)
    uint16_t rotation_seconds;      // Rotate batches every N seconds (0=disabled)
} SpamConfig;

static SpamConfig spam_config = {
    .tx_power = 84,                 // Max power (21dBm)
    .beacon_interval = 30,          // Fast interval
    .random_macs = 1,               // Random MACs
    .use_custom_list = 0,           // Default list
    .burst_count = 3,               // 3 beacons per SSID
    .simultaneous_networks = 10,    // Show 10 at once
    .rotation_seconds = 45          // Rotate every 45s
};

// Default funny SSID templates
static const char *default_ssids[] = {
    "Fuck ICE",
    "I'm like 90% sure no one likes Mrs. Edwards",
    "NSA Listening Post",
    "The Glowies Are Everyevere",
    "Theres no limit to the larp",
    "Martin Router King",
    "I'm not funny",
    "404 Network Unavailable",
    "I'm bored",
    "No Stewie",
    "It Burns When IP",
    "Iron Man Dies",
    "Fly Me To The Moon",
    "I didn't do it",
    "Anarchy isn't terrorism",
    "Get Off My LAN",
    "House LANister",
    "Winternet Is Coming",
    "No More Mr WiFi",
    "Router? I Barely Know Her"
};

#define NUM_DEFAULT_SSIDS (sizeof(default_ssids) / sizeof(default_ssids[0]))

// Custom SSID list (user-configurable)
static char custom_ssids[MAX_CUSTOM_SSIDS][33];
static uint8_t custom_ssid_count = 0;

// Custom beacon frame structure
typedef struct {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t destination[6];
    uint8_t source[6];
    uint8_t bssid[6];
    uint8_t seq_ctrl[2];
    uint8_t timestamp[8];
    uint8_t beacon_interval[2];
    uint8_t capability[2];
    uint8_t tag_ssid;
    uint8_t tag_ssid_len;
    uint8_t ssid[32];
} __attribute__((packed)) beacon_frame_t;

// Send raw beacon frame
static void send_beacon(const char *ssid) {
    beacon_frame_t beacon = {0};
    
    // Frame Control: Type=Management, Subtype=Beacon
    beacon.frame_ctrl[0] = 0x80;
    beacon.frame_ctrl[1] = 0x00;
    
    // Duration
    beacon.duration[0] = 0x00;
    beacon.duration[1] = 0x00;
    
    // Destination: Broadcast
    memset(beacon.destination, 0xFF, 6);
    
    // Source & BSSID: Random or fixed MAC
    if (spam_config.random_macs) {
        esp_fill_random(beacon.source, 6);
        beacon.source[0] = (beacon.source[0] & 0xFE) | 0x02;
    } else {
        static uint8_t base_mac[6] = {0x02, 0xCA, 0xFE, 0x00, 0x00, 0x00};
        static uint8_t mac_counter = 0;
        memcpy(beacon.source, base_mac, 6);
        beacon.source[5] = mac_counter++;
    }
    memcpy(beacon.bssid, beacon.source, 6);
    
    // Sequence control
    static uint16_t seq_num = 0;
    beacon.seq_ctrl[0] = (seq_num & 0x0F) << 4;
    beacon.seq_ctrl[1] = (seq_num & 0x0FF0) >> 4;
    seq_num++;
    
    // Timestamp
    uint64_t timestamp = esp_timer_get_time();
    memcpy(beacon.timestamp, &timestamp, 8);
    
    // Beacon interval (100 TUs)
    beacon.beacon_interval[0] = 0x64;
    beacon.beacon_interval[1] = 0x00;
    
    // Capability: ESS
    beacon.capability[0] = 0x01;
    beacon.capability[1] = 0x00;
    
    // SSID Tag
    beacon.tag_ssid = 0x00;
    size_t ssid_len = strlen(ssid);
    if (ssid_len > 32) ssid_len = 32;
    beacon.tag_ssid_len = ssid_len;
    memcpy(beacon.ssid, ssid, ssid_len);
    
    size_t packet_size = sizeof(beacon_frame_t) - (32 - ssid_len);
    esp_wifi_80211_tx(WIFI_IF_AP, &beacon, packet_size, false);
}

// Smart spam task with burst + rotation
static void spam_task(void *param) {
    const char **ssid_list;
    uint8_t total_ssid_count;
    
    // Select SSID list
    if (spam_config.use_custom_list && custom_ssid_count > 0) {
        ssid_list = (const char **)custom_ssids;
        total_ssid_count = custom_ssid_count;
    } else {
        ssid_list = default_ssids;
        total_ssid_count = NUM_DEFAULT_SSIDS;
    }
    
    // Determine active network count
    uint8_t active_count = spam_config.simultaneous_networks;
    if (active_count == 0 || active_count > total_ssid_count) {
        active_count = total_ssid_count; // Show all
    }
    
    uint8_t rotation_offset = 0;
    uint32_t last_rotation = xTaskGetTickCount();
    uint8_t ssid_index = 0;
    
    ESP_LOGI(SPAM_TAG, "Smart spam: %d total, %d active, burst=%d, interval=%dms, rotate=%ds", 
             total_ssid_count, active_count, spam_config.burst_count, 
             spam_config.beacon_interval, spam_config.rotation_seconds);
    
    while (spam_running) {
        // Check rotation
        if (spam_config.rotation_seconds > 0 && active_count < total_ssid_count) {
            uint32_t now = xTaskGetTickCount();
            if ((now - last_rotation) > pdMS_TO_TICKS(spam_config.rotation_seconds * 1000)) {
                rotation_offset = (rotation_offset + active_count) % total_ssid_count;
                last_rotation = now;
                ESP_LOGI(SPAM_TAG, "Rotating to networks %d-%d", 
                         rotation_offset, (rotation_offset + active_count - 1) % total_ssid_count);
            }
        }
        
        // Calculate actual SSID index
        uint8_t actual_index = (rotation_offset + ssid_index) % total_ssid_count;
        
        // Burst transmission
        for (uint8_t burst = 0; burst < spam_config.burst_count; burst++) {
            send_beacon(ssid_list[actual_index]);
            if (burst < spam_config.burst_count - 1) {
                vTaskDelay(pdMS_TO_TICKS(8)); // 8ms between bursts
            }
        }
        
        // Next SSID in active batch
        ssid_index = (ssid_index + 1) % active_count;
        
        // Main interval delay
        vTaskDelay(pdMS_TO_TICKS(spam_config.beacon_interval));
    }
    
    ESP_LOGI(SPAM_TAG, "Beacon spam stopped");
    spam_task_handle = NULL;
    vTaskDelete(NULL);
}

// Get current config
static inline SpamConfig* spam_get_config(void) {
    return &spam_config;
}

// Set TX power (8-84)
static inline void spam_set_tx_power(uint8_t power) {
    if (power < 8) power = 8;
    if (power > 84) power = 84;
    spam_config.tx_power = power;
}

// Set beacon interval (ms)
static inline void spam_set_interval(uint16_t interval_ms) {
    if (interval_ms < 20) interval_ms = 20;
    if (interval_ms > 1000) interval_ms = 1000;
    spam_config.beacon_interval = interval_ms;
}

// Set burst count
static inline void spam_set_burst(uint8_t burst) {
    if (burst < 1) burst = 1;
    if (burst > 5) burst = 5;
    spam_config.burst_count = burst;
}

// Set simultaneous networks
static inline void spam_set_simultaneous(uint8_t count) {
    spam_config.simultaneous_networks = count; // 0 = all
}

// Set rotation seconds
static inline void spam_set_rotation(uint16_t seconds) {
    spam_config.rotation_seconds = seconds; // 0 = disabled
}

// Toggle random MACs
static inline void spam_set_random_macs(uint8_t enable) {
    spam_config.random_macs = enable;
}

// Add custom SSID
static inline uint8_t spam_add_custom_ssid(const char *ssid) {
    if (custom_ssid_count >= MAX_CUSTOM_SSIDS) return 0;
    strncpy(custom_ssids[custom_ssid_count], ssid, 32);
    custom_ssids[custom_ssid_count][32] = '\0';
    custom_ssid_count++;
    return 1;
}

// Clear custom SSID list
static inline void spam_clear_custom_ssids(void) {
    custom_ssid_count = 0;
}

// Use custom list
static inline void spam_use_custom_list(uint8_t enable) {
    spam_config.use_custom_list = enable;
}

// Get SSID count
static inline uint8_t spam_get_ssid_count(void) {
    return spam_config.use_custom_list ? custom_ssid_count : NUM_DEFAULT_SSIDS;
}

// Start beacon spam
static inline uint8_t spam_start(void) {
    if (spam_running) return 0;
    if (spam_config.use_custom_list && custom_ssid_count == 0) return 0;
    
    static uint8_t wifi_init_done = 0;
    if (!wifi_init_done) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(spam_config.tx_power));
        
        wifi_init_done = 1;
    } else {
        esp_wifi_set_max_tx_power(spam_config.tx_power);
    }
    
    spam_running = 1;
    
    BaseType_t result = xTaskCreate(spam_task, "spam_task", 4096, NULL, 5, &spam_task_handle);
    if (result != pdPASS) {
        spam_running = 0;
        return 0;
    }
    
    return 1;
}

// Stop beacon spam
static inline void spam_stop(void) {
    if (!spam_running) return;
    spam_running = 0;
    uint8_t wait_count = 0;
    while (spam_task_handle != NULL && wait_count < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }
}

// Check if spam is running
static inline uint8_t spam_is_running(void) {
    return spam_running;
}

#endif
