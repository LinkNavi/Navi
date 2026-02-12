// arp_poison_menu.c - ARP Poisoning Attack Menu
#include "include/menu.h"
#include "include/modules/arp_poison.h"
#include "include/drivers/display.h"
#include "include/drivers/rotary_pcnt.h"
#include "include/drivers/wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"

static const char *TAG = "ARP_Poison_Menu";

extern RotaryPCNT encoder;
extern void back_to_main(void);

static Menu arp_poison_menu;

// Router info
static char router_ip[16] = {0};
static char router_mac[18] = {0};
static uint8_t router_configured = 0;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void back_to_arp_poison_menu(void) {
    menu_set_status("ARP Poison");
    menu_set_active(&arp_poison_menu);
    menu_draw();
}

// Scan network for devices
static void arp_poison_scan_devices(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Scanning network...");
    println("");
    println("Finding devices");
    display_show();
    
    // Check if connected to WiFi
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        display_clear();
        set_cursor(2, 10);
        println("Not connected!");
        println("");
        println("Connect to WiFi");
        println("in STA mode first");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_arp_poison_menu();
        return;
    }
    
    uint8_t count = arp_poison_scan_network();
    
    display_clear();
    set_cursor(2, 10);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Found %d devices", count);
    println(buf);
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_arp_poison_menu();
}

// Configure router (gateway)
static void arp_poison_configure_router(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("Configure Router");
    println("");
    println("Getting gateway info");
    println("from DHCP...");
    display_show();
    
    delay(500);
    
    // Get gateway IP from DHCP
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        display_clear();
        set_cursor(2, 10);
        println("Not connected!");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_arp_poison_menu();
        return;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    
    // Format router IP
    snprintf(router_ip, sizeof(router_ip), "%d.%d.%d.%d",
             (int)((ip_info.gw.addr >> 0) & 0xFF),
             (int)((ip_info.gw.addr >> 8) & 0xFF),
             (int)((ip_info.gw.addr >> 16) & 0xFF),
             (int)((ip_info.gw.addr >> 24) & 0xFF));
    
    // Get router MAC from ARP table (simplified - use WiFi AP's BSSID)
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    snprintf(router_mac, sizeof(router_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             ap_info.bssid[0], ap_info.bssid[1], ap_info.bssid[2],
             ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5]);
    
    router_configured = 1;
    
    display_clear();
    set_cursor(2, 8);
    println("Router Configured!");
    println("");
    println("IP:");
    println(router_ip);
    println("");
    println("MAC:");
    println(router_mac);
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_arp_poison_menu();
}

// Add target all devices
static void arp_poison_target_all_menu(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("TARGET ALL DEVICES");
    println("");
    
    uint8_t device_count;
    arp_poison_get_scanned_devices(&device_count);
    
    if (device_count == 0) {
        println("No devices found!");
        println("");
        println("Scan network first");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_arp_poison_menu();
        return;
    }
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Found: %d devices", device_count);
    println(buf);
    println("");
    println("This will target");
    println("ALL devices on");
    println("the network");
    println("(except router)");
    println("");
    println("⚠️  MASS MITM!");
    println("");
    println("Press: Add All");
    println("Hold: Cancel");
    display_show();
    
    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint8_t confirmed = 0;
    
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) < 5000) {
        rotary_pcnt_read(&encoder);
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            confirmed = 1;
            break;
        }
        
        delay(10);
    }
    
    if (!confirmed) {
        back_to_arp_poison_menu();
        return;
    }
    
    delay(200);
    
    display_clear();
    set_cursor(2, 10);
    println("Adding all targets...");
    display_show();
    
    uint8_t added = arp_poison_target_all();
    
    display_clear();
    set_cursor(2, 10);
    
    snprintf(buf, sizeof(buf), "Added %d targets!", added);
    println(buf);
    println("");
    println("Ready to attack");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_arp_poison_menu();
}

// Add target victim
static void arp_poison_add_target_menu(void) {
    if (!router_configured) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("Configure router");
        println("first!");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_arp_poison_menu();
        return;
    }
    
    // Get scanned devices
    uint8_t device_count;
    network_device_t *devices = arp_poison_get_scanned_devices(&device_count);
    
    if (device_count == 0) {
        display_clear();
        set_cursor(2, 10);
        println("No devices found!");
        println("");
        println("Scan network first");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_arp_poison_menu();
        return;
    }
    
    uint8_t index = 0;
    uint8_t scroll_offset = 0;
    
    while (1) {
        display_clear();
        
        // Title
        fill_rect(0, 0, WIDTH, 12, 1);
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        
        const char *title = "Select Victim";
        print(title);
        draw_hline(0, 12, WIDTH, 1);
        
        // List devices
        uint8_t visible = (HEIGHT - 24) / 10;
        uint8_t y = 14;
        
        for (uint8_t i = scroll_offset; i < device_count && i < scroll_offset + visible; i++) {
            if (i == index) {
                fill_rect(2, y, WIDTH - 4, 10, 1);
            }
            
            set_cursor(4, y + 7);
            
            // Format device info
            char dev_str[32];
            if (devices[i].is_router) {
                snprintf(dev_str, sizeof(dev_str), "Router");
            } else if (devices[i].ip != 0) {
                snprintf(dev_str, sizeof(dev_str), "%d.%d.%d.%d",
                        (int)((devices[i].ip >> 0) & 0xFF),
                        (int)((devices[i].ip >> 8) & 0xFF),
                        (int)((devices[i].ip >> 16) & 0xFF),
                        (int)((devices[i].ip >> 24) & 0xFF));
            } else {
                snprintf(dev_str, sizeof(dev_str), MACSTR, MAC2STR(devices[i].mac));
            }
            
            if (i == index) {
                // Inverted text
                const char *s = dev_str;
                while (*s) {
                    if (*s >= TomThumb.first && *s <= TomThumb.last) {
                        const GFXglyph *g = &TomThumb.glyph[*s - TomThumb.first];
                        const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
                        uint16_t bit_idx = 0;
                        for (uint8_t yy = 0; yy < g->height; yy++) {
                            for (uint8_t xx = 0; xx < g->width; xx++) {
                                if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                                    int16_t px = cursor_x + g->xOffset + xx;
                                    int16_t py = cursor_y + g->yOffset + yy;
                                    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
                                        framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                                }
                                bit_idx++;
                            }
                        }
                        cursor_x += g->xAdvance;
                    }
                    s++;
                }
            } else {
                print(dev_str);
            }
            
            y += 10;
        }
        
        // Status bar
        draw_hline(0, HEIGHT - 10, WIDTH, 1);
        set_cursor(2, HEIGHT - 3);
        
        char status[32];
        snprintf(status, sizeof(status), "%d/%d", index + 1, device_count);
        print(status);
        
        display_show();
        
        // Input
        int8_t dir = rotary_pcnt_read(&encoder);
        
        if (dir > 0) {
            if (index < device_count - 1) {
                index++;
                if (index >= scroll_offset + visible) scroll_offset++;
            }
        } else if (dir < 0) {
            if (index > 0) {
                index--;
                if (index < scroll_offset) scroll_offset--;
            }
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            delay(200);
            
            // Add target
            char ip_str[16], mac_str[18];
            
            if (devices[index].ip != 0) {
                snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
                        (int)((devices[index].ip >> 0) & 0xFF),
                        (int)((devices[index].ip >> 8) & 0xFF),
                        (int)((devices[index].ip >> 16) & 0xFF),
                        (int)((devices[index].ip >> 24) & 0xFF));
            } else {
                strcpy(ip_str, "0.0.0.0");
            }
            
            snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                    devices[index].mac[0], devices[index].mac[1], devices[index].mac[2],
                    devices[index].mac[3], devices[index].mac[4], devices[index].mac[5]);
            
            if (arp_poison_add_target(ip_str, mac_str)) {
                display_clear();
                set_cursor(2, 10);
                println("Target Added!");
                println("");
                println(ip_str);
                display_show();
                delay(1000);
            }
            break;
        }
        
        // Hold to exit
        static uint32_t hold_start = 0;
        if (gpio_get_level((gpio_num_t)encoder.pin_sw) == 0) {
            if (hold_start == 0) hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - hold_start) > 1000) break;
        } else {
            hold_start = 0;
        }
        
        delay(5);
    }
    
    back_to_arp_poison_menu();
}

// Start ARP poisoning attack
static void arp_poison_start_attack(void) {
    if (!router_configured) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("Configure router");
        println("first!");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_arp_poison_menu();
        return;
    }
    
    ARPPoison *stats = arp_poison_get_stats();
    if (stats->target_count == 0) {
        display_clear();
        set_cursor(2, 10);
        println("No targets!");
        println("");
        println("Add targets first");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_arp_poison_menu();
        return;
    }
    
    // Confirmation
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("ARP POISON ATTACK");
    println("");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Targets: %d", stats->target_count);
    println(buf);
    
    println("Router:");
    println(router_ip);
    println("");
    println("⚠️  This will MITM");
    println("all victim traffic!");
    println("");
    println("Enable DNS Spoof");
    println("to hijack domains");
    println("");
    println("Press: Start");
    println("Hold: Cancel");
    display_show();
    
    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint8_t confirmed = 0;
    
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) < 5000) {
        rotary_pcnt_read(&encoder);
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            confirmed = 1;
            break;
        }
        
        delay(10);
    }
    
    if (!confirmed) {
        back_to_arp_poison_menu();
        return;
    }
    
    delay(200);
    
    // Start attack
    display_clear();
    set_cursor(2, 10);
    println("Starting...");
    display_show();
    
    if (arp_poison_start(router_ip, router_mac)) {
        display_clear();
        set_cursor(2, 8);
        println("🎯 ARP POISON ACTIVE!");
        println("");
        
        snprintf(buf, sizeof(buf), "Targets: %d", stats->target_count);
        println(buf);
        
        println("");
        println("Traffic flowing");
        println("through ESP32!");
        println("");
        println("Watch serial log");
        println("for intercepted");
        println("packets");
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Failed to start!");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_arp_poison_menu();
}

// Stop attack
static void arp_poison_stop_attack(void) {
    arp_poison_stop();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("ARP Poison Stopped");
    println("");
    
    ARPPoison *stats = arp_poison_get_stats();
    
    char buf[32];
    snprintf(buf, sizeof(buf), "ARP sent: %lu", stats->arp_sent);
    println(buf);
    
    snprintf(buf, sizeof(buf), "Intercepted: %lu", stats->packets_intercepted);
    println(buf);
    
    println("");
    println("ARP tables restored");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_arp_poison_menu();
}

// Show statistics
static void arp_poison_show_stats(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("ARP Poison Stats");
    println("");
    
    ARPPoison *stats = arp_poison_get_stats();
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Status: %s", 
             arp_poison_is_running() ? "RUNNING" : "STOPPED");
    println(buf);
    
    snprintf(buf, sizeof(buf), "Targets: %d", stats->target_count);
    println(buf);
    
    snprintf(buf, sizeof(buf), "ARP sent: %lu", stats->arp_sent);
    println(buf);
    
    snprintf(buf, sizeof(buf), "Intercepted: %lu", stats->packets_intercepted);
    println(buf);
    
    snprintf(buf, sizeof(buf), "Forwarded: %lu", stats->packets_forwarded);
    println(buf);
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_arp_poison_menu();
}

// Clear targets
static void arp_poison_clear_targets_menu(void) {
    arp_poison_clear_targets();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Targets Cleared");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_arp_poison_menu();
}

// Info screen
static void arp_poison_show_info(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("ARP POISONING");
    println("");
    println("Man-in-the-Middle");
    println("attack on existing");
    println("WiFi networks");
    println("");
    println("How it works:");
    println("1. Connect to WiFi");
    println("2. Send fake ARP");
    println("3. Victims think");
    println("   YOU are the router");
    println("4. Traffic flows");
    println("   through ESP32");
    println("5. Hijack DNS/HTTP");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_arp_poison_menu();
}

// Initialize menu
void arp_poison_menu_init(void) {
    ESP_LOGI(TAG, "Initializing ARP Poison menu");
    
    menu_init(&arp_poison_menu, "ARP Poison");
    menu_add_item_icon(&arp_poison_menu, "I", "Info", arp_poison_show_info);
    menu_add_item_icon(&arp_poison_menu, "S", "Scan Network", arp_poison_scan_devices);
    menu_add_item_icon(&arp_poison_menu, "R", "Config Router", arp_poison_configure_router);
    menu_add_item_icon(&arp_poison_menu, "T", "Add Target", arp_poison_add_target_menu);
    menu_add_item_icon(&arp_poison_menu, "*", "Target ALL", arp_poison_target_all_menu);
    menu_add_item_icon(&arp_poison_menu, "A", "Start Attack", arp_poison_start_attack);
    menu_add_item_icon(&arp_poison_menu, "X", "Stop", arp_poison_stop_attack);
    menu_add_item_icon(&arp_poison_menu, "?", "Statistics", arp_poison_show_stats);
    menu_add_item_icon(&arp_poison_menu, "C", "Clear Targets", arp_poison_clear_targets_menu);
    menu_add_item_icon(&arp_poison_menu, "<", "Back", back_to_main);
}

void arp_poison_menu_open(void) {
    menu_set_status("ARP Poison");
    menu_set_active(&arp_poison_menu);
    menu_draw();
}
