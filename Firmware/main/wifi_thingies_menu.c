// wifi_thingies_menu.c
#include "include/menu.h"
#include "include/modules/wifi_spam.h"
#include "include/modules/wifi_deauth.h"
#include "include/modules/wifi_portal.h"
#include "include/modules/file_browser.h"
#include "include/drivers/display.h"
#include "modules/wifi_karma.h"
#include "include/drivers/rotary_pcnt.h"
#include "include/rotary_text_input.h"
#include "include/drivers/spiffs_storage.h"
#include "arp_poison_menu.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <stdio.h>
#include "null_ssid_spam.h"
#include "evil_twin_menu.h"
#include "dns_spoof_menu.h"
extern void back_to_main(void);
extern RotaryPCNT encoder;

static Menu wifi_menu;
static Menu spam_submenu;
static Menu deauth_submenu;
static Menu portal_submenu;
static Menu browser_submenu;

// WiFi Scanner
typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi;
} wifi_ap_t;

static wifi_ap_t scanned_aps[20];
static uint8_t scanned_count = 0;
static uint8_t selected_ap_index = 0;

static void wifi_scan_done(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 20) ap_count = 20;
    
    wifi_ap_record_t ap_records[20];
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    
    scanned_count = ap_count;
    for (uint8_t i = 0; i < ap_count; i++) {
        strncpy(scanned_aps[i].ssid, (char*)ap_records[i].ssid, 32);
        scanned_aps[i].ssid[32] = 0;
        memcpy(scanned_aps[i].bssid, ap_records[i].bssid, 6);
        scanned_aps[i].channel = ap_records[i].primary;
        scanned_aps[i].rssi = ap_records[i].rssi;
    }
}

static void wifi_start_scan(void) {
    static uint8_t wifi_scan_init = 0;
    
    if (!wifi_scan_init) {
        esp_err_t err;
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
        
        if (!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
            esp_netif_create_default_wifi_sta();
        }
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &wifi_scan_done, NULL);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        wifi_scan_init = 1;
    }
    
    wifi_scan_config_t scan_config = {0};
    scan_config.show_hidden = true;
    esp_wifi_scan_start(&scan_config, false);
}// ==================== NAVIGATION ====================

static void goto_spam_menu(void) {
    menu_set_active(&spam_submenu);
}

static void goto_deauth_menu(void) {
    menu_set_active(&deauth_submenu);
}

static void goto_portal_menu(void) {
    menu_set_active(&portal_submenu);
}

static void goto_browser_menu(void) {
    menu_set_active(&browser_submenu);
}

static void goto_wifi_menu(void) {
    menu_set_active(&wifi_menu);
}

// ==================== FILE BROWSER ====================

static void browser_start_handler(void) {
    display_clear();
    draw_string(0, 8, "Starting Browser...", FONT_TOMTHUMB);
    display_show();
    
    if (file_browser_start()) {
        display_clear();
        draw_string(0, 8, "Browser: :8080", FONT_TOMTHUMB);
        draw_string(0, 16, "IP: 192.168.4.1", FONT_TOMTHUMB);
        draw_string(0, 24, "Upload portal.html", FONT_TOMTHUMB);
    } else {
        display_clear();
        draw_string(0, 8, "Failed!", FONT_TOMTHUMB);
    }
    display_show();
    vTaskDelay(pdMS_TO_TICKS(3000));
}

static void browser_stop_handler(void) {
    file_browser_stop();
    display_clear();
    draw_string(0, 8, "Browser Stopped", FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ==================== BEACON SPAM ====================

static void spam_start_beacon(void) {
    if (spam_start()) {
        display_clear();
        draw_string(0, 8, "Spam Started!", FONT_TOMTHUMB);
        SpamConfig *cfg = spam_get_config();
        char buf[32];
        snprintf(buf, 32, "Networks: %d", spam_get_ssid_count());
        draw_string(0, 16, buf, FONT_TOMTHUMB);
        snprintf(buf, 32, "Power: %ddBm", cfg->tx_power / 4);
        draw_string(0, 24, buf, FONT_TOMTHUMB);
        display_show();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void spam_stop_beacon(void) {
    spam_stop();
    display_clear();
    draw_string(0, 8, "Spam Stopped", FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void spam_configure_power(void) {
    SpamConfig *cfg = spam_get_config();
    uint8_t power = cfg->tx_power;
    
    while (1) {
        display_clear();
        draw_string(0, 8, "TX Power", FONT_TOMTHUMB);
        
        char buf[32];
        snprintf(buf, 32, "Power: %ddBm", power / 4);
        draw_string(0, 24, buf, FONT_TOMTHUMB);
        
        if (power >= 80) draw_string(0, 32, "Range: 300-500ft", FONT_TOMTHUMB);
        else if (power >= 60) draw_string(0, 32, "Range: 200-300ft", FONT_TOMTHUMB);
        else draw_string(0, 32, "Range: 100-200ft", FONT_TOMTHUMB);
        
        display_show();
        
        int8_t rot = rotary_pcnt_read(&encoder);
        if (rot > 0 && power < 84) power += 4;
        if (rot < 0 && power > 8) power -= 4;
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            spam_set_tx_power(power);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void spam_configure_interval(void) {
    SpamConfig *cfg = spam_get_config();
    uint16_t interval = cfg->beacon_interval;
    
    while (1) {
        display_clear();
        draw_string(0, 8, "Beacon Interval", FONT_TOMTHUMB);
        
        char buf[32];
        snprintf(buf, 32, "Interval: %dms", interval);
        draw_string(0, 24, buf, FONT_TOMTHUMB);
        display_show();
        
        int8_t rot = rotary_pcnt_read(&encoder);
        if (rot > 0 && interval < 1000) interval += 10;
        if (rot < 0 && interval > 20) interval -= 10;
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            spam_set_interval(interval);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void spam_toggle_random_macs(void) {
    SpamConfig *cfg = spam_get_config();
    uint8_t current_state = cfg->randomize_order;  // Changed from random_macs
    spam_set_random_macs(!current_state);
    
    display_clear();
    draw_string(0, 8, "Random Order", FONT_TOMTHUMB);  // Better label
    draw_string(0, 16, current_state ? "Disabled" : "Enabled", FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void spam_add_custom(void) {
    char ssid[33];
    if (text_input_get(&encoder, "Custom SSID", ssid, sizeof(ssid), "")) {
        if (spam_add_custom_ssid(ssid)) {
            display_clear();
            draw_string(0, 8, "Added!", FONT_TOMTHUMB);
            draw_string(0, 16, ssid, FONT_TOMTHUMB);
        } else {
            display_clear();
            draw_string(0, 8, "Failed - List full", FONT_TOMTHUMB);
        }
        display_show();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

static void spam_toggle_list(void) {
    SpamConfig *cfg = spam_get_config();
    spam_use_custom_list(!cfg->use_custom_list);
    
    display_clear();
    draw_string(0, 8, "SSID List", FONT_TOMTHUMB);
    draw_string(0, 16, cfg->use_custom_list ? "Custom" : "Default", FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void spam_show_status(void) {
    SpamConfig *cfg = spam_get_config();
    
    display_clear();
    draw_string(0, 8, "Beacon Spam", FONT_TOMTHUMB);
    
    char buf[32];
    snprintf(buf, 32, "Status: %s", spam_is_running() ? "RUN" : "STOP");
    draw_string(0, 16, buf, FONT_TOMTHUMB);
    
    snprintf(buf, 32, "Networks: %d", spam_get_ssid_count());
    draw_string(0, 24, buf, FONT_TOMTHUMB);
    
    snprintf(buf, 32, "Power: %ddBm", cfg->tx_power / 4);
    draw_string(0, 32, buf, FONT_TOMTHUMB);
    
    snprintf(buf, 32, "Interval: %dms", cfg->beacon_interval);
    draw_string(0, 40, buf, FONT_TOMTHUMB);
    
    display_show();
    vTaskDelay(pdMS_TO_TICKS(3000));
}

// ==================== DEAUTH ====================

static void deauth_select_level(void) {
    deauth_init_config();
    DeauthLevel level = DEAUTH_LEVEL_SINGLE;
    
    while (1) {
        display_clear();
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        
        println("Aggression Level");
        println("");
        
        // Show all levels with descriptions
        if (level == DEAUTH_LEVEL_SINGLE) print("> ");
        else print("  ");
        println("1: Single Target");
        
        if (level == DEAUTH_LEVEL_MULTI) print("> ");
        else print("  ");
        println("2: Multi Target");
        
        if (level == DEAUTH_LEVEL_AGGRESSIVE) print("> ");
        else print("  ");
        println("3: Aggressive");
        
        if (level == DEAUTH_LEVEL_NUCLEAR) print("> ");
        else print("  ");
        println("4: NUCLEAR");
        
        println("");
        println("Turn: Select");
        println("Press: Confirm");
        
        display_show();
        
        int8_t rot = rotary_pcnt_read(&encoder);
        if (rot > 0) level = (level + 1) % 4;
        if (rot < 0) level = (level + 3) % 4;
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            deauth_set_level(level);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void deauth_show_config(void) {
    DeauthConfig *cfg = deauth_get_config();
    
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("Deauth Config");
    println("");
    
    char buf[32];
    snprintf(buf, 32, "Level: %s", deauth_get_level_name(cfg->level));
    println(buf);
    
    snprintf(buf, 32, "Burst: %d pkts", cfg->packets_per_burst);
    println(buf);
    
    snprintf(buf, 32, "Interval: %dms", cfg->burst_interval);
    println(buf);
    
    snprintf(buf, 32, "Ch Hop: %s", cfg->channel_hop ? "ON" : "OFF");
    println(buf);
    
    snprintf(buf, 32, "Broadcast: %s", cfg->broadcast_mode ? "ON" : "OFF");
    println(buf);
    
    println("");
    
    if (deauth_is_running()) {
        snprintf(buf, 32, "Packets: %lu",(long) deauth_get_packet_count());
        println(buf);
    }
    
    display_show();
    vTaskDelay(pdMS_TO_TICKS(3000));
}

static void deauth_start_attack(void) {
    if (deauth_get_target_count() == 0) {
        display_clear();
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        println("No targets!");
        println("");
        println("Scan & select");
        println("networks first");
        display_show();
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }
    
    if (deauth_start()) {
        display_clear();
        set_cursor(2, 8);
        set_font(FONT_TOMTHUMB);
        
        DeauthConfig *cfg = deauth_get_config();
        
        println("Deauth Started!");
        println("");
        
        char buf[32];
        snprintf(buf, 32, "Level: %s", deauth_get_level_name(cfg->level));
        println(buf);
        
        snprintf(buf, 32, "Targets: %d", deauth_get_target_count());
        println(buf);
        
        println("");
        
        switch (cfg->level) {
            case DEAUTH_LEVEL_SINGLE:
                println("Focused attack");
                break;
            case DEAUTH_LEVEL_MULTI:
                println("Multiple targets");
                break;
            case DEAUTH_LEVEL_AGGRESSIVE:
                println("Channel hopping!");
                break;
            case DEAUTH_LEVEL_NUCLEAR:
                println("MAXIMUM CHAOS!");
                break;
        }
        
        display_show();
        vTaskDelay(pdMS_TO_TICKS(2000));
    } else {
        display_clear();
        set_cursor(2, 8);
        println("Failed to start!");
        display_show();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

static void deauth_show_stats(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("Deauth Stats");
    println("");
    
    char buf[32];
    snprintf(buf, 32, "Status: %s", deauth_is_running() ? "RUNNING" : "STOPPED");
    println(buf);
    
    snprintf(buf, 32, "Targets: %d", deauth_get_target_count());
    println(buf);
    
    snprintf(buf, 32, "Packets: %lu", (long)deauth_get_packet_count());
    println(buf);
    
    DeauthConfig *cfg = deauth_get_config();
    snprintf(buf, 32, "Level: %s", deauth_get_level_name(cfg->level));
    println(buf);
    
    if (deauth_is_running()) {
        uint32_t pps = deauth_get_packet_count() / 
                       ((xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000 + 1);
        snprintf(buf, 32, "Rate: %lu pkt/s",(long) pps);
        println(buf);
    }
    
    display_show();
    vTaskDelay(pdMS_TO_TICKS(3000));
}
static void deauth_scan_networks(void) {
    display_clear();
    draw_string(0, 8, "Scanning WiFi...", FONT_TOMTHUMB);
    display_show();
    
    wifi_start_scan();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    display_clear();
    char buf[32];
    snprintf(buf, 32, "Found: %d nets", scanned_count);
    draw_string(0, 8, buf, FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1500));
}

static void deauth_select_target(void) {
    if (scanned_count == 0) {
        display_clear();
        draw_string(0, 8, "Scan first!", FONT_TOMTHUMB);
        display_show();
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }
    
    uint8_t index = 0;
    
    while (1) {
        display_clear();
        draw_string(0, 8, "Select Target", FONT_TOMTHUMB);
        
        char buf[32];
        snprintf(buf, 32, "%d/%d", index + 1, scanned_count);
        draw_string(0, 16, buf, FONT_TOMTHUMB);
        
        draw_string(0, 24, scanned_aps[index].ssid, FONT_TOMTHUMB);
        
        snprintf(buf, 32, "Ch:%d RSSI:%d", scanned_aps[index].channel, scanned_aps[index].rssi);
        draw_string(0, 32, buf, FONT_TOMTHUMB);
        
        display_show();
        
        int8_t rot = rotary_pcnt_read(&encoder);
        if (rot > 0) index = (index + 1) % scanned_count;
        if (rot < 0) index = (index == 0) ? scanned_count - 1 : index - 1;
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            deauth_add_target(scanned_aps[index].bssid, scanned_aps[index].channel, scanned_aps[index].ssid);
            display_clear();
            draw_string(0, 8, "Target Added!", FONT_TOMTHUMB);
            display_show();
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}



static void deauth_stop_attack(void) {
    deauth_stop();
    display_clear();
    draw_string(0, 8, "Deauth Stopped", FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void deauth_clear_targets_handler(void) {
    deauth_clear_targets();
    display_clear();
    draw_string(0, 8, "Targets Cleared", FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void deauth_show_targets(void) {
    display_clear();
    draw_string(0, 8, "Deauth Targets", FONT_TOMTHUMB);
    
    char buf[32];
    snprintf(buf, 32, "Count: %d", deauth_get_target_count());
    draw_string(0, 16, buf, FONT_TOMTHUMB);
    
    for (uint8_t i = 0; i < deauth_get_target_count() && i < 4; i++) {
        DeauthTarget *t = deauth_get_target(i);
        snprintf(buf, 32, "%d: %.20s", i + 1, t->ssid);
        draw_string(0, 24 + (i * 8), buf, FONT_TOMTHUMB);
    }
    
    display_show();
    vTaskDelay(pdMS_TO_TICKS(3000));
}

// ==================== PORTAL ====================

static void portal_select_network(void) {
    if (scanned_count == 0) {
        display_clear();
        draw_string(0, 8, "Scan first!", FONT_TOMTHUMB);
        display_show();
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }
    
    uint8_t index = 0;
    
    while (1) {
        display_clear();
        draw_string(0, 8, "Clone Network", FONT_TOMTHUMB);
        
        char buf[32];
        snprintf(buf, 32, "%d/%d", index + 1, scanned_count);
        draw_string(0, 16, buf, FONT_TOMTHUMB);
        draw_string(0, 24, scanned_aps[index].ssid, FONT_TOMTHUMB);
        
        display_show();
        
        int8_t rot = rotary_pcnt_read(&encoder);
        if (rot > 0) index = (index + 1) % scanned_count;
        if (rot < 0) index = (index == 0) ? scanned_count - 1 : index - 1;
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            selected_ap_index = index;
            display_clear();
            draw_string(0, 8, "Selected!", FONT_TOMTHUMB);
            display_show();
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void portal_start_handler(void) {
    if (scanned_count == 0 || selected_ap_index >= scanned_count) {
        display_clear();
        draw_string(0, 8, "Select net first!", FONT_TOMTHUMB);
        display_show();
        vTaskDelay(pdMS_TO_TICKS(1500));
        return;
    }
    
    display_clear();
    draw_string(0, 8, "Starting Portal...", FONT_TOMTHUMB);
    display_show();
    
    if (portal_start(scanned_aps[selected_ap_index].ssid)) {
        display_clear();
        draw_string(0, 8, "Portal Active!", FONT_TOMTHUMB);
        draw_string(0, 16, scanned_aps[selected_ap_index].ssid, FONT_TOMTHUMB);
        draw_string(0, 24, "IP: 192.168.4.1", FONT_TOMTHUMB);
        display_show();
        vTaskDelay(pdMS_TO_TICKS(3000));
    } else {
        display_clear();
        draw_string(0, 8, "Failed!", FONT_TOMTHUMB);
        display_show();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

static void portal_stop_handler(void) {
    portal_stop();
    display_clear();
    draw_string(0, 8, "Portal Stopped", FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void portal_view_captures(void) {
    display_clear();
    draw_string(0, 8, "Captured Creds", FONT_TOMTHUMB);
    draw_string(0, 16, "Check SPIFFS:", FONT_TOMTHUMB);
    draw_string(0, 24, "captures/", FONT_TOMTHUMB);
    draw_string(0, 32, "credentials.txt", FONT_TOMTHUMB);
    display_show();
    vTaskDelay(pdMS_TO_TICKS(3000));
}

// ==================== MENU INIT ====================


void init_wifi_thingies_submenu(Menu *parent_menu) {
    menu_init(&wifi_menu, "WiFi Thingies");
    menu_add_item(&wifi_menu, "Beacon Spam", goto_spam_menu);
    menu_add_item(&wifi_menu, "Deauth", goto_deauth_menu);
    menu_add_item(&wifi_menu, "Evil Portal", goto_portal_menu);
    menu_add_item(&wifi_menu, "Evil Twin", evil_twin_menu_open);
    menu_add_item(&wifi_menu, "ARP Poison", arp_poison_menu_open);
    menu_add_item(&wifi_menu, "DNS Spoof", dns_spoof_menu_open);
    menu_add_item(&wifi_menu, "Null SSID", null_ssid_menu_open);
    menu_add_item(&wifi_menu, "Karma Attack", karma_menu_open);
    menu_add_item(&wifi_menu, "File Browser", goto_browser_menu);
 
    menu_add_item(&wifi_menu, "Back", back_to_main);
    
    menu_init(&spam_submenu, "Beacon Spam");
    menu_add_item(&spam_submenu, "Status", spam_show_status);
    menu_add_item(&spam_submenu, "Start", spam_start_beacon);
    menu_add_item(&spam_submenu, "Stop", spam_stop_beacon);
    menu_add_item(&spam_submenu, "TX Power", spam_configure_power);
    menu_add_item(&spam_submenu, "Interval", spam_configure_interval);
    menu_add_item(&spam_submenu, "Random MACs", spam_toggle_random_macs);
    menu_add_item(&spam_submenu, "Add Custom", spam_add_custom);
    menu_add_item(&spam_submenu, "Toggle List", spam_toggle_list);
    menu_add_item(&spam_submenu, "Back", goto_wifi_menu);
    
 menu_init(&deauth_submenu, "Deauth");
menu_add_item(&deauth_submenu, "Set Level", deauth_select_level);
menu_add_item(&deauth_submenu, "Config", deauth_show_config);
menu_add_item(&deauth_submenu, "Scan", deauth_scan_networks);
menu_add_item(&deauth_submenu, "Select Target", deauth_select_target);
menu_add_item(&deauth_submenu, "Show Targets", deauth_show_targets);
menu_add_item(&deauth_submenu, "Start", deauth_start_attack);
menu_add_item(&deauth_submenu, "Stats", deauth_show_stats);
menu_add_item(&deauth_submenu, "Stop", deauth_stop_attack);
menu_add_item(&deauth_submenu, "Clear", deauth_clear_targets_handler);
menu_add_item(&deauth_submenu, "Back", goto_wifi_menu);
    
    menu_init(&portal_submenu, "Evil Portal");
    menu_add_item(&portal_submenu, "Scan", deauth_scan_networks);
    menu_add_item(&portal_submenu, "Select Net", portal_select_network);
    menu_add_item(&portal_submenu, "Start", portal_start_handler);
    menu_add_item(&portal_submenu, "Stop", portal_stop_handler);
    menu_add_item(&portal_submenu, "Captures", portal_view_captures);
    menu_add_item(&portal_submenu, "Back", goto_wifi_menu);
    
    menu_init(&browser_submenu, "File Browser");
    menu_add_item(&browser_submenu, "Start", browser_start_handler);
    menu_add_item(&browser_submenu, "Stop", browser_stop_handler);
    menu_add_item(&browser_submenu, "Back", goto_wifi_menu);
}




void wifi_thingies_init(void) {
    init_wifi_thingies_submenu(NULL);
evil_twin_menu_init();
arp_poison_menu_init();
dns_spoof_menu_init();
null_ssid_menu_init();
}

void wifi_thingies_open(void) {
    menu_set_active(&wifi_menu);
    menu_draw();
}
