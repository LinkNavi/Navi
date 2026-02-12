// null_ssid_spam_menu.c - Null SSID Spam Menu
#include "include/menu.h"
#include "include/modules/null_ssid_spam.h"
#include "include/drivers/display.h"
#include "include/drivers/rotary_pcnt.h"
#include "esp_log.h"

static const char *TAG = "Null_SSID_Menu";

extern RotaryPCNT encoder;
extern void back_to_main(void);

static Menu null_ssid_menu;
static uint8_t selected_channel = 1;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void back_to_null_ssid_menu(void) {
    menu_set_status("Null SSID");
    menu_set_active(&null_ssid_menu);
    menu_draw();
}

// Info screen
static void null_ssid_show_info(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("NULL SSID ATTACK");
    println("");
    println("Crashes iOS WiFi");
    println("Settings by spamming");
    println("30 networks with");
    println("empty SSIDs");
    println("");
    println("Effect:");
    println("- iOS freezes when");
    println("  viewing WiFi list");
    println("- May require force");
    println("  restart");
    println("- Works on iPhone,");
    println("  iPad, Mac");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_null_ssid_menu();
}

// Select channel
static void null_ssid_select_channel(void) {
    uint8_t channel = selected_channel;
    
    while (1) {
        display_clear();
        
        // Title
        fill_rect(0, 0, WIDTH, 12, 1);
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        
        const char *title = "Select Channel";
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
        
        // Channel display
        set_cursor(WIDTH/2 - 15, HEIGHT/2);
        char ch_str[8];
        snprintf(ch_str, sizeof(ch_str), "Ch %d", channel);
        print(ch_str);
        
        // Instructions
        set_cursor(2, HEIGHT - 10);
        print("Rotate: Change");
        set_cursor(2, HEIGHT - 3);
        print("Press: Confirm");
        
        display_show();
        
        // Input
        int8_t dir = rotary_pcnt_read(&encoder);
        
        if (dir > 0) {
            if (channel < 14) channel++;
        } else if (dir < 0) {
            if (channel > 1) channel--;
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            selected_channel = channel;
            delay(200);
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
    
    back_to_null_ssid_menu();
}

// Start attack
static void null_ssid_start_attack(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("NULL SSID ATTACK");
    println("");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Channel: %d", selected_channel);
    println(buf);
    println("");
    println("⚠️  WARNING!");
    println("");
    println("This will CRASH");
    println("nearby iOS devices");
    println("when they open WiFi");
    println("Settings!");
    println("");
    println("They may need to");
    println("force-restart");
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
        back_to_null_ssid_menu();
        return;
    }
    
    delay(200);
    
    // Start attack
    display_clear();
    set_cursor(2, 10);
    println("Starting attack...");
    display_show();
    
    if (null_ssid_spam_start(selected_channel)) {
        display_clear();
        set_cursor(2, 8);
        println("💀 NULL SSID ACTIVE!");
        println("");
        
        snprintf(buf, sizeof(buf), "Channel: %d", selected_channel);
        println(buf);
        println("");
        println("Spamming 30 null");
        println("SSIDs continuously");
        println("");
        println("iOS WiFi Settings");
        println("will freeze!");
        println("");
        println("Watch serial log");
        println("for packet count");
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
    back_to_null_ssid_menu();
}

// Stop attack
static void null_ssid_stop_attack(void) {
    null_ssid_spam_stop();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Attack Stopped");
    println("");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Sent: %lu beacons", null_ssid_spam_get_packet_count());
    println(buf);
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_null_ssid_menu();
}

// Show statistics
static void null_ssid_show_stats(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("Null SSID Stats");
    println("");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Status: %s", 
             null_ssid_spam_is_running() ? "RUNNING" : "STOPPED");
    println(buf);
    
    if (null_ssid_spam_is_running()) {
        snprintf(buf, sizeof(buf), "Channel: %d", selected_channel);
        println(buf);
    }
    
    snprintf(buf, sizeof(buf), "Beacons: %lu", null_ssid_spam_get_packet_count());
    println(buf);
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_null_ssid_menu();
}

// Initialize menu
void null_ssid_menu_init(void) {
    ESP_LOGI(TAG, "Initializing Null SSID menu");
    
    menu_init(&null_ssid_menu, "Null SSID");
    menu_add_item_icon(&null_ssid_menu, "I", "Info", null_ssid_show_info);
    menu_add_item_icon(&null_ssid_menu, "C", "Channel", null_ssid_select_channel);
    menu_add_item_icon(&null_ssid_menu, "A", "Start Attack", null_ssid_start_attack);
    menu_add_item_icon(&null_ssid_menu, "X", "Stop", null_ssid_stop_attack);
    menu_add_item_icon(&null_ssid_menu, "?", "Statistics", null_ssid_show_stats);
    menu_add_item_icon(&null_ssid_menu, "<", "Back", back_to_main);
}

void null_ssid_menu_open(void) {
    menu_set_status("Null SSID");
    menu_set_active(&null_ssid_menu);
    menu_draw();
}
