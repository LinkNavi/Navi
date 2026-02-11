// xbegone_menu.c - X-BE-GONE using embedded IR database
#include "xbegone_menu.h"
#include "ir_database.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "XBEGONE";

Menu xbegone_main_menu;
Menu xbegone_power_menu;
Menu xbegone_category_menu;
Menu xbegone_volume_menu;
Menu xbegone_channel_menu;
Menu xbegone_misc_menu;

char xbegone_selected_category[32] = "";

// Hold-to-cancel tracking
static uint32_t cancel_hold_start = 0;
#define CANCEL_HOLD_MS 600

// Pin for direct button read during blast (avoid rotary state issues)
#define BLAST_SW_PIN 7

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static inline uint32_t now_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

// Check if button is being held to cancel
// Returns 1 if cancelled
static uint8_t check_cancel(void) {
    if (gpio_get_level((gpio_num_t)BLAST_SW_PIN) == 0) {
        if (cancel_hold_start == 0) {
            cancel_hold_start = now_ms();
        } else if (now_ms() - cancel_hold_start >= CANCEL_HOLD_MS) {
            return 1;
        }
    } else {
        cancel_hold_start = 0;
    }
    return 0;
}

// Progress callback — draws progress, checks cancel
// Only redraws every few devices to avoid I2C/watchdog issues
static uint8_t blast_progress(const BlastProgress *p) {
    // Check cancel first (cheap, no display)
    if (check_cancel()) return 0;
    
    // Only update display every 8 devices, on hits, first, or last
    uint8_t should_draw = (p->current <= 1) || 
                          (p->current == p->total) || 
                          (p->current % 8 == 0) ||
                          p->hit;
    
    // Yield to RTOS to prevent watchdog reset
    vTaskDelay(pdMS_TO_TICKS(should_draw ? 5 : 1));
    
    if (!should_draw) return 1;
    
    display_clear();
    set_font(FONT_TOMTHUMB);
    
    // Simple title (no inverted text to save time)
    set_cursor(2, 8);
    print("X-BE-GONE");
    draw_hline(0, 10, WIDTH, 1);
    
    // Counter
    char msg[32];
    set_cursor(2, 20);
    snprintf(msg, sizeof(msg), "%d/%d", p->current, p->total);
    print(msg);
    
    // Progress bar
    draw_rect(3, 24, WIDTH - 6, 8, 1);
    if (p->total > 0) {
        uint16_t fill = (uint16_t)(WIDTH - 8) * p->current / p->total;
        if (fill > 0) fill_rect(4, 25, fill, 6, 1);
    }
    
    // Device info
    set_cursor(2, 42);
    char trunc[22];
    snprintf(trunc, sizeof(trunc), "%s", p->brand);
    print(trunc);
    
    set_cursor(2, 50);
    snprintf(trunc, sizeof(trunc), "%s", p->model);
    print(trunc);
    
    // Hit count + TX indicator
    set_cursor(2, 62);
    snprintf(msg, sizeof(msg), "Hits: %d", p->sent);
    print(msg);
    
    if (p->hit) {
        fill_rect(WIDTH - 20, 56, 18, 12, 1);
        set_cursor(WIDTH - 18, 64);
        // Inverted "TX"
        const char *tx = "TX";
        while (*tx) {
            if (*tx >= TomThumb.first && *tx <= TomThumb.last) {
                const GFXglyph *g = &TomThumb.glyph[*tx - TomThumb.first];
                const uint8_t *bmp = TomThumb.bitmap + g->bitmapOffset;
                uint16_t bi = 0;
                for (uint8_t yy = 0; yy < g->height; yy++) {
                    for (uint8_t xx = 0; xx < g->width; xx++) {
                        if (bmp[bi >> 3] & (0x80 >> (bi & 7))) {
                            int16_t px = cursor_x + g->xOffset + xx;
                            int16_t py = 64 + g->yOffset + yy;
                            if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
                                framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                        }
                        bi++;
                    }
                }
                cursor_x += g->xAdvance;
            }
            tx++;
        }
    }
    
    // Cancel hint
    set_cursor(2, HEIGHT - 3);
    print("Hold to cancel");
    
    display_show();
    
    return 1;
}

// Generic blast function used by all actions
// Returns: sent count, sets *cancelled if user cancelled
static uint16_t xbegone_blast(const char *pattern, uint8_t *cancelled) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    
    cancel_hold_start = 0;
    *cancelled = 0;
    
    uint16_t total = ir_db_count_matching(cat);
    
    if (total == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No devices found!");
        if (cat) {
            char fmsg[48];
            snprintf(fmsg, sizeof(fmsg), "Filter: %s", cat);
            println(fmsg);
        }
        println("");
        println("Press to continue");
        display_show();
        while (!rotary_button_pressed(&encoder)) {
            rotary_read(&encoder);
            delay(10);
        }
        delay(200);
        return 0;
    }
    
    uint16_t sent = ir_db_blast_category_cb(cat, pattern, blast_progress);
    
    // Check if we were cancelled (sent < what we'd expect if we went through all)
    // Simple: if cancel_hold_start is still active, user cancelled
    if (gpio_get_level((gpio_num_t)BLAST_SW_PIN) == 0 && cancel_hold_start > 0) {
        *cancelled = 1;
    }
    
    return sent;
}

// Show result screen after blast
static void xbegone_show_result(uint16_t sent, uint8_t cancelled) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    if (cancelled) {
        println("Cancelled!");
    } else {
        println("Complete!");
    }
    
    println("");
    char msg[32];
    snprintf(msg, sizeof(msg), "Sent: %d signals", sent);
    println(msg);
    println("");
    println("Press to continue");
    display_show();
    
    // Wait for button release first (from cancel hold)
    while (gpio_get_level((gpio_num_t)BLAST_SW_PIN) == 0) {
        delay(10);
    }
    delay(200);
    
    while (!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
}

// Repeat blast N times with cancel support
static void xbegone_blast_repeat(const char *pattern, uint8_t times) {
    uint16_t total_sent = 0;
    
    for (uint8_t rep = 0; rep < times; rep++) {
        // Show repeat header briefly
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        char msg[32];
        snprintf(msg, sizeof(msg), "Round %d/%d", rep + 1, times);
        println(msg);
        display_show();
        delay(300);
        
        uint8_t cancelled = 0;
        uint16_t sent = xbegone_blast(pattern, &cancelled);
        total_sent += sent;
        
        if (cancelled) {
            xbegone_show_result(total_sent, 1);
            return;
        }
        
        if (rep < times - 1) delay(300);
    }
    
    xbegone_show_result(total_sent, 0);
}

// ========== Menu navigation ==========

static void back_to_xbegone_main(void) {
    menu_set_status("X-BE-GONE");
    menu_set_active(&xbegone_main_menu);
    menu_draw();
}

static void open_xbegone_power(void) {
    menu_set_status("Power");
    menu_set_active(&xbegone_power_menu);
    menu_draw();
}

static void open_xbegone_volume(void) {
    menu_set_status("Volume");
    menu_set_active(&xbegone_volume_menu);
    menu_draw();
}

static void open_xbegone_channel(void) {
    menu_set_status("Channel");
    menu_set_active(&xbegone_channel_menu);
    menu_draw();
}

static void open_xbegone_misc(void) {
    menu_set_status("Misc");
    menu_set_active(&xbegone_misc_menu);
    menu_draw();
}

static void open_xbegone_category(void) {
    menu_set_status("Category");
    menu_set_active(&xbegone_category_menu);
    menu_draw();
}

// ========== Category selection ==========

static void xbegone_select_all_categories(void) {
    xbegone_selected_category[0] = '\0';
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("All categories");
    display_show();
    delay(800);
    back_to_xbegone_main();
}

static void xbegone_select_category_tvs(void) {
    strcpy(xbegone_selected_category, "TVs");
    display_clear(); set_cursor(2, 10); set_font(FONT_TOMTHUMB);
    println("TVs selected!"); display_show(); delay(800);
    back_to_xbegone_main();
}

static void xbegone_select_category_acs(void) {
    strcpy(xbegone_selected_category, "ACs");
    display_clear(); set_cursor(2, 10); set_font(FONT_TOMTHUMB);
    println("ACs selected!"); display_show(); delay(800);
    back_to_xbegone_main();
}

static void xbegone_select_category_projectors(void) {
    strcpy(xbegone_selected_category, "Projectors");
    display_clear(); set_cursor(2, 10); set_font(FONT_TOMTHUMB);
    println("Projectors!"); display_show(); delay(800);
    back_to_xbegone_main();
}

static void xbegone_select_category_soundbars(void) {
    strcpy(xbegone_selected_category, "SoundBars");
    display_clear(); set_cursor(2, 10); set_font(FONT_TOMTHUMB);
    println("SoundBars!"); display_show(); delay(800);
    back_to_xbegone_main();
}

// ========== Power ==========

static void xbegone_power_off_all(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("power_off", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

static void xbegone_power_on_all(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("power_on", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

static void xbegone_power_toggle_all(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("power", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

// ========== Volume ==========

static void xbegone_vol_up_1(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("vol_up", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

static void xbegone_vol_up_5(void) {
    xbegone_blast_repeat("vol_up", 5);
    back_to_xbegone_main();
}

static void xbegone_vol_up_10(void) {
    xbegone_blast_repeat("vol_up", 10);
    back_to_xbegone_main();
}

static void xbegone_vol_down_1(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("vol_dn", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

static void xbegone_vol_down_5(void) {
    xbegone_blast_repeat("vol_dn", 5);
    back_to_xbegone_main();
}

static void xbegone_mute_all(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("mute", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

// ========== Channel ==========

static void xbegone_ch_up(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("ch_next", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

static void xbegone_ch_down(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("ch_prev", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

// ========== Misc ==========

static void xbegone_source(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("source", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

static void xbegone_menu_cmd(void) {
    uint8_t cancelled;
    uint16_t sent = xbegone_blast("menu", &cancelled);
    xbegone_show_result(sent, cancelled);
    back_to_xbegone_main();
}

// ========== Init ==========

void xbegone_open_main(void) {
    ESP_LOGI(TAG, "Opening X-BE-GONE menu");
    menu_set_status("X-BE-GONE");
    menu_set_active(&xbegone_main_menu);
    menu_draw();
}

void xbegone_init_menus(void) {
    ESP_LOGI(TAG, "Initializing X-BE-GONE menus");
    
    menu_init(&xbegone_main_menu, "X-BE-GONE");
    menu_add_item_icon(&xbegone_main_menu, "P", "Power", open_xbegone_power);
    menu_add_item_icon(&xbegone_main_menu, "V", "Volume", open_xbegone_volume);
    menu_add_item_icon(&xbegone_main_menu, "C", "Channel", open_xbegone_channel);
    menu_add_item_icon(&xbegone_main_menu, "M", "Misc", open_xbegone_misc);
    menu_add_item_icon(&xbegone_main_menu, "F", "Filter", open_xbegone_category);
    menu_add_item_icon(&xbegone_main_menu, "<", "Back", back_to_main);
    
    menu_init(&xbegone_power_menu, "Power");
    menu_add_item_icon(&xbegone_power_menu, "O", "OFF All", xbegone_power_off_all);
    menu_add_item_icon(&xbegone_power_menu, "I", "ON All", xbegone_power_on_all);
    menu_add_item_icon(&xbegone_power_menu, "T", "Toggle", xbegone_power_toggle_all);
    menu_add_item_icon(&xbegone_power_menu, "<", "Back", back_to_xbegone_main);
    
    menu_init(&xbegone_volume_menu, "Volume");
    menu_add_item_icon(&xbegone_volume_menu, "+", "Up x1", xbegone_vol_up_1);
    menu_add_item_icon(&xbegone_volume_menu, "+", "Up x5", xbegone_vol_up_5);
    menu_add_item_icon(&xbegone_volume_menu, "+", "Up x10", xbegone_vol_up_10);
    menu_add_item_icon(&xbegone_volume_menu, "-", "Down x1", xbegone_vol_down_1);
    menu_add_item_icon(&xbegone_volume_menu, "-", "Down x5", xbegone_vol_down_5);
    menu_add_item_icon(&xbegone_volume_menu, "M", "Mute", xbegone_mute_all);
    menu_add_item_icon(&xbegone_volume_menu, "<", "Back", back_to_xbegone_main);
    
    menu_init(&xbegone_channel_menu, "Channel");
    menu_add_item_icon(&xbegone_channel_menu, "+", "Up", xbegone_ch_up);
    menu_add_item_icon(&xbegone_channel_menu, "-", "Down", xbegone_ch_down);
    menu_add_item_icon(&xbegone_channel_menu, "<", "Back", back_to_xbegone_main);
    
    menu_init(&xbegone_misc_menu, "Misc");
    menu_add_item_icon(&xbegone_misc_menu, "S", "Source", xbegone_source);
    menu_add_item_icon(&xbegone_misc_menu, "M", "Menu", xbegone_menu_cmd);
    menu_add_item_icon(&xbegone_misc_menu, "<", "Back", back_to_xbegone_main);
    
    menu_init(&xbegone_category_menu, "Filter");
    menu_add_item_icon(&xbegone_category_menu, "*", "All", xbegone_select_all_categories);
    menu_add_item_icon(&xbegone_category_menu, "T", "TVs", xbegone_select_category_tvs);
    menu_add_item_icon(&xbegone_category_menu, "A", "ACs", xbegone_select_category_acs);
    menu_add_item_icon(&xbegone_category_menu, "P", "Projectors", xbegone_select_category_projectors);
    menu_add_item_icon(&xbegone_category_menu, "S", "SoundBars", xbegone_select_category_soundbars);
    menu_add_item_icon(&xbegone_category_menu, "<", "Back", back_to_xbegone_main);
}
