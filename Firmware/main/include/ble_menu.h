// ble_menu.h - BLE menu system
#ifndef BLE_MENU_H
#define BLE_MENU_H

#include <stdint.h>
#include "menu.h"
#include "drivers/ble.h"

extern Menu ble_main_menu;

void ble_menu_init(void);
void ble_menu_open(void);

#endif
