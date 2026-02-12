// dns_spoof_menu.c - DNS Spoofing Attack Menu
#include "include/menu.h"
#include "include/modules/dns_spoof.h"
#include "include/drivers/display.h"
#include "include/drivers/rotary_pcnt.h"
#include "include/rotary_text_input.h"
#include "esp_log.h"

static const char *TAG = "DNS_Spoof_Menu";

extern RotaryPCNT encoder;
extern void back_to_main(void);

static Menu dns_spoof_menu;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void back_to_spoof_menu(void) {
    menu_set_status("DNS Spoof");
    menu_set_active(&dns_spoof_menu);
    menu_draw();
}

// Start DNS spoofing - redirect all
static void dns_spoof_start_all(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("DNS SPOOFING");
    println("Mode: Redirect All");
    println("");
    println("All DNS queries will");
    println("be redirected to");
    println("your AP IP");
    println("");
    println("⚠️  Evil Twin must");
    println("   be running first!");
    println("");
    println("Press: Start");
    println("Hold: Cancel");
    display_show();
    
    // Wait for confirmation
    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint8_t confirmed = 0;
    
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start) < 5000) {
        rotary_pcnt_read(&encoder);
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            confirmed = 1;
            break;
        }
        
        delay(10);
    }
    
    if (!confirmed) {
        back_to_spoof_menu();
        return;
    }
    
    delay(200);
    
    // Start attack
    display_clear();
    set_cursor(2, 10);
    println("Starting...");
    display_show();
    
    if (dns_spoof_start(SPOOF_MODE_ALL)) {
        display_clear();
        set_cursor(2, 10);
        println("✅ DNS SPOOF ACTIVE!");
        println("");
        println("All domains redirect");
        println("to your portal");
        println("");
        println("Victims will see");
        println("your Evil Twin for");
        println("ALL websites!");
    } else {
        display_clear();
        set_cursor(2, 10);
        println("❌ Failed!");
        println("");
        println("Make sure Evil Twin");
        println("or AP is running");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Blackhole mode - block all sites
static void dns_spoof_start_blackhole(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("DNS BLACKHOLE");
    println("");
    println("Return 0.0.0.0 for");
    println("all DNS queries");
    println("");
    println("Effect: No websites");
    println("will load for victims");
    println("");
    println("Press to start");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    
    if (dns_spoof_start(SPOOF_MODE_BLACKHOLE)) {
        display_clear();
        set_cursor(2, 10);
        println("🕳️  BLACKHOLE ACTIVE!");
        println("");
        println("All websites blocked");
        println("for connected victims");
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Failed to start");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Selective mode - spoof specific domains
static void dns_spoof_start_selective(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("SELECTIVE SPOOFING");
    println("");
    
    if (dns_spoof.target_count == 0) {
        println("No targets set!");
        println("");
        println("Add targets first");
        println("from the menu");
        println("");
        println("Press to continue");
        display_show();
        
        while (!rotary_pcnt_button_pressed(&encoder)) {
            rotary_pcnt_read(&encoder);
            delay(10);
        }
        delay(200);
        back_to_spoof_menu();
        return;
    }
    
    println("Targets:");
    for (uint8_t i = 0; i < dns_spoof.target_count && i < 4; i++) {
        char buf[20];
        snprintf(buf, sizeof(buf), "- %.16s", dns_spoof.target_domains[i]);
        println(buf);
    }
    
    if (dns_spoof.target_count > 4) {
        char buf[16];
        snprintf(buf, sizeof(buf), "...+%d more", dns_spoof.target_count - 4);
        println(buf);
    }
    
    println("");
    println("Press to start");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    
    if (dns_spoof_start(SPOOF_MODE_SELECTIVE)) {
        display_clear();
        set_cursor(2, 10);
        println("🎯 SELECTIVE ACTIVE!");
        println("");
        
        char buf[32];
        snprintf(buf, sizeof(buf), "Spoofing %d domains", dns_spoof.target_count);
        println(buf);
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Failed to start");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Chaos mode - random IPs
static void dns_spoof_start_chaos(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("CHAOS MODE");
    println("");
    println("Return random IPs");
    println("for all DNS queries");
    println("");
    println("Effect: Complete");
    println("network confusion");
    println("");
    println("⚠️  FOR FUN ONLY!");
    println("");
    println("Press to start");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    
    if (dns_spoof_start(SPOOF_MODE_RANDOM)) {
        display_clear();
        set_cursor(2, 10);
        println("🎲 CHAOS UNLEASHED!");
        println("");
        println("Every DNS query");
        println("returns random IP");
    } else {
        display_clear();
        set_cursor(2, 10);
        println("Failed to start");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Stop spoofing
static void dns_spoof_stop_attack(void) {
    dns_spoof_stop();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("DNS Spoof Stopped");
    println("");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Total spoofs: %lu", dns_spoof_get_count());
    println(buf);
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Show statistics
static void dns_spoof_show_stats(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("DNS Spoof Stats");
    println("");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Status: %s", 
             dns_spoof_is_running() ? "RUNNING" : "STOPPED");
    println(buf);
    
    if (dns_spoof_is_running()) {
        snprintf(buf, sizeof(buf), "Mode: %s", 
                 dns_spoof_get_mode_name(dns_spoof_get_mode()));
        println(buf);
    }
    
    snprintf(buf, sizeof(buf), "Spoofs: %lu", dns_spoof_get_count());
    println(buf);
    
    println("");
    println("Live monitoring:");
    println("Check serial output");
    println("to see queries as");
    println("they are spoofed");
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Add target domain
static void dns_spoof_add_domain(void) {
    char domain[64];
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("ADD TARGET DOMAIN");
    println("");
    println("Enter domain to");
    println("spoof in selective");
    println("mode");
    println("");
    println("Examples:");
    println("  google.com");
    println("  facebook.com");
    println("  bank.com");
    println("");
    println("Press to start");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(300);
    
    if (text_input_get(&encoder, "Domain", domain, sizeof(domain), "")) {
        if (strlen(domain) > 0) {
            if (dns_spoof_add_target(domain)) {
                display_clear();
                set_cursor(2, 10);
                println("Target Added!");
                println("");
                println(domain);
            } else {
                display_clear();
                set_cursor(2, 10);
                println("Failed - List Full");
            }
            
            println("");
            println("Press to continue");
            display_show();
            
            while (!rotary_pcnt_button_pressed(&encoder)) {
                rotary_pcnt_read(&encoder);
                delay(10);
            }
            delay(200);
        }
    }
    
    back_to_spoof_menu();
}

// View target list
static void dns_spoof_view_targets(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("Target Domains");
    println("");
    
    if (dns_spoof.target_count == 0) {
        println("No targets set");
        println("");
        println("Add domains from");
        println("the menu");
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Count: %d", dns_spoof.target_count);
        println(buf);
        println("");
        
        for (uint8_t i = 0; i < dns_spoof.target_count && i < 8; i++) {
            char line[24];
            snprintf(line, sizeof(line), "%.20s", dns_spoof.target_domains[i]);
            println(line);
        }
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Clear targets
static void dns_spoof_clear_domains(void) {
    dns_spoof_clear_targets();
    
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    println("Targets Cleared");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Info screen
static void dns_spoof_show_info(void) {
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    println("DNS SPOOFING");
    println("");
    println("Intercepts DNS");
    println("queries and returns");
    println("fake IP addresses");
    println("");
    println("Modes:");
    println("- Redirect: Portal");
    println("- Blackhole: Block");
    println("- Selective: Target");
    println("- Chaos: Random");
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_spoof_menu();
}

// Initialize menu
void dns_spoof_menu_init(void) {
    ESP_LOGI(TAG, "Initializing DNS Spoof menu");
    
    menu_init(&dns_spoof_menu, "DNS Spoof");
    menu_add_item_icon(&dns_spoof_menu, "I", "Info", dns_spoof_show_info);
    menu_add_item_icon(&dns_spoof_menu, "A", "Redirect All", dns_spoof_start_all);
    menu_add_item_icon(&dns_spoof_menu, "B", "Blackhole", dns_spoof_start_blackhole);
    menu_add_item_icon(&dns_spoof_menu, "S", "Selective", dns_spoof_start_selective);
    menu_add_item_icon(&dns_spoof_menu, "C", "Chaos Mode", dns_spoof_start_chaos);
    menu_add_item_icon(&dns_spoof_menu, "X", "Stop", dns_spoof_stop_attack);
    menu_add_item_icon(&dns_spoof_menu, "?", "Statistics", dns_spoof_show_stats);
    menu_add_item_icon(&dns_spoof_menu, "+", "Add Domain", dns_spoof_add_domain);
    menu_add_item_icon(&dns_spoof_menu, "V", "View Targets", dns_spoof_view_targets);
    menu_add_item_icon(&dns_spoof_menu, "D", "Clear Targets", dns_spoof_clear_domains);
    menu_add_item_icon(&dns_spoof_menu, "<", "Back", back_to_main);
}

void dns_spoof_menu_open(void) {
    menu_set_status("DNS Spoof");
    menu_set_active(&dns_spoof_menu);
    menu_draw();
}
