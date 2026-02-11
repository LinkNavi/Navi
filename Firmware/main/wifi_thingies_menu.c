// wifi_thingies_menu.c - WiFi advanced features menu
#include "wifi_thingies_menu.h"
#include "menu.h"
#include "wifi_bridge.h"
#include "wifi_bridge_runtime.h"
#include "drivers/wifi.h"
#include "drivers/display.h"
#include "drivers/rotary_pcnt.h"
#include "rotary_text_input.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WiFi_Thingies";

extern RotaryPCNT encoder;
extern void back_to_main(void);

Menu wifi_thingies_main_menu;
Menu wifi_bridge_menu;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void back_to_thingies_main(void) {
    menu_set_status("Thingies");
    menu_set_active(&wifi_thingies_main_menu);
    menu_draw();
}

static void back_to_bridge_menu(void) {
    menu_set_status("Bridge");
    menu_set_active(&wifi_bridge_menu);
    menu_draw();
}

// Show current bridge config
static void bridge_show_config(void) {
    BridgeConfig *cfg = bridge_get_config();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Bridge Config:");
    println("");
    
    println("Upstream WiFi:");
    if (cfg->upstream_ssid[0]) {
        print("  ");
        println(cfg->upstream_ssid);
        print("  Pass: ");
        if (cfg->upstream_password[0]) {
            println("****");
        } else {
            println("(none)");
        }
    } else {
        println("  (not set)");
    }
    
    println("");
    println("Bridge AP:");
    print("  ");
    println(cfg->bridge_ssid);
    print("  Pass: ");
    println(cfg->bridge_password);
    
    println("");
    print("Status: ");
    println(cfg->enabled ? "Enabled" : "Disabled");
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_bridge_menu();
}

// Configure upstream network using WiFi scan
static void bridge_config_upstream(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Scanning networks");
    println("for upstream...");
    display_show();
    
    // Scan for networks
    wifi_ap_record_t ap_list[20];
    uint16_t ap_count = wifi_scan_networks(ap_list, 20);
    
    if (ap_count == 0) {
        display_clear();
        set_cursor(2, 10);
        println("No networks found!");
        println("");
        println("Press to continue");
        display_show();
        
        while(!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_bridge_menu();
        return;
    }
    
    // Display networks for selection
    uint8_t selected = 0;
    uint8_t scroll_offset = 0;
    
    while (1) {
        display_clear();
        
        // Title bar
        fill_rect(0, 0, WIDTH, 12, 1);
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        
        const char *title = "Select Upstream";
        const char *t = title;
        int16_t tx = 2;
        while (*t) {
            if (*t >= TomThumb.first && *t <= TomThumb.last) {
                const GFXglyph *g = &TomThumb.glyph[*t - TomThumb.first];
                const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
                
                uint16_t bit_idx = 0;
                for (uint8_t yy = 0; yy < g->height; yy++) {
                    for (uint8_t xx = 0; xx < g->width; xx++) {
                        if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                            int16_t px = tx + g->xOffset + xx;
                            int16_t py = 8 + g->yOffset + yy;
                            if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                                framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                            }
                        }
                        bit_idx++;
                    }
                }
                tx += g->xAdvance;
            }
            t++;
        }
        
        draw_hline(0, 12, WIDTH, 1);
        
        // List networks
        uint8_t visible = (HEIGHT - 24) / 10;
        uint8_t y = 14;
        
        for (uint8_t i = scroll_offset; i < ap_count && i < scroll_offset + visible; i++) {
            if (i == selected) {
                fill_rect(2, y, WIDTH - 4, 10, 1);
            }
            
            set_cursor(4, y + 7);
            
            // Lock icon for secured networks
            if (ap_list[i].authmode != WIFI_AUTH_OPEN) {
                print("L");
            } else {
                print("O");
            }
            
            set_cursor(cursor_x + 2, y + 7);
            
            // Truncate SSID if too long
            char ssid_display[18];
            strncpy(ssid_display, (char *)ap_list[i].ssid, 17);
            ssid_display[17] = '\0';
            
            if (i == selected) {
                // Inverted text
                const char *s = ssid_display;
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
                                    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                                        framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                                    }
                                }
                                bit_idx++;
                            }
                        }
                        cursor_x += g->xAdvance;
                    }
                    s++;
                }
            } else {
                print(ssid_display);
            }
            
            y += 10;
        }
        
        // Status bar
        draw_hline(0, HEIGHT - 10, WIDTH, 1);
        set_cursor(2, HEIGHT - 3);
        char status[32];
        snprintf(status, sizeof(status), "%d/%d", selected + 1, ap_count);
        print(status);
        
        display_show();
        
        // Handle input
        int8_t dir = rotary_pcnt_read(&encoder);
        
        if (dir > 0) {
            if (selected < ap_count - 1) {
                selected++;
                if (selected >= scroll_offset + visible) {
                    scroll_offset++;
                }
            }
        } else if (dir < 0) {
            if (selected > 0) {
                selected--;
                if (selected < scroll_offset) {
                    scroll_offset--;
                }
            }
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            delay(200);
            break;
        }
        
        // Hold to cancel
        static uint32_t hold_start = 0;
        if (gpio_get_level((gpio_num_t)encoder.pin_sw) == 0) {
            if (hold_start == 0) {
                hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            } else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - hold_start) > 1000) {
                back_to_bridge_menu();
                return;
            }
        } else {
            hold_start = 0;
        }
        
        delay(5);
    }
    
    // Selected a network
    char ssid[33];
    char password[64] = "";
    strncpy(ssid, (char *)ap_list[selected].ssid, 32);
    ssid[32] = '\0';
    
    // Check if network needs password
    if (ap_list[selected].authmode != WIFI_AUTH_OPEN) {
        // Needs password
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("Enter password");
        println("for:");
        println("");
        println(ssid);
        println("");
        
        // Show security type
        const char *auth_str = "Unknown";
        switch (ap_list[selected].authmode) {
            case WIFI_AUTH_WEP: auth_str = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: auth_str = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: auth_str = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: auth_str = "WPA/WPA2"; break;
            case WIFI_AUTH_WPA3_PSK: auth_str = "WPA3"; break;
            case WIFI_AUTH_WPA2_WPA3_PSK: auth_str = "WPA2/WPA3"; break;
            default: break;
        }
        print("Security: ");
        println(auth_str);
        
        println("");
        println("Press to start");
        display_show();
        
        while(!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(300);
        
        if (!text_input_get(&encoder, "Upstream Pass", password, sizeof(password), NULL)) {
            back_to_bridge_menu();
            return;
        }
        
        if (strlen(password) == 0) {
            display_clear();
            set_cursor(2, 10);
            println("Password required");
            println("for secure network!");
            println("");
            println("Press to continue");
            display_show();
            
            while(!rotary_pcnt_button_pressed(&encoder)) {
                rotary_pcnt_read(&encoder);
                delay(10);
            }
            delay(200);
            back_to_bridge_menu();
            return;
        }
    } else {
        // Open network - no password needed
        ESP_LOGI(TAG, "Selected open network: %s", ssid);
    }
    
    // Save config
    bridge_set_upstream(ssid, password);
    bridge_save_config();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Upstream saved!");
    println("");
    println(ssid);
    
    if (ap_list[selected].authmode != WIFI_AUTH_OPEN) {
        println("Password: ****");
    } else {
        println("(Open network)");
    }
    
    println("");
    println("Saved to flash");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_bridge_menu();
}

// Configure bridge AP
static void bridge_config_ap(void) {
    char ssid[32] = "";
    char password[64] = "";
    BridgeConfig *cfg = bridge_get_config();
    
    // Load existing
    strncpy(ssid, cfg->bridge_ssid, sizeof(ssid) - 1);
    strncpy(password, cfg->bridge_password, sizeof(password) - 1);
    
    // Step 1: Enter AP SSID
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Bridge AP Name");
    println("");
    println("This is the WiFi");
    println("name you'll see");
    println("");
    println("Press to start");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(300);
    
    if (!text_input_get(&encoder, "Bridge SSID", ssid, sizeof(ssid), ssid)) {
        back_to_bridge_menu();
        return;
    }
    
    if (strlen(ssid) == 0) {
        display_clear();
        set_cursor(2, 10);
        println("SSID required!");
        println("");
        println("Press to continue");
        display_show();
        
        while(!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_bridge_menu();
        return;
    }
    
    // Step 2: Enter AP password
    display_clear();
    set_cursor(2, 10);
    println("Bridge Password");
    println("");
    println("Password to");
    println("connect to bridge");
    println("");
    println("Press to start");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(300);
    
    if (!text_input_get(&encoder, "Bridge Pass", password, sizeof(password), password)) {
        back_to_bridge_menu();
        return;
    }
    
    // Save config
    bridge_set_ap(ssid, password);
    bridge_save_config();
    
    display_clear();
    set_cursor(2, 10);
    println("Bridge AP saved!");
    println("");
    println(ssid);
    print("Pass: ");
    println(password);
    println("");
    println("Saved to flash");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_bridge_menu();
}

// Toggle bridge enabled/disabled
static void bridge_toggle_enabled(void) {
    BridgeConfig *cfg = bridge_get_config();
    
    cfg->enabled = !cfg->enabled;
    bridge_save_config();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Bridge mode:");
    println("");
    if (cfg->enabled) {
        println("ENABLED");
        println("");
        println("Will start bridge");
        println("on next boot");
    } else {
        println("DISABLED");
    }
    println("");
    println("Saved to flash");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_bridge_menu();
}

// Start the bridge
static void bridge_start_now(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Starting bridge...");
    display_show();
    delay(500);
    
    if (bridge_start()) {
        display_clear();
        set_cursor(2, 10);
        println("Bridge started!");
        println("");
        
        BridgeConfig *cfg = bridge_get_config();
        println("AP Name:");
        println(cfg->bridge_ssid);
        println("");
        println("AP IP:");
        println("192.168.4.1");
        println("");
        println("Press to continue");
        display_show();
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Bridge failed!");
        println("");
        println("Check config:");
        println("- Set Upstream");
        println("- Set Bridge AP");
        println("- Enable it");
        println("");
        println("Press to continue");
        display_show();
    }
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_bridge_menu();
}

// Stop the bridge
static void bridge_stop_now(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Stopping bridge...");
    display_show();
    delay(500);
    
    bridge_stop();
    
    display_clear();
    set_cursor(2, 10);
    println("Bridge stopped!");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_bridge_menu();
}

// Clear all bridge config
static void bridge_clear_all(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Clear bridge");
    println("config?");
    println("");
    println("This will delete");
    println("all saved bridge");
    println("settings");
    println("");
    println("Turn: Yes/No");
    println("Press: Confirm");
    display_show();
    
    uint8_t confirm = 0;
    while (1) {
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir != 0) {
            confirm = !confirm;
            display_clear();
            set_cursor(2, 10);
            println("Clear config?");
            println("");
            if (confirm) {
                println("> YES - Clear");
                println("  NO  - Keep");
            } else {
                println("  YES - Clear");
                println("> NO  - Keep");
            }
            display_show();
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            delay(200);
            break;
        }
        delay(10);
    }
    
    if (confirm) {
        bridge_clear_config();
        
        display_clear();
        set_cursor(2, 10);
        println("Config cleared!");
        println("");
        println("All bridge settings");
        println("have been deleted");
        println("");
        println("Press to continue");
        display_show();
    } else {
        back_to_bridge_menu();
        return;
    }
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_bridge_menu();
}

// Open bridge menu
void wifi_thingies_open_bridge(void) {
    menu_set_status("Bridge");
    menu_set_active(&wifi_bridge_menu);
    menu_draw();
}

// Open main thingies menu
void wifi_thingies_open(void) {
    menu_set_status("Thingies");
    menu_set_active(&wifi_thingies_main_menu);
    menu_draw();
}

// Initialize all menus
void wifi_thingies_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi Thingies menus");
    
    // Initialize bridge system
    bridge_init();
    
    // Main thingies menu
    menu_init(&wifi_thingies_main_menu, "WiFi Thingies");
    menu_add_item_icon(&wifi_thingies_main_menu, "B", "Bridge Mode", wifi_thingies_open_bridge);
    menu_add_item_icon(&wifi_thingies_main_menu, "<", "Back", back_to_main);
    
    // Bridge submenu
    menu_init(&wifi_bridge_menu, "Bridge Mode");
    menu_add_item_icon(&wifi_bridge_menu, "V", "View Config", bridge_show_config);
    menu_add_item_icon(&wifi_bridge_menu, "U", "Set Upstream", bridge_config_upstream);
    menu_add_item_icon(&wifi_bridge_menu, "A", "Set Bridge AP", bridge_config_ap);
    menu_add_item_icon(&wifi_bridge_menu, "S", "Start Bridge", bridge_start_now);
    menu_add_item_icon(&wifi_bridge_menu, "T", "Stop Bridge", bridge_stop_now);
    menu_add_item_icon(&wifi_bridge_menu, "X", "Clear Config", bridge_clear_all);
    menu_add_item_icon(&wifi_bridge_menu, "<", "Back", back_to_thingies_main);
}
