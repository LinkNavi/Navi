#include "wifi_thingies_menu.h"
#include "menu.h"
#include "wifi_bridge.h"
#include "wifi_bridge_runtime.h"
#include "modules/wifi_spam.h"
#include "drivers/wifi.h"
#include "drivers/display.h"
#include "drivers/rotary_pcnt.h"
#include "rotary_text_input.h"
#include "esp_log.h"
#include <string.h>
#include "esp_timer.h"  
#include "esp_random.h"
static const char *TAG = "WiFi_Thingies";

extern RotaryPCNT encoder;
extern void back_to_main(void);

Menu wifi_thingies_main_menu;
Menu wifi_bridge_menu;
Menu wifi_spam_menu;

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

static void back_to_spam_menu(void) {
    menu_set_status("Spam");
    menu_set_active(&wifi_spam_menu);
    menu_draw();
}

// ========== BRIDGE FUNCTIONS ==========

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

// ========== BEACON SPAM FUNCTIONS ==========


static void spam_configure_power(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    SpamConfig *cfg = spam_get_config();
    uint8_t power_level = cfg->tx_power / 4; // Convert to dBm
    
    println("TX Power Config");
    println("");
    println("Turn: Adjust");
    println("Press: Confirm");
    println("");
    println("Range:");
    println("Low:  2dBm  (30ft)");
    println("Med: 11dBm (100ft)");
    println("High: 21dBm (200ft)");
    
    char msg[32];
    snprintf(msg, sizeof(msg), "> %ddBm", power_level);
    println("");
    println(msg);
    display_show();
    
    while (1) {
        int8_t dir = rotary_pcnt_read(&encoder);
        
        if (dir > 0) {
            power_level++;
            if (power_level > 21) power_level = 21;
        } else if (dir < 0) {
            power_level--;
            if (power_level < 2) power_level = 2;
        }
        
        if (dir != 0) {
            display_clear();
            set_cursor(2, 10);
            println("TX Power Config");
            println("");
            
            // Show range estimate
            const char *range_str;
            if (power_level < 8) range_str = "~30-50ft";
            else if (power_level < 14) range_str = "~50-100ft";
            else if (power_level < 18) range_str = "~100-150ft";
            else range_str = "~150-200ft";
            
            print("Power: ");
            snprintf(msg, sizeof(msg), "%ddBm", power_level);
            println(msg);
            print("Range: ");
            println(range_str);
            println("");
            println("Turn: Adjust");
            println("Press: Save");
            display_show();
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            delay(200);
            break;
        }
        
        delay(10);
    }
    
    spam_set_tx_power(power_level * 4);
    
    display_clear();
    set_cursor(2, 10);
    println("Saved!");
    display_show();
    delay(500);
    
    back_to_spam_menu();
}

static void spam_configure_interval(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    SpamConfig *cfg = spam_get_config();
    uint16_t interval = cfg->beacon_interval;
    
    println("Beacon Interval");
    println("");
    println("Turn: Adjust");
    println("Press: Confirm");
    println("");
    println("Lower = Faster");
    println("Higher = Stable");
    
    char msg[32];
    snprintf(msg, sizeof(msg), "> %dms", interval);
    println("");
    println(msg);
    display_show();
    
    while (1) {
        int8_t dir = rotary_pcnt_read(&encoder);
        
        if (dir > 0) {
            interval += 50;
            if (interval > 1000) interval = 1000;
        } else if (dir < 0) {
            interval -= 50;
            if (interval < 50) interval = 50;
        }
        
        if (dir != 0) {
            display_clear();
            set_cursor(2, 10);
            println("Beacon Interval");
            println("");
            
            snprintf(msg, sizeof(msg), "Interval: %dms", interval);
            println(msg);
            println("");
            
            if (interval < 100) {
                println("FAST - Aggressive");
            } else if (interval < 200) {
                println("NORMAL - Balanced");
            } else {
                println("SLOW - Conservative");
            }
            
            println("");
            println("Turn: Adjust");
            println("Press: Save");
            display_show();
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            delay(200);
            break;
        }
        
        delay(10);
    }
    
    spam_set_interval(interval);
    
    display_clear();
    set_cursor(2, 10);
    println("Saved!");
    display_show();
    delay(500);
    
    back_to_spam_menu();
}

static void spam_toggle_random_macs(void) {
    SpamConfig *cfg = spam_get_config();
    spam_set_random_macs(!cfg->random_macs);
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Random MACs");
    println("");
    println(cfg->random_macs ? "ENABLED" : "DISABLED");
    println("");
    
    if (cfg->random_macs) {
        println("Each beacon gets");
        println("a new random MAC");
        println("");
        println("Harder to filter");
    } else {
        println("Sequential MACs");
        println("");
        println("Easier to track");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spam_menu();
}

static void spam_add_custom(void) {
    char ssid[33] = "";
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Add Custom SSID");
    println("");
    println("Enter network name");
    println("to broadcast");
    println("");
    snprintf(ssid, sizeof(ssid), "%d/%d used", spam_get_ssid_count(), MAX_CUSTOM_SSIDS);
    println(ssid);
    println("");
    println("Press to start");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(300);
    
    ssid[0] = '\0';
    if (!text_input_get(&encoder, "Custom SSID", ssid, sizeof(ssid), NULL)) {
        back_to_spam_menu();
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
        back_to_spam_menu();
        return;
    }
    
    if (spam_add_custom_ssid(ssid)) {
        display_clear();
        set_cursor(2, 10);
        println("Added!");
        println("");
        println(ssid);
        println("");
        println("Custom list now:");
        char msg[32];
        snprintf(msg, sizeof(msg), "%d networks", custom_ssid_count);
        println(msg);
        println("");
        println("Press to continue");
        display_show();
    } else {
        display_clear();
        set_cursor(2, 10);
        println("List full!");
        println("");
        println("Max 32 custom");
        println("networks");
        println("");
        println("Press to continue");
        display_show();
    }
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spam_menu();
}

static void spam_toggle_list(void) {
    SpamConfig *cfg = spam_get_config();
    spam_use_custom_list(!cfg->use_custom_list);
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("SSID List Mode");
    println("");
    
    if (cfg->use_custom_list) {
        println("CUSTOM LIST");
        println("");
        char msg[32];
        snprintf(msg, sizeof(msg), "%d custom SSIDs", custom_ssid_count);
        println(msg);
        
        if (custom_ssid_count == 0) {
            println("");
            println("WARNING: List empty!");
            println("Add SSIDs first");
        }
    } else {
        println("DEFAULT LIST");
        println("");
        println("20 funny networks");
        println("built-in");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spam_menu();
}

static void spam_clear_custom(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Clear custom list?");
    println("");
    char msg[32];
    snprintf(msg, sizeof(msg), "%d networks", custom_ssid_count);
    println(msg);
    println("will be deleted");
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
            println("Clear custom list?");
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
        spam_clear_custom_ssids();
        
        display_clear();
        set_cursor(2, 10);
        println("Cleared!");
        println("");
        println("Custom list empty");
        display_show();
        delay(1000);
    }
    
    back_to_spam_menu();
}

// Update spam_show_status to show config
static void spam_show_status(void) {
    SpamConfig *cfg = spam_get_config();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Beacon Spam");
    println("");
    
    if (spam_is_running()) {
        println("Status: ACTIVE");
    } else {
        println("Status: Stopped");
    }
    
    println("");
    println("Config:");
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Power: %ddBm", cfg->tx_power / 4);
    println(msg);
    
    snprintf(msg, sizeof(msg), "Interval: %dms", cfg->beacon_interval);
    println(msg);
    
    println(cfg->random_macs ? "MACs: Random" : "MACs: Fixed");
    
    if (cfg->use_custom_list) {
        snprintf(msg, sizeof(msg), "List: Custom (%d)", custom_ssid_count);
    } else {
        snprintf(msg, sizeof(msg), "List: Default (20)");
    }
    println(msg);
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spam_menu();
}

static void spam_start_beacon(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Starting beacon");
    println("spam...");
    println("");
    println("This will flood");
    println("WiFi lists with");
    println("fake networks!");
    println("");
    println("Broadcasting:");
    println("20 SSIDs");
    display_show();
    delay(1500);
    
    if (spam_start()) {
        display_clear();
        set_cursor(2, 10);
        println("Beacon spam");
        println("ACTIVE!");
        println("");
        println("Check WiFi list");
        println("on other devices");
        println("");
        println("You should see:");
        println("- FBI Van");
        println("- Virus Point");
        println("- NSA Post");
        println("- Pretty Fly WiFi");
        println("...and 16 more!");
        println("");
        println("Press to continue");
        display_show();
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Failed to start!");
        println("");
        println("WiFi may be busy");
        println("");
        println("Press to continue");
        display_show();
    }
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spam_menu();
}

static void spam_stop_beacon(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Stopping spam...");
    println("");
    println("Halting beacon");
    println("broadcasts");
    display_show();
    delay(500);
    
    spam_stop();
    
    display_clear();
    set_cursor(2, 10);
    println("Spam stopped!");
    println("");
    println("WiFi list should");
    println("clear shortly");
    println("");
    println("Fake networks");
    println("will disappear");
    println("in ~30 seconds");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spam_menu();
}

static void spam_about(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Beacon Spam");
    println("");
    println("Broadcasts fake");
    println("WiFi networks");
    println("");
    println("How it works:");
    println("- Sends 802.11");
    println("  beacon frames");
    println("- Random MACs");
    println("- 20 SSIDs");
    println("- 100ms interval");
    println("");
    println("Networks appear");
    println("real but aren't");
    println("connectable!");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spam_menu();
}

// ========== MENU NAVIGATION ==========

static void wifi_thingies_open_bridge(void) {
    menu_set_status("Bridge");
    menu_set_active(&wifi_bridge_menu);
    menu_draw();
}

static void wifi_thingies_open_spam(void) {
    menu_set_status("Spam");
    menu_set_active(&wifi_spam_menu);
    menu_draw();
}

void wifi_thingies_open(void) {
    menu_set_status("Thingies");
    menu_set_active(&wifi_thingies_main_menu);
    menu_draw();
}

// ========== MENU INITIALIZATION ==========

void wifi_thingies_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi Thingies menus");
    
    // Initialize bridge system
    bridge_init();
    
    // Main thingies menu
    menu_init(&wifi_thingies_main_menu, "WiFi Thingies");
    menu_add_item_icon(&wifi_thingies_main_menu, "B", "Bridge Mode", wifi_thingies_open_bridge);
    menu_add_item_icon(&wifi_thingies_main_menu, "S", "Beacon Spam", wifi_thingies_open_spam);
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
    
    // Beacon Spam submenu
    menu_init(&wifi_spam_menu, "Beacon Spam");
    menu_add_item_icon(&wifi_spam_menu, "I", "Status", spam_show_status);
    menu_add_item_icon(&wifi_spam_menu, "S", "Start Spam", spam_start_beacon);
    menu_add_item_icon(&wifi_spam_menu, "T", "Stop Spam", spam_stop_beacon);
    menu_add_item_icon(&wifi_spam_menu, "P", "TX Power", spam_configure_power);
    menu_add_item_icon(&wifi_spam_menu, "V", "Interval", spam_configure_interval);
    menu_add_item_icon(&wifi_spam_menu, "M", "Random MACs", spam_toggle_random_macs);
    menu_add_item_icon(&wifi_spam_menu, "L", "Toggle List", spam_toggle_list);
    menu_add_item_icon(&wifi_spam_menu, "A", "Add Custom", spam_add_custom);

    menu_add_item_icon(&wifi_spam_menu, "<", "Back", back_to_thingies_main);
}
