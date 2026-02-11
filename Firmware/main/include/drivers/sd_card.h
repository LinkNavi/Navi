#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define SD_CMD0   0x00  // GO_IDLE_STATE
#define SD_CMD8   0x08  // SEND_IF_COND
#define SD_CMD17  0x11  // READ_SINGLE_BLOCK
#define SD_CMD24  0x18  // WRITE_BLOCK
#define SD_CMD55  0x37  // APP_CMD
#define SD_CMD58  0x3A  // READ_OCR
#define SD_ACMD41 0x29  // SD_SEND_OP_COND

#define SD_BLOCK_SIZE 512

typedef struct {
    spi_device_handle_t spi;
    uint8_t cs_pin;
    uint8_t type;
} SD_Card;

static spi_device_handle_t sd_spi;
static uint8_t sd_cs;

static inline void sd_cs_high(void) {
    gpio_set_level((gpio_num_t)sd_cs, 1);
}

static inline void sd_cs_low(void) {
    gpio_set_level((gpio_num_t)sd_cs, 0);
}

static inline uint8_t sd_spi_transfer(uint8_t data) {
    spi_transaction_t trans = {};
    trans.length = 8;
    trans.tx_data[0] = data;
    trans.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    spi_device_transmit(sd_spi, &trans);
    return trans.rx_data[0];
}

static inline void sd_send_cmd(uint8_t cmd, uint32_t arg) {
    sd_spi_transfer(0x40 | cmd);
    sd_spi_transfer(arg >> 24);
    sd_spi_transfer(arg >> 16);
    sd_spi_transfer(arg >> 8);
    sd_spi_transfer(arg);
    sd_spi_transfer(cmd == SD_CMD0 ? 0x95 : cmd == SD_CMD8 ? 0x87 : 0xFF);
}

static inline uint8_t sd_get_response(void) {
    for (uint16_t i = 0; i < 1000; i++) {
        uint8_t r = sd_spi_transfer(0xFF);
        if (!(r & 0x80)) return r;
    }
    return 0xFF;
}

static inline uint8_t sd_init(uint8_t mosi, uint8_t miso, uint8_t clk, uint8_t cs) {
    sd_cs = cs;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << cs),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    sd_cs_high();
    
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = mosi;
    bus_cfg.miso_io_num = miso;
    bus_cfg.sclk_io_num = clk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 512;
    spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED);
    
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = 400000;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = -1;
    dev_cfg.queue_size = 1;
    spi_bus_add_device(SPI2_HOST, &dev_cfg, &sd_spi);
    
    // Init sequence
    for (uint8_t i = 0; i < 10; i++) sd_spi_transfer(0xFF);
    
    sd_cs_low();
    sd_send_cmd(SD_CMD0, 0);
    uint8_t r1 = sd_get_response();
    sd_cs_high();
    
    if (r1 != 0x01) return 0;
    
    sd_cs_low();
    sd_send_cmd(SD_CMD8, 0x1AA);
    r1 = sd_get_response();
    for (uint8_t i = 0; i < 4; i++) sd_spi_transfer(0xFF);
    sd_cs_high();
    
    // ACMD41
    for (uint16_t i = 0; i < 1000; i++) {
        sd_cs_low();
        sd_send_cmd(SD_CMD55, 0);
        sd_get_response();
        sd_cs_high();
        
        sd_cs_low();
        sd_send_cmd(SD_ACMD41, 0x40000000);
        r1 = sd_get_response();
        sd_cs_high();
        
        if (r1 == 0x00) break;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    return r1 == 0x00 ? 1 : 0;
}

static inline uint8_t sd_read_block(uint32_t block, uint8_t *buffer) {
    sd_cs_low();
    sd_send_cmd(SD_CMD17, block);
    
    if (sd_get_response() != 0x00) {
        sd_cs_high();
        return 0;
    }
    
    // Wait for data token
    while (sd_spi_transfer(0xFF) != 0xFE);
    
    // Read block
    for (uint16_t i = 0; i < SD_BLOCK_SIZE; i++) {
        buffer[i] = sd_spi_transfer(0xFF);
    }
    
    // CRC
    sd_spi_transfer(0xFF);
    sd_spi_transfer(0xFF);
    
    sd_cs_high();
    return 1;
}

static inline uint8_t sd_write_block(uint32_t block, const uint8_t *buffer) {
    sd_cs_low();
    sd_send_cmd(SD_CMD24, block);
    
    if (sd_get_response() != 0x00) {
        sd_cs_high();
        return 0;
    }
    
    // Data token
    sd_spi_transfer(0xFE);
    
    // Write block
    for (uint16_t i = 0; i < SD_BLOCK_SIZE; i++) {
        sd_spi_transfer(buffer[i]);
    }
    
    // Dummy CRC
    sd_spi_transfer(0xFF);
    sd_spi_transfer(0xFF);
    
    // Check response
    uint8_t resp = sd_spi_transfer(0xFF);
    sd_cs_high();
    
    return (resp & 0x1F) == 0x05 ? 1 : 0;
}

#define FAT16_BOOT_SECTOR_SIZE 512
#define FAT16_SECTORS_PER_CLUSTER 8
#define FAT16_RESERVED_SECTORS 1
#define FAT16_NUM_FATS 2
#define FAT16_ROOT_ENTRIES 512
#define FAT16_TOTAL_SECTORS 65536

static inline uint8_t sd_format_fat16(void) {
    uint8_t boot_sector[512] = {0};
    
    // Jump instruction
    boot_sector[0] = 0xEB;
    boot_sector[1] = 0x3C;
    boot_sector[2] = 0x90;
    
    // OEM name
    memcpy(&boot_sector[3], "MSWIN4.1", 8);
    
    // BPB (BIOS Parameter Block)
    boot_sector[11] = 0x00; boot_sector[12] = 0x02; // Bytes per sector (512)
    boot_sector[13] = FAT16_SECTORS_PER_CLUSTER;     // Sectors per cluster
    boot_sector[14] = FAT16_RESERVED_SECTORS; boot_sector[15] = 0x00; // Reserved sectors
    boot_sector[16] = FAT16_NUM_FATS;                // Number of FATs
    boot_sector[17] = 0x00; boot_sector[18] = 0x02;  // Root entries (512)
    boot_sector[19] = 0x00; boot_sector[20] = 0x00;  // Total sectors (use 32-bit field)
    boot_sector[21] = 0xF8;                          // Media descriptor
    boot_sector[22] = 0x00; boot_sector[23] = 0x01;  // Sectors per FAT (256)
    boot_sector[24] = 0x3F; boot_sector[25] = 0x00;  // Sectors per track
    boot_sector[26] = 0xFF; boot_sector[27] = 0x00;  // Number of heads
    boot_sector[28] = 0x00; boot_sector[29] = 0x00;  // Hidden sectors
    boot_sector[30] = 0x00; boot_sector[31] = 0x00;
    boot_sector[32] = 0x00; boot_sector[33] = 0x00;  // Total sectors (32-bit)
    boot_sector[34] = 0x01; boot_sector[35] = 0x00;
    
    // Extended BPB
    boot_sector[36] = 0x80;                          // Drive number
    boot_sector[37] = 0x00;                          // Reserved
    boot_sector[38] = 0x29;                          // Boot signature
    boot_sector[39] = 0x12; boot_sector[40] = 0x34;  // Volume ID
    boot_sector[41] = 0x56; boot_sector[42] = 0x78;
    memcpy(&boot_sector[43], "ESP32 SD   ", 11);    // Volume label
    memcpy(&boot_sector[54], "FAT16   ", 8);        // Filesystem type
    
    // Boot signature
    boot_sector[510] = 0x55;
    boot_sector[511] = 0xAA;
    
    // Write boot sector
    if (!sd_write_block(0, boot_sector)) return 0;
    
    // Clear FAT tables
    uint8_t fat_sector[512];
    memset(fat_sector, 0, 512);
    fat_sector[0] = 0xF8; // Media descriptor
    fat_sector[1] = 0xFF;
    fat_sector[2] = 0xFF;
    fat_sector[3] = 0xFF;
    
    for (uint8_t fat = 0; fat < FAT16_NUM_FATS; fat++) {
        uint32_t fat_start = FAT16_RESERVED_SECTORS + (fat * 256);
        sd_write_block(fat_start, fat_sector);
        
        // Clear rest of FAT
        memset(fat_sector, 0, 512);
        for (uint16_t i = 1; i < 256; i++) {
            sd_write_block(fat_start + i, fat_sector);
        }
    }
    
    // Clear root directory
    memset(fat_sector, 0, 512);
    uint32_t root_start = FAT16_RESERVED_SECTORS + (FAT16_NUM_FATS * 256);
    for (uint16_t i = 0; i < 32; i++) { // 512 entries * 32 bytes / 512
        sd_write_block(root_start + i, fat_sector);
    }
    
    return 1;
}

static inline uint8_t sd_is_fat_formatted(void) {
    uint8_t boot_sector[512];
    
    if (!sd_read_block(0, boot_sector)) return 0;
    
    // Check boot signature
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) return 0;
    
    // Check filesystem type string
    if (memcmp(&boot_sector[54], "FAT", 3) == 0) return 1;  // FAT16/FAT32
    if (memcmp(&boot_sector[82], "FAT32", 5) == 0) return 1; // FAT32
    
    // Check bytes per sector (should be 512)
    uint16_t bytes_per_sector = boot_sector[11] | (boot_sector[12] << 8);
    if (bytes_per_sector != 512) return 0;
    
    // Check sectors per cluster (power of 2)
    uint8_t spc = boot_sector[13];
    if (spc == 0 || (spc & (spc - 1)) != 0) return 0;
    
    return 1;
}

static inline uint8_t sd_format_fat16_safe(void) {
    if (sd_is_fat_formatted()) return 2; // Already formatted
    return sd_format_fat16() ? 1 : 0;    // 1=success, 0=fail
}

#define FAT16_ROOT_DIR_START (FAT16_RESERVED_SECTORS + (FAT16_NUM_FATS * 256))
#define FAT16_DATA_START (FAT16_ROOT_DIR_START + 32)

typedef struct {
    char name[11];      // 8.3 format
    uint8_t attr;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t cluster;
    uint32_t size;
} __attribute__((packed)) FAT_DirEntry;

static inline uint16_t sd_find_free_cluster(void) {
    uint8_t fat_sector[512];
    
    for (uint16_t sector = 0; sector < 256; sector++) {
        sd_read_block(FAT16_RESERVED_SECTORS + sector, fat_sector);
        
        for (uint16_t i = 0; i < 256; i++) {
            uint16_t cluster = sector * 256 + i;
            if (cluster < 2) continue; // Skip reserved
            
            uint16_t entry = fat_sector[i*2] | (fat_sector[i*2+1] << 8);
            if (entry == 0x0000) return cluster;
        }
    }
    return 0xFFFF;
}

static inline void sd_write_fat_entry(uint16_t cluster, uint16_t value) {
    uint8_t fat_sector[512];
    uint16_t sector = cluster / 256;
    uint16_t offset = (cluster % 256) * 2;
    
    for (uint8_t fat = 0; fat < FAT16_NUM_FATS; fat++) {
        uint32_t fat_addr = FAT16_RESERVED_SECTORS + (fat * 256) + sector;
        sd_read_block(fat_addr, fat_sector);
        fat_sector[offset] = value & 0xFF;
        fat_sector[offset + 1] = value >> 8;
        sd_write_block(fat_addr, fat_sector);
    }
}

static inline uint8_t sd_write_file(const char *filename, const uint8_t *data, uint32_t size) {
    uint8_t root_sector[512];
    FAT_DirEntry *entry = NULL;
    uint16_t entry_sector = 0;
    uint16_t entry_offset = 0;
    
    // Convert filename to 8.3 format
    char name83[11];
    memset(name83, ' ', 11);
    uint8_t i = 0, j = 0;
    while (filename[i] && filename[i] != '.' && j < 8) {
        name83[j++] = filename[i++] >= 'a' ? filename[i-1] - 32 : filename[i-1];
    }
    if (filename[i] == '.') {
        i++;
        j = 8;
        while (filename[i] && j < 11) {
            name83[j++] = filename[i++] >= 'a' ? filename[i-1] - 32 : filename[i-1];
        }
    }
    
    // Find empty directory entry
    for (uint16_t sec = 0; sec < 32; sec++) {
        sd_read_block(FAT16_ROOT_DIR_START + sec, root_sector);
        
        for (uint16_t e = 0; e < 16; e++) {
            FAT_DirEntry *dir = (FAT_DirEntry*)&root_sector[e * 32];
            if (dir->name[0] == 0x00 || dir->name[0] == 0xE5) {
                entry = dir;
                entry_sector = sec;
                entry_offset = e;
                goto found_entry;
            }
        }
    }
    return 0; // Root dir full
    
found_entry:
    // Allocate clusters
    uint16_t first_cluster = sd_find_free_cluster();
    if (first_cluster == 0xFFFF) return 0;
    
    uint16_t current_cluster = first_cluster;
    uint32_t remaining = size;
    uint32_t data_offset = 0;
    
    while (remaining > 0) {
        uint32_t chunk_size = remaining > (FAT16_SECTORS_PER_CLUSTER * 512) ? 
                              (FAT16_SECTORS_PER_CLUSTER * 512) : remaining;
        
        // Write data to cluster
        uint32_t cluster_sector = FAT16_DATA_START + ((current_cluster - 2) * FAT16_SECTORS_PER_CLUSTER);
        for (uint8_t s = 0; s < FAT16_SECTORS_PER_CLUSTER; s++) {
            if (data_offset >= size) break;
            
            uint8_t sector_data[512];
            uint32_t copy_size = (size - data_offset) > 512 ? 512 : (size - data_offset);
            memcpy(sector_data, &data[data_offset], copy_size);
            if (copy_size < 512) memset(&sector_data[copy_size], 0, 512 - copy_size);
            
            sd_write_block(cluster_sector + s, sector_data);
            data_offset += copy_size;
        }
        
        remaining -= chunk_size;
        
        if (remaining > 0) {
            uint16_t next_cluster = sd_find_free_cluster();
            if (next_cluster == 0xFFFF) return 0;
            sd_write_fat_entry(current_cluster, next_cluster);
            current_cluster = next_cluster;
        } else {
            sd_write_fat_entry(current_cluster, 0xFFFF); // EOF
        }
    }
    
    // Create directory entry
    memcpy(entry->name, name83, 11);
    entry->attr = 0x20; // Archive
    entry->cluster = first_cluster;
    entry->size = size;
    entry->time = 0;
    entry->date = 0;
    
    sd_write_block(FAT16_ROOT_DIR_START + entry_sector, root_sector);
    return 1;
}

// Add to sd_card.h - complete folder support

static inline uint16_t sd_find_dir_cluster(const char *path) {
    if (path[0] == '/' && path[1] == '\0') return 0; // Root
    
    uint8_t sector[512];
    uint16_t current_cluster = 0; // Start at root
    char name[12] = {0};
    uint8_t name_idx = 0;
    uint8_t i = (path[0] == '/') ? 1 : 0;
    
    while (1) {
        // Parse next path component
        name_idx = 0;
        memset(name, 0, 12);
        while (path[i] && path[i] != '/' && name_idx < 11) {
            name[name_idx++] = path[i++];
        }
        if (name_idx == 0) break;
        
        // Convert to 8.3
        char name83[11];
        memset(name83, ' ', 11);
        uint8_t j = 0, k = 0;
        while (name[j] && name[j] != '.' && k < 8) {
            name83[k++] = name[j] >= 'a' ? name[j] - 32 : name[j];
            j++;
        }
        if (name[j] == '.') {
            j++;
            k = 8;
            while (name[j] && k < 11) {
                name83[k++] = name[j] >= 'a' ? name[j] - 32 : name[j];
                j++;
            }
        }
        
        // Search directory
        uint8_t found = 0;
        uint32_t dir_start = (current_cluster == 0) ? FAT16_ROOT_DIR_START : 
                             FAT16_DATA_START + ((current_cluster - 2) * FAT16_SECTORS_PER_CLUSTER);
        uint8_t dir_sectors = (current_cluster == 0) ? 32 : FAT16_SECTORS_PER_CLUSTER;
        
        for (uint8_t s = 0; s < dir_sectors; s++) {
            sd_read_block(dir_start + s, sector);
            for (uint8_t e = 0; e < 16; e++) {
                FAT_DirEntry *entry = (FAT_DirEntry*)&sector[e * 32];
                if (entry->name[0] == 0x00) goto not_found;
                if (entry->name[0] == 0xE5) continue;
                
                if (memcmp(entry->name, name83, 11) == 0) {
                    if (!(entry->attr & 0x10)) return 0xFFFF; // Not a directory
                    current_cluster = entry->cluster;
                    found = 1;
                    break;
                }
            }
            if (found) break;
        }
        
        if (!found) return 0xFFFF;
        if (path[i] == '\0') return current_cluster;
        i++;
    }
    
not_found:
    return 0xFFFF;
}

static inline uint8_t sd_write_file_path(const char *path, const uint8_t *data, uint32_t size) {
    // Parse directory and filename
    char dir_path[256] = {0};
    char filename[64] = {0};
    
    int16_t last_slash = -1;
    for (uint16_t i = 0; path[i]; i++) {
        if (path[i] == '/') last_slash = i;
    }
    
    uint16_t dir_cluster = 0;
    if (last_slash > 0) {
        memcpy(dir_path, path, last_slash);
        dir_path[last_slash] = '\0';
        strcpy(filename, &path[last_slash + 1]);
        dir_cluster = sd_find_dir_cluster(dir_path);
        if (dir_cluster == 0xFFFF) return 0; // Dir not found
    } else {
        strcpy(filename, path[0] == '/' ? &path[1] : path);
    }
    
    // Convert filename to 8.3
    char name83[11];
    memset(name83, ' ', 11);
    uint8_t i = 0, j = 0;
    while (filename[i] && filename[i] != '.' && j < 8) {
        name83[j++] = filename[i] >= 'a' ? filename[i] - 32 : filename[i];
        i++;
    }
    if (filename[i] == '.') {
        i++;
        j = 8;
        while (filename[i] && j < 11) {
            name83[j++] = filename[i] >= 'a' ? filename[i] - 32 : filename[i];
            i++;
        }
    }
    
    // Find empty entry in directory
    uint8_t sector[512];
    uint32_t dir_start = (dir_cluster == 0) ? FAT16_ROOT_DIR_START : 
                         FAT16_DATA_START + ((dir_cluster - 2) * FAT16_SECTORS_PER_CLUSTER);
    uint8_t dir_sectors = (dir_cluster == 0) ? 32 : FAT16_SECTORS_PER_CLUSTER;
    
    FAT_DirEntry *entry = NULL;
    uint16_t entry_sector = 0;
    
    for (uint8_t s = 0; s < dir_sectors; s++) {
        sd_read_block(dir_start + s, sector);
        for (uint8_t e = 0; e < 16; e++) {
            FAT_DirEntry *dir = (FAT_DirEntry*)&sector[e * 32];
            if (dir->name[0] == 0x00 || dir->name[0] == 0xE5) {
                entry = dir;
                entry_sector = s;
                goto found_entry;
            }
        }
    }
    return 0;
    
found_entry:
    // Allocate clusters and write data
    uint16_t first_cluster = sd_find_free_cluster();
    if (first_cluster == 0xFFFF) return 0;
    
    uint16_t current_cluster = first_cluster;
    uint32_t remaining = size;
    uint32_t data_offset = 0;
    
    while (remaining > 0) {
        uint32_t cluster_sector = FAT16_DATA_START + ((current_cluster - 2) * FAT16_SECTORS_PER_CLUSTER);
        
        for (uint8_t s = 0; s < FAT16_SECTORS_PER_CLUSTER; s++) {
            if (data_offset >= size) break;
            
            uint8_t sector_data[512];
            uint32_t copy_size = (size - data_offset) > 512 ? 512 : (size - data_offset);
            memcpy(sector_data, &data[data_offset], copy_size);
            if (copy_size < 512) memset(&sector_data[copy_size], 0, 512 - copy_size);
            
            sd_write_block(cluster_sector + s, sector_data);
            data_offset += copy_size;
        }
        
        remaining = (remaining > (FAT16_SECTORS_PER_CLUSTER * 512)) ? 
                    (remaining - (FAT16_SECTORS_PER_CLUSTER * 512)) : 0;
        
        if (remaining > 0) {
            uint16_t next_cluster = sd_find_free_cluster();
            if (next_cluster == 0xFFFF) return 0;
            sd_write_fat_entry(current_cluster, next_cluster);
            current_cluster = next_cluster;
        } else {
            sd_write_fat_entry(current_cluster, 0xFFFF);
        }
    }
    
    // Write directory entry
    memcpy(entry->name, name83, 11);
    entry->attr = 0x20;
    entry->cluster = first_cluster;
    entry->size = size;
    entry->time = 0;
    entry->date = 0;
    
    sd_write_block(dir_start + entry_sector, sector);
    return 1;
}

static inline uint8_t sd_mkdir_path(const char *path) {
    char current_path[256] = "/";
    uint8_t path_len = 0;
    uint8_t i = (path[0] == '/') ? 1 : 0;
    
    while (path[i]) {
        // Build path component
        while (path[i] && path[i] != '/') {
            current_path[path_len + 1] = path[i];
            path_len++;
            i++;
        }
        current_path[path_len + 1] = '\0';
        
        // Check if exists
        if (sd_find_dir_cluster(current_path) == 0xFFFF) {
            // Need to create this directory
            char parent[256] = {0};
            char dirname[64] = {0};
            
            int16_t last_slash = -1;
            for (uint16_t j = 0; current_path[j]; j++) {
                if (current_path[j] == '/') last_slash = j;
            }
            
            uint16_t parent_cluster = 0;
            if (last_slash > 0) {
                memcpy(parent, current_path, last_slash);
                parent[last_slash] = '\0';
                strcpy(dirname, &current_path[last_slash + 1]);
                parent_cluster = sd_find_dir_cluster(parent);
            } else {
                strcpy(dirname, current_path[0] == '/' ? &current_path[1] : current_path);
            }
            
            // Convert to 8.3
            char name83[11];
            memset(name83, ' ', 11);
            uint8_t j = 0, k = 0;
            while (dirname[j] && j < 8) {
                name83[k++] = dirname[j] >= 'a' ? dirname[j] - 32 : dirname[j];
                j++;
            }
            
            // Find empty entry
            uint8_t sector[512];
            uint32_t dir_start = (parent_cluster == 0) ? FAT16_ROOT_DIR_START : 
                                 FAT16_DATA_START + ((parent_cluster - 2) * FAT16_SECTORS_PER_CLUSTER);
            uint8_t dir_sectors = (parent_cluster == 0) ? 32 : FAT16_SECTORS_PER_CLUSTER;
            
            FAT_DirEntry *entry = NULL;
            uint16_t entry_sector = 0;
            
            for (uint8_t s = 0; s < dir_sectors; s++) {
                sd_read_block(dir_start + s, sector);
                for (uint8_t e = 0; e < 16; e++) {
                    FAT_DirEntry *dir = (FAT_DirEntry*)&sector[e * 32];
                    if (dir->name[0] == 0x00 || dir->name[0] == 0xE5) {
                        entry = dir;
                        entry_sector = s;
                        goto create_dir;
                    }
                }
            }
            return 0;
            
create_dir:
            uint16_t cluster = sd_find_free_cluster();
            if (cluster == 0xFFFF) return 0;
            sd_write_fat_entry(cluster, 0xFFFF);
            
            // Create . and .. entries
            uint8_t dir_data[512] = {0};
            FAT_DirEntry *dot = (FAT_DirEntry*)&dir_data[0];
            FAT_DirEntry *dotdot = (FAT_DirEntry*)&dir_data[32];
            
            memset(dot->name, ' ', 11);
            dot->name[0] = '.';
            dot->attr = 0x10;
            dot->cluster = cluster;
            
            memset(dotdot->name, ' ', 11);
            dotdot->name[0] = '.';
            dotdot->name[1] = '.';
            dotdot->attr = 0x10;
            dotdot->cluster = parent_cluster;
            
            uint32_t cluster_sector = FAT16_DATA_START + ((cluster - 2) * FAT16_SECTORS_PER_CLUSTER);
            sd_write_block(cluster_sector, dir_data);
            
            // Add to parent
            memcpy(entry->name, name83, 11);
            entry->attr = 0x10;
            entry->cluster = cluster;
            entry->size = 0;
            
            sd_write_block(dir_start + entry_sector, sector);
        }
        
        if (path[i] == '/') i++;
    }
    
    return 1;
}

static inline uint8_t sd_read_file_path(const char *path, uint8_t *buffer, uint32_t *size) {
    // Parse path
    char dir_path[256] = {0};
    char filename[64] = {0};
    
    int16_t last_slash = -1;
    for (uint16_t i = 0; path[i]; i++) {
        if (path[i] == '/') last_slash = i;
    }
    
    uint16_t dir_cluster = 0;
    if (last_slash > 0) {
        memcpy(dir_path, path, last_slash);
        dir_path[last_slash] = '\0';
        strcpy(filename, &path[last_slash + 1]);
        dir_cluster = sd_find_dir_cluster(dir_path);
        if (dir_cluster == 0xFFFF) return 0;
    } else {
        strcpy(filename, path[0] == '/' ? &path[1] : path);
    }
    
    // Convert filename to 8.3
    char name83[11];
    memset(name83, ' ', 11);
    uint8_t i = 0, j = 0;
    while (filename[i] && filename[i] != '.' && j < 8) {
        name83[j++] = filename[i] >= 'a' ? filename[i] - 32 : filename[i];
        i++;
    }
    if (filename[i] == '.') {
        i++;
        j = 8;
        while (filename[i] && j < 11) {
            name83[j++] = filename[i] >= 'a' ? filename[i] - 32 : filename[i];
            i++;
        }
    }
    
    // Find file
    uint8_t sector[512];
    uint32_t dir_start = (dir_cluster == 0) ? FAT16_ROOT_DIR_START : 
                         FAT16_DATA_START + ((dir_cluster - 2) * FAT16_SECTORS_PER_CLUSTER);
    uint8_t dir_sectors = (dir_cluster == 0) ? 32 : FAT16_SECTORS_PER_CLUSTER;
    
    uint16_t file_cluster = 0;
    uint32_t file_size = 0;
    
    for (uint8_t s = 0; s < dir_sectors; s++) {
        sd_read_block(dir_start + s, sector);
        for (uint8_t e = 0; e < 16; e++) {
            FAT_DirEntry *entry = (FAT_DirEntry*)&sector[e * 32];
            if (entry->name[0] == 0x00) return 0;
            if (entry->name[0] == 0xE5) continue;
            
            if (memcmp(entry->name, name83, 11) == 0) {
                file_cluster = entry->cluster;
                file_size = entry->size;
                goto found_file;
            }
        }
    }
    return 0;
    
found_file:
    *size = file_size;
    uint32_t offset = 0;
    uint16_t current_cluster = file_cluster;
    
    while (current_cluster != 0xFFFF && current_cluster != 0x0000) {
        uint32_t cluster_sector = FAT16_DATA_START + ((current_cluster - 2) * FAT16_SECTORS_PER_CLUSTER);
        
        for (uint8_t s = 0; s < FAT16_SECTORS_PER_CLUSTER; s++) {
            if (offset >= file_size) break;
            
            sd_read_block(cluster_sector + s, sector);
            uint32_t copy_size = (file_size - offset) > 512 ? 512 : (file_size - offset);
            memcpy(&buffer[offset], sector, copy_size);
            offset += copy_size;
        }
        
        // Get next cluster from FAT
        uint8_t fat_sector[512];
        uint16_t fat_sec = current_cluster / 256;
        uint16_t fat_off = (current_cluster % 256) * 2;
        sd_read_block(FAT16_RESERVED_SECTORS + fat_sec, fat_sector);
        current_cluster = fat_sector[fat_off] | (fat_sector[fat_off + 1] << 8);
    }
    
    return 1;
}

#endif
