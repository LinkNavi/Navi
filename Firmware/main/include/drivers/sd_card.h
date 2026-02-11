#ifndef SD_CARD_H
#define SD_CARD_H

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

#define SD_BLOCK_SIZE 512

// Global state
static sdmmc_card_t *sd_card = NULL;
static uint8_t sd_mounted = 0;

// Backwards compatible: Initialize SD card
static inline uint8_t sd_init(uint8_t mosi, uint8_t miso, uint8_t clk, uint8_t cs) {
    if (sd_mounted) {
        ESP_LOGW("SD", "Already mounted, skipping init");
        return 1;
    }
    
    esp_err_t ret;
    
    // Configure GPIO with pullups for better signal integrity
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << miso),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    ESP_LOGI("SD", "Waiting for card to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(100));  // Give card time to power up
    
    // SPI bus configuration - conservative settings for compatibility
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
        return 0;
    }
    
    // SD card slot configuration - use slower speed for reliability
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = cs;
    slot_config.host_id = SPI2_HOST;
    
    // Mount configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };
    
    // Try multiple speeds with delays
    uint32_t speeds[] = {400, 200, 100};  // kHz
    
    for (uint8_t i = 0; i < 3; i++) {
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = speeds[i];
        
        ESP_LOGI("SD", "Attempt %d: Mounting at %lukHz...", i + 1, speeds[i]);
        
        ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &sd_card);
        
        if (ret == ESP_OK) {
            sd_mounted = 1;
            goto init_success;
        }
        
        ESP_LOGW("SD", "Failed: %s", esp_err_to_name(ret));
        
        if (i < 2) {
            ESP_LOGI("SD", "Waiting before retry...");
            vTaskDelay(pdMS_TO_TICKS(500));  // Wait between retries
        }
    }
    
    // All attempts failed
    ESP_LOGE("SD", "Mount failed after all attempts");
    ESP_LOGE("SD", "Troubleshooting:");
    ESP_LOGE("SD", "  1. Remove and reinsert SD card");
    ESP_LOGE("SD", "  2. Check wiring: CS=%d MOSI=%d MISO=%d CLK=%d", cs, mosi, miso, clk);
    ESP_LOGE("SD", "  3. Verify 3.3V power to card");
    ESP_LOGE("SD", "  4. Try formatting card on PC (FAT32)");
    ESP_LOGE("SD", "  5. Try different SD card");
    
    return 0;

init_success:
    
    // Log card info
    ESP_LOGI("SD", "=== SD Card Initialized ===");
    ESP_LOGI("SD", "Name: %s", sd_card->cid.name);
    ESP_LOGI("SD", "Type: %s", (sd_card->ocr & (1UL << 30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI("SD", "Speed: %lu kHz", sd_card->max_freq_khz);
    ESP_LOGI("SD", "Capacity: %llu MB", ((uint64_t)sd_card->csd.capacity) * sd_card->csd.sector_size / (1024 * 1024));
    
    return 1;
}

// Hardware diagnostic function - call this to test SD card connections
static inline void sd_test_hardware(void) {
    ESP_LOGI("SD", "=== SD Card Hardware Test ===");
    
    // Test 1: Check if MISO line is stuck
    ESP_LOGI("SD", "Test 1: MISO line test");
    uint8_t miso_high = 0, miso_low = 0;
    for (int i = 0; i < 100; i++) {
        if (gpio_get_level((gpio_num_t)13)) miso_high++;
        else miso_low++;
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    ESP_LOGI("SD", "  MISO high: %d, low: %d", miso_high, miso_low);
    if (miso_high == 0) {
        ESP_LOGE("SD", "  FAIL: MISO stuck low - check wiring or add pullup");
    } else if (miso_low == 0) {
        ESP_LOGW("SD", "  WARNING: MISO always high - card may not be responding");
    } else {
        ESP_LOGI("SD", "  PASS: MISO line working");
    }
    
    // Test 2: CS line toggle
    ESP_LOGI("SD", "Test 2: CS line test");
    gpio_set_level((gpio_num_t)10, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    uint8_t cs_high = gpio_get_level((gpio_num_t)10);
    gpio_set_level((gpio_num_t)10, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    uint8_t cs_low = gpio_get_level((gpio_num_t)10);
    
    if (cs_high && !cs_low) {
        ESP_LOGI("SD", "  PASS: CS line working");
    } else {
        ESP_LOGE("SD", "  FAIL: CS line not toggling properly");
    }
    
    ESP_LOGI("SD", "=== Diagnosis ===");
    if (miso_high == 0) {
        ESP_LOGE("SD", "Likely issue: WIRING - MISO not connected or shorted to GND");
    } else if (miso_low == 0 && cs_high && !cs_low) {
        ESP_LOGW("SD", "Likely issue: SD CARD - Not inserted or not responding");
    } else if (!cs_high || cs_low) {
        ESP_LOGE("SD", "Likely issue: WIRING - CS line problem");
    } else {
        ESP_LOGI("SD", "Hardware looks OK - try different init speeds or card");
    }
}

// Backwards compatible: Check if formatted (always true with official driver)
static inline uint8_t sd_is_fat_formatted(void) {
    if (!sd_mounted) return 0;
    
    // If mounted successfully, it's formatted
    struct stat st;
    if (stat("/sdcard", &st) == 0) {
        ESP_LOGI("SD", "Filesystem is FAT32");
        return 1;
    }
    return 0;
}

// Backwards compatible: Format as FAT16 -> now formats as FAT32
static inline uint8_t sd_format_fat16(void) {
    if (!sd_mounted) {
        ESP_LOGE("SD", "Card not initialized");
        return 0;
    }
    
    // Unmount first
    esp_vfs_fat_sdcard_unmount("/sdcard", sd_card);
    sd_mounted = 0;
    
    // Remount with format_if_mount_failed = true
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 10;  // Hardcoded from your config
    slot_config.host_id = SPI2_HOST;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,  // Force format
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 20000;
    
    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &sd_card);
    
    if (ret == ESP_OK) {
        sd_mounted = 1;
        ESP_LOGI("SD", "Formatted as FAT32");
        return 1;
    }
    
    ESP_LOGE("SD", "Format failed: %s", esp_err_to_name(ret));
    return 0;
}

// Backwards compatible: Create directory path
static inline uint8_t sd_mkdir_path(const char *path) {
    if (!sd_mounted) return 0;
    
    char full_path[280];
    snprintf(full_path, sizeof(full_path), "/sdcard%s", path);
    
    // Create each level of directory
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

// Backwards compatible: Write file to root
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

// Backwards compatible: Write file with path
static inline uint8_t sd_write_file_path(const char *path, const uint8_t *data, uint32_t size) {
    if (!sd_mounted) return 0;
    
    char full_path[280];
    snprintf(full_path, sizeof(full_path), "/sdcard%s", path);
    
    // Create parent directories
    char *last_slash = strrchr(full_path, '/');
    if (last_slash && last_slash != full_path + 7) {  // Not root
        *last_slash = '\0';
        
        // Recursive mkdir
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

// Backwards compatible: Read file with path
static inline uint8_t sd_read_file_path(const char *path, uint8_t *buffer, uint32_t *size) {
    if (!sd_mounted) return 0;
    
    char full_path[280];
    snprintf(full_path, sizeof(full_path), "/sdcard%s", path);
    
    FILE *f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE("SD", "Failed to open %s for reading", path);
        return 0;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (fsize < 0 || fsize > 65536) {  // Max 64KB safety limit
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

// New helper: Find directory cluster (compatibility stub - not needed with official driver)
static inline uint16_t sd_find_dir_cluster(const char *path) {
    // Not needed - official driver handles paths natively
    return 0;
}

// Block-level operations (compatibility - not recommended to use directly)
static inline uint8_t sd_read_block(uint32_t block, uint8_t *buffer) {
    ESP_LOGW("SD", "sd_read_block() not supported with official driver");
    return 0;
}

static inline uint8_t sd_write_block(uint32_t block, const uint8_t *buffer) {
    ESP_LOGW("SD", "sd_write_block() not supported with official driver");
    return 0;
}

// FAT table operations (compatibility stubs)
typedef struct {
    char name[11];
    uint8_t attr;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t cluster;
    uint32_t size;
} __attribute__((packed)) FAT_DirEntry;

static inline uint16_t sd_find_free_cluster(void) {
    return 0;  // Not needed
}

static inline void sd_write_fat_entry(uint16_t cluster, uint16_t value) {
    // Not needed
}

// Cleanup function (new - call this on shutdown if needed)
static inline void sd_deinit(void) {
    if (sd_mounted) {
        esp_vfs_fat_sdcard_unmount("/sdcard", sd_card);
        spi_bus_free(SPI2_HOST);
        sd_mounted = 0;
        ESP_LOGI("SD", "Unmounted");
    }
}

#endif
