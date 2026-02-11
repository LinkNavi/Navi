// ir_database.h - Simplified wrapper for generated IR database
#ifndef IR_DATABASE_H
#define IR_DATABASE_H

#include <stdint.h>
#include <string.h>
#include "ir_signals.h"
#include "drivers/ir.h"

// Execute IR command from database
static inline void ir_db_execute(const IRSignal *signal) {
    if (!signal) return;
    
    // Parse protocol
    if (strcmp(signal->protocol, "NEC") == 0 || strcmp(signal->protocol, "NECext") == 0) {
        // Parse address (first byte of hex string)
        uint8_t addr = 0;
        if (signal->address[0] >= '0' && signal->address[0] <= '9') 
            addr = (signal->address[0] - '0') << 4;
        else if (signal->address[0] >= 'A' && signal->address[0] <= 'F') 
            addr = (signal->address[0] - 'A' + 10) << 4;
        
        if (signal->address[1] >= '0' && signal->address[1] <= '9') 
            addr |= signal->address[1] - '0';
        else if (signal->address[1] >= 'A' && signal->address[1] <= 'F') 
            addr |= signal->address[1] - 'A' + 10;
        
        // Parse command (first byte of hex string)
        uint8_t cmd = 0;
        if (signal->command[0] >= '0' && signal->command[0] <= '9') 
            cmd = (signal->command[0] - '0') << 4;
        else if (signal->command[0] >= 'A' && signal->command[0] <= 'F') 
            cmd = (signal->command[0] - 'A' + 10) << 4;
        
        if (signal->command[1] >= '0' && signal->command[1] <= '9') 
            cmd |= signal->command[1] - '0';
        else if (signal->command[1] >= 'A' && signal->command[1] <= 'F') 
            cmd |= signal->command[1] - 'A' + 10;
        
        ir_send_nec(addr, cmd);
    }
}

// Find signal in device by name (case-insensitive)
static inline const IRSignal* ir_db_find_signal(const IRDevice *device, const char *name) {
    if (!device) return NULL;
    
    for (size_t i = 0; i < device->signal_count; i++) {
        if (strcasecmp(device->signals[i].name, name) == 0) {
            return &device->signals[i];
        }
    }
    return NULL;
}

// Check if signal name matches pattern (for power, vol_up, etc)
static inline uint8_t ir_db_signal_matches(const char *signal_name, const char *pattern) {
    char lower[64];
    for (uint8_t i = 0; i < 63 && signal_name[i]; i++) {
        lower[i] = (signal_name[i] >= 'A' && signal_name[i] <= 'Z') ? 
                   signal_name[i] + 32 : signal_name[i];
    }
    lower[63] = '\0';
    
    return strstr(lower, pattern) != NULL;
}

// Execute all devices matching category with specific signal pattern
static inline uint16_t ir_db_blast_category(const char *category, const char *signal_pattern) {
    uint16_t count = 0;
    
    for (size_t i = 0; i < IR_DEVICE_COUNT; i++) {
        const IRDevice *dev = &IR_DEVICES[i];
        
        // Filter by category if specified
        if (category && category[0] && strcmp(dev->category, category) != 0) {
            continue;
        }
        
        // Find matching signal in device
        for (size_t j = 0; j < dev->signal_count; j++) {
            if (ir_db_signal_matches(dev->signals[j].name, signal_pattern)) {
                ir_db_execute(&dev->signals[j]);
                count++;
                break; // Only one signal per device
            }
        }
    }
    
    return count;
}

// Get unique categories
static inline uint8_t ir_db_get_categories(char categories[][32], uint8_t max_count) {
    uint8_t count = 0;
    
    for (size_t i = 0; i < IR_DEVICE_COUNT && count < max_count; i++) {
        const char *cat = IR_DEVICES[i].category;
        
        // Check if already in list
        uint8_t found = 0;
        for (uint8_t j = 0; j < count; j++) {
            if (strcmp(categories[j], cat) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            strncpy(categories[count], cat, 31);
            categories[count][31] = '\0';
            count++;
        }
    }
    
    return count;
}

#endif
