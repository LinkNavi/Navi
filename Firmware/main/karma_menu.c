// karma_menu.c - Karma Attack Menu for ESP32
#include "include/menu.h"
#include "include/modules/wifi_karma.h"
#include "include/drivers/display.h"
#include "include/drivers/rotary_pcnt.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "Karma_Menu";

extern RotaryPCNT encoder;
extern void back_to_main(void);

static Menu karma_main_menu;
static Menu karma_targets_menu;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Navigation
static void goto_karma_main(void) {
    menu_set_status("Karma");
    menu_set_active(&karma_main_menu);
    menu_draw();
}

static void goto_targets_menu(void) {
    menu_set_status("Targets");
    menu_set_active(&karma_targets_menu);
    menu_draw();
}

// Start passive listening
static void karma_start_passive_mode(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    if (karma_start_passive()) {
        println("Karma Started!");
        println("");
        println("Listening for");
        println("probe requests");
        println("");
        println("Collecting SSIDs");
        println("that devices are");
        println("searching for...");
    } else {
        println("Failed to start!");
        println("");
        println("Already running?");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    goto_karma_main();
}

// Stop karma
static void karma_stop_collection(void) {
    karma_stop();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Karma Stopped");
    println("");
    
    uint16_t unique, probes;
    karma_get_stats(&unique, &probes);
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Unique: %d", unique);
    println(msg);
    snprintf(msg, sizeof(msg), "Probes: %d", probes);
    println(msg);
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    goto_karma_main();
}

// Show statistics
static void karma_show_stats(void) {
    uint16_t unique, probes;
    karma_get_stats(&unique, &probes);
    
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("Karma Statistics");
    println("");
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Status: %s", karma_is_running() ? "RUNNING" : "STOPPED");
    println(msg);
    
    snprintf(msg, sizeof(msg), "Unique SSIDs: %d", unique);
    println(msg);
    
    snprintf(msg, sizeof(msg), "Total Probes: %d", probes);
    println(msg);
    
    if (karma_get_current_ap()) {
        println("");
        println("Fake AP Active:");
        println(karma_get_current_ap());
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    goto_karma_main();
}

// View discovered targets
static void karma_view_targets(void) {
    uint8_t count = karma_target_count;
    
    if (count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No targets yet!");
        println("");
        println("Start passive");
        println("mode first");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        goto_karma_main();
        return;
    }
    
    // Sort by activity
    karma_sort_by_activity();
    
    uint8_t selected = 0;
    uint8_t scroll_offset = 0;
    
    while (1) {
        display_clear();
        
        // Title
        fill_rect(0, 0, WIDTH, 12, 1);
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        
        const char *title = "Karma Targets";
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
        
        // List targets
        uint8_t visible = (HEIGHT - 24) / 10;
        uint8_t y = 14;
        
        for (uint8_t i = scroll_offset; i < count && i < scroll_offset + visible; i++) {
            KarmaTarget *target = karma_get_target(i);
            
            if (i == selected) {
                fill_rect(2, y, WIDTH - 4, 10, 1);
            }
            
            set_cursor(4, y + 7);
            
            char display_text[24];
            snprintf(display_text, sizeof(display_text), "%.15s (%d)", 
                    target->ssid, target->probe_count);
            
            if (i == selected) {
                // Inverted
                const char *s = display_text;
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
                print(display_text);
            }
            
            y += 10;
        }
        
        // Status bar
        draw_hline(0, HEIGHT - 10, WIDTH, 1);
        set_cursor(2, HEIGHT - 3);
        char status[32];
        snprintf(status, sizeof(status), "%d/%d", selected + 1, count);
        print(status);
        
        display_show();
        
        // Input
        int8_t dir = rotary_pcnt_read(&encoder);
        
        if (dir > 0) {
            if (selected < count - 1) {
                selected++;
                if (selected >= scroll_offset + visible) scroll_offset++;
            }
        } else if (dir < 0) {
            if (selected > 0) {
                selected--;
                if (selected < scroll_offset) scroll_offset--;
            }
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            delay(200);
            
            // Selected - create fake AP
            KarmaTarget *target = karma_get_target(selected);
            karma_create_fake_ap(target->ssid);
            
            display_clear();
            set_cursor(2, 10);
            println("Fake AP Created!");
            println("");
            println(target->ssid);
            println("");
            println("Waiting for");
            println("victim to connect");
            display_show();
            delay(3000);
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
    
    goto_karma_main();
}

// Clear targets
static void karma_clear_all_targets(void) {
    karma_clear_targets();
    
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
    goto_karma_main();
}

// Show info about karma
static void karma_show_info(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("Karma Attack");
    println("");
    println("Collects WiFi probe");
    println("requests to learn");
    println("what networks");
    println("devices search for");
    println("");
    println("PASSIVE: Safe");
    println("ACTIVE: Creates");
    println("  fake APs");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    goto_karma_main();
}

static void karma_start_auto_mode(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("AUTO-RESPOND MODE");
    println("");
    println("⚠️  WARNING  ⚠️");
    println("");
    println("This will create");
    println("fake APs for");
    println("discovered probes");
    println("");
    println("Press: Start");
    println("Hold: Cancel");
    display_show();
    
    // Confirmation
    uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint8_t confirmed = 0;
    
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start_time) < 5000) {
        rotary_pcnt_read(&encoder);
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            confirmed = 1;
            break;
        }
        
        delay(10);
    }
    
    if (!confirmed) {
        goto_karma_main();
        return;
    }
    
    delay(200);
    
    // Show configuration
    display_clear();
    set_cursor(2, 10);
    
    KarmaConfig *cfg = karma_get_config();
    
    println("Starting...");
    println("");
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Listen: %ds", cfg->listen_time);
    println(msg);
    snprintf(msg, sizeof(msg), "AP Time: %ds", cfg->ap_time);
    println(msg);
    snprintf(msg, sizeof(msg), "Min Probes: %d", cfg->min_probes);
    println(msg);
    
    display_show();
    
    if (karma_start_auto_respond()) {
        delay(2000);
        
        display_clear();
        set_cursor(2, 10);
        println("AUTO-RESPOND");
        println("ACTIVE!");
        println("");
        println("Cycling:");
        println("Listen → AP");
        println("→ Listen → AP");
        println("");
        println("Check serial log");
        println("for activity");
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
    goto_karma_main();
}

// Configure auto-respond settings
static void karma_configure_auto(void) {
    KarmaConfig *cfg = karma_get_config();
    uint8_t setting = 0; // 0=listen_time, 1=ap_time, 2=min_probes
    
    while (1) {
        display_clear();
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        
        println("Auto-Respond Config");
        println("");
        
        char msg[32];
        
        // Listen time
        if (setting == 0) print("> ");
        else print("  ");
        snprintf(msg, sizeof(msg), "Listen: %ds", cfg->listen_time);
        println(msg);
        
        // AP time
        if (setting == 1) print("> ");
        else print("  ");
        snprintf(msg, sizeof(msg), "AP Time: %ds", cfg->ap_time);
        println(msg);
        
        // Min probes
        if (setting == 2) print("> ");
        else print("  ");
        snprintf(msg, sizeof(msg), "Min Probes: %d", cfg->min_probes);
        println(msg);
        
        println("");
        println("Turn: Change");
        println("Press: Next");
        println("Hold: Save");
        
        display_show();
        
        // Input
        int8_t dir = rotary_pcnt_read(&encoder);
        
        if (dir != 0) {
            switch (setting) {
                case 0: // Listen time
                    if (dir > 0 && cfg->listen_time < 60) cfg->listen_time++;
                    if (dir < 0 && cfg->listen_time > 1) cfg->listen_time--;
                    break;
                case 1: // AP time
                    if (dir > 0 && cfg->ap_time < 120) cfg->ap_time += 5;
                    if (dir < 0 && cfg->ap_time > 5) cfg->ap_time -= 5;
                    break;
                case 2: // Min probes
                    if (dir > 0 && cfg->min_probes < 10) cfg->min_probes++;
                    if (dir < 0 && cfg->min_probes > 1) cfg->min_probes--;
                    break;
            }
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            setting = (setting + 1) % 3;
            delay(200);
        }
        
        // Hold to save
        static uint32_t hold_start = 0;
        if (gpio_get_level((gpio_num_t)encoder.pin_sw) == 0) {
            if (hold_start == 0) hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - hold_start) > 1000) {
                display_clear();
                set_cursor(2, HEIGHT/2);
                println("Saved!");
                display_show();
                delay(1000);
                break;
            }
        } else {
            hold_start = 0;
        }
        
        delay(50);
    }
    
    goto_karma_main();
}

// Update menu_init to include new options:
static void karma_menu_init(void) {
    ESP_LOGI(TAG, "Initializing Karma menus");
    
    menu_init(&karma_main_menu, "Karma Attack");
    menu_add_item_icon(&karma_main_menu, "I", "Info", karma_show_info);
    menu_add_item_icon(&karma_main_menu, "L", "Start Passive", karma_start_passive_mode);
    menu_add_item_icon(&karma_main_menu, "A", "Auto-Respond", karma_start_auto_mode);      // NEW
    menu_add_item_icon(&karma_main_menu, "C", "Configure Auto", karma_configure_auto);    // NEW
    menu_add_item_icon(&karma_main_menu, "X", "Stop", karma_stop_collection);
    menu_add_item_icon(&karma_main_menu, "?", "Statistics", karma_show_stats);
    menu_add_item_icon(&karma_main_menu, "T", "View Targets", karma_view_targets);
    menu_add_item_icon(&karma_main_menu, "D", "Clear Targets", karma_clear_all_targets);
    menu_add_item_icon(&karma_main_menu, "<", "Back", back_to_main);
}

void karma_menu_open(void) {
    menu_set_status("Karma");
    menu_set_active(&karma_main_menu);
    menu_draw();
}
