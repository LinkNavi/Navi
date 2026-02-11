// ir_database.h - Simplified wrapper for generated IR database
#ifndef IR_DATABASE_H
#define IR_DATABASE_H

#include <stdint.h>
#include <string.h>
#include "ir_signals.h"
#include "drivers/ir.h"

// Blast progress info passed to callback
typedef struct {
    uint16_t current;       // Current device index (0-based)
    uint16_t total;         // Total devices to process
    uint16_t sent;          // Signals sent so far
    uint8_t  hit;           // 1 if this device had a matching signal and it was sent
    const char *brand;      // Current device brand
    const char *model;      // Current device model
} BlastProgress;

// Return 0 from callback to cancel, 1 to continue
typedef uint8_t (*blast_progress_cb)(const BlastProgress *progress);

// Parse one hex byte from string like "2F", returns 0 on failure
static inline uint8_t ir_parse_hex(const char *s, uint8_t *out) {
    uint8_t val = 0;
    for (uint8_t i = 0; i < 2; i++) {
        char c = s[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= c - '0';
        else if (c >= 'A' && c <= 'F') val |= c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') val |= c - 'a' + 10;
        else return 0;
    }
    *out = val;
    return 1;
}

// Parse 4 bytes from "XX XX XX XX" format
static inline uint8_t ir_parse_4bytes(const char *s, uint8_t out[4]) {
    // Format: "XX XX XX XX" (11 chars) or "XX" (just first byte)
    if (!s || strlen(s) < 2) return 0;
    
    if (!ir_parse_hex(s, &out[0])) return 0;
    
    // Try to parse remaining bytes, default to 0
    out[1] = 0; out[2] = 0; out[3] = 0;
    
    if (strlen(s) >= 5 && s[2] == ' ')
        ir_parse_hex(s + 3, &out[1]);
    if (strlen(s) >= 8 && s[5] == ' ')
        ir_parse_hex(s + 6, &out[2]);
    if (strlen(s) >= 11 && s[8] == ' ')
        ir_parse_hex(s + 9, &out[3]);
    
    return 1;
}

// Execute IR command from database
// NEC: sends addr, ~addr, cmd, ~cmd (standard)
// NECext: sends all 4 address bytes and all 4 command bytes as-is
static inline void ir_db_execute(const IRSignal *signal) {
    if (!signal) return;
    if (!signal->protocol || !signal->address || !signal->command) return;
    
    if (strcmp(signal->protocol, "NEC") == 0) {
        uint8_t addr, cmd;
        if (!ir_parse_hex(signal->address, &addr)) return;
        if (!ir_parse_hex(signal->command, &cmd)) return;
        ir_send_nec(addr, cmd);  // Standard: auto ~addr, ~cmd
    }
    else if (strcmp(signal->protocol, "NECext") == 0) {
        // NECext: addr and cmd bytes from DB are the raw frame bytes
        // "01 FF 00 00" -> send 0x01, 0xFF as addr pair
        // "12 ED 00 00" -> send 0x12, 0xED as cmd pair
        uint8_t addr_bytes[4], cmd_bytes[4];
        if (!ir_parse_4bytes(signal->address, addr_bytes)) return;
        if (!ir_parse_4bytes(signal->command, cmd_bytes)) return;
        ir_send_nec_raw(addr_bytes[0], addr_bytes[1], cmd_bytes[0], cmd_bytes[1]);
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

// Check if signal name matches pattern
static inline uint8_t ir_db_signal_matches(const char *signal_name, const char *pattern) {
    char lower[64];
    uint8_t i;
    for (i = 0; i < 63 && signal_name[i]; i++) {
        lower[i] = (signal_name[i] >= 'A' && signal_name[i] <= 'Z') ? 
                   signal_name[i] + 32 : signal_name[i];
    }
    lower[i] = '\0';
    
    return strstr(lower, pattern) != NULL;
}

// Count how many devices match (for progress total)
static inline uint16_t ir_db_count_matching(const char *category) {
    uint16_t count = 0;
    for (size_t i = 0; i < IR_DEVICE_COUNT; i++) {
        if (category && category[0] && strcmp(IR_DEVICES[i].category, category) != 0)
            continue;
        count++;
    }
    return count;
}

// Blast with progress callback and cancel support
static inline uint16_t ir_db_blast_category_cb(const char *category, const char *signal_pattern, blast_progress_cb cb) {
    uint16_t sent = 0;
    uint16_t total = ir_db_count_matching(category);
    uint16_t current = 0;
    
    for (size_t i = 0; i < IR_DEVICE_COUNT; i++) {
        const IRDevice *dev = &IR_DEVICES[i];
        
        if (category && category[0] && strcmp(dev->category, category) != 0)
            continue;
        
        current++;
        
        // Try to send matching signal first
        uint8_t hit = 0;
        for (size_t j = 0; j < dev->signal_count; j++) {
            if (ir_db_signal_matches(dev->signals[j].name, signal_pattern)) {
                ir_db_execute(&dev->signals[j]);
                sent++;
                hit = 1;
                break;
            }
        }
        
        // Then update UI and check cancel
        if (cb) {
            BlastProgress prog = {
                .current = current,
                .total = total,
                .sent = sent,
                .hit = hit,
                .brand = dev->brand,
                .model = dev->model
            };
            if (!cb(&prog)) {
                return sent; // Cancelled
            }
        }
    }
    
    return sent;
}

// Legacy wrapper without callback
static inline uint16_t ir_db_blast_category(const char *category, const char *signal_pattern) {
    return ir_db_blast_category_cb(category, signal_pattern, NULL);
}

// Get unique categories
static inline uint8_t ir_db_get_categories(char categories[][32], uint8_t max_count) {
    uint8_t count = 0;
    
    for (size_t i = 0; i < IR_DEVICE_COUNT && count < max_count; i++) {
        const char *cat = IR_DEVICES[i].category;
        
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
