// xbegone_menu.h - X-BE-GONE Menu System
#ifndef XBEGONE_MENU_H
#define XBEGONE_MENU_H

#include <stdint.h>
#include "menu.h"
#include "ir_system.h"
#include "drivers/display.h"
#include "drivers/rotary.h"

// External dependencies that Main.c provides
extern Rotary encoder;
extern void back_to_ir_menu(void);
extern uint8_t ir_folder_scanned;

// Menu declarations
extern Menu xbegone_main_menu;
extern Menu xbegone_power_menu;
extern Menu xbegone_category_menu;
extern Menu xbegone_volume_menu;
extern Menu xbegone_channel_menu;
extern Menu xbegone_misc_menu;

// Category selection state
extern char xbegone_selected_category[MAX_CATEGORY_LEN];

// Initialize all X-BE-GONE menus
void xbegone_init_menus(void);

// Open the main X-BE-GONE menu
void xbegone_open_main(void);

#endif // XBEGONE_MENU_H
