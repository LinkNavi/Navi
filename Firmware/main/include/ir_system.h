// ir_system.h - Enhanced IR file parser and X-BE-GONE system with signal type support
#ifndef IR_SYSTEM_H
#define IR_SYSTEM_H

#include <stdint.h>
#include <string.h>
#include "drivers/ir.h"
#include "drivers/sd_card.h"
#include "drivers/display.h"

#define MAX_IR_COMMANDS 32
#define MAX_IR_NAME_LEN 32
#define IR_FILE_BUFFER 512
#define MAX_FILENAME_LEN 320 
#define MAX_IR_FILES 64
#define MAX_CATEGORY_LEN 32

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
    char category[MAX_CATEGORY_LEN];  // e.g., "TVs", "ACs", etc.
    IR_Command commands[MAX_IR_COMMANDS];
    uint8_t count;
} IR_File;

// Signal type for targeted blasting
typedef enum {
    SIGNAL_POWER_OFF,
    SIGNAL_POWER_ON,
    SIGNAL_POWER_TOGGLE,
    SIGNAL_VOL_UP,
    SIGNAL_VOL_DOWN,
    SIGNAL_MUTE,
    SIGNAL_CH_UP,
    SIGNAL_CH_DOWN,
    SIGNAL_SOURCE,
    SIGNAL_MENU,
    SIGNAL_ALL
} SignalType;

static IR_File current_ir_file;
static char ir_folder_path[256] = "/IR";

// Extended file list with category support
typedef struct {
    char files[MAX_IR_FILES][MAX_FILENAME_LEN];  // Changed from [64]
    char categories[MAX_IR_FILES][MAX_CATEGORY_LEN];
    uint8_t count;
} IR_FileList;

static IR_FileList ir_file_list;

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

// Extract category from path (e.g., "/IR/TVs/Samsung/TV.ir" -> "TVs")
static inline void extract_category(const char *filepath, char *category) {
    category[0] = '\0';
    
    // Find the second slash (after /IR/)
    const char *start = strchr(filepath, '/');
    if (start) {
        start = strchr(start + 1, '/');
        if (start) {
            start++; // Skip the slash
            const char *end = strchr(start, '/');
            if (end) {
                size_t len = end - start;
                if (len > MAX_CATEGORY_LEN - 1) len = MAX_CATEGORY_LEN - 1;
                strncpy(category, start, len);
                category[len] = '\0';
            } else {
                // No further slash, use rest of string
                strncpy(category, start, MAX_CATEGORY_LEN - 1);
                category[MAX_CATEGORY_LEN - 1] = '\0';
            }
        }
    }
}

// Load IR file from SD card
static inline uint8_t ir_load_file(const char *path) {
    uint8_t buffer[IR_FILE_BUFFER];
    uint32_t size;
    
    if (!sd_read_file_path(path, buffer, &size)) return 0;
    if (size == 0 || size >= IR_FILE_BUFFER) return 0;
    
    buffer[size] = '\0';
    current_ir_file.count = 0;
    
    // Extract category from path
    extract_category(path, current_ir_file.category);
    
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

// Check if command name matches signal type
static inline uint8_t ir_command_matches_type(const char *name, SignalType type) {
    // Convert name to lowercase for comparison
    char lower_name[MAX_IR_NAME_LEN];
    for (uint8_t i = 0; i < MAX_IR_NAME_LEN && name[i]; i++) {
        lower_name[i] = (name[i] >= 'A' && name[i] <= 'Z') ? name[i] + 32 : name[i];
    }
    lower_name[MAX_IR_NAME_LEN - 1] = '\0';
    
    switch (type) {
        case SIGNAL_POWER_OFF:
            return strstr(lower_name, "power_off") != NULL ||
                   strstr(lower_name, "poweroff") != NULL ||
                   strcmp(lower_name, "off") == 0;
                   
        case SIGNAL_POWER_ON:
            return strstr(lower_name, "power_on") != NULL ||
                   strstr(lower_name, "poweron") != NULL ||
                   strcmp(lower_name, "on") == 0;
                   
        case SIGNAL_POWER_TOGGLE:
            return strcmp(lower_name, "power") == 0 ||
                   strcmp(lower_name, "toggle") == 0;
                   
        case SIGNAL_VOL_UP:
            return strstr(lower_name, "vol_up") != NULL ||
                   strstr(lower_name, "volup") != NULL ||
                   strstr(lower_name, "volume_up") != NULL ||
                   strcmp(lower_name, "vol+") == 0;
                   
        case SIGNAL_VOL_DOWN:
            return strstr(lower_name, "vol_dn") != NULL ||
                   strstr(lower_name, "vol_down") != NULL ||
                   strstr(lower_name, "voldown") != NULL ||
                   strstr(lower_name, "volume_down") != NULL ||
                   strcmp(lower_name, "vol-") == 0;
                   
        case SIGNAL_MUTE:
            return strcmp(lower_name, "mute") == 0;
            
        case SIGNAL_CH_UP:
            return strstr(lower_name, "ch_next") != NULL ||
                   strstr(lower_name, "ch_up") != NULL ||
                   strstr(lower_name, "channel_up") != NULL ||
                   strcmp(lower_name, "ch+") == 0;
                   
        case SIGNAL_CH_DOWN:
            return strstr(lower_name, "ch_prev") != NULL ||
                   strstr(lower_name, "ch_down") != NULL ||
                   strstr(lower_name, "channel_down") != NULL ||
                   strcmp(lower_name, "ch-") == 0;
                   
        case SIGNAL_SOURCE:
            return strcmp(lower_name, "source") == 0 ||
                   strcmp(lower_name, "input") == 0;
                   
        case SIGNAL_MENU:
            return strcmp(lower_name, "menu") == 0;
                   
        case SIGNAL_ALL:
            return 1;
            
        default:
            return 0;
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

// Find and execute command by signal type
static inline uint8_t ir_execute_by_type(SignalType type) {
    for (uint8_t i = 0; i < current_ir_file.count; i++) {
        if (ir_command_matches_type(current_ir_file.commands[i].name, type)) {
            ir_execute_command(&current_ir_file.commands[i]);
            return 1;
        }
    }
    return 0;
}

// ir_system.h line 265-310
static inline void ir_scan_directory_recursive(const char *base_path, const char *current_path) {
    char full_path[512];  // Increased from 280
    snprintf(full_path, sizeof(full_path), "/sdcard%s%s", base_path, current_path);
    
    DIR *dir = opendir(full_path);
    if (!dir) return;
    
    struct dirent *entry;
    struct stat st;
    
    while ((entry = readdir(dir)) != NULL && ir_file_list.count < MAX_IR_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char entry_path[768];  // Increased: 512 (path) + 256 (filename)
        snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path, entry->d_name);
        
        if (stat(entry_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                char new_path[512];  // Increased from 256
                snprintf(new_path, sizeof(new_path), "%s/%s", current_path, entry->d_name);
                ir_scan_directory_recursive(base_path, new_path);
            } else {
                size_t len = strlen(entry->d_name);
                if (len > 3) {
                    const char *ext = &entry->d_name[len - 3];
                    if (strcasecmp(ext, ".IR") == 0) {
                        snprintf(ir_file_list.files[ir_file_list.count], 
                                sizeof(ir_file_list.files[0]), 
                                "%s/%s", current_path, entry->d_name);
                        
                        char full_rel_path[512];  // Increased from 280
                        snprintf(full_rel_path, sizeof(full_rel_path), "%s/%s", current_path, entry->d_name);
                        extract_category(full_rel_path, ir_file_list.categories[ir_file_list.count]);
                        
                        ir_file_list.count++;
                    }
                }
            }
        }
    }
    
    closedir(dir);
}
// Scan IR folder and all subdirectories
static inline uint8_t ir_scan_folder(const char *folder) {
    ir_file_list.count = 0;
    strcpy(ir_folder_path, folder);
    
    ir_scan_directory_recursive(folder, "");
    
    return ir_file_list.count;
}

// Get signal type name for display
static inline const char* ir_get_signal_type_name(SignalType type) {
    switch (type) {
        case SIGNAL_POWER_OFF: return "Power Off";
        case SIGNAL_POWER_ON: return "Power On";
        case SIGNAL_POWER_TOGGLE: return "Power Toggle";
        case SIGNAL_VOL_UP: return "Volume Up";
        case SIGNAL_VOL_DOWN: return "Volume Down";
        case SIGNAL_MUTE: return "Mute";
        case SIGNAL_CH_UP: return "Channel Up";
        case SIGNAL_CH_DOWN: return "Channel Down";
        case SIGNAL_SOURCE: return "Source/Input";
        case SIGNAL_MENU: return "Menu";
        case SIGNAL_ALL: return "All Signals";
        default: return "Unknown";
    }
}

// X-BE-GONE: Execute specific signal type from all files
static inline void ir_xbegone_run_signal_type(SignalType type, const char *category_filter) {
    uint8_t executed = 0;
    uint8_t total_files = 0;
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("X-BE-GONE Active!");
    println("");
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Signal: %s", ir_get_signal_type_name(type));
    println(msg);
    
    if (category_filter && category_filter[0]) {
        snprintf(msg, sizeof(msg), "Category: %s", category_filter);
        println(msg);
    } else {
        println("Category: All");
    }
    
    println("");
    display_show();
    
    char filepath[600];
    
    for (uint8_t i = 0; i < ir_file_list.count; i++) {
        // Apply category filter if specified
        if (category_filter && category_filter[0]) {
            if (strcasecmp(ir_file_list.categories[i], category_filter) != 0) {
                continue;
            }
        }
        
        total_files++;
        snprintf(filepath, sizeof(filepath), "%s%s", ir_folder_path, ir_file_list.files[i]);
        
        if (ir_load_file(filepath)) {
            // Try to execute matching signal
            if (ir_execute_by_type(type)) {
                executed++;
                
                // Extract just the filename for display
                const char *filename = strrchr(ir_file_list.files[i], '/');
                if (filename) filename++;
                else filename = ir_file_list.files[i];
                
                // Truncate if too long
                char display_name[20];
                strncpy(display_name, filename, 19);
                display_name[19] = '\0';
                
                println(display_name);
                display_show();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    println("");
    snprintf(msg, sizeof(msg), "Sent: %d/%d", executed, total_files);
    println(msg);
    println("");
    println("Press to continue");
    display_show();
}

// X-BE-GONE: Execute all signals from all files (original behavior)
static inline void ir_xbegone_run_all(void) {
    ir_xbegone_run_signal_type(SIGNAL_POWER_TOGGLE, NULL);
}

// Get list of unique categories
static inline uint8_t ir_get_categories(char categories[][MAX_CATEGORY_LEN], uint8_t max_categories) {
    uint8_t count = 0;
    
    for (uint8_t i = 0; i < ir_file_list.count && count < max_categories; i++) {
        // Check if category already in list
        uint8_t found = 0;
        for (uint8_t j = 0; j < count; j++) {
            if (strcasecmp(categories[j], ir_file_list.categories[i]) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found && ir_file_list.categories[i][0]) {
            strncpy(categories[count], ir_file_list.categories[i], MAX_CATEGORY_LEN - 1);
            categories[count][MAX_CATEGORY_LEN - 1] = '\0';
            count++;
        }
    }
    
    return count;
}

// Repeat signal multiple times
static inline void ir_repeat_signal_type(SignalType type, const char *category_filter, uint8_t repeat_count) {
    for (uint8_t rep = 0; rep < repeat_count; rep++) {
        display_clear();
        set_cursor(2, 10);
        set_font(FONT_TOMTHUMB);
        
        char msg[32];
        snprintf(msg, sizeof(msg), "Repeat %d/%d", rep + 1, repeat_count);
        println(msg);
        println("");
        display_show();
        
        ir_xbegone_run_signal_type(type, category_filter);
        
        if (rep < repeat_count - 1) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

#endif
