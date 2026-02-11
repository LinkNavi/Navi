// ir.h - IR blaster using RMT hardware (ESP-IDF v5.x)
#ifndef IR_BLASTER_H
#define IR_BLASTER_H

#include <stdint.h>
#include <string.h>
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"

// NEC timings (microseconds)
#define NEC_HEADER_HIGH  9000
#define NEC_HEADER_LOW   4500
#define NEC_BIT_HIGH     560
#define NEC_BIT_ONE_LOW  1690
#define NEC_BIT_ZERO_LOW 560
#define NEC_END_HIGH     560
// CRITICAL: The trailing space must be long enough for the receiver to detect
// end-of-frame. NEC total frame = 108ms. With 560us here the receiver may not
// decode the frame at all. Set to ~67ms to pad to 108ms total.
#define NEC_END_LOW      67500

#define IR_CARRIER_FREQ  38000
#define IR_RESOLUTION_HZ 1000000

// Shared handles — defined once in ir_globals.c
extern rmt_channel_handle_t ir_channel;
extern rmt_encoder_t *ir_nec_enc;

// Custom NEC encoder
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *copy_encoder;
    rmt_encoder_t *bytes_encoder;
    rmt_symbol_word_t nec_header;
    rmt_symbol_word_t nec_end;
    int state;
} ir_nec_encoder_t;

static inline size_t ir_nec_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                    const void *primary_data, size_t data_size,
                                    rmt_encode_state_t *ret_state) {
    ir_nec_encoder_t *nec = __containerof(encoder, ir_nec_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    const uint8_t *data = (const uint8_t *)primary_data;

    switch (nec->state) {
    case 0:
        encoded_symbols += nec->copy_encoder->encode(nec->copy_encoder, channel,
                            &nec->nec_header, sizeof(rmt_symbol_word_t), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            nec->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    // fall through
    case 1:
        encoded_symbols += nec->bytes_encoder->encode(nec->bytes_encoder, channel,
                            data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            nec->state = 2;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    // fall through
    case 2:
        encoded_symbols += nec->copy_encoder->encode(nec->copy_encoder, channel,
                            &nec->nec_end, sizeof(rmt_symbol_word_t), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            nec->state = RMT_ENCODING_RESET;
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out;
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static inline esp_err_t ir_nec_reset(rmt_encoder_t *encoder) {
    ir_nec_encoder_t *nec = __containerof(encoder, ir_nec_encoder_t, base);
    rmt_encoder_reset(nec->copy_encoder);
    rmt_encoder_reset(nec->bytes_encoder);
    nec->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static inline esp_err_t ir_nec_del(rmt_encoder_t *encoder) {
    ir_nec_encoder_t *nec = __containerof(encoder, ir_nec_encoder_t, base);
    rmt_del_encoder(nec->copy_encoder);
    rmt_del_encoder(nec->bytes_encoder);
    free(nec);
    return ESP_OK;
}

static inline void ir_init(uint8_t pin) {
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = 128,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &ir_channel));

    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = 0.33,
        .frequency_hz = IR_CARRIER_FREQ,
        .flags.polarity_active_low = false,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(ir_channel, &carrier_cfg));

    ir_nec_encoder_t *nec = calloc(1, sizeof(ir_nec_encoder_t));
    nec->base.encode = ir_nec_encode;
    nec->base.reset = ir_nec_reset;
    nec->base.del = ir_nec_del;

    nec->nec_header.duration0 = NEC_HEADER_HIGH;
    nec->nec_header.level0 = 1;
    nec->nec_header.duration1 = NEC_HEADER_LOW;
    nec->nec_header.level1 = 0;

    nec->nec_end.duration0 = NEC_END_HIGH;
    nec->nec_end.level0 = 1;
    nec->nec_end.duration1 = NEC_END_LOW;
    nec->nec_end.level1 = 0;

    rmt_copy_encoder_config_t copy_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &nec->copy_encoder));

    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = {
            .duration0 = NEC_BIT_HIGH,
            .level0 = 1,
            .duration1 = NEC_BIT_ZERO_LOW,
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = NEC_BIT_HIGH,
            .level0 = 1,
            .duration1 = NEC_BIT_ONE_LOW,
            .level1 = 0,
        },
        .flags.msb_first = 0,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bytes_cfg, &nec->bytes_encoder));

    ir_nec_enc = &nec->base;

    ESP_ERROR_CHECK(rmt_enable(ir_channel));
    ESP_LOGI("IR", "RMT IR initialized on pin %d", pin);
}

// Send raw 4 bytes — used for NECext where addr2 != ~addr1
// Sends the frame twice (command + repeat) for reliability
static inline void ir_send_nec_raw(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    if (!ir_channel || !ir_nec_enc) return;

    uint8_t payload[4] = { b0, b1, b2, b3 };

    rmt_tx_wait_all_done(ir_channel, 200);
    rmt_encoder_reset(ir_nec_enc);

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags.eot_level = 0,
    };

    // Send command frame
    esp_err_t ret = rmt_transmit(ir_channel, ir_nec_enc, payload, sizeof(payload), &tx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW("IR", "TX fail: %s (0x%02X 0x%02X 0x%02X 0x%02X)",
                 esp_err_to_name(ret), b0, b1, b2, b3);
        return;
    }
    rmt_tx_wait_all_done(ir_channel, 200);

    // Send it a second time for reliability (many TVs need this)
    rmt_encoder_reset(ir_nec_enc);
    rmt_transmit(ir_channel, ir_nec_enc, payload, sizeof(payload), &tx_cfg);
    rmt_tx_wait_all_done(ir_channel, 200);

  rmt_encoder_reset(ir_nec_enc);
    rmt_transmit(ir_channel, ir_nec_enc, payload, sizeof(payload), &tx_cfg);
    rmt_tx_wait_all_done(ir_channel, 200);

  rmt_encoder_reset(ir_nec_enc);
    rmt_transmit(ir_channel, ir_nec_enc, payload, sizeof(payload), &tx_cfg);
    rmt_tx_wait_all_done(ir_channel, 200);
}

// Standard NEC: auto-generates ~addr and ~cmd
static inline void ir_send_nec(uint8_t address, uint8_t command) {
    ir_send_nec_raw(address, (uint8_t)~address, command, (uint8_t)~command);
}

static inline void ir_send_raw(const uint16_t *data, uint16_t length) {
    (void)data;
    (void)length;
}

#endif
