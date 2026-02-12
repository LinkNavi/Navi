
// evil_twin_menu.c - Evil Twin Attack Menu
#include "include/menu.h"
#include "include/modules/evil_twin.h"
#include "include/drivers/display.h"
#include "include/drivers/rotary_pcnt.h"
#include "include/drivers/wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"

static const char *TAG = "Evil_Twin_Menu";

extern RotaryPCNT encoder;
extern void back_to_main(void);

static Menu evil_twin_menu;

// Scanned networks
static wifi_ap_record_t scanned_aps[20];
static uint8_t scanned_count = 0;
static uint8_t selected_ap_index = 0;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void back_to_evil_twin_menu(void) {
    menu_set_status("Evil Twin");
    menu_set_active(&evil_twin_menu);
    menu_draw();
}

// Scan for networks
static void evil_twin_scan_networks(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Scanning WiFi...");
    println("");
    println("Looking for targets");
    display_show();
    
    // Initialize WiFi for scanning if needed
    static uint8_t wifi_scan_init = 0;
    if (!wifi_scan_init) {
        esp_err_t err;
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        if (!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
            esp_netif_create_default_wifi_sta();
        }
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        wifi_scan_init = 1;
    }
    
    // Scan
    wifi_scan_config_t scan_config = {0};
    scan_config.show_hidden = false;
    esp_wifi_scan_start(&scan_config, true);
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 20) ap_count = 20;
    
    esp_wifi_scan_get_ap_records(&ap_count, scanned_aps);
    scanned_count = ap_count;
    
    display_clear();
    set_cursor(2, 10);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Found %d networks", scanned_count);
    println(buf);
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_evil_twin_menu();
}

// Select target network
static void evil_twin_select_target(void) {
    if (scanned_count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No networks found!");
        println("");
        println("Scan first");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_evil_twin_menu();
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
        
        const char *title = "Select Target";
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
        
        // List networks
        uint8_t visible = (HEIGHT - 24) / 10;
        uint8_t y = 14;
        
        for (uint8_t i = scroll_offset; i < scanned_count && i < scroll_offset + visible; i++) {
            if (i == index) {
                fill_rect(2, y, WIDTH - 4, 10, 1);
            }
            
            set_cursor(4, y + 7);
            
            // Security icon
            if (scanned_aps[i].authmode != WIFI_AUTH_OPEN) {
                print("L");
            } else {
                print("O");
            }
            
            set_cursor(cursor_x + 2, y + 7);
            
            // SSID (truncated)
            char ssid_display[18];
            snprintf(ssid_display, sizeof(ssid_display), "%.15s", (char *)scanned_aps[i].ssid);
            
            if (i == index) {
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
        
        // Status bar
        draw_hline(0, HEIGHT - 10, WIDTH, 1);
        set_cursor(2, HEIGHT - 3);
        
        char status[32];
        snprintf(status, sizeof(status), "%d/%d Ch%d %ddBm", 
                 index + 1, scanned_count,
                 scanned_aps[index].primary,
                 scanned_aps[index].rssi);
        print(status);
        
        display_show();
        
        // Input
        int8_t dir = rotary_pcnt_read(&encoder);
        
        if (dir > 0) {
            if (index < scanned_count - 1) {
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
            selected_ap_index = index;
            delay(200);
            
            display_clear();
            set_cursor(2, 10);
            println("Target Selected!");
            println("");
            println((char *)scanned_aps[index].ssid);
            display_show();
            delay(1500);
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
    
    back_to_evil_twin_menu();
}

// Start Evil Twin with deauth
static void evil_twin_start_with_deauth(void) {
    if (scanned_count == 0 || selected_ap_index >= scanned_count) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No target selected!");
        println("");
        println("Scan and select");
        println("a network first");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_evil_twin_menu();
        return;
    }
    
    // Confirmation
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("EVIL TWIN ATTACK");
    println("");
    println("Target:");
    println((char *)scanned_aps[selected_ap_index].ssid);
    println("");
    println("Mode: Clone + Deauth");
    println("");
    println("⚠️  This will:");
    println("1. Clone target AP");
    println("2. Kick victims off");
    println("3. Capture credentials");
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
        back_to_evil_twin_menu();
        return;
    }
    
    delay(200);
    
    // Start attack
    display_clear();
    set_cursor(2, 10);
    println("Starting...");
    display_show();
    
    if (evil_twin_start((char *)scanned_aps[selected_ap_index].ssid,
                        scanned_aps[selected_ap_index].bssid,
                        scanned_aps[selected_ap_index].primary,
                        1)) { // Enable deauth
        display_clear();
        set_cursor(2, 8);
        println("🎭 EVIL TWIN ACTIVE!");
        println("");
        println("Clone:");
        println((char *)scanned_aps[selected_ap_index].ssid);
        println("");
        
        char buf[32];
        snprintf(buf, sizeof(buf), "Ch: %d", scanned_aps[selected_ap_index].primary);
        println(buf);
        println("");
        println("Deauth: ATTACKING");
        println("Portal: 192.168.4.1");
        println("");
        println("Watch serial log");
        println("for victims!");
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
    back_to_evil_twin_menu();
}

// Start without deauth (passive)
static void evil_twin_start_passive(void) {
    if (scanned_count == 0 || selected_ap_index >= scanned_count) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No target selected!");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_evil_twin_menu();
        return;
    }
    
    display_clear();
    set_cursor(2, 10);
    println("Starting passive...");
    display_show();
    
    if (evil_twin_start((char *)scanned_aps[selected_ap_index].ssid,
                        scanned_aps[selected_ap_index].bssid,
                        scanned_aps[selected_ap_index].primary,
                        0)) { // No deauth
        display_clear();
        set_cursor(2, 10);
        println("Evil Twin (Passive)");
        println("");
        println((char *)scanned_aps[selected_ap_index].ssid);
        println("");
        println("No deauth attack");
        println("Waiting for victims");
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
    back_to_evil_twin_menu();
}

// Stop attack
static void evil_twin_stop_attack(void) {
    evil_twin_stop();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Evil Twin Stopped");
    println("");
    
    EvilTwin *stats = evil_twin_get_stats();
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Deauths: %lu", stats->total_deauths);
    println(buf);
    
    snprintf(buf, sizeof(buf), "Victims: %lu", stats->victims_connected);
    println(buf);
    
    snprintf(buf, sizeof(buf), "Captured: %lu", stats->credentials_captured);
    println(buf);
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_evil_twin_menu();
}

// Show statistics
static void evil_twin_show_stats(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("Evil Twin Stats");
    println("");
    
    EvilTwin *stats = evil_twin_get_stats();
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Status: %s", 
             evil_twin_is_running() ? "RUNNING" : "STOPPED");
    println(buf);
    
    if (evil_twin_is_running()) {
        println("");
        println("Target:");
        println(evil_twin_get_target_ssid());
        println("");
    }
    
    snprintf(buf, sizeof(buf), "Deauths: %lu", stats->total_deauths);
    println(buf);
    
    snprintf(buf, sizeof(buf), "Victims: %lu", stats->victims_connected);
    println(buf);
    
    snprintf(buf, sizeof(buf), "Captured: %lu", stats->credentials_captured);
    println(buf);
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_evil_twin_menu();
}

// Info screen
static void evil_twin_show_info(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("EVIL TWIN ATTACK");
    println("");
    println("Clones a real WiFi");
    println("network and tricks");
    println("victims into");
    println("connecting");
    println("");
    println("With deauth:");
    println("- Kicks victims off");
    println("- Forces reconnect");
    println("- They connect to");
    println("  YOUR fake AP!");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_evil_twin_menu();
}

// Initialize menu
void evil_twin_menu_init(void) {
    ESP_LOGI(TAG, "Initializing Evil Twin menu");
    
    menu_init(&evil_twin_menu, "Evil Twin");
    menu_add_item_icon(&evil_twin_menu, "I", "Info", evil_twin_show_info);
    menu_add_item_icon(&evil_twin_menu, "S", "Scan Networks", evil_twin_scan_networks);
    menu_add_item_icon(&evil_twin_menu, "T", "Select Target", evil_twin_select_target);
    menu_add_item_icon(&evil_twin_menu, "A", "Start Attack", evil_twin_start_with_deauth);
    menu_add_item_icon(&evil_twin_menu, "P", "Passive Mode", evil_twin_start_passive);
    menu_add_item_icon(&evil_twin_menu, "X", "Stop", evil_twin_stop_attack);
    menu_add_item_icon(&evil_twin_menu, "?", "Statistics", evil_twin_show_stats);
    menu_add_item_icon(&evil_twin_menu, "<", "Back", back_to_main);
}

void evil_twin_menu_open(void) {
    menu_set_status("Evil Twin");
    menu_set_active(&evil_twin_menu);
    menu_draw();
}
