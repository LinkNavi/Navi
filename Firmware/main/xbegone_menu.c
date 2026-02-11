// xbegone_menu.c - X-BE-GONE Menu System Implementation
#include "xbegone_menu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// Menu instances
Menu xbegone_main_menu;
Menu xbegone_power_menu;
Menu xbegone_category_menu;
Menu xbegone_volume_menu;
Menu xbegone_channel_menu;
Menu xbegone_misc_menu;

// Category selection state
char xbegone_selected_category[MAX_CATEGORY_LEN] = "";

// Helper for delay
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// =============================================================================
// NAVIGATION FUNCTIONS
// =============================================================================

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

// =============================================================================
// CATEGORY SELECTION FUNCTIONS
// =============================================================================

static void xbegone_select_all_categories(void) {
    xbegone_selected_category[0] = '\0';  // Empty = all categories
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
    println("TVs category");
    println("selected!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

static void xbegone_select_category_acs(void) {
    strcpy(xbegone_selected_category, "ACs");
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("ACs category");
    println("selected!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

static void xbegone_select_category_projectors(void) {
    strcpy(xbegone_selected_category, "Projectors");
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Projectors");
    println("selected!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

static void xbegone_select_category_soundbars(void) {
    strcpy(xbegone_selected_category, "SoundBars");
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("SoundBars");
    println("selected!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

static void xbegone_select_category_other(void) {
    strcpy(xbegone_selected_category, "Other");
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Other category");
    println("selected!");
    display_show();
    delay(1000);
    back_to_xbegone_main();
}

// =============================================================================
// POWER FUNCTIONS
// =============================================================================

static void xbegone_power_off_all(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        println("Scan first");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_POWER_OFF, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_power_on_all(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        println("Scan first");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_POWER_ON, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_power_toggle_all(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        println("Scan first");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_POWER_TOGGLE, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

// =============================================================================
// VOLUME FUNCTIONS
// =============================================================================

static void xbegone_vol_up_1(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_VOL_UP, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_vol_up_5(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_repeat_signal_type(SIGNAL_VOL_UP, cat, 5);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_vol_up_10(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_repeat_signal_type(SIGNAL_VOL_UP, cat, 10);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_vol_down_1(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_VOL_DOWN, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_vol_down_5(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_repeat_signal_type(SIGNAL_VOL_DOWN, cat, 5);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_mute_all(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_MUTE, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

// =============================================================================
// CHANNEL FUNCTIONS
// =============================================================================

static void xbegone_ch_up(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_CH_UP, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_ch_down(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_CH_DOWN, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

// =============================================================================
// MISC FUNCTIONS
// =============================================================================

static void xbegone_source(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_SOURCE, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

static void xbegone_menu(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        display_show();
        delay(1500);
        back_to_xbegone_main();
        return;
    }
    
    const char *cat = xbegone_selected_category[0] ? xbegone_selected_category : NULL;
    ir_xbegone_run_signal_type(SIGNAL_MENU, cat);
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_xbegone_main();
}

// =============================================================================
// PUBLIC FUNCTIONS
// =============================================================================

void xbegone_open_main(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No IR files!");
        println("Scan first");
        display_show();
        delay(1500);
        back_to_ir_menu();
        return;
    }
    
    menu_set_status("X-BE-GONE");
    menu_set_active(&xbegone_main_menu);
    menu_draw();
}

void xbegone_init_menus(void) {
    // Main X-BE-GONE menu
    menu_init(&xbegone_main_menu, "X-BE-GONE");
    menu_add_item_icon(&xbegone_main_menu, "P", "Power", open_xbegone_power);
    menu_add_item_icon(&xbegone_main_menu, "V", "Volume", open_xbegone_volume);
    menu_add_item_icon(&xbegone_main_menu, "C", "Channel", open_xbegone_channel);
    menu_add_item_icon(&xbegone_main_menu, "M", "Misc", open_xbegone_misc);
    menu_add_item_icon(&xbegone_main_menu, "F", "Filter", open_xbegone_category);
    menu_add_item_icon(&xbegone_main_menu, "<", "Back", back_to_ir_menu);
    
    // Power submenu
    menu_init(&xbegone_power_menu, "Power");
    menu_add_item_icon(&xbegone_power_menu, "O", "Power OFF All", xbegone_power_off_all);
    menu_add_item_icon(&xbegone_power_menu, "I", "Power ON All", xbegone_power_on_all);
    menu_add_item_icon(&xbegone_power_menu, "T", "Toggle All", xbegone_power_toggle_all);
    menu_add_item_icon(&xbegone_power_menu, "<", "Back", back_to_xbegone_main);
    
    // Volume submenu
    menu_init(&xbegone_volume_menu, "Volume");
    menu_add_item_icon(&xbegone_volume_menu, "+", "Vol Up x1", xbegone_vol_up_1);
    menu_add_item_icon(&xbegone_volume_menu, "+", "Vol Up x5", xbegone_vol_up_5);
    menu_add_item_icon(&xbegone_volume_menu, "+", "Vol Up x10", xbegone_vol_up_10);
    menu_add_item_icon(&xbegone_volume_menu, "-", "Vol Down x1", xbegone_vol_down_1);
    menu_add_item_icon(&xbegone_volume_menu, "-", "Vol Down x5", xbegone_vol_down_5);
    menu_add_item_icon(&xbegone_volume_menu, "M", "Mute All", xbegone_mute_all);
    menu_add_item_icon(&xbegone_volume_menu, "<", "Back", back_to_xbegone_main);
    
    // Channel submenu
    menu_init(&xbegone_channel_menu, "Channel");
    menu_add_item_icon(&xbegone_channel_menu, "+", "Ch Up", xbegone_ch_up);
    menu_add_item_icon(&xbegone_channel_menu, "-", "Ch Down", xbegone_ch_down);
    menu_add_item_icon(&xbegone_channel_menu, "<", "Back", back_to_xbegone_main);
    
    // Misc submenu
    menu_init(&xbegone_misc_menu, "Misc");
    menu_add_item_icon(&xbegone_misc_menu, "S", "Source/Input", xbegone_source);
    menu_add_item_icon(&xbegone_misc_menu, "M", "Menu", xbegone_menu);
    menu_add_item_icon(&xbegone_misc_menu, "<", "Back", back_to_xbegone_main);
    
    // Category filter submenu
    menu_init(&xbegone_category_menu, "Filter");
    menu_add_item_icon(&xbegone_category_menu, "*", "All", xbegone_select_all_categories);
    menu_add_item_icon(&xbegone_category_menu, "T", "TVs", xbegone_select_category_tvs);
    menu_add_item_icon(&xbegone_category_menu, "A", "ACs", xbegone_select_category_acs);
    menu_add_item_icon(&xbegone_category_menu, "P", "Projectors", xbegone_select_category_projectors);
    menu_add_item_icon(&xbegone_category_menu, "S", "SoundBars", xbegone_select_category_soundbars);
    menu_add_item_icon(&xbegone_category_menu, "O", "Other", xbegone_select_category_other);
    menu_add_item_icon(&xbegone_category_menu, "<", "Back", back_to_xbegone_main);
}
