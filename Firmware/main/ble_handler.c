// ble_handler.c - BLE command processor for Navi
#include "drivers/ble.h"
#include "drivers/ble_commands.h"
#include "esp_log.h"

static const char *TAG = "BLE_Handler";

void ble_handler_init(void) {
    ESP_LOGI(TAG, "Initializing BLE handler");
    ble_init(ble_process_command);
    ESP_LOGI(TAG, "BLE handler ready");
}
