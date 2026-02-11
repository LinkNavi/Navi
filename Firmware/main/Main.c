#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define DISPLAY_TYPE DISPLAY_SH1107
#include "drivers/display.h"
#include "menu.h"
#include "drivers/rotary.h"
#include "drivers/font.h"
#include "drivers/sd_card.h"

static const char *TAG = "NAVI";

#define I2C_SDA 16
#define I2C_SCL 17
#define ROTARY_CLK 5
#define ROTARY_DT 18
#define ROTARY_SW 7
#define SD_MOSI 11
#define SD_MISO 13
#define SD_CLK 12
#define SD_CS 10
#define IR_PIN 4
Rotary encoder;
Menu main_menu;
Menu settings_menu;
Menu display_menu;
Menu sd_menu;
uint8_t sd_initialized = 0;

// Helper function to replace Arduino delay
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Helper function to replace Arduino millis
static inline uint32_t millis(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void init_i2c(void) {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_SDA;
    conf.scl_io_num = (gpio_num_t)I2C_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    
    ESP_LOGI(TAG, "I2C initialized on SDA=%d, SCL=%d", I2C_SDA, I2C_SCL);
}

void back_to_main(void) {
    menu_set_status("Ready");
    menu_set_active(&main_menu);
    menu_draw();
}

void open_settings(void) {
    menu_set_status("Config");
    menu_set_active(&settings_menu);
    menu_draw();
}

void open_display_settings(void) {
    menu_set_status("Display");
    menu_set_active(&display_menu);
    menu_draw();
}

void open_sd_menu(void) {
    if (sd_initialized) {
        menu_set_status("SD OK");
    } else {
        menu_set_status("No SD");
    }
    menu_set_active(&sd_menu);
    menu_draw();
}

void toggle_invert(void) {
    invert_display();
    display_show();
    delay(1000);
    open_display_settings();
}

void adjust_contrast(void) {
    static uint8_t contrast = 0xCF;
    contrast = (contrast + 32) & 0xFF;
    set_contrast(contrast);
    ESP_LOGI(TAG, "Contrast set to 0x%02X", contrast);
    open_display_settings();
}

void sd_test_init(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    print("Initializing SD...");
    display_show();
    
    ESP_LOGI(TAG, "Initializing SD card on CS=%d, MOSI=%d, MISO=%d, CLK=%d", 
             SD_CS, SD_MOSI, SD_MISO, SD_CLK);
    
    if (sd_init(SD_MOSI, SD_MISO, SD_CLK, SD_CS)) {
        sd_initialized = 1;
        display_clear();
        set_cursor(2, 10);
        println("SD Card OK!");
        
        if (sd_is_fat_formatted()) {
            println("Already FAT16");
            ESP_LOGI(TAG, "SD card already formatted");
        } else {
            println("Not formatted");
            println("Formatting...");
            display_show();
            delay(500);
            
            ESP_LOGI(TAG, "Formatting SD card as FAT16...");
            if (sd_format_fat16()) {
                println("Format OK!");
                ESP_LOGI(TAG, "SD card formatted successfully");
            } else {
                println("Format FAILED!");
                ESP_LOGE(TAG, "SD card format failed");
            }
        }
        
        println("");
        println("Press to continue");
        display_show();
    } else {
        display_clear();
        set_cursor(2, 10);
        println("SD Init FAILED!");
        println("Check wiring");
        println("");
        println("Press to continue");
        display_show();
        ESP_LOGE(TAG, "SD card initialization failed");
    }
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    open_sd_menu();
}

void sd_test_write(void) {
    if (!sd_initialized) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("SD not ready!");
        println("Init first");
        display_show();
        delay(1500);
        open_sd_menu();
        return;
    }
    
    display_clear();
    set_cursor(2, 10);
    println("Writing files...");
    display_show();
    
    ESP_LOGI(TAG, "Starting SD write tests");
    
    // Test 1: Root file
    const char *test1 = "Hello from ESP32!";
    if (sd_write_file("TEST.TXT", (uint8_t*)test1, strlen(test1))) {
        println("ROOT: OK");
        ESP_LOGI(TAG, "Root file write OK");
    } else {
        println("ROOT: FAIL");
        ESP_LOGE(TAG, "Root file write failed");
    }
    display_show();
    delay(300);
    
    // Test 2: Create directory
    if (sd_mkdir_path("/LOGS")) {
        println("MKDIR: OK");
        ESP_LOGI(TAG, "Directory /LOGS created");
    } else {
        println("MKDIR: FAIL");
        ESP_LOGE(TAG, "Directory /LOGS creation failed");
    }
    display_show();
    delay(300);
    
    // Test 3: Write to directory
    const char *test2 = "Log entry 1\nSystem boot OK\n";
    if (sd_write_file_path("/LOGS/BOOT.TXT", (uint8_t*)test2, strlen(test2))) {
        println("SUBDIR: OK");
        ESP_LOGI(TAG, "Subdirectory file write OK");
    } else {
        println("SUBDIR: FAIL");
        ESP_LOGE(TAG, "Subdirectory file write failed");
    }
    display_show();
    delay(300);
    
    // Test 4: Nested directories
    if (sd_mkdir_path("/DATA/2025/FEB")) {
        println("NESTED: OK");
        ESP_LOGI(TAG, "Nested directories created");
    } else {
        println("NESTED: FAIL");
        ESP_LOGE(TAG, "Nested directory creation failed");
    }
    display_show();
    delay(300);
    
    // Test 5: Write to nested
    const char *test3 = "Temperature: 25C\nHumidity: 60%\n";
    if (sd_write_file_path("/DATA/2025/FEB/SENSOR.TXT", (uint8_t*)test3, strlen(test3))) {
        println("DEEP: OK");
        ESP_LOGI(TAG, "Deep nested file write OK");
    } else {
        println("DEEP: FAIL");
        ESP_LOGE(TAG, "Deep nested file write failed");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    open_sd_menu();
}

void sd_test_read(void) {
    if (!sd_initialized) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("SD not ready!");
        display_show();
        delay(1500);
        open_sd_menu();
        return;
    }
    
    display_clear();
    set_cursor(2, 10);
    println("Reading TEST.TXT");
    println("");
    display_show();
    
    uint8_t buffer[256];
    uint32_t size;
    
    ESP_LOGI(TAG, "Reading /TEST.TXT");
    if (sd_read_file_path("/TEST.TXT", buffer, &size)) {
        buffer[size] = '\0';
        print((char*)buffer);
        println("");
        println("Read OK!");
        ESP_LOGI(TAG, "File read OK, size=%ld bytes", size);
    } else {
        println("Read FAILED!");
        ESP_LOGE(TAG, "File read failed");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    open_sd_menu();
}

void about_screen(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("ESP32-S3 Demo");
    println("Version 1.0");
    println("");
    println("Features:");
    println("- Menu system");
    println("- Rotary input");
    println("- SD FAT16");
    println("");
    println("Press to return");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_main();
}

void game_screen(void) {
    display_clear();
    int16_t x = WIDTH/2, y = HEIGHT/2;
    int16_t score = 0;
    
    ESP_LOGI(TAG, "Starting game");
    
    while(1) {
        int8_t dir = rotary_read(&encoder);
        if (dir > 0) {
            y = (y - 2 + HEIGHT) % HEIGHT;
            score++;
        }
        if (dir < 0) {
            y = (y + 2) % HEIGHT;
            score++;
        }
        if (rotary_button_pressed(&encoder)) break;
        
        display_clear();
        fill_circle(x, y, 3, 1);
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        print("Score: ");
        char score_str[10];
        snprintf(score_str, sizeof(score_str), "%d", score);
        print(score_str);
        display_show();
        delay(10);
    }
    delay(200);
    ESP_LOGI(TAG, "Game ended, score=%d", score);
    back_to_main();
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting Navi firmware");
    
    // Initialize I2C
    init_i2c();
    
    // Initialize display
    display_init();
    display_clear();
    
    // Splash screen
    set_cursor(10, 20);
    set_font(FONT_FREEMONO_9PT);
    print("Navi.");
    set_cursor(20, 40);
    set_font(FONT_TOMTHUMB);
    print("Booting Up!");
    display_show();
    delay(1000);
    
    // Initialize rotary encoder
rotary_init(&encoder, ROTARY_CLK, ROTARY_DT, ROTARY_SW);
ESP_LOGI(TAG, "Rotary encoder initialized on CLK=%d, DT=%d, SW=%d", 
         ROTARY_CLK, ROTARY_DT, ROTARY_SW);

// Test encoder pins
ESP_LOGI(TAG, "Testing encoder pins - turn the knob now:");
for (int i = 0; i < 20; i++) {
    uint8_t clk = gpio_get_level((gpio_num_t)ROTARY_CLK);
    uint8_t dt = gpio_get_level((gpio_num_t)ROTARY_DT);
    printf("  CLK=%d DT=%d\n", clk, dt);
    delay(100);
}
ESP_LOGI(TAG, "Pin test complete");

// Main menu with icons
menu_init(&main_menu, "Main Menu");
    
    // Main menu with icons
    menu_init(&main_menu, "Main Menu");
    menu_add_item_icon(&main_menu, "D", "SD Card", open_sd_menu);
    menu_add_item_icon(&main_menu, "S", "Settings", open_settings);
    menu_add_item_icon(&main_menu, "G", "Game", game_screen);
    menu_add_item_icon(&main_menu, "?", "About", about_screen);
    
    // Settings menu
    menu_init(&settings_menu, "Settings");
    menu_add_item_icon(&settings_menu, "V", "Display", open_display_settings);
    menu_add_item_icon(&settings_menu, "<", "Back", back_to_main);
    
    // Display menu
    menu_init(&display_menu, "Display");
    menu_add_item_icon(&display_menu, "!", "Invert", toggle_invert);
    menu_add_item_icon(&display_menu, "+", "Contrast", adjust_contrast);
    menu_add_item_icon(&display_menu, "<", "Back", open_settings);
    
    // SD menu
    menu_init(&sd_menu, "SD Card");
    menu_add_item_icon(&sd_menu, "I", "Initialize", sd_test_init);
    menu_add_item_icon(&sd_menu, "W", "Write Test", sd_test_write);
    menu_add_item_icon(&sd_menu, "R", "Read Test", sd_test_read);
    menu_add_item_icon(&sd_menu, "<", "Back", back_to_main);
    
    menu_set_status("Ready");
    menu_set_active(&main_menu);
    menu_draw();
    
    ESP_LOGI(TAG, "Initialization complete, entering main loop");
    
    // Main loop
    while(1) {
        int8_t dir = rotary_read(&encoder);
        
        if (dir > 0) {
            menu_next();
            menu_draw();
            delay(50);
        } else if (dir < 0) {
            menu_prev();
            menu_draw();
            delay(50);
        }
        
        if (rotary_button_pressed(&encoder)) {
            menu_select();
            delay(200);
        }
        
        delay(5);
    }
}
