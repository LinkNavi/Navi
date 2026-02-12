// pin_config_menu.c - Pin configuration menu
#include "include/pin_config.h"
#include "include/menu.h"
#include "include/drivers/display.h"
#include "include/drivers/rotary_pcnt.h"
#include "esp_log.h"

static const char *TAG = "PinConfigMenu";

extern RotaryPCNT encoder;

extern void back_to_main(void);

static Menu pin_config_main_menu;
static Menu pin_config_i2c_menu;
static Menu pin_config_rotary_menu;
static Menu pin_config_ir_menu;
static Menu pin_config_sd_menu;

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Generic pin selector
static uint8_t select_pin(const char *title, uint8_t current, const char *category) {
    uint8_t pin = current;
    
    while (1) {
        display_clear();
        set_font(FONT_TOMTHUMB);
        
        // Title
        fill_rect(0, 0, WIDTH, 10, 1);
        set_cursor(2, 7);
        const char *t = title;
        int16_t tx = 2;
        while (*t) {
            if (*t >= TomThumb.first && *t <= TomThumb.last) {
                const GFXglyph *g = &TomThumb.glyph[*t - TomThumb.first];
                const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
                uint16_t bit_idx = 0;
                for (uint8_t yy = 0; yy < g->height; yy++) {
                    for (uint8_t xx = 0; xx < g->width; xx++) {
                        if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                            int16_t px = tx + g->xOffset + xx;
                            int16_t py = 7 + g->yOffset + yy;
                            if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
                                framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                        }
                        bit_idx++;
                    }
                }
                tx += g->xAdvance;
            }
            t++;
        }
        draw_hline(0, 10, WIDTH, 1);
        
        // Current pin
        set_cursor(2, 30);
        char msg[32];
        snprintf(msg, sizeof(msg), "GPIO: %d", pin);
        println(msg);
        
        // Validation
        set_cursor(2, 40);
        if (!pin_is_valid(pin)) {
            println("Invalid pin!");
        } else if (pin_has_conflict(pin, category)) {
            println("Conflict!");
        } else {
            println("OK");
        }
        
        // Instructions
        set_cursor(2, HEIGHT - 20);
        println("Turn: Select");
        set_cursor(2, HEIGHT - 12);
        println("Press: Confirm");
        set_cursor(2, HEIGHT - 4);
        println("Hold: Cancel");
        
        display_show();
        
        // Input
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir > 0 && pin < 48) pin++;
        if (dir < 0 && pin > 0) pin--;
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            if (pin_is_valid(pin) && !pin_has_conflict(pin, category)) {
                delay(200);
                return pin;
            }
            delay(200);
        }
        
        // Hold to cancel
        static uint32_t hold_start = 0;
        if (gpio_get_level((gpio_num_t)encoder.pin_sw) == 0) {
            if (hold_start == 0) hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - hold_start) > 1000) {
                return current;  // Return original value
            }
        } else {
            hold_start = 0;
        }
        
        delay(50);
    }
}

// Navigation
static void back_to_pin_main(void) {
    menu_set_status("Pin Config");
    menu_set_active(&pin_config_main_menu);
    menu_draw();
}

static void open_i2c_config(void) {
    menu_set_status("I2C");
    menu_set_active(&pin_config_i2c_menu);
    menu_draw();
}

static void open_rotary_config(void) {
    menu_set_status("Rotary");
    menu_set_active(&pin_config_rotary_menu);
    menu_draw();
}

static void open_ir_config(void) {
    menu_set_status("IR");
    menu_set_active(&pin_config_ir_menu);
    menu_draw();
}

static void open_sd_config(void) {
    menu_set_status("SD Card");
    menu_set_active(&pin_config_sd_menu);
    menu_draw();
}

// I2C Config
static void config_i2c_sda(void) {
    PinConfig *cfg = pin_config_get();
    cfg->i2c_sda = select_pin("I2C SDA", cfg->i2c_sda, "i2c");
    open_i2c_config();
}

static void config_i2c_scl(void) {
    PinConfig *cfg = pin_config_get();
    cfg->i2c_scl = select_pin("I2C SCL", cfg->i2c_scl, "i2c");
    open_i2c_config();
}

// Rotary Config
static void config_rotary_clk(void) {
    PinConfig *cfg = pin_config_get();
    cfg->rotary_clk = select_pin("Rotary CLK", cfg->rotary_clk, "rotary");
    open_rotary_config();
}

static void config_rotary_dt(void) {
    PinConfig *cfg = pin_config_get();
    cfg->rotary_dt = select_pin("Rotary DT", cfg->rotary_dt, "rotary");
    open_rotary_config();
}

static void config_rotary_sw(void) {
    PinConfig *cfg = pin_config_get();
    cfg->rotary_sw = select_pin("Rotary SW", cfg->rotary_sw, "rotary");
    open_rotary_config();
}

// IR Config
static void config_ir_pin(void) {
    PinConfig *cfg = pin_config_get();
    cfg->ir_pin = select_pin("IR Blaster", cfg->ir_pin, "ir");
    back_to_pin_main();
}

// SD Config
static void config_sd_mosi(void) {
    PinConfig *cfg = pin_config_get();
    cfg->sd_mosi = select_pin("SD MOSI", cfg->sd_mosi, "sd");
    open_sd_config();
}

static void config_sd_miso(void) {
    PinConfig *cfg = pin_config_get();
    cfg->sd_miso = select_pin("SD MISO", cfg->sd_miso, "sd");
    open_sd_config();
}

static void config_sd_clk(void) {
    PinConfig *cfg = pin_config_get();
    cfg->sd_clk = select_pin("SD CLK", cfg->sd_clk, "sd");
    open_sd_config();
}

static void config_sd_cs(void) {
    PinConfig *cfg = pin_config_get();
    cfg->sd_cs = select_pin("SD CS", cfg->sd_cs, "sd");
    open_sd_config();
}

// Save config
static void save_pin_config(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    
    if (pin_config_save()) {
        println("Saved!");
        println("");
        println("Restart required");
        println("to apply changes");
    } else {
        println("Save failed!");
    }
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_pin_main();
}

// Reset to defaults
static void reset_pin_config(void) {
    display_clear();
    set_cursor(2, 10);
    set_font(FONT_TOMTHUMB);
    println("Reset to defaults?");
    println("");
    println("Turn: Yes/No");
    println("Press: Confirm");
    display_show();
    
    uint8_t confirm = 0;
    while (1) {
        int8_t dir = rotary_pcnt_read(&encoder);
        if (dir != 0) {
            confirm = !confirm;
            display_clear();
            set_cursor(2, 10);
            println("Reset to defaults?");
            println("");
            if (confirm) {
                println("> YES");
                println("  NO");
            } else {
                println("  YES");
                println("> NO");
            }
            display_show();
        }
        
        if (rotary_pcnt_button_pressed(&encoder)) {
            delay(200);
            break;
        }
        delay(10);
    }
    
    if (confirm) {
        pin_config_reset();
        display_clear();
        set_cursor(2, 10);
        println("Reset!");
        println("");
        println("Don't forget");
        println("to save!");
        display_show();
        delay(2000);
    }
    
    back_to_pin_main();
}

// View current config
static void view_pin_config(void) {
    PinConfig *cfg = pin_config_get();
    
    display_clear();
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    char msg[32];
    println("Current Pins:");
    println("");
    
    snprintf(msg, sizeof(msg), "I2C: %d,%d", cfg->i2c_sda, cfg->i2c_scl);
    println(msg);
    
    snprintf(msg, sizeof(msg), "Rot: %d,%d,%d", cfg->rotary_clk, cfg->rotary_dt, cfg->rotary_sw);
    println(msg);
    
    snprintf(msg, sizeof(msg), "IR: %d", cfg->ir_pin);
    println(msg);
    
    snprintf(msg, sizeof(msg), "SD: %d,%d,%d,%d", 
             cfg->sd_mosi, cfg->sd_miso, cfg->sd_clk, cfg->sd_cs);
    println(msg);
    
    println("");
    println("Press to continue");
    display_show();
    
    while (!rotary_pcnt_button_pressed(&encoder)) {
        rotary_pcnt_read(&encoder);
        delay(10);
    }
    delay(200);
    back_to_pin_main();
}

// Menu initialization
void pin_config_menu_init(void) {
    ESP_LOGI(TAG, "Initializing pin config menus");
    
    menu_init(&pin_config_main_menu, "Pin Config");
    menu_add_item_icon(&pin_config_main_menu, "V", "View All", view_pin_config);
    menu_add_item_icon(&pin_config_main_menu, "I", "I2C", open_i2c_config);
    menu_add_item_icon(&pin_config_main_menu, "R", "Rotary", open_rotary_config);
    menu_add_item_icon(&pin_config_main_menu, "X", "IR", open_ir_config);
    menu_add_item_icon(&pin_config_main_menu, "S", "SD Card", open_sd_config);
    menu_add_item_icon(&pin_config_main_menu, "!", "Save", save_pin_config);
    menu_add_item_icon(&pin_config_main_menu, "D", "Defaults", reset_pin_config);
    menu_add_item_icon(&pin_config_main_menu, "<", "Back", back_to_main);
    
    menu_init(&pin_config_i2c_menu, "I2C Pins");
    menu_add_item_icon(&pin_config_i2c_menu, "S", "SDA", config_i2c_sda);
    menu_add_item_icon(&pin_config_i2c_menu, "C", "SCL", config_i2c_scl);
    menu_add_item_icon(&pin_config_i2c_menu, "<", "Back", back_to_pin_main);
    
    menu_init(&pin_config_rotary_menu, "Rotary Pins");
    menu_add_item_icon(&pin_config_rotary_menu, "C", "CLK", config_rotary_clk);
    menu_add_item_icon(&pin_config_rotary_menu, "D", "DT", config_rotary_dt);
    menu_add_item_icon(&pin_config_rotary_menu, "S", "SW", config_rotary_sw);
    menu_add_item_icon(&pin_config_rotary_menu, "<", "Back", back_to_pin_main);
    
    menu_init(&pin_config_ir_menu, "IR Pin");
    menu_add_item_icon(&pin_config_ir_menu, "I", "IR Pin", config_ir_pin);
    menu_add_item_icon(&pin_config_ir_menu, "<", "Back", back_to_pin_main);
    
    menu_init(&pin_config_sd_menu, "SD Pins");
    menu_add_item_icon(&pin_config_sd_menu, "O", "MOSI", config_sd_mosi);
    menu_add_item_icon(&pin_config_sd_menu, "I", "MISO", config_sd_miso);
    menu_add_item_icon(&pin_config_sd_menu, "C", "CLK", config_sd_clk);
    menu_add_item_icon(&pin_config_sd_menu, "S", "CS", config_sd_cs);
    menu_add_item_icon(&pin_config_sd_menu, "<", "Back", back_to_pin_main);
}

void pin_config_menu_open(void) {
    menu_set_status("Pin Config");
    menu_set_active(&pin_config_main_menu);
    menu_draw();
}
