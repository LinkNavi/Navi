// wifi_menu.c - WiFi menu with rotary encoder text input
#include "wifi_menu.h"
#include "drivers/wifi.h"
#include "drivers/display.h"
#include "drivers/rotary_pcnt.h"
#include "rotary_text_input.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WiFi_Menu";

extern RotaryPCNT encoder;
extern void back_to_main(void);

Menu wifi_main_menu;
Menu wifi_scan_menu;


static wifi_ap_record_t ap_list[20];

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void back_to_wifi_main(void) {
    menu_set_status("WiFi");
    menu_set_active(&wifi_main_menu);
    menu_draw();
}

static void wifi_show_status(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    if (wifi_is_connected()) {
        println("WiFi: Connected");
        println("");
        
        char ip_str[32];
        wifi_get_ip_string(ip_str, sizeof(ip_str));
        print("IP: ");
        println(ip_str);
    } else {
        println("WiFi: Disconnected");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_wifi_main();
}

static void wifi_connect_saved(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Loading saved");
    println("credentials...");
    display_show();
    
    char ssid[32];
    char password[64];
    
    if (!wifi_load_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        display_clear();
        set_cursor(2, 10);
        println("No saved");
        println("credentials!");
        println("");
        println("Use Manual Setup");
        println("to configure WiFi");
        println("");
        println("Press to continue");
        display_show();
        
        while(!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_wifi_main();
        return;
    }
    
    display_clear();
    set_cursor(2, 10);
    println("Connecting to:");
    println(ssid);
    println("");
    println("Please wait...");
    display_show();
    
    if (wifi_init_sta(ssid, password)) {
        display_clear();
        set_cursor(2, 10);
        println("Connected!");
        println("");
        
        char ip_str[32];
        wifi_get_ip_string(ip_str, sizeof(ip_str));
        print("IP: ");
        println(ip_str);
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Failed to");
        println("connect!");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_wifi_main();
}

static void wifi_disconnect_network(void) {
    wifi_disconnect();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Disconnected");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_wifi_main();
}

// NEW: Manual WiFi setup using rotary encoder text input
static void wifi_manual_setup(void) {
    char ssid[32] = "";
    char password[64] = "";
    
    // Step 1: Enter SSID
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Manual WiFi Setup");
    println("");
    println("Step 1: Enter SSID");
    println("");
    println("Press to start");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(300);
    
    if (!text_input_get(&encoder, "WiFi SSID", ssid, sizeof(ssid), NULL)) {
        // Cancelled
        back_to_wifi_main();
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
        back_to_wifi_main();
        return;
    }
    
    // Step 2: Enter password
    display_clear();
    set_cursor(2, 10);
    println("Manual WiFi Setup");
    println("");
    println("Step 2: Password");
    println("");
    println("Press to start");
    println("(hold to skip)");
    display_show();
    
    uint32_t wait_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint8_t skip_password = 0;
    
    while(1) {
        rotary_pcnt_read(&encoder);
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            break;
        }
        
        // Hold to skip password (open network)
        if (gpio_get_level((gpio_num_t)encoder.pin_sw) == 0) {
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS - wait_start) > 1000) {
                skip_password = 1;
                break;
            }
        }
        
        delay(10);
    }
    delay(300);
    
    if (!skip_password) {
        if (!text_input_get(&encoder, "WiFi Password", password, sizeof(password), NULL)) {
            // Cancelled
            back_to_wifi_main();
            return;
        }
    }
    
    // Step 3: Confirm and connect
    display_clear();
    set_cursor(2, 10);
    println("Connect to:");
    println("");
    println(ssid);
    if (strlen(password) > 0) {
        println("Password: ****");
    } else {
        println("(Open network)");
    }
    println("");
    println("Save & Connect?");
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
            println("Connect to:");
            println("");
            println(ssid);
            if (strlen(password) > 0) {
                println("Password: ****");
            } else {
                println("(Open network)");
            }
            println("");
            println("Save & Connect?");
            println("");
            if (confirm) {
                println("> YES");
                println("  NO");
            } else {
                println("  YES");
                println("> NO");
            }
            display_show();
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            delay(200);
            break;
        }
        delay(10);
    }
    
    if (!confirm) {
        back_to_wifi_main();
        return;
    }
    
    // Save credentials
    if (wifi_save_credentials(ssid, password)) {
        ESP_LOGI(TAG, "Credentials saved");
    }
    
    // Connect
    display_clear();
    set_cursor(2, 10);
    println("Connecting...");
    println("");
    println(ssid);
    display_show();
    
    if (wifi_init_sta(ssid, password)) {
        display_clear();
        set_cursor(2, 10);
        println("Connected!");
        println("");
        
        char ip_str[32];
        wifi_get_ip_string(ip_str, sizeof(ip_str));
        print("IP: ");
        println(ip_str);
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Connection");
        println("failed!");
        println("");
        println("Check SSID and");
        println("password");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_wifi_main();
}

static void wifi_scan_and_display(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Scanning...");
    display_show();
    
    // Scan for networks
    uint16_t ap_count = wifi_scan_networks(ap_list, 20);
    
    if (ap_count == 0) {
        display_clear();
        set_cursor(2, 10);
        println("No networks");
        println("found!");
        println("");
        println("Press to continue");
        display_show();
        
        while(!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_wifi_main();
        return;
    }
    
    // Display networks with scrolling
    uint8_t selected = 0;
    uint8_t scroll_offset = 0;
    
    while (1) {
        display_clear();
        
        // Title bar
        fill_rect(0, 0, WIDTH, 12, 1);
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        
        // Draw "Networks" inverted
        const char *title = "Networks";
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
            
            // Selected a network - get password if needed
            char ssid[33];
            char password[64] = "";
            strncpy(ssid, (char *)ap_list[selected].ssid, 32);
            ssid[32] = '\0';
            
            if (ap_list[selected].authmode != WIFI_AUTH_OPEN) {
                // Needs password
                display_clear();
                set_cursor(2, 10);
                println("Enter password");
                println("for:");
                println("");
                println(ssid);
                println("");
                println("Press to start");
                display_show();
                
                while(!rotary_pcnt_button_pressed(&encoder)) {
                    rotary_pcnt_read(&encoder);
                    delay(10);
                }
                delay(300);
                
                if (!text_input_get(&encoder, "WiFi Password", password, sizeof(password), NULL)) {
                    // Cancelled
                    continue;
                }
            }
            
            // Save and connect
            wifi_save_credentials(ssid, password);
            
            display_clear();
            set_cursor(2, 10);
            println("Connecting...");
            println(ssid);
            display_show();
            
            if (wifi_init_sta(ssid, password)) {
                display_clear();
                set_cursor(2, 10);
                println("Connected!");
                println("");
                
                char ip_str[32];
                wifi_get_ip_string(ip_str, sizeof(ip_str));
                print("IP: ");
                println(ip_str);
            } else {
                display_clear();
                set_cursor(2, 10);
                println("Failed!");
            }
            
            println("");
            println("Press to continue");
            display_show();
            
            while(!rotary_pcnt_button_pressed(&encoder)) {
                rotary_pcnt_read(&encoder);
                delay(10);
            }
            delay(200);
            break;
        }
        
        // Hold to exit
        static uint32_t hold_start = 0;
        if (gpio_get_level((gpio_num_t)encoder.pin_sw) == 0) {
            if (hold_start == 0) {
                hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            } else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - hold_start) > 1000) {
                break;
            }
        } else {
            hold_start = 0;
        }
        
        delay(5);
    }
    
    back_to_wifi_main();
}

void wifi_menu_open(void) {
    menu_set_status("WiFi");
    menu_set_active(&wifi_main_menu);
    menu_draw();
}

void wifi_menu_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi menus");
    
    menu_init(&wifi_main_menu, "WiFi");
    menu_add_item_icon(&wifi_main_menu, "C", "Connect", wifi_connect_saved);
    menu_add_item_icon(&wifi_main_menu, "M", "Manual Setup", wifi_manual_setup);
    menu_add_item_icon(&wifi_main_menu, "S", "Scan", wifi_scan_and_display);
    menu_add_item_icon(&wifi_main_menu, "I", "Status", wifi_show_status);
    menu_add_item_icon(&wifi_main_menu, "D", "Disconnect", wifi_disconnect_network);
    menu_add_item_icon(&wifi_main_menu, "<", "Back", back_to_main);
}
