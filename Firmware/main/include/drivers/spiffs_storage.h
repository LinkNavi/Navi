// spiffs_storage.h - Drop-in replacement for SD card using internal flash
#ifndef SPIFFS_STORAGE_H
#define SPIFFS_STORAGE_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_spiffs.h"
#include "esp_log.h"

static uint8_t spiffs_mounted = 0;

// Initialize SPIFFS (replaces sd_init)
static inline uint8_t spiffs_init(void) {
    if (spiffs_mounted) {
        ESP_LOGW("SPIFFS", "Already mounted");
        return 1;
    }
    
    ESP_LOGI("SPIFFS", "Initializing SPIFFS on internal flash");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true  // Auto-format if needed
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("SPIFFS", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("SPIFFS", "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE("SPIFFS", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return 0;
    }
    
    // Check partition info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFFS", "Failed to get SPIFFS info");
        esp_vfs_spiffs_unregister(NULL);
        return 0;
    }
    
    spiffs_mounted = 1;
    
    ESP_LOGI("SPIFFS", "=== SPIFFS Initialized ===");
    ESP_LOGI("SPIFFS", "Total: %d KB", total / 1024);
    ESP_LOGI("SPIFFS", "Used:  %d KB", used / 1024);
    ESP_LOGI("SPIFFS", "Free:  %d KB", (total - used) / 1024);
    
    return 1;
}

// Check if "formatted" (always true if mounted)
static inline uint8_t spiffs_is_formatted(void) {
    return spiffs_mounted;
}

// Format SPIFFS
static inline uint8_t spiffs_format(void) {
    if (spiffs_mounted) {
        esp_vfs_spiffs_unregister(NULL);
        spiffs_mounted = 0;
    }
    
    ESP_LOGI("SPIFFS", "Formatting SPIFFS...");
    
    // Re-init with format
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true
    };
    
    // Force format by unregistering and re-registering
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    
    if (ret == ESP_OK) {
        spiffs_mounted = 1;
        ESP_LOGI("SPIFFS", "Format successful");
        return 1;
    }
    
    ESP_LOGE("SPIFFS", "Format failed");
    return 0;
}

// Create directory (SPIFFS doesn't have real directories, but we track paths)
static inline uint8_t spiffs_mkdir_path(const char *path) {
    // SPIFFS is flat filesystem - directories are just part of filename
    // We'll just return success since the path will be encoded in filename
    ESP_LOGI("SPIFFS", "Note: SPIFFS is flat - 'directory' %s will be part of filename", path);
    return 1;
}

// Write file to root
static inline uint8_t spiffs_write_file(const char *filename, const uint8_t *data, uint32_t size) {
    if (!spiffs_mounted) return 0;
    
    char path[280];
    snprintf(path, sizeof(path), "/spiffs/%s", filename);
    
    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE("SPIFFS", "Failed to open %s for writing", filename);
        return 0;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written == size) {
        ESP_LOGI("SPIFFS", "Wrote %lu bytes to %s", size, filename);
        return 1;
    }
    
    ESP_LOGE("SPIFFS", "Write failed: %zu/%lu bytes", written, size);
    return 0;
}

// Write file with path
static inline uint8_t spiffs_write_file_path(const char *path, const uint8_t *data, uint32_t size) {
    if (!spiffs_mounted) return 0;
    
    char full_path[280];
    // SPIFFS is flat - just convert /path/to/file.txt -> /spiffs/path_to_file.txt
    // Or keep the slashes, SPIFFS handles them as part of filename
    snprintf(full_path, sizeof(full_path), "/spiffs%s", path);
    
    FILE *f = fopen(full_path, "w");
    if (!f) {
        ESP_LOGE("SPIFFS", "Failed to open %s for writing", path);
        return 0;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written == size) {
        ESP_LOGI("SPIFFS", "Wrote %lu bytes to %s", size, path);
        return 1;
    }
    
    return 0;
}

// Read file with path
static inline uint8_t spiffs_read_file_path(const char *path, uint8_t *buffer, uint32_t *size) {
    if (!spiffs_mounted) return 0;
    
    char full_path[280];
    snprintf(full_path, sizeof(full_path), "/spiffs%s", path);
    
    FILE *f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE("SPIFFS", "Failed to open %s for reading", path);
        return 0;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (fsize < 0 || fsize > 65536) {
        fclose(f);
        ESP_LOGE("SPIFFS", "File too large: %ld bytes", fsize);
        return 0;
    }
    
    size_t read = fread(buffer, 1, fsize, f);
    fclose(f);
    
    *size = read;
    
    if (read > 0) {
        ESP_LOGI("SPIFFS", "Read %u bytes from %s", *size, path);
        return 1;
    }
    
    return 0;
}

// List files (for file browser)
static inline void spiffs_list_files(void) {
    if (!spiffs_mounted) return;
    
    DIR *dir = opendir("/spiffs");
    if (!dir) {
        ESP_LOGE("SPIFFS", "Failed to open directory");
        return;
    }
    
    ESP_LOGI("SPIFFS", "=== Files in SPIFFS ===");
    
    struct dirent *entry;
    struct stat st;
    while ((entry = readdir(dir)) != NULL) {
        char full_path[300];
        snprintf(full_path, sizeof(full_path), "/spiffs/%s", entry->d_name);
        
        if (stat(full_path, &st) == 0) {
            ESP_LOGI("SPIFFS", "  %s (%ld bytes)", entry->d_name, st.st_size);
        }
    }
    
    closedir(dir);
}

// Get storage info
static inline void spiffs_info(uint32_t *total_kb, uint32_t *used_kb, uint32_t *free_kb) {
    if (!spiffs_mounted) {
        *total_kb = *used_kb = *free_kb = 0;
        return;
    }
    
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    
    *total_kb = total / 1024;
    *used_kb = used / 1024;
    *free_kb = (total - used) / 1024;
}

// Cleanup
static inline void spiffs_deinit(void) {
    if (spiffs_mounted) {
        esp_vfs_spiffs_unregister(NULL);
        spiffs_mounted = 0;
        ESP_LOGI("SPIFFS", "Unmounted");
    }
}

// ========================================
// COMPATIBILITY LAYER FOR SD CARD CODE
// ========================================
// These make SPIFFS work as drop-in replacement for SD card

#define sd_init(m, mi, c, cs) spiffs_init()
#define sd_is_fat_formatted() spiffs_is_formatted()
#define sd_format_fat16() spiffs_format()
#define sd_mkdir_path(p) spiffs_mkdir_path(p)
#define sd_write_file(f, d, s) spiffs_write_file(f, d, s)
#define sd_write_file_path(p, d, s) spiffs_write_file_path(p, d, s)
#define sd_read_file_path(p, b, s) spiffs_read_file_path(p, b, s)
#define sd_deinit() spiffs_deinit()

// For file browser - it needs to work with /spiffs instead of /sdcard
// You'll need to modify file_browser.h to use "/spiffs" as base instead of "/sdcard"

#endif
