// ble_handler.h - BLE handler interface for Navi
#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <stdint.h>

// Initialize BLE handler
void ble_handler_init(void);

// Check if BLE is connected
uint8_t ble_handler_is_connected(void);

// Send notification to connected client
void ble_handler_notify(const char *data);

#endif
