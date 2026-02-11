// ir_system.h - Optimized IR file parser and X-BE-GONE system
#ifndef IR_SYSTEM_H
#define IR_SYSTEM_H

#include <stdint.h>
#include <string.h>
#include "drivers/ir.h"
#include "drivers/sd_card.h"
#include "drivers/display.h"

// FAT16 constants for backwards compatibility (not actually used with official driver)
#define FAT16_ROOT_DIR_START 0
#define FAT16_DATA_START 0
#define FAT16_SECTORS_PER_CLUSTER 8

#define MAX_IR_COMMANDS 32
#define MAX_IR_NAME_LEN 32
#define IR_FILE_BUFFER 512

typedef struct {
    char name[MAX_IR_NAME_LEN];
    uint8_t protocol;  // 0=NEC, 1=Raw
    uint8_t address;
    uint8_t command;
    uint16_t raw_len;
    uint16_t *raw_data;  // For raw protocol (not implemented yet)
} IR_Command;

typedef struct {
    char filename[64];
    IR_Command commands[MAX_IR_COMMANDS];
    uint8_t count;
} IR_File;

static IR_File current_ir_file;
static char ir_folder_path[256] = "/IR";

// Parse hex value from string
static inline uint8_t parse_hex_byte(const char *str) {
    uint8_t val = 0;
    for (uint8_t i = 0; i < 2 && str[i]; i++) {
        val <<= 4;
        if (str[i] >= '0' && str[i] <= '9') val |= str[i] - '0';
        else if (str[i] >= 'A' && str[i] <= 'F') val |= str[i] - 'A' + 10;
        else if (str[i] >= 'a' && str[i] <= 'f') val |= str[i] - 'a' + 10;
    }
    return val;
}

// Load IR file from SD card
static inline uint8_t ir_load_file(const char *path) {
    uint8_t buffer[IR_FILE_BUFFER];
    uint32_t size;
    
    if (!sd_read_file_path(path, buffer, &size)) return 0;
    if (size == 0 || size >= IR_FILE_BUFFER) return 0;
    
    buffer[size] = '\0';
    current_ir_file.count = 0;
    
    // Parse file line by line
    char *line = (char*)buffer;
    char *next_line;
    IR_Command *cmd = NULL;
    
    while (line && *line && current_ir_file.count < MAX_IR_COMMANDS) {
        // Find next line
        next_line = strchr(line, '\n');
        if (next_line) {
            *next_line = '\0';
            next_line++;
        }
        
        // Remove \r if present
        char *cr = strchr(line, '\r');
        if (cr) *cr = '\0';
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            line = next_line;
            continue;
        }
        
        // Parse "name: XXXXX"
        if (strncmp(line, "name: ", 6) == 0) {
            cmd = &current_ir_file.commands[current_ir_file.count];
            strncpy(cmd->name, line + 6, MAX_IR_NAME_LEN - 1);
            cmd->name[MAX_IR_NAME_LEN - 1] = '\0';
            current_ir_file.count++;
        }
        // Parse "protocol: NEC"
        else if (cmd && strncmp(line, "protocol: ", 10) == 0) {
            if (strncmp(line + 10, "NEC", 3) == 0) {
                cmd->protocol = 0;
            }
        }
        // Parse "address: 00 00 00 00"
        else if (cmd && strncmp(line, "address: ", 9) == 0) {
            cmd->address = parse_hex_byte(line + 9);
        }
        // Parse "command: 40 00 00 00"
        else if (cmd && strncmp(line, "command: ", 9) == 0) {
            cmd->command = parse_hex_byte(line + 9);
        }
        
        line = next_line;
    }
    
    return current_ir_file.count > 0;
}

// Execute single IR command
static inline void ir_execute_command(IR_Command *cmd) {
    if (cmd->protocol == 0) {  // NEC
        ir_send_nec(cmd->address, cmd->command);
    }
}

// Execute all commands in file with delay
static inline void ir_execute_all(uint16_t delay_ms) {
    for (uint8_t i = 0; i < current_ir_file.count; i++) {
        ir_execute_command(&current_ir_file.commands[i]);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// Find and execute specific command by name
static inline uint8_t ir_execute_by_name(const char *name) {
    for (uint8_t i = 0; i < current_ir_file.count; i++) {
        if (strcmp(current_ir_file.commands[i].name, name) == 0) {
            ir_execute_command(&current_ir_file.commands[i]);
            return 1;
        }
    }
    return 0;
}

// List IR files in directory (simplified - lists root only)
typedef struct {
    char files[16][64];  // Max 16 files
    uint8_t count;
} IR_FileList;

static IR_FileList ir_file_list;

static inline uint8_t ir_scan_folder(const char *folder) {
    ir_file_list.count = 0;
    
    char full_path[280];
    snprintf(full_path, sizeof(full_path), "/sdcard%s", folder);
    
    DIR *dir = opendir(full_path);
    if (!dir) {
        return 0;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && ir_file_list.count < 16) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check for .IR or .ir extension
        size_t len = strlen(entry->d_name);
        if (len > 3) {
            const char *ext = &entry->d_name[len - 3];
            if (strcasecmp(ext, ".IR") == 0) {
                strncpy(ir_file_list.files[ir_file_list.count], entry->d_name, 63);
                ir_file_list.files[ir_file_list.count][63] = '\0';
                ir_file_list.count++;
            }
        }
    }
    
    closedir(dir);
    return ir_file_list.count;
}

// X-BE-GONE: Execute "Power" command from all IR files
static inline void ir_xbegone_run_all(void) {
    uint8_t executed = 0;
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("X-BE-GONE Active!");
    println("");
    display_show();
    
    char filepath[320];  // 256 (path) + 64 (filename) = 320
    
    for (uint8_t i = 0; i < ir_file_list.count; i++) {
        snprintf(filepath, sizeof(filepath), "%s/%s", ir_folder_path, ir_file_list.files[i]);
        
        if (ir_load_file(filepath)) {
            // Try to find and execute Power command
            if (ir_execute_by_name("Power") || 
                ir_execute_by_name("POWER") || 
                ir_execute_by_name("Power_on") ||
                ir_execute_by_name("Power_off")) {
                executed++;
                
                println(ir_file_list.files[i]);
                display_show();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    println("");
    char result[32];
    snprintf(result, sizeof(result), "Sent: %d", executed);
    println(result);
    println("");
    println("Press to continue");
    display_show();
}

#endif
