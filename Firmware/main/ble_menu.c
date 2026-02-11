// ble_menu.c - BLE menu implementation
#include "ble_menu.h"
#include "drivers/ble.h"
#include "drivers/display.h"
#include "drivers/rotary_pcnt.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BLE_Menu";

extern RotaryPCNT encoder;
extern void back_to_main(void);

Menu ble_main_menu;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void show_ble_status(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("BLE Status");
    println("");
    
    if (ble_is_connected()) {
        println("Status: Connected");
        println("");
        println("Device: Ocarina App");
    } else {
        println("Status: Advertising");
        println("");
        println("Name: Navi-Esp32");
        println("");
        println("Waiting for");
        println("connection from");
        println("Ocarina app...");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    ble_menu_open();
}

static void show_connection_info(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Connection Info");
    println("");
    println("Service UUID:");
    println("4fafc201-1fb5");
    println("459e-8fcc");
    println("c5c9c331914b");
    println("");
    println("Char UUID:");
    println("beb5483e-36e1");
    println("4688-b7f5");
    println("ea07361b26a8");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    ble_menu_open();
}

static void test_ble_send(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    if (!ble_is_connected()) {
        println("Not connected!");
        println("");
        println("Connect from app");
        println("first");
    } else {
        println("Sending test");
        println("message...");
        display_show();
        delay(500);
        
        ble_send_string("Hello from Navi!");
        
        display_clear();
        set_cursor(2, 10);
        println("Message sent!");
        println("");
        println("Check app for");
        println("response");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    ble_menu_open();
}

void ble_menu_open(void) {
    menu_set_status(ble_is_connected() ? "BLE OK" : "BLE Adv");
    menu_set_active(&ble_main_menu);
    menu_draw();
}

void ble_menu_init(void) {
    ESP_LOGI(TAG, "Initializing BLE menu");
    
    menu_init(&ble_main_menu, "Bluetooth");
    menu_add_item_icon(&ble_main_menu, "S", "Status", show_ble_status);
    menu_add_item_icon(&ble_main_menu, "I", "Connection Info", show_connection_info);
    menu_add_item_icon(&ble_main_menu, "T", "Test Send", test_ble_send);
    menu_add_item_icon(&ble_main_menu, "<", "Back", back_to_main);
}
