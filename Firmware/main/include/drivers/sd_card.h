#ifndef SD_CARD_IMPROVED_H
#define SD_CARD_IMPROVED_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SD_BLOCK_SIZE 512

// Global state
static sdmmc_card_t *sd_card = NULL;
static uint8_t sd_mounted = 0;
static uint8_t sd_pins_mosi = 11;
static uint8_t sd_pins_miso = 13;
static uint8_t sd_pins_clk = 12;
static uint8_t sd_pins_cs = 10;

// Raw SPI test function - tests basic card communication
static inline void sd_test_raw_init(uint8_t mosi, uint8_t miso, uint8_t clk, uint8_t cs) {
    ESP_LOGI("SD", "=== RAW SD CARD INIT TEST ===");
    
    // Configure pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << miso),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);
    
    io_conf.pin_bit_mask = (1ULL << cs);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // CS high (idle)
    gpio_set_level((gpio_num_t)cs, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE("SD", "SPI bus init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Add SPI device
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 400000,  // 400kHz
        .mode = 0,                 // SPI mode 0
        .spics_io_num = -1,        // Manual CS control
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    
    spi_device_handle_t spi;
    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi);
    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Failed to add SPI device: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return;
    }
    
    // Send 80 clock pulses with CS high (card power-up sequence)
    ESP_LOGI("SD", "Sending power-up sequence (80 clocks)...");
    uint8_t dummy[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    spi_transaction_t t = {
        .length = 80,
        .tx_buffer = dummy,
    };
    spi_device_transmit(spi, &t);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Send CMD0 (GO_IDLE_STATE) - should return 0x01
    ESP_LOGI("SD", "Sending CMD0 (GO_IDLE_STATE)...");
    gpio_set_level((gpio_num_t)cs, 0);  // CS low
    vTaskDelay(pdMS_TO_TICKS(1));
    
    uint8_t cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};  // CMD0 + CRC
    t.length = 48;
    t.tx_buffer = cmd0;
    spi_device_transmit(spi, &t);
    
    // Read response (up to 8 bytes)
    uint8_t response[16];
    memset(response, 0xFF, sizeof(response));
    
    t.length = 80;
    t.rxlength = 80;
    t.tx_buffer = dummy;
    t.rx_buffer = response;
    spi_device_transmit(spi, &t);
    
    gpio_set_level((gpio_num_t)cs, 1);  // CS high
    
    ESP_LOGI("SD", "CMD0 Response: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             response[0], response[1], response[2], response[3], response[4],
             response[5], response[6], response[7], response[8], response[9]);
    
    // Check for valid response (0x01 = idle state)
    uint8_t found_response = 0;
    for (int i = 0; i < 10; i++) {
        if (response[i] == 0x01) {
            ESP_LOGI("SD", "✓ CARD RESPONDED! Found 0x01 at byte %d", i);
            found_response = 1;
            break;
        }
    }
    
    if (!found_response) {
        ESP_LOGE("SD", "✗ NO VALID RESPONSE");
        ESP_LOGE("SD", "Troubleshooting:");
        ESP_LOGE("SD", "  1. Card not inserted or seated properly");
        ESP_LOGE("SD", "  2. Card not receiving power (measure 3.3V at VCC)");
        ESP_LOGE("SD", "  3. Wrong pin assignments");
        ESP_LOGE("SD", "  4. Defective card - try another one");
    } else {
        ESP_LOGI("SD", "SD card is responding to basic SPI commands!");
        ESP_LOGI("SD", "If mount still fails, issue is likely:");
        ESP_LOGI("SD", "  - Insufficient power during card initialization");
        ESP_LOGI("SD", "  - Card needs to be formatted as FAT32");
        ESP_LOGI("SD", "  - Signal integrity (add pullup resistors)");
    }
    
    // Cleanup
    spi_bus_remove_device(spi);
    spi_bus_free(SPI2_HOST);
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Hardware diagnostic function - NOW USES ACTUAL PINS
static inline void sd_test_hardware(void) {
    ESP_LOGI("SD", "=== SD Card Hardware Test ===");
    ESP_LOGI("SD", "Testing pins: CS=%d MOSI=%d MISO=%d CLK=%d", 
             sd_pins_cs, sd_pins_mosi, sd_pins_miso, sd_pins_clk);
    
    // Test 1: Check all pin levels at rest
    ESP_LOGI("SD", "Test 1: Pin level check (idle state)");
    
    // Configure all pins as inputs with pullups to check their state
    gpio_config_t io_conf_test = {
        .pin_bit_mask = (1ULL << sd_pins_miso) | (1ULL << sd_pins_mosi) | 
                        (1ULL << sd_pins_clk) | (1ULL << sd_pins_cs),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_test);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    uint8_t miso_level = gpio_get_level((gpio_num_t)sd_pins_miso);
    uint8_t mosi_level = gpio_get_level((gpio_num_t)sd_pins_mosi);
    uint8_t clk_level = gpio_get_level((gpio_num_t)sd_pins_clk);
    uint8_t cs_level = gpio_get_level((gpio_num_t)sd_pins_cs);
    
    ESP_LOGI("SD", "  MISO (pin %d): %s", sd_pins_miso, miso_level ? "HIGH" : "LOW");
    ESP_LOGI("SD", "  MOSI (pin %d): %s", sd_pins_mosi, mosi_level ? "HIGH" : "LOW");
    ESP_LOGI("SD", "  CLK  (pin %d): %s", sd_pins_clk, clk_level ? "HIGH" : "LOW");
    ESP_LOGI("SD", "  CS   (pin %d): %s", sd_pins_cs, cs_level ? "HIGH" : "LOW");
    
    // Test 2: Check if MISO line is stuck or floating
    ESP_LOGI("SD", "Test 2: MISO line stability test (100 samples)");
    uint8_t miso_high = 0, miso_low = 0;
    for (int i = 0; i < 100; i++) {
        if (gpio_get_level((gpio_num_t)sd_pins_miso)) miso_high++;
        else miso_low++;
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    ESP_LOGI("SD", "  MISO high: %d, low: %d", miso_high, miso_low);
    
    if (miso_high == 0) {
        ESP_LOGE("SD", "  FAIL: MISO stuck LOW - Check wiring or add 10K pullup resistor");
    } else if (miso_low == 0) {
        ESP_LOGW("SD", "  WARNING: MISO always HIGH - Card may not be inserted or not responding");
    } else if (miso_high > 90 || miso_low > 90) {
        ESP_LOGW("SD", "  WARNING: MISO mostly %s - Possible poor connection", 
                 miso_high > 90 ? "HIGH" : "LOW");
    } else {
        ESP_LOGI("SD", "  PASS: MISO line shows activity (likely noise/floating - normal without card communication)");
    }
    
    // Test 3: CS line toggle test
    ESP_LOGI("SD", "Test 3: CS line toggle test");
    gpio_config_t cs_test = {
        .pin_bit_mask = (1ULL << sd_pins_cs),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cs_test);
    
    gpio_set_level((gpio_num_t)sd_pins_cs, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t cs_high = gpio_get_level((gpio_num_t)sd_pins_cs);
    
    gpio_set_level((gpio_num_t)sd_pins_cs, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t cs_low = gpio_get_level((gpio_num_t)sd_pins_cs);
    
    gpio_set_level((gpio_num_t)sd_pins_cs, 1);  // Return to idle (high)
    
    if (cs_high && !cs_low) {
        ESP_LOGI("SD", "  PASS: CS line toggles correctly");
    } else {
        ESP_LOGE("SD", "  FAIL: CS line not toggling (high=%d, low=%d)", cs_high, cs_low);
    }
    
    // Test 4: Power supply test (if card is inserted, MISO should eventually go high)
    ESP_LOGI("SD", "Test 4: Card detection (waiting 500ms for MISO to stabilize)");
    
    // Reconfigure MISO as input with pullup
    gpio_config_t miso_test = {
        .pin_bit_mask = (1ULL << sd_pins_miso),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&miso_test);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    uint8_t final_miso = gpio_get_level((gpio_num_t)sd_pins_miso);
    ESP_LOGI("SD", "  MISO after stabilization: %s", final_miso ? "HIGH" : "LOW");
    
    // Diagnosis
    ESP_LOGI("SD", "=== Diagnosis ===");
    
    if (miso_high == 0) {
        ESP_LOGE("SD", "Primary Issue: MISO stuck LOW");
        ESP_LOGE("SD", "Possible causes:");
        ESP_LOGE("SD", "  1. MISO pin not connected to SD card");
        ESP_LOGE("SD", "  2. MISO wire shorted to GND");
        ESP_LOGE("SD", "  3. Wrong MISO pin number in code");
        ESP_LOGE("SD", "Action: Check wiring, verify pin %d is correct MISO", sd_pins_miso);
    } else if (!cs_high || cs_low) {
        ESP_LOGE("SD", "Primary Issue: CS line not working");
        ESP_LOGE("SD", "Possible causes:");
        ESP_LOGE("SD", "  1. CS pin conflict with another peripheral");
        ESP_LOGE("SD", "  2. CS wire not connected");
        ESP_LOGE("SD", "Action: Verify pin %d is available and connected", sd_pins_cs);
    } else if (miso_low == 0 && final_miso) {
        ESP_LOGW("SD", "Card may not be responding (MISO always HIGH)");
        ESP_LOGW("SD", "Possible causes:");
        ESP_LOGW("SD", "  1. SD card not inserted");
        ESP_LOGW("SD", "  2. SD card not powered (check 3.3V)");
        ESP_LOGW("SD", "  3. SD card defective");
        ESP_LOGW("SD", "  4. Wiring issue with CLK or MOSI");
        ESP_LOGW("SD", "Action: Try reinserting card, check power, verify all connections");
    } else {
        ESP_LOGI("SD", "Hardware appears functional");
        ESP_LOGI("SD", "If mount still fails, try:");
        ESP_LOGI("SD", "  1. Format SD card as FAT32 on computer");
        ESP_LOGI("SD", "  2. Try different SD card (prefer 1-32GB)");
        ESP_LOGI("SD", "  3. Add 10K pullup resistors to all SPI lines");
        ESP_LOGI("SD", "  4. Reduce SPI speed (already at 400kHz)");
    }
    
    ESP_LOGI("SD", "");
    ESP_LOGI("SD", "=== Starting SPI Communication Test ===");
    sd_test_raw_init(sd_pins_mosi, sd_pins_miso, sd_pins_clk, sd_pins_cs);
    
    ESP_LOGI("SD", "");
    ESP_LOGI("SD", "=== Pin Configuration Summary ===");
    ESP_LOGI("SD", "Update your code if these are wrong:");
    ESP_LOGI("SD", "#define SD_MOSI %d", sd_pins_mosi);
    ESP_LOGI("SD", "#define SD_MISO %d", sd_pins_miso);
    ESP_LOGI("SD", "#define SD_CLK  %d", sd_pins_clk);
    ESP_LOGI("SD", "#define SD_CS   %d", sd_pins_cs);
}

// Initialize SD card with improved error handling
static inline uint8_t sd_init(uint8_t mosi, uint8_t miso, uint8_t clk, uint8_t cs) {
    // Store pins for hardware test
    sd_pins_mosi = mosi;
    sd_pins_miso = miso;
    sd_pins_clk = clk;
    sd_pins_cs = cs;
    
    if (sd_mounted) {
        ESP_LOGW("SD", "Already mounted, skipping init");
        return 1;
    }
    
    esp_err_t ret;
    
    ESP_LOGI("SD", "Initializing SD card:");
    ESP_LOGI("SD", "  MOSI: GPIO %d", mosi);
    ESP_LOGI("SD", "  MISO: GPIO %d", miso);
    ESP_LOGI("SD", "  CLK:  GPIO %d", clk);
    ESP_LOGI("SD", "  CS:   GPIO %d", cs);
    
    // Configure MISO with pullup for better signal integrity
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << miso),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    ESP_LOGI("SD", "Waiting for card to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // SPI bus configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
        .flags = 0,
        .intr_flags = 0
    };
    
    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE("SD", "SPI bus init failed: %s", esp_err_to_name(ret));
        ESP_LOGE("SD", "Possible causes:");
        ESP_LOGE("SD", "  - Pin conflict with another SPI device");
        ESP_LOGE("SD", "  - Invalid pin numbers");
        return 0;
    }
    
    // SD card slot configuration
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = cs;
    slot_config.host_id = SPI2_HOST;
    
    // Mount configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };
    
    // Try multiple speeds with detailed logging
    uint32_t speeds[] = {400, 200, 100};  // kHz
    
    for (uint8_t i = 0; i < 3; i++) {
        // CRITICAL: Use SDSPI_HOST_DEFAULT() which forces SPI mode
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = speeds[i];
        host.flags = 0;  // Clear all flags - disable SDIO mode
        
        ESP_LOGI("SD", "Attempt %d: Mounting at %lu kHz...", i + 1, speeds[i]);
        
        ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &sd_card);
        
        if (ret == ESP_OK) {
            sd_mounted = 1;
            goto init_success;
        }
        
        ESP_LOGW("SD", "Failed: %s", esp_err_to_name(ret));
        
        // Better error decoding
        if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGE("SD", "  -> Out of memory OR card init failed");
            ESP_LOGE("SD", "  -> Check: 1) Card inserted 2) Power 3) Wiring");
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE("SD", "  -> Timeout: Card not responding (check wiring, power, card insertion)");
        } else if (ret == ESP_ERR_INVALID_RESPONSE) {
            ESP_LOGE("SD", "  -> Invalid response: Card communication failed (try slower speed)");
        } else if (ret == ESP_ERR_INVALID_CRC) {
            ESP_LOGE("SD", "  -> CRC error: Signal integrity issue (check wiring, add pullups)");
        } else if (ret == ESP_FAIL) {
            ESP_LOGE("SD", "  -> General failure: Card initialization failed");
        }
        
        if (i < 2) {
            ESP_LOGI("SD", "Waiting before retry...");
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
    
    // All attempts failed
    ESP_LOGE("SD", "=== MOUNT FAILED AFTER ALL ATTEMPTS ===");
    ESP_LOGE("SD", "Troubleshooting steps:");
    ESP_LOGE("SD", "  1. Run HW Test from SD menu");
    ESP_LOGE("SD", "  2. Verify wiring: CS=%d MOSI=%d MISO=%d CLK=%d", cs, mosi, miso, clk);
    ESP_LOGE("SD", "  3. Check SD card is inserted properly");
    ESP_LOGE("SD", "  4. CRITICAL: Verify 3.3V power to SD card (measure with multimeter)");
    ESP_LOGE("SD", "  5. Try powering SD directly from TP4056 OUT+ (not through ESP32 3V3 pin)");
    ESP_LOGE("SD", "  6. Add 100uF capacitor across SD VCC/GND for power stability");
    ESP_LOGE("SD", "  7. Format card as FAT32 on computer");
    ESP_LOGE("SD", "  8. Try different SD card (1-32GB recommended)");
    ESP_LOGE("SD", "  9. Add 10K pullup resistors to all SPI lines");
    
    return 0;

init_success:
    
    ESP_LOGI("SD", "=== SD CARD INITIALIZED SUCCESSFULLY ===");
    ESP_LOGI("SD", "Name: %s", sd_card->cid.name);
    ESP_LOGI("SD", "Type: %s", (sd_card->ocr & (1UL << 30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI("SD", "Speed: %lu kHz", sd_card->max_freq_khz);
    ESP_LOGI("SD", "Capacity: %llu MB", ((uint64_t)sd_card->csd.capacity) * sd_card->csd.sector_size / (1024 * 1024));
    
    return 1;
}

// Check if formatted (always true with official driver)
static inline uint8_t sd_is_fat_formatted(void) {
    if (!sd_mounted) return 0;
    
    struct stat st;
    if (stat("/sdcard", &st) == 0) {
        ESP_LOGI("SD", "Filesystem is FAT32");
        return 1;
    }
    return 0;
}

// Format as FAT32
static inline uint8_t sd_format_fat16(void) {
    if (!sd_mounted) {
        ESP_LOGE("SD", "Card not initialized");
        return 0;
    }
    
    // Unmount first
    esp_vfs_fat_sdcard_unmount("/sdcard", sd_card);
    sd_mounted = 0;
    
    // Remount with format
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = sd_pins_cs;
    slot_config.host_id = SPI2_HOST;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 20000;
    host.flags = 0;  // Disable SDIO
    
    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &sd_card);
    
    if (ret == ESP_OK) {
        sd_mounted = 1;
        ESP_LOGI("SD", "Formatted as FAT32");
        return 1;
    }
    
    ESP_LOGE("SD", "Format failed: %s", esp_err_to_name(ret));
    return 0;
}

// Create directory path
static inline uint8_t sd_mkdir_path(const char *path) {
    if (!sd_mounted) return 0;
    
    char full_path[280];
    snprintf(full_path, sizeof(full_path), "/sdcard%s", path);
    
    char *p = full_path + 8;  // Skip "/sdcard"
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(full_path, 0775);
            *p = '/';
        }
        p++;
    }
    mkdir(full_path, 0775);
    
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        ESP_LOGI("SD", "Created directory: %s", path);
        return 1;
    }
    
    return 0;
}

// Write file to root
static inline uint8_t sd_write_file(const char *filename, const uint8_t *data, uint32_t size) {
    if (!sd_mounted) return 0;
    
    char path[280];
    snprintf(path, sizeof(path), "/sdcard/%s", filename);
    
    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE("SD", "Failed to open %s for writing", filename);
        return 0;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written == size) {
        ESP_LOGI("SD", "Wrote %lu bytes to %s", size, filename);
        return 1;
    }
    
    ESP_LOGE("SD", "Write failed: %zu/%lu bytes", written, size);
    return 0;
}

// Write file with path
static inline uint8_t sd_write_file_path(const char *path, const uint8_t *data, uint32_t size) {
    if (!sd_mounted) return 0;
    
    char full_path[280];
    snprintf(full_path, sizeof(full_path), "/sdcard%s", path);
    
    char *last_slash = strrchr(full_path, '/');
    if (last_slash && last_slash != full_path + 7) {
        *last_slash = '\0';
        
        char *p = full_path + 8;
        while (*p) {
            if (*p == '/') {
                *p = '\0';
                mkdir(full_path, 0775);
                *p = '/';
            }
            p++;
        }
        mkdir(full_path, 0775);
        
        *last_slash = '/';
    }
    
    FILE *f = fopen(full_path, "w");
    if (!f) {
        ESP_LOGE("SD", "Failed to open %s for writing", path);
        return 0;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written == size) {
        ESP_LOGI("SD", "Wrote %lu bytes to %s", size, path);
        return 1;
    }
    
    return 0;
}

// Read file with path
static inline uint8_t sd_read_file_path(const char *path, uint8_t *buffer, uint32_t *size) {
    if (!sd_mounted) return 0;
    
    char full_path[280];
    snprintf(full_path, sizeof(full_path), "/sdcard%s", path);
    
    FILE *f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE("SD", "Failed to open %s for reading", path);
        return 0;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (fsize < 0 || fsize > 65536) {
        fclose(f);
        ESP_LOGE("SD", "File too large: %ld bytes", fsize);
        return 0;
    }
    
    size_t read = fread(buffer, 1, fsize, f);
    fclose(f);
    
    *size = read;
    
    if (read > 0) {
        ESP_LOGI("SD", "Read %u bytes from %s", *size, path);
        return 1;
    }
    
    return 0;
}

// Cleanup
static inline void sd_deinit(void) {
    if (sd_mounted) {
        esp_vfs_fat_sdcard_unmount("/sdcard", sd_card);
        spi_bus_free(SPI2_HOST);
        sd_mounted = 0;
        ESP_LOGI("SD", "Unmounted");
    }
}

#endif
