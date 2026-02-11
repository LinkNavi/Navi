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
#include "drivers/ir.h"
#include "ir_system.h"
#include "file_browser.h"

static const char *TAG = "NAVI";

#define I2C_SDA 16
#define I2C_SCL 17
#define ROTARY_CLK 5
#define ROTARY_DT 6
#define ROTARY_SW 7
#define SD_MOSI 21
#define SD_MISO 35
#define SD_CLK 20
#define SD_CS 19
#define IR_PIN 4

Rotary encoder;
Menu main_menu;
Menu settings_menu;
Menu display_menu;
Menu sd_menu;
Menu ir_menu;
Menu ir_file_menu;
Menu files_menu;
uint8_t sd_initialized = 0;
uint8_t ir_folder_scanned = 0;
uint8_t current_ir_file_index = 0;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

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

void sd_hardware_test(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Testing hardware...");
    display_show();
    
    sd_test_hardware();
    
    display_clear();
    set_cursor(2, 10);
    println("Test complete!");
    println("Check serial log");
    println("for results");
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

void sd_test_init(void) {
    if (sd_initialized) {
        // Already initialized - ask if they want to reformat
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("SD already init!");
        println("");
        println("Format anyway?");
        println("");
        println("Turn: Select");
        println("Press: Confirm");
        display_show();
        
        uint8_t choice = 0; // 0=No, 1=Yes
        while(1) {
            int8_t dir = rotary_read(&encoder);
            if (dir != 0) {
                choice = !choice;
                display_clear();
                set_cursor(2, 10);
                println("SD already init!");
                println("");
                println("Format anyway?");
                println("");
                if (choice) {
                    println("> YES - Format");
                    println("  NO  - Cancel");
                } else {
                    println("  YES - Format");
                    println("> NO  - Cancel");
                }
                display_show();
            }
            
            if (rotary_button_pressed(&encoder)) {
                delay(200);
                break;
            }
            delay(10);
        }
        
        if (!choice) {
            open_sd_menu();
            return;
        }
    }
    
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
            println("");
            println("Format anyway?");
            println("");
            println("Turn: Select");
            println("Press: Confirm");
            display_show();
            
            uint8_t do_format = 0;
            while(1) {
                int8_t dir = rotary_read(&encoder);
                if (dir != 0) {
                    do_format = !do_format;
                    display_clear();
                    set_cursor(2, 10);
                    println("Already FAT16");
                    println("");
                    println("Format anyway?");
                    println("");
                    if (do_format) {
                        println("> YES - Format");
                        println("  NO  - Keep");
                    } else {
                        println("  YES - Format");
                        println("> NO  - Keep");
                    }
                    display_show();
                }
                
                if (rotary_button_pressed(&encoder)) {
                    delay(200);
                    break;
                }
                delay(10);
            }
            
            if (do_format) {
                display_clear();
                set_cursor(2, 10);
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
            } else {
                ESP_LOGI(TAG, "Keeping existing format");
            }
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
        println("Check wiring:");
        println("CS=10 MOSI=11");
        println("MISO=13 CLK=12");
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
    
    if (sd_mkdir_path("/LOGS")) {
        println("MKDIR: OK");
        ESP_LOGI(TAG, "Directory /LOGS created");
    } else {
        println("MKDIR: FAIL");
        ESP_LOGE(TAG, "Directory /LOGS creation failed");
    }
    display_show();
    delay(300);
    
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
    
    if (sd_mkdir_path("/DATA/2025/FEB")) {
        println("NESTED: OK");
        ESP_LOGI(TAG, "Nested directories created");
    } else {
        println("NESTED: FAIL");
        ESP_LOGE(TAG, "Nested directory creation failed");
    }
    display_show();
    delay(300);
    
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

// ===== IR MENU FUNCTIONS =====

void open_ir_menu(void) {
    menu_set_status("IR Ready");
    menu_set_active(&ir_menu);
    menu_draw();
}

void back_to_ir_menu(void) {
    menu_set_status("IR Ready");
    menu_set_active(&ir_menu);
    menu_draw();
}

void open_file_browser(void) {
    if (!sd_initialized) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No SD card!");
        display_show();
        delay(1500);
        back_to_main();
        return;
    }
    
    file_browser_init("/");
    if (!file_browser_scan()) {
        display_clear();
        set_cursor(2, 10);
        println("Scan failed!");
        display_show();
        delay(1500);
        back_to_main();
        return;
    }
    
    ESP_LOGI(TAG, "File browser opened");
    
    // File browser loop
    while (1) {
        file_browser_draw();
        
        while (1) {
            int8_t dir = rotary_read(&encoder);
            
            if (dir > 0) {
                file_browser_next();
                break;
            } else if (dir < 0) {
                file_browser_prev();
                break;
            }
            
            if (rotary_button_pressed(&encoder)) {
                delay(200);
                
                // Check if it's a directory
                if (browser.files[browser.selected].is_dir) {
                    file_browser_enter(browser.selected);
                    break;
                } else {
                    // Try to open as text file
                    if (file_browser_read_text(browser.selected)) {
                        // Text viewer loop
                        while (1) {
                            file_browser_draw_text();
                            
                            uint8_t exit_viewer = 0;
                            while (1) {
                                int8_t dir = rotary_read(&encoder);
                                
                                if (dir > 0) {
                                    text_viewer_scroll_down();
                                    break;
                                } else if (dir < 0) {
                                    text_viewer_scroll_up();
                                    break;
                                }
                                
                                if (rotary_button_pressed(&encoder)) {
                                    delay(200);
                                    exit_viewer = 1;
                                    text_viewer_active = 0;
                                    break;
                                }
                                
                                delay(5);
                            }
                            
                            if (exit_viewer) break;
                        }
                        break;
                    }
                }
            }
            
            delay(5);
        }
        
        // Check if we should exit browser (back at root with ..)
        if (strcmp(browser.current_path, "/") == 0 && browser.selected == 0 && browser.count == 0) {
            break;
        }
        
        // Hold button longer to exit
        static uint32_t hold_start = 0;
        if (gpio_get_level((gpio_num_t)ROTARY_SW) == 0) {
            if (hold_start == 0) hold_start = millis();
            if (millis() - hold_start > 1000) {
                delay(200);
                break;
            }
        } else {
            hold_start = 0;
        }
    }
    
    back_to_main();
}

void ir_scan_files(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Scanning /IR...");
    display_show();
    
    if (!sd_initialized) {
        println("");
        println("No SD card!");
        display_show();
        delay(1500);
        open_ir_menu();
        return;
    }
    
    // Create /IR folder if it doesn't exist
    sd_mkdir_path("/IR");
    
    uint8_t count = ir_scan_folder("/IR");
    ir_folder_scanned = (count > 0);
    
    display_clear();
    set_cursor(2, 10);
    if (count > 0) {
        println("Scan complete!");
        println("");
        char msg[32];
        snprintf(msg, sizeof(msg), "Found %d files", count);
        println(msg);
        ESP_LOGI(TAG, "Found %d IR files", count);
    } else {
        println("No IR files!");
        println("");
        println("Place .IR files");
        println("in /IR folder");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    open_ir_menu();
}

void ir_xbegone_execute(void) {
    if (!sd_initialized) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No SD card!");
        display_show();
        delay(1500);
        open_ir_menu();
        return;
    }
    
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        println("No IR files!");
        println("Scan first");
        display_show();
        delay(1500);
        open_ir_menu();
        return;
    }
    
    ESP_LOGI(TAG, "Executing X-BE-GONE on %d files", ir_file_list.count);
    ir_xbegone_run_all();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    open_ir_menu();
}

void ir_browse_files(void) {
    if (!ir_folder_scanned || ir_file_list.count == 0) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        println("No files!");
        println("Scan first");
        display_show();
        delay(1500);
        open_ir_menu();
        return;
    }
    
    // Rebuild file menu dynamically
    menu_init(&ir_file_menu, "IR Files");
    for (uint8_t i = 0; i < ir_file_list.count && i < MAX_MENU_ITEMS - 1; i++) {
        // We'll use a static action for now - in real implementation you'd need unique handlers
        menu_add_item_icon(&ir_file_menu, "F", ir_file_list.files[i], NULL);
    }
    menu_add_item_icon(&ir_file_menu, "<", "Back", back_to_ir_menu);
    
    menu_set_status("Browse");
    menu_set_active(&ir_file_menu);
    menu_draw();
}

void ir_test_signal(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Testing IR...");
    println("");
    println("Sending NEC test");
    println("Address: 0x00");
    println("Command: 0x40");
    display_show();
    
    ESP_LOGI(TAG, "Sending test IR signal");
    ir_send_nec(0x00, 0x40);
    
    delay(500);
    println("");
    println("Sent!");
    println("");
    println("Press to continue");
    display_show();
    
    while(!rotary_button_pressed(&encoder)) {
        rotary_read(&encoder);
        delay(10);
    }
    delay(200);
    open_ir_menu();
}

void about_screen(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("ESP32-S3 Demo");
    println("Version 1.1");
    println("");
    println("Features:");
    println("- Menu system");
    println("- Rotary input");
    println("- SD FAT16");
    println("- IR Blaster");
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
    ESP_LOGI(TAG, "Starting Navi firmware v1.1 with IR support");
    
    init_i2c();
    display_init();
    display_clear();
    
    set_cursor(10, 20);
    set_font(FONT_FREEMONO_9PT);
    print("Navi.");
    set_cursor(20, 40);
    set_font(FONT_TOMTHUMB);
    print("Booting Up!");
    display_show();
    delay(1000);
    
    // Initialize IR
    ir_init(IR_PIN);
    ESP_LOGI(TAG, "IR initialized on pin %d", IR_PIN);
    
    rotary_init(&encoder, ROTARY_CLK, ROTARY_DT, ROTARY_SW);
    ESP_LOGI(TAG, "Rotary encoder initialized on CLK=%d, DT=%d, SW=%d", 
             ROTARY_CLK, ROTARY_DT, ROTARY_SW);
    
    // Main menu
    menu_init(&main_menu, "Main Menu");
    menu_add_item_icon(&main_menu, "I", "IR Control", open_ir_menu);
    menu_add_item_icon(&main_menu, "F", "Files", open_file_browser);
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
    menu_add_item_icon(&sd_menu, "T", "HW Test", sd_hardware_test);
    menu_add_item_icon(&sd_menu, "W", "Write Test", sd_test_write);
    menu_add_item_icon(&sd_menu, "R", "Read Test", sd_test_read);
    menu_add_item_icon(&sd_menu, "<", "Back", back_to_main);
    
    // IR menu
    menu_init(&ir_menu, "IR Control");
    menu_add_item_icon(&ir_menu, "X", "X-BE-GONE", ir_xbegone_execute);
    menu_add_item_icon(&ir_menu, "S", "Scan Files", ir_scan_files);
    menu_add_item_icon(&ir_menu, "B", "Browse", ir_browse_files);
    menu_add_item_icon(&ir_menu, "T", "Test Signal", ir_test_signal);
    menu_add_item_icon(&ir_menu, "<", "Back", back_to_main);
    
    menu_set_status("Ready");
    menu_set_active(&main_menu);
    menu_draw();
    
    ESP_LOGI(TAG, "Initialization complete, entering main loop");
    
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
