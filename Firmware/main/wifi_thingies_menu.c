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

// ========== BRIDGE FUNCTIONS (unchanged) ==========

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

static void bridge_config_upstream(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Scanning networks");
    println("for upstream...");
    display_show();
    
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
    
    uint8_t selected = 0;
    uint8_t scroll_offset = 0;
    
    while (1) {
        display_clear();
        
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
                            if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
                                framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                        }
                        bit_idx++;
                    }
                }
                tx += g->xAdvance;
            }
            t++;
        }
        
        draw_hline(0, 12, WIDTH, 1);
        
        uint8_t visible = (HEIGHT - 24) / 10;
        uint8_t y = 14;
        
        for (uint8_t i = scroll_offset; i < ap_count && i < scroll_offset + visible; i++) {
            if (i == selected) fill_rect(2, y, WIDTH - 4, 10, 1);
            
            set_cursor(4, y + 7);
            print(ap_list[i].authmode != WIFI_AUTH_OPEN ? "L" : "O");
            set_cursor(cursor_x + 2, y + 7);
            
            char ssid_display[18];
            strncpy(ssid_display, (char *)ap_list[i].ssid, 17);
            ssid_display[17] = '\0';
            
            if (i == selected) {
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
                print(ssid_display);
            }
            y += 10;
        }
        
        draw_hline(0, HEIGHT - 10, WIDTH, 1);
        set_cursor(2, HEIGHT - 3);
        char status[32];
        snprintf(status, sizeof(status), "%d/%d", selected + 1, ap_count);
        print(status);
        display_show();
        
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir > 0 && selected < ap_count - 1) {
            selected++;
            if (selected >= scroll_offset + visible) scroll_offset++;
        } else if (dir < 0 && selected > 0) {
            selected--;
            if (selected < scroll_offset) scroll_offset--;
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) { delay(200); break; }
        
        static uint32_t hold_start = 0;
        if (gpio_get_level((gpio_num_t)encoder.pin_sw) == 0) {
            if (hold_start == 0) hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - hold_start) > 1000) {
                back_to_bridge_menu(); return;
            }
        } else hold_start = 0;
        delay(5);
    }
    
    char ssid[33];
    char password[64] = "";
    strncpy(ssid, (char *)ap_list[selected].ssid, 32);
    ssid[32] = '\0';
    
    if (ap_list[selected].authmode != WIFI_AUTH_OPEN) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("Enter password for:");
        println(ssid);
        println("");
        println("Press to start");
        display_show();
        
        while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
        delay(300);
        
        if (!text_input_get(&encoder, "Upstream Pass", password, sizeof(password), NULL)) {
            back_to_bridge_menu(); return;
        }
    }
    
    bridge_set_upstream(ssid, password);
    bridge_save_config();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Upstream saved!");
    println(ssid);
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_bridge_menu();
}

static void bridge_config_ap(void) {
    char ssid[32] = "";
    char password[64] = "";
    BridgeConfig *cfg = bridge_get_config();
    strncpy(ssid, cfg->bridge_ssid, sizeof(ssid) - 1);
    strncpy(password, cfg->bridge_password, sizeof(password) - 1);
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Bridge AP Name");
    println("Press to start");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(300);
    
    if (!text_input_get(&encoder, "Bridge SSID", ssid, sizeof(ssid), ssid)) {
        back_to_bridge_menu(); return;
    }
    if (strlen(ssid) == 0) { back_to_bridge_menu(); return; }
    
    display_clear();
    set_cursor(2, 10);
    println("Bridge Password");
    println("Press to start");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(300);
    
    if (!text_input_get(&encoder, "Bridge Pass", password, sizeof(password), password)) {
        back_to_bridge_menu(); return;
    }
    
    bridge_set_ap(ssid, password);
    bridge_save_config();
    
    display_clear();
    set_cursor(2, 10);
    println("Bridge AP saved!");
    println(ssid);
    println("");
    println("Press to continue");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_bridge_menu();
}

static void bridge_toggle_enabled(void) {
    BridgeConfig *cfg = bridge_get_config();
    cfg->enabled = !cfg->enabled;
    bridge_save_config();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println(cfg->enabled ? "Bridge ENABLED" : "Bridge DISABLED");
    println("");
    println("Press to continue");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_bridge_menu();
}

static void bridge_start_now(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Starting bridge...");
    display_show();
    delay(500);
    
    if (bridge_start()) {
        BridgeConfig *cfg = bridge_get_config();
        display_clear();
        set_cursor(2, 10);
        println("Bridge started!");
        println("");
        println("AP:");
        println(cfg->bridge_ssid);
        println("IP: 192.168.4.1");
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Bridge failed!");
        println("Check config");
    }
    println("");
    println("Press to continue");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_bridge_menu();
}

static void bridge_stop_now(void) {
    bridge_stop();
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Bridge stopped!");
    println("");
    println("Press to continue");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_bridge_menu();
}

static void bridge_clear_all(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Clear config?");
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
            println(confirm ? "> YES" : "  YES");
            println(confirm ? "  NO" : "> NO");
            display_show();
        }
        if (rotary_pcnt_button_pressed(&encoder)) { delay(200); break; }
        delay(10);
    }
    
    if (confirm) {
        bridge_clear_config();
        display_clear();
        set_cursor(2, 10);
        println("Config cleared!");
        println("");
        println("Press to continue");
        display_show();
        while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
        delay(200);
    }
    back_to_bridge_menu();
}

// ========== BEACON SPAM FUNCTIONS ==========

static void spam_show_status(void) {
    SpamConfig *cfg = spam_get_config();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Beacon Spam");
    println("");
    println(spam_is_running() ? "Status: ACTIVE" : "Status: Stopped");
    println("");
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Power: %ddBm", cfg->tx_power / 4);
    println(msg);
    
    snprintf(msg, sizeof(msg), "Delay: %dms", cfg->per_ssid_delay);
    println(msg);
    
    snprintf(msg, sizeof(msg), "Burst: %dx", cfg->burst_count);
    println(msg);
    
    snprintf(msg, sizeof(msg), "Dupes: %dx", cfg->duplicates);
    println(msg);
    
    uint8_t ssids = spam_get_ssid_count();
    uint16_t total = ssids * cfg->duplicates;
    snprintf(msg, sizeof(msg), "Networks: %d", total);
    println(msg);
    
    if (cfg->use_custom_list) {
        snprintf(msg, sizeof(msg), "List: Custom (%d)", custom_ssid_count);
    } else {
        snprintf(msg, sizeof(msg), "List: Default (%d)", NUM_DEFAULT_SSIDS);
    }
    println(msg);
    
    println(cfg->randomize_order ? "Order: Random" : "Order: Sequential");
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_spam_menu();
}

static void spam_start_beacon(void) {
    SpamConfig *cfg = spam_get_config();
    uint16_t total = spam_get_ssid_count() * cfg->duplicates;
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Starting spam...");
    println("");
    char msg[32];
    snprintf(msg, sizeof(msg), "%d fake networks", total);
    println(msg);
    display_show();
    delay(500);
    
    if (spam_start()) {
        display_clear();
        set_cursor(2, 10);
        println("Spam ACTIVE!");
        println("");
        snprintf(msg, sizeof(msg), "%d beacons cycling", spam_get_beacon_count());
        println(msg);
        println("");
        println("Check WiFi list");
        println("on other devices");
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Failed to start!");
    }
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_spam_menu();
}

static void spam_stop_beacon(void) {
    spam_stop();
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Spam stopped!");
    println("");
    println("Networks disappear");
    println("in ~30 seconds");
    println("");
    println("Press to continue");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_spam_menu();
}

static void spam_configure_power(void) {
    SpamConfig *cfg = spam_get_config();
    uint8_t power_level = cfg->tx_power / 4;
    char msg[32];
    
    while (1) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("TX Power");
        println("");
        snprintf(msg, sizeof(msg), "> %ddBm", power_level);
        println(msg);
        println("");
        const char *range = power_level < 8 ? "~30ft" : power_level < 14 ? "~100ft" : power_level < 18 ? "~150ft" : "~200ft";
        print("Range: "); println(range);
        println("");
        println("Turn: Adjust");
        println("Press: Save");
        display_show();
        
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir > 0 && power_level < 21) power_level++;
        if (dir < 0 && power_level > 2) power_level--;
        if (rotary_pcnt_button_pressed(&encoder)) { delay(200); break; }
        delay(10);
    }
    
    spam_set_tx_power(power_level * 4);
    back_to_spam_menu();
}

static void spam_configure_delay(void) {
    SpamConfig *cfg = spam_get_config();
    uint8_t del = cfg->per_ssid_delay;
    char msg[32];
    
    while (1) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("Per-SSID Delay");
        println("");
        snprintf(msg, sizeof(msg), "> %dms", del);
        println(msg);
        println("");
        if (del == 0) println("MAX SPEED (0ms)");
        else if (del <= 2) println("Very fast");
        else if (del <= 5) println("Fast");
        else println("Conservative");
        println("");
        println("Turn: Adjust");
        println("Press: Save");
        display_show();
        
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir > 0 && del < 50) del++;
        if (dir < 0 && del > 0) del--;
        if (rotary_pcnt_button_pressed(&encoder)) { delay(200); break; }
        delay(10);
    }
    
    spam_set_per_ssid_delay(del);
    back_to_spam_menu();
}

static void spam_configure_duplicates(void) {
    SpamConfig *cfg = spam_get_config();
    uint8_t dups = cfg->duplicates;
    char msg[32];
    
    while (1) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("Duplicates");
        println("(per SSID)");
        println("");
        snprintf(msg, sizeof(msg), "> %dx", dups);
        println(msg);
        println("");
        uint16_t total = spam_get_ssid_count() * dups;
        snprintf(msg, sizeof(msg), "= %d networks", total);
        println(msg);
        println("");
        println("Higher = more visible");
        println("networks shown");
        println("");
        println("Turn: Adjust");
        println("Press: Save");
        display_show();
        
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir > 0 && dups < 10) dups++;
        if (dir < 0 && dups > 1) dups--;
        if (rotary_pcnt_button_pressed(&encoder)) { delay(200); break; }
        delay(10);
    }
    
    spam_set_duplicates(dups);
    // Rebuild if already running
    if (spam_is_running()) spam_rebuild();
    back_to_spam_menu();
}

static void spam_configure_burst(void) {
    SpamConfig *cfg = spam_get_config();
    uint8_t burst = cfg->burst_count;
    char msg[32];
    
    while (1) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("Burst Count");
        println("");
        snprintf(msg, sizeof(msg), "> %dx", burst);
        println(msg);
        println("");
        println("Sends per beacon");
        println("Higher = more");
        println("reliable visibility");
        println("");
        println("Turn: Adjust");
        println("Press: Save");
        display_show();
        
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir > 0 && burst < 10) burst++;
        if (dir < 0 && burst > 1) burst--;
        if (rotary_pcnt_button_pressed(&encoder)) { delay(200); break; }
        delay(10);
    }
    
    spam_set_burst(burst);
    back_to_spam_menu();
}

static void spam_toggle_random_order(void) {
    SpamConfig *cfg = spam_get_config();
    spam_set_random_macs(!cfg->randomize_order);
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Send Order");
    println("");
    println(cfg->randomize_order ? "RANDOM" : "SEQUENTIAL");
    println("");
    println("Press to continue");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_spam_menu();
}

static void spam_add_custom(void) {
    char ssid[33] = "";
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Add Custom SSID");
    println("Press to start");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(300);
    
    if (!text_input_get(&encoder, "Custom SSID", ssid, sizeof(ssid), NULL)) {
        back_to_spam_menu(); return;
    }
    if (strlen(ssid) == 0) { back_to_spam_menu(); return; }
    
    if (spam_add_custom_ssid(ssid)) {
        display_clear();
        set_cursor(2, 10);
        println("Added!");
        println(ssid);
        char msg[32];
        snprintf(msg, sizeof(msg), "Total: %d", custom_ssid_count);
        println(msg);
    } else {
        display_clear();
        set_cursor(2, 10);
        println("List full!");
    }
    println("");
    println("Press to continue");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_spam_menu();
}

static void spam_toggle_list(void) {
    SpamConfig *cfg = spam_get_config();
    spam_use_custom_list(!cfg->use_custom_list);
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println(cfg->use_custom_list ? "CUSTOM LIST" : "DEFAULT LIST");
    println("");
    char msg[32];
    snprintf(msg, sizeof(msg), "%d SSIDs", spam_get_ssid_count());
    println(msg);
    if (cfg->use_custom_list && custom_ssid_count == 0) {
        println("WARNING: Empty!");
    }
    println("");
    println("Press to continue");
    display_show();
    while(!rotary_pcnt_button_pressed(&encoder)) { rotary_pcnt_read(&encoder); delay(10); }
    delay(200);
    back_to_spam_menu();
}

static void spam_clear_custom(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Clear custom list?");
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
            println(confirm ? "> YES" : "  YES");
            println(confirm ? "  NO" : "> NO");
            display_show();
        }
        if (rotary_pcnt_button_pressed(&encoder)) { delay(200); break; }
        delay(10);
    }
    
    if (confirm) {
        spam_clear_custom_ssids();
        display_clear();
        set_cursor(2, 10);
        println("Cleared!");
        display_show();
        delay(500);
    }
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

// ========== INIT ==========

void wifi_thingies_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi Thingies menus");
    
    bridge_init();
    
    menu_init(&wifi_thingies_main_menu, "WiFi Thingies");
    menu_add_item_icon(&wifi_thingies_main_menu, "B", "Bridge Mode", wifi_thingies_open_bridge);
    menu_add_item_icon(&wifi_thingies_main_menu, "S", "Beacon Spam", wifi_thingies_open_spam);
    menu_add_item_icon(&wifi_thingies_main_menu, "<", "Back", back_to_main);
    
    menu_init(&wifi_bridge_menu, "Bridge Mode");
    menu_add_item_icon(&wifi_bridge_menu, "V", "View Config", bridge_show_config);
    menu_add_item_icon(&wifi_bridge_menu, "U", "Set Upstream", bridge_config_upstream);
    menu_add_item_icon(&wifi_bridge_menu, "A", "Set Bridge AP", bridge_config_ap);
    menu_add_item_icon(&wifi_bridge_menu, "S", "Start Bridge", bridge_start_now);
    menu_add_item_icon(&wifi_bridge_menu, "T", "Stop Bridge", bridge_stop_now);
    menu_add_item_icon(&wifi_bridge_menu, "X", "Clear Config", bridge_clear_all);
    menu_add_item_icon(&wifi_bridge_menu, "<", "Back", back_to_thingies_main);
    
    menu_init(&wifi_spam_menu, "Beacon Spam");
    menu_add_item_icon(&wifi_spam_menu, "I", "Status", spam_show_status);
    menu_add_item_icon(&wifi_spam_menu, "S", "Start Spam", spam_start_beacon);
    menu_add_item_icon(&wifi_spam_menu, "T", "Stop Spam", spam_stop_beacon);
    menu_add_item_icon(&wifi_spam_menu, "D", "Duplicates", spam_configure_duplicates);
    menu_add_item_icon(&wifi_spam_menu, "P", "TX Power", spam_configure_power);
    menu_add_item_icon(&wifi_spam_menu, "V", "Speed", spam_configure_delay);
    menu_add_item_icon(&wifi_spam_menu, "B", "Burst", spam_configure_burst);
    menu_add_item_icon(&wifi_spam_menu, "R", "Shuffle", spam_toggle_random_order);
    // Note: MAX_MENU_ITEMS is 8, so custom SSID management needs a submenu or
    // you can increase MAX_MENU_ITEMS. For now the most important configs are here.
     menu_add_item_icon(&wifi_spam_menu, "L", "Toggle List", spam_toggle_list);
     menu_add_item_icon(&wifi_spam_menu, "A", "Add Custom", spam_add_custom);
    menu_add_item_icon(&wifi_spam_menu, "<", "Back", back_to_thingies_main);
}
