// wifi_menu.h - WiFi configuration UI
#ifndef WIFI_MENU_H
#define WIFI_MENU_H

#include <stdint.h>
#include "menu.h"

extern Menu wifi_main_menu;
extern Menu wifi_scan_menu;

void wifi_menu_init(void);
void wifi_menu_open(void);

#endif
