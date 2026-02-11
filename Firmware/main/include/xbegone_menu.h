// xbegone_menu.h - X-BE-GONE Menu System
#ifndef XBEGONE_MENU_H
#define XBEGONE_MENU_H

#include <stdint.h>
#include "menu.h"
#include "drivers/display.h"
#include "drivers/rotary.h"

extern Rotary encoder;
extern void back_to_ir_menu(void);
extern void back_to_main(void);

extern Menu xbegone_main_menu;
extern Menu xbegone_power_menu;
extern Menu xbegone_category_menu;
extern Menu xbegone_volume_menu;
extern Menu xbegone_channel_menu;
extern Menu xbegone_misc_menu;

extern char xbegone_selected_category[32];

void xbegone_init_menus(void);
void xbegone_open_main(void);

#endif
