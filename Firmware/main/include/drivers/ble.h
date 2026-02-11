// ble.h - BLE UART service for ESP32
#ifndef BLE_H
#define BLE_H

#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BLE_DEVICE_NAME "Navi-Esp32"
#define BLE_MTU_SIZE 512

// UUIDs matching Android app
#define BLE_SERVICE_UUID 0x4fafc201, 0x1fb5, 0x459e, 0x8fcc, 0xc5c9c331914b
#define BLE_CHAR_UUID 0xbeb5483e, 0x36e1, 0x4688, 0xb7f5, 0xea07361b26a8

// Command buffer
#define BLE_CMD_BUFFER_SIZE 256
static char ble_cmd_buffer[BLE_CMD_BUFFER_SIZE];
static uint16_t ble_cmd_len = 0;
static uint8_t ble_connected = 0;

// Connection handle
static uint16_t ble_conn_handle = 0;

// Callback for received commands
typedef void (*ble_cmd_callback_t)(const char *cmd, uint16_t len);
static ble_cmd_callback_t ble_cmd_callback = NULL;

// Characteristic handles
static uint16_t ble_char_val_handle;

// BLE characteristic access callback
static int ble_char_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI("BLE", "Read request");
            // Send response data if any
            os_mbuf_append(ctxt->om, ble_cmd_buffer, ble_cmd_len);
            return 0;
            
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            ESP_LOGI("BLE", "Write request, len=%d", OS_MBUF_PKTLEN(ctxt->om));
            
            // Copy data to buffer
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len > BLE_CMD_BUFFER_SIZE - 1) {
                len = BLE_CMD_BUFFER_SIZE - 1;
            }
            
            os_mbuf_copydata(ctxt->om, 0, len, ble_cmd_buffer);
            ble_cmd_buffer[len] = '\0';
            ble_cmd_len = len;
            
            ESP_LOGI("BLE", "Received: %s", ble_cmd_buffer);
            
            // Call command callback
            if (ble_cmd_callback) {
                ble_cmd_callback(ble_cmd_buffer, ble_cmd_len);
            }
            
            return 0;
            
        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

// GATT service definition
static const struct ble_gatt_svc_def ble_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(BLE_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID128_DECLARE(BLE_CHAR_UUID),
                .access_cb = ble_char_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | 
                         BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
                .val_handle = &ble_char_val_handle,
            },
            {0}
        }
    },
    {0}
};

// GAP event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI("BLE", "Connection %s, status=%d",
                     event->connect.status == 0 ? "established" : "failed",
                     event->connect.status);
            if (event->connect.status == 0) {
                ble_connected = 1;
                ble_conn_handle = event->connect.conn_handle;
            }
            break;
            
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI("BLE", "Disconnect, reason=%d", event->disconnect.reason);
            ble_connected = 0;
            
            // Resume advertising
            ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                             &(struct ble_gap_adv_params){}, NULL, NULL);
            break;
            
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI("BLE", "Advertising complete");
            break;
            
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI("BLE", "MTU update: %d", event->mtu.value);
            break;
            
        default:
            break;
    }
    return 0;
}

// Start advertising
static inline void ble_start_advertising(void) {
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    
    // Set device name
    fields.name = (uint8_t *)BLE_DEVICE_NAME;
    fields.name_len = strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;
    
    // Set flags
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    
    ble_gap_adv_set_fields(&fields);
    
    // Start advertising
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                     &adv_params, ble_gap_event, NULL);
    
    ESP_LOGI("BLE", "Advertising started as '%s'", BLE_DEVICE_NAME);
}

// BLE host task
static void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// BLE sync callback
static void ble_on_sync(void) {
    ESP_LOGI("BLE", "BLE host synchronized");
    ble_start_advertising();
}

// BLE reset callback
static void ble_on_reset(int reason) {
    ESP_LOGI("BLE", "BLE host reset, reason=%d", reason);
}

// Initialize BLE
static inline void ble_init(ble_cmd_callback_t callback) {
    ble_cmd_callback = callback;
    
    ESP_ERROR_CHECK(nimble_port_init());
    
    // Configure host
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    
    // Initialize GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    // Register custom GATT service
    ESP_ERROR_CHECK(ble_gatts_count_cfg(ble_gatt_svcs));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(ble_gatt_svcs));
    
    // Set device name
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(BLE_DEVICE_NAME));
    
    // Start BLE host task
    nimble_port_freertos_init(ble_host_task);
    
    ESP_LOGI("BLE", "BLE initialized as '%s'", BLE_DEVICE_NAME);
}

// Send notification to client
static inline esp_err_t ble_notify(const char *data, uint16_t len) {
    if (!ble_connected) {
        return ESP_FAIL;
    }
    
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) {
        return ESP_ERR_NO_MEM;
    }
    
    int rc = ble_gattc_notify_custom(ble_conn_handle, ble_char_val_handle, om);
    if (rc != 0) {
        ESP_LOGE("BLE", "Notify failed, rc=%d", rc);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// Check if BLE is connected
static inline uint8_t ble_is_connected(void) {
    return ble_connected;
}

// Send string via BLE
static inline void ble_send_string(const char *str) {
    ble_notify(str, strlen(str));
}

// Send formatted string via BLE
static inline void ble_printf(const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    if (len > 0 && len < sizeof(buffer)) {
        ble_notify(buffer, len);
    }
}

#endif
