// xbegone_menu.c - X-BE-GONE using embedded IR database
#include "xbegone_menu.h"
#include "ir_database.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "XBEGONE";

Menu xbegone_main_menu;
Menu xbegone_power_menu;
Menu xbegone_category_menu;
Menu xbegone_volume_menu;
Menu xbegone_channel_menu;
Menu xbegone_misc_menu;

char xbegone_selected_category[32] = "";

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

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

// Category selection
static void xbegone_select_all_categories(void) {
    xbegone_selected_category[0] = '\0';
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("All categories");
    println("selected!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

static void xbegone_select_category_tvs(void) {
    strcpy(xbegone_selected_category, "TVs");
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("TVs selected!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

static void xbegone_select_category_acs(void) {
    strcpy(xbegone_selected_category, "ACs");
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("ACs selected!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

static void xbegone_select_category_projectors(void) {
    strcpy(xbegone_selected_category, "Projectors");
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Projectors!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

static void xbegone_select_category_soundbars(void) {
    strcpy(xbegone_selected_category, "SoundBars");
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("SoundBars!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

// Power functions
static void xbegone_power_off_all(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Blasting OFF...");
    display_show();
    
    uint16_t sent = ir_db_blast_category(cat, "power_off");
    
    display_clear();
    set_cursor(2, 10);
    char msg[32];
    snprintf(msg, sizeof(msg), "Sent: %d devices", sent);
    println(msg);
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_power_on_all(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Blasting ON...");
    display_show();
    
    uint16_t sent = ir_db_blast_category(cat, "power_on");
    
    display_clear();
    set_cursor(2, 10);
    char msg[32];
    snprintf(msg, sizeof(msg), "Sent: %d devices", sent);
    println(msg);
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_power_toggle_all(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Toggling power...");
    display_show();
    
    uint16_t sent = ir_db_blast_category(cat, "power");
    
    display_clear();
    set_cursor(2, 10);
    char msg[32];
    snprintf(msg, sizeof(msg), "Sent: %d devices", sent);
    println(msg);
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

// Volume functions
static void xbegone_vol_up_1(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    uint16_t sent = ir_db_blast_category(cat, "vol_up");
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    char msg[32];
    snprintf(msg, sizeof(msg), "Vol Up: %d", sent);
    println(msg);
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_vol_up_5(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    
    for (uint8_t i = 0; i < 5; i++) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        char msg[32];
        snprintf(msg, sizeof(msg), "Vol Up %d/5", i + 1);
        println(msg);
        display_show();
        
        ir_db_blast_category(cat, "vol_up");
        delay(300);
    }
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_vol_up_10(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    
    for (uint8_t i = 0; i < 10; i++) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        char msg[32];
        snprintf(msg, sizeof(msg), "Vol Up %d/10", i + 1);
        println(msg);
        display_show();
        
        ir_db_blast_category(cat, "vol_up");
        delay(300);
    }
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_vol_down_1(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    uint16_t sent = ir_db_blast_category(cat, "vol_dn");
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    char msg[32];
    snprintf(msg, sizeof(msg), "Vol Down: %d", sent);
    println(msg);
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_vol_down_5(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    
    for (uint8_t i = 0; i < 5; i++) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        char msg[32];
        snprintf(msg, sizeof(msg), "Vol Down %d/5", i + 1);
        println(msg);
        display_show();
        
        ir_db_blast_category(cat, "vol_dn");
        delay(300);
    }
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_mute_all(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    uint16_t sent = ir_db_blast_category(cat, "mute");
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    char msg[32];
    snprintf(msg, sizeof(msg), "Mute: %d", sent);
    println(msg);
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

// Channel functions
static void xbegone_ch_up(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    uint16_t sent = ir_db_blast_category(cat, "ch_next");
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    char msg[32];
    snprintf(msg, sizeof(msg), "Ch Up: %d", sent);
    println(msg);
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_ch_down(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    uint16_t sent = ir_db_blast_category(cat, "ch_prev");
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    char msg[32];
    snprintf(msg, sizeof(msg), "Ch Down: %d", sent);
    println(msg);
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

// Misc functions
static void xbegone_source(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    uint16_t sent = ir_db_blast_category(cat, "source");
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    char msg[32];
    snprintf(msg, sizeof(msg), "Source: %d", sent);
    println(msg);
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_menu(void) {
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    uint16_t sent = ir_db_blast_category(cat, "menu");
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    char msg[32];
    snprintf(msg, sizeof(msg), "Menu: %d", sent);
    println(msg);
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

void xbegone_open_main(void) {
    ESP_LOGI(TAG, "Opening X-BE-GONE menu, items=%d", xbegone_main_menu.item_count);
    menu_set_status("X-BE-GONE");
    menu_set_active(&xbegone_main_menu);
    menu_draw();
    ESP_LOGI(TAG, "X-BE-GONE menu active");
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
    
    ESP_LOGI(TAG, "Main menu initialized with %d items", xbegone_main_menu.item_count);
    
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
    menu_add_item_icon(&xbegone_misc_menu, "M", "Menu", xbegone_menu);
    menu_add_item_icon(&xbegone_misc_menu, "<", "Back", back_to_xbegone_main);
    
    menu_init(&xbegone_category_menu, "Filter");
    menu_add_item_icon(&xbegone_category_menu, "*", "All", xbegone_select_all_categories);
    menu_add_item_icon(&xbegone_category_menu, "T", "TVs", xbegone_select_category_tvs);
    menu_add_item_icon(&xbegone_category_menu, "A", "ACs", xbegone_select_category_acs);
    menu_add_item_icon(&xbegone_category_menu, "P", "Projectors", xbegone_select_category_projectors);
    menu_add_item_icon(&xbegone_category_menu, "S", "SoundBars", xbegone_select_category_soundbars);
    menu_add_item_icon(&xbegone_category_menu, "<", "Back", back_to_xbegone_main);
}
