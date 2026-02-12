// Main.c - Updated with BLE support
#include "ble_menu.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "drivers/ble.h"
#include "drivers/ble_commands.h"
#include "drivers/display.h"
#include "drivers/font.h"
#include "drivers/ir.h"
#include "drivers/rotary_pcnt.h"
#include "drivers/sd_card.h"
#include "esp_log.h"
#include "file_browser_local.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/pin_config.h"
#include "ir_system.h"
#include "menu.h"
#include "nvs_flash.h"
#include "pin_config.h"
#include "pin_config_menu.h"
#include "rotary_debug.h"
#include "wifi_menu.h"
#include "wifi_thingies_menu.h"
#include <stdio.h>
#include <string.h>
static const char *TAG = "NAVI";
#include "drivers/ble.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "modules/wifi_karma.h"
#include "pong.h"



// Global BLE state variables
uint8_t g_ble_connected = 0;
uint16_t g_ble_conn_handle = 0;
char g_ble_cmd_buffer[BLE_CMD_BUFFER_SIZE];
uint16_t g_ble_cmd_len = 0;
ble_cmd_callback_t g_ble_cmd_callback = NULL;
uint16_t g_ble_char_val_handle = 0;
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
static uint8_t display_sleeping = 0;
rmt_channel_handle_t ir_channel = NULL;
rmt_encoder_t *ir_nec_enc = NULL;

Menu *current_menu = NULL;
char status_text[32] = "";
RotaryPCNT encoder;

static Menu games_menu;
static Menu main_menu;
static Menu settings_menu;
static Menu display_menu;
static Menu sd_menu;
static Menu ir_menu;
static Menu ir_file_menu;
static Menu files_menu;
static Menu power_menu;
uint8_t sd_initialized = 0;
uint8_t ir_folder_scanned = 0;
uint8_t current_ir_file_index = 0;

static inline void delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static inline uint32_t millis(void) {
  return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void init_i2c(uint8_t sda, uint8_t scl) {
  i2c_config_t conf = {};
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = (gpio_num_t)sda;
  conf.scl_io_num = (gpio_num_t)scl;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = 400000;
  i2c_param_config(I2C_NUM_0, &conf);
  i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

  ESP_LOGI(TAG, "I2C initialized on SDA=%d, SCL=%d", sda, scl);
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

void open_power_menu(void) {
  menu_set_status("Power");
  menu_set_active(&power_menu);
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

  while (!rotary_pcnt_button_pressed(&encoder)) {
    rotary_pcnt_read(&encoder);
    delay(10);
  }
  delay(200);
  open_sd_menu();
}

void sd_test_init(void) {
  PinConfig *pins = pin_config_get();
  if (sd_initialized) {
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

    uint8_t choice = 0;
    while (1) {
      int8_t dir = rotary_pcnt_read(&encoder);
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

      if (rotary_pcnt_button_pressed(&encoder)) {
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
           pins->sd_cs, pins->sd_mosi, pins->sd_miso, pins->sd_clk);

  if (sd_init(pins->sd_mosi, pins->sd_miso, pins->sd_clk, pins->sd_cs)) {
    sd_initialized = 1;
    display_clear();
    set_cursor(2, 10);
    println("SD Card OK!");

    if (sd_is_fat_formatted()) {
      println("Already FAT");
      println("");
      println("Format anyway?");
      println("");
      println("Turn: Select");
      println("Press: Confirm");
      display_show();

      uint8_t do_format = 0;
      while (1) {
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir != 0) {
          do_format = !do_format;
          display_clear();
          set_cursor(2, 10);
          println("Already FAT");
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

        if (rotary_pcnt_button_pressed(&encoder)) {
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

        ESP_LOGI(TAG, "Formatting SD card...");
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

      ESP_LOGI(TAG, "Formatting SD card...");
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
    char msg[32];
    snprintf(msg, sizeof(msg), "MOSI=%d MISO=%d", pins->sd_mosi, pins->sd_miso);
    println(msg);
    snprintf(msg, sizeof(msg), "CLK=%d CS=%d", pins->sd_clk, pins->sd_cs);
    println(msg);
    println("");
    println("Press to continue");
    display_show();
    ESP_LOGE(TAG, "SD card initialization failed");
  }

  while (!rotary_pcnt_button_pressed(&encoder)) {
    rotary_pcnt_read(&encoder);
    delay(10);
  }
  delay(200);
  open_sd_menu();
}
static void open_games_menu(void) {
    menu_set_status("Games");
    menu_set_active(&games_menu);
    menu_draw();
}

static void back_to_games_menu(void) {
    menu_set_status("Games");
    menu_set_active(&games_menu);
    menu_draw();
}

static void play_pong(void) {
    pong_play(&encoder);
    back_to_games_menu();
}

static void play_ball_game(void) {
    // This is your original game
    display_clear();
    int16_t x = WIDTH / 2, y = HEIGHT / 2;
    int16_t score = 0;

    ESP_LOGI(TAG, "Starting ball game");

    while (1) {
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir > 0) {
            y = (y - 2 + HEIGHT) % HEIGHT;
            score++;
        }
        if (dir < 0) {
            y = (y + 2) % HEIGHT;
            score++;
        }
        if (rotary_pcnt_button_pressed(&encoder))
            break;

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
    ESP_LOGI(TAG, "Ball game ended, score=%d", score);
    back_to_games_menu();
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
  if (sd_write_file("TEST.TXT", (uint8_t *)test1, strlen(test1))) {
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
  if (sd_write_file_path("/LOGS/BOOT.TXT", (uint8_t *)test2, strlen(test2))) {
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
  if (sd_write_file_path("/DATA/2025/FEB/SENSOR.TXT", (uint8_t *)test3,
                         strlen(test3))) {
    println("DEEP: OK");
    ESP_LOGI(TAG, "Deep nested file write OK");
  } else {
    println("DEEP: FAIL");
    ESP_LOGE(TAG, "Deep nested file write failed");
  }

  println("");
  println("Press to continue");
  display_show();

  while (!rotary_pcnt_button_pressed(&encoder)) {
    rotary_pcnt_read(&encoder);
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
    print((char *)buffer);
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

  while (!rotary_pcnt_button_pressed(&encoder)) {
    rotary_pcnt_read(&encoder);
    delay(10);
  }
  delay(200);
  open_sd_menu();
}

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
  PinConfig *pins = pin_config_get();
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

  while (1) {
    file_browser_draw();

    while (1) {
      int8_t dir = rotary_pcnt_read(&encoder);

      if (dir > 0) {
        file_browser_next();
        break;
      } else if (dir < 0) {
        file_browser_prev();
        break;
      }

      if (rotary_pcnt_button_pressed(&encoder)) {
        delay(200);

        if (browser.files[browser.selected].is_dir) {
          file_browser_enter(browser.selected);
          break;
        } else {
          if (file_browser_read_text(browser.selected)) {
            while (1) {
              file_browser_draw_text();

              uint8_t exit_viewer = 0;
              while (1) {
                int8_t dir = rotary_pcnt_read(&encoder);

                if (dir > 0) {
                  text_viewer_scroll_down();
                  break;
                } else if (dir < 0) {
                  text_viewer_scroll_up();
                  break;
                }

                if (rotary_pcnt_button_pressed(&encoder)) {
                  delay(200);
                  exit_viewer = 1;
                  text_viewer_active = 0;
                  break;
                }

                delay(5);
              }

              if (exit_viewer)
                break;
            }
            break;
          }
        }
      }

      delay(5);
    }

    if (strcmp(browser.current_path, "/") == 0 && browser.selected == 0 &&
        browser.count == 0) {
      break;
    }

    static uint32_t hold_start = 0;
    if (gpio_get_level((gpio_num_t)pins->rotary_sw) == 0) {
      if (hold_start == 0)
        hold_start = millis();
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

  while (!rotary_pcnt_button_pressed(&encoder)) {
    rotary_pcnt_read(&encoder);
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

  menu_init(&ir_file_menu, "IR Files");
  for (uint8_t i = 0; i < ir_file_list.count && i < MAX_MENU_ITEMS - 1; i++) {
    menu_add_item_icon(&ir_file_menu, "F", ir_file_list.files[i], NULL);
  }
  menu_add_item_icon(&ir_file_menu, "<", "Back", back_to_ir_menu);

  menu_set_status("Browse");
  menu_set_active(&ir_file_menu);
  menu_draw();
}

void rotary_debug_screen(void) {
  rotary_debug_run(&encoder);
  back_to_main();
}

void ir_test_signal(void) {
  display_clear();
  set_cursor(2, 10);
  set_font(FONT_TOMTHUMB);
  println("Testing TCL Roku...");
  println("");
  println("Power Toggle");
  println("Addr: EA C7");
  println("Cmd:  17 E8");
  display_show();

  ESP_LOGI(TAG, "Sending TCL Roku power toggle");
  ir_send_nec_raw(0xEA, 0xC7, 0x17, 0xE8);

  delay(500);
  println("");
  println("Sent!");
  println("Press to continue");
  display_show();

  while (!rotary_pcnt_button_pressed(&encoder)) {
    rotary_pcnt_read(&encoder);
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
  println("Version 1.3");
  println("");
  println("Features:");
  println("- IR Blaster");
  println("- X-BE-GONE");
  println("- SD FAT32");
  println("- File Browser");
  println("- BLE Control");
  println("");
  println("Press to return");
  display_show();

  while (!rotary_pcnt_button_pressed(&encoder)) {
    rotary_pcnt_read(&encoder);
    delay(10);
  }
  delay(200);
  back_to_main();
}

void game_screen(void) {
  display_clear();
  int16_t x = WIDTH / 2, y = HEIGHT / 2;
  int16_t score = 0;

  ESP_LOGI(TAG, "Starting game");

  while (1) {
    int8_t dir = rotary_pcnt_read(&encoder);
    if (dir > 0) {
      y = (y - 2 + HEIGHT) % HEIGHT;
      score++;
    }
    if (dir < 0) {
      y = (y + 2) % HEIGHT;
      score++;
    }
    if (rotary_pcnt_button_pressed(&encoder))
      break;

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

// Power Stuff
void power_off(void) {
    PinConfig *pins = pin_config_get();
    
    // Show shutdown message
    display_clear();
    set_cursor(2, HEIGHT/2 - 10);
    set_font(FONT_TOMTHUMB);
    println("Powering Off...");
    println("");
    println("Press button");
    println("to wake up");
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    // Clear and turn off display
    display_clear();
    display_show();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Entering deep sleep - wake on button press (GPIO %d)", pins->rotary_sw);
    
    // Configure wake-up on rotary button press (button is active low - pressed = 0)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)pins->rotary_sw, 0);
    
    // Enter deep sleep (device will restart when button is pressed)
    esp_deep_sleep_start();
}

void power_sleep(void) {
    PinConfig *pins = pin_config_get();
    static uint8_t display_sleeping = 0;
    
    if (!display_sleeping) {
        // Going to sleep
        display_clear();
        set_cursor(2, HEIGHT/2 - 10);
        set_font(FONT_TOMTHUMB);
        println("Display Sleep");
        println("");
        println("Turn or press");
        println("to wake up");
        display_show();
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Turn off display
        display_clear();
        display_show();
        display_sleeping = 1;
        
        ESP_LOGI(TAG, "Display sleeping - encoder activity will wake");
    } else {
        // Waking up from sleep
        display_sleeping = 0;
        ESP_LOGI(TAG, "Waking from display sleep");
        
        // Show wake message briefly
        display_clear();
        set_cursor(2, HEIGHT/2);
        set_font(FONT_TOMTHUMB);
        print("Waking up...");
        display_show();
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Return to main menu
        back_to_main();
    }
}

void power_restart(void) { 
    // Show restart message
    display_clear();
    set_cursor(2, HEIGHT/2);
    set_font(FONT_TOMTHUMB);
    println("Restarting...");
    println("");
    println("Please wait");
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    esp_restart();
}
// Main

void app_main(void) {
  ESP_LOGI(TAG, "Starting Navi firmware v1.5");
  ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
esp_log_level_set("wifi", ESP_LOG_ERROR);
  // Initialize pin config FIRST
  pin_config_init();
  PinConfig *pins = pin_config_get();

  // Use dynamic pins
  init_i2c(pins->i2c_sda, pins->i2c_scl); // Changed
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

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Initialize BLE
  ESP_LOGI(TAG, "Initializing BLE...");
  ble_init(ble_process_command);
  ESP_LOGI(TAG, "BLE initialized as 'Navi-Esp32'");

  // Init WiFi
  wifi_init_system();
  ESP_LOGI(TAG, "WiFi initialized");

  ir_init(pins->ir_pin);                                   // Changed
  ESP_LOGI(TAG, "IR initialized on pin %d", pins->ir_pin); // Changed

  rotary_pcnt_init(&encoder, pins->rotary_clk, pins->rotary_dt,
                   pins->rotary_sw); // Changed
  ESP_LOGI(TAG, "RotaryPCNT encoder initialized on CLK=%d, DT=%d, SW=%d",
           pins->rotary_clk, pins->rotary_dt, pins->rotary_sw); // Changed

  wifi_menu_init();
  wifi_thingies_init();
  ble_menu_init();

	karma_menu_init();
  // Initialize pin config menu (ADD THIS)
  pin_config_menu_init();

menu_init(&main_menu, "Main Menu");
menu_add_item_icon(&main_menu, "W", "WiFi", wifi_menu_open);
menu_add_item_icon(&main_menu, "T", "WiFi Thingies", wifi_thingies_open);
menu_add_item_icon(&main_menu, "B", "Bluetooth", ble_menu_open);
menu_add_item_icon(&main_menu, "I", "IR Control", open_ir_menu);
menu_add_item_icon(&main_menu, "F", "Files", open_file_browser);
menu_add_item_icon(&main_menu, "D", "SD Card", open_sd_menu);
menu_add_item_icon(&main_menu, "S", "Settings", open_settings);
menu_add_item_icon(&main_menu, "G", "Games", open_games_menu);  // Changed from game_screen
menu_add_item_icon(&main_menu, "P", "Power Menu", open_power_menu);
menu_add_item_icon(&main_menu, "?", "About", about_screen);

  menu_init(&settings_menu, "Settings");
  menu_add_item_icon(&settings_menu, "V", "Display", open_display_settings);
  menu_add_item_icon(&settings_menu, "P", "Pin Config",
                     pin_config_menu_open); // ADD THIS
  menu_add_item_icon(&settings_menu, "R", "Rotary Test", rotary_debug_screen);
  menu_add_item_icon(&settings_menu, "<", "Back", back_to_main);
  menu_init(&display_menu, "Display");
  menu_add_item_icon(&display_menu, "!", "Invert", toggle_invert);
  menu_add_item_icon(&display_menu, "+", "Contrast", adjust_contrast);
  menu_add_item_icon(&display_menu, "<", "Back", open_settings);

menu_init(&games_menu, "Games");
menu_add_item_icon(&games_menu, "P", "Pong", play_pong);
menu_add_item_icon(&games_menu, "B", "Ball", play_ball_game);
menu_add_item_icon(&games_menu, "<", "Back", back_to_main);

  menu_init(&sd_menu, "SD Card");
  menu_add_item_icon(&sd_menu, "I", "Initialize", sd_test_init);
  menu_add_item_icon(&sd_menu, "T", "HW Test", sd_hardware_test);
  menu_add_item_icon(&sd_menu, "W", "Write Test", sd_test_write);
  menu_add_item_icon(&sd_menu, "R", "Read Test", sd_test_read);
  menu_add_item_icon(&sd_menu, "<", "Back", back_to_main);

  menu_init(&ir_menu, "IR Control");
  menu_add_item_icon(&ir_menu, "S", "Scan Files", ir_scan_files);
  menu_add_item_icon(&ir_menu, "B", "Browse", ir_browse_files);
  menu_add_item_icon(&ir_menu, "T", "Test Signal", ir_test_signal);
  menu_add_item_icon(&ir_menu, "<", "Back", back_to_main);

  menu_init(&power_menu, "Power Menu");
  menu_add_item_icon(&power_menu, "P", "Power Off", power_off);
  menu_add_item_icon(&power_menu, "S", "Sleep", power_sleep);
  menu_add_item_icon(&power_menu, "R", "Restart", power_restart);

  menu_set_status("Ready");
  menu_set_active(&main_menu);
  menu_draw();

  ESP_LOGI(TAG, "Initialization complete");
  ESP_LOGI(TAG, "BLE advertising as 'Navi-Esp32'");
  ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

 while (1) {
    int8_t dir = rotary_pcnt_read(&encoder);

    // Check if we should wake from display sleep
    if (display_sleeping && (dir != 0 || rotary_pcnt_button_pressed(&encoder))) {
        // Wake from sleep
        display_sleeping = 0;
        back_to_main();
        delay(300);  // Debounce
        continue;
    }

    // Skip normal menu processing if display is sleeping
    if (display_sleeping) {
        delay(50);
        continue;
    }

    // Normal menu operation
    if (dir > 0) {
        menu_next();
        menu_draw();
        delay(50);
    } else if (dir < 0) {
        menu_prev();
        menu_draw();
        delay(50);
    }

    if (rotary_pcnt_button_pressed(&encoder)) {
        menu_select();
        delay(200);
    }

    delay(5);
}
}
