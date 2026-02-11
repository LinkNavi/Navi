// bytes.h
#ifndef BYTES_H
#define BYTES_H

#include <stdint.h>

// Bit manipulation helpers
#define BIT(n) (1 << (n))
#define SET_BIT(reg, bit) ((reg) |= BIT(bit))
#define CLEAR_BIT(reg, bit) ((reg) &= ~BIT(bit))
#define TOGGLE_BIT(reg, bit) ((reg) ^= BIT(bit))
#define CHECK_BIT(reg, bit) (((reg) >> (bit)) & 1)

// Byte packing/unpacking
static inline uint16_t bytes_to_u16(uint8_t high, uint8_t low) {
    return ((uint16_t)high << 8) | low;
}

static inline uint32_t bytes_to_u32(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0) {
    return ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) | 
           ((uint32_t)b1 << 8) | b0;
}

static inline void u16_to_bytes(uint16_t val, uint8_t *high, uint8_t *low) {
    *high = (val >> 8) & 0xFF;
    *low = val & 0xFF;
}

static inline void u32_to_bytes(uint32_t val, uint8_t *b3, uint8_t *b2, uint8_t *b1, uint8_t *b0) {
    *b3 = (val >> 24) & 0xFF;
    *b2 = (val >> 16) & 0xFF;
    *b1 = (val >> 8) & 0xFF;
    *b0 = val & 0xFF;
}

// Circular buffer for byte streams
typedef struct {
    uint8_t *buffer;
    uint16_t size;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} ByteBuffer;

static inline void byte_buffer_init(ByteBuffer *bb, uint8_t *buffer, uint16_t size) {
    bb->buffer = buffer;
    bb->size = size;
    bb->head = 0;
    bb->tail = 0;
    bb->count = 0;
}

static inline uint8_t byte_buffer_push(ByteBuffer *bb, uint8_t byte) {
    if (bb->count >= bb->size) return 0; // Full
    bb->buffer[bb->head] = byte;
    bb->head = (bb->head + 1) % bb->size;
    bb->count++;
    return 1;
}

static inline uint8_t byte_buffer_pop(ByteBuffer *bb, uint8_t *byte) {
    if (bb->count == 0) return 0; // Empty
    *byte = bb->buffer[bb->tail];
    bb->tail = (bb->tail + 1) % bb->size;
    bb->count--;
    return 1;
}

static inline uint8_t byte_buffer_peek(ByteBuffer *bb, uint8_t *byte) {
    if (bb->count == 0) return 0;
    *byte = bb->buffer[bb->tail];
    return 1;
}

static inline uint16_t byte_buffer_available(ByteBuffer *bb) {
    return bb->count;
}

static inline uint16_t byte_buffer_space(ByteBuffer *bb) {
    return bb->size - bb->count;
}

static inline void byte_buffer_clear(ByteBuffer *bb) {
    bb->head = 0;
    bb->tail = 0;
    bb->count = 0;
}

// Checksum helpers
static inline uint8_t checksum_xor(const uint8_t *data, uint16_t len) {
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

static inline uint8_t checksum_add(const uint8_t *data, uint16_t len) {
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

static inline uint16_t crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

#endif
