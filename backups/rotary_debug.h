// rotary_debug.h - Rotation debug visualization and logging
#ifndef ROTARY_DEBUG_H
#define ROTARY_DEBUG_H

#include <stdint.h>
#include "drivers/display.h"
#include "drivers/rotary.h"
#include "esp_log.h"

static const char *ROTARY_DEBUG_TAG = "RotaryDebug";

typedef struct {
    int32_t total_cw;       // Total clockwise rotations
    int32_t total_ccw;      // Total counter-clockwise rotations
    int32_t total_presses;  // Total button presses
    int32_t position;       // Current position
    uint32_t last_event_time;
    uint8_t last_clk;
    uint8_t last_dt;
    uint8_t last_sw;
    uint8_t current_clk;
    uint8_t current_dt;
    uint8_t current_sw;
    int8_t last_direction;
    uint32_t event_count;   // Total events
    uint32_t missed_count;  // Potential missed events (rapid changes)
} RotaryDebugStats;

static RotaryDebugStats debug_stats;

// Initialize debug stats
static inline void rotary_debug_init(void) {
    debug_stats.total_cw = 0;
    debug_stats.total_ccw = 0;
    debug_stats.total_presses = 0;
    debug_stats.position = 0;
    debug_stats.last_event_time = 0;
    debug_stats.last_clk = 1;
    debug_stats.last_dt = 1;
    debug_stats.last_sw = 1;
    debug_stats.current_clk = 1;
    debug_stats.current_dt = 1;
    debug_stats.current_sw = 1;
    debug_stats.last_direction = 0;
    debug_stats.event_count = 0;
    debug_stats.missed_count = 0;
    
    ESP_LOGI(ROTARY_DEBUG_TAG, "=== Rotary Debug Initialized ===");
}

// Update debug stats with current encoder state
static inline void rotary_debug_update(Rotary *encoder) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Read raw pin states
    debug_stats.current_clk = gpio_get_level((gpio_num_t)encoder->pin_clk);
    debug_stats.current_dt = gpio_get_level((gpio_num_t)encoder->pin_dt);
    debug_stats.current_sw = gpio_get_level((gpio_num_t)encoder->pin_sw);
    
    // Read encoder direction
    int8_t dir = rotary_read(encoder);
    
    if (dir != 0) {
        debug_stats.event_count++;
        uint32_t delta = now - debug_stats.last_event_time;
        
        if (dir > 0) {
            debug_stats.total_cw++;
            debug_stats.position++;
            debug_stats.last_direction = 1;
            ESP_LOGI(ROTARY_DEBUG_TAG, "CW  | Pos: %4ld | Delta: %3lums | CLK:%d DT:%d | Total: CW=%ld CCW=%ld",
                     debug_stats.position, delta,
                     debug_stats.current_clk, debug_stats.current_dt,
                     debug_stats.total_cw, debug_stats.total_ccw);
        } else if (dir < 0) {
            debug_stats.total_ccw++;
            debug_stats.position--;
            debug_stats.last_direction = -1;
            ESP_LOGI(ROTARY_DEBUG_TAG, "CCW | Pos: %4ld | Delta: %3lums | CLK:%d DT:%d | Total: CW=%ld CCW=%ld",
                     debug_stats.position, delta,
                     debug_stats.current_clk, debug_stats.current_dt,
                     debug_stats.total_cw, debug_stats.total_ccw);
        }
        
        // Detect potential missed events (very rapid changes)
        if (delta < 20 && debug_stats.event_count > 1) {
            debug_stats.missed_count++;
            ESP_LOGW(ROTARY_DEBUG_TAG, "FAST ROTATION detected (%lums) - possible bounce or rapid turn", delta);
        }
        
        debug_stats.last_event_time = now;
    }
    
    // Button state changes
    if (rotary_button_pressed(encoder)) {
        debug_stats.total_presses++;
        ESP_LOGI(ROTARY_DEBUG_TAG, "BTN | Press #%ld | SW:0", debug_stats.total_presses);
    }
    
    // Update last pin states for display
    debug_stats.last_clk = debug_stats.current_clk;
    debug_stats.last_dt = debug_stats.current_dt;
    debug_stats.last_sw = debug_stats.current_sw;
}

// Draw debug screen
static inline void rotary_debug_draw(void) {
    display_clear();
    set_font(FONT_TOMTHUMB);
    
    // Title bar
    fill_rect(0, 0, WIDTH, 10, 1);
    set_cursor(2, 7);
    
    // Draw "Rotary Debug" inverted
    const char *title = "Rotary Debug";
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
                        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                            framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                        }
                    }
                    bit_idx++;
                }
            }
            tx += g->xAdvance;
        }
        t++;
    }
    
    draw_hline(0, 10, WIDTH, 1);
    
    // Position display (large)
    set_cursor(2, 20);
    print("Position:");
    set_cursor(2, 28);
    set_font(FONT_FREEMONO_9PT);
    char pos_str[16];
    snprintf(pos_str, sizeof(pos_str), "%ld", debug_stats.position);
    print(pos_str);
    
    // Direction indicator
    set_font(FONT_TOMTHUMB);
    if (debug_stats.last_direction > 0) {
        fill_rect(WIDTH - 25, 18, 20, 12, 1);
        set_cursor(WIDTH - 22, 26);
        // Inverted "CW"
        const char *cw = "CW";
        int16_t cx = WIDTH - 22;
        while (*cw) {
            if (*cw >= TomThumb.first && *cw <= TomThumb.last) {
                const GFXglyph *g = &TomThumb.glyph[*cw - TomThumb.first];
                const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
                
                uint16_t bit_idx = 0;
                for (uint8_t yy = 0; yy < g->height; yy++) {
                    for (uint8_t xx = 0; xx < g->width; xx++) {
                        if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                            int16_t px = cx + g->xOffset + xx;
                            int16_t py = 26 + g->yOffset + yy;
                            if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                                framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                            }
                        }
                        bit_idx++;
                    }
                }
                cx += g->xAdvance;
            }
            cw++;
        }
    } else if (debug_stats.last_direction < 0) {
        fill_rect(WIDTH - 25, 18, 20, 12, 1);
        set_cursor(WIDTH - 24, 26);
        // Inverted "CCW"
        const char *ccw = "CCW";
        int16_t cx = WIDTH - 24;
        while (*ccw) {
            if (*ccw >= TomThumb.first && *ccw <= TomThumb.last) {
                const GFXglyph *g = &TomThumb.glyph[*ccw - TomThumb.first];
                const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
                
                uint16_t bit_idx = 0;
                for (uint8_t yy = 0; yy < g->height; yy++) {
                    for (uint8_t xx = 0; xx < g->width; xx++) {
                        if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                            int16_t px = cx + g->xOffset + xx;
                            int16_t py = 26 + g->yOffset + yy;
                            if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                                framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                            }
                        }
                        bit_idx++;
                    }
                }
                cx += g->xAdvance;
            }
            ccw++;
        }
    }
    
    draw_hline(0, 38, WIDTH, 1);
    
    // Statistics
    set_font(FONT_TOMTHUMB);
    set_cursor(2, 46);
    char stats[32];
    snprintf(stats, sizeof(stats), "CW:  %ld", debug_stats.total_cw);
    println(stats);
    
    snprintf(stats, sizeof(stats), "CCW: %ld", debug_stats.total_ccw);
    println(stats);
    
    snprintf(stats, sizeof(stats), "BTN: %ld", debug_stats.total_presses);
    println(stats);
    
    snprintf(stats, sizeof(stats), "Events: %ld", debug_stats.event_count);
    println(stats);
    
    draw_hline(0, 78, WIDTH, 1);
    
    // Pin states (real-time)
    set_cursor(2, 86);
    snprintf(stats, sizeof(stats), "CLK:%d DT:%d SW:%d",
             debug_stats.current_clk,
             debug_stats.current_dt,
             debug_stats.current_sw);
    print(stats);
    
    // Timing info
    set_cursor(2, 94);
    uint32_t since_last = xTaskGetTickCount() * portTICK_PERIOD_MS - debug_stats.last_event_time;
    snprintf(stats, sizeof(stats), "Last: %lums ago", since_last);
    print(stats);
    
    // Fast rotations warning
    if (debug_stats.missed_count > 0) {
        set_cursor(2, 102);
        snprintf(stats, sizeof(stats), "Fast: %ld", debug_stats.missed_count);
        print(stats);
    }
    
    // Help
    draw_hline(0, HEIGHT - 18, WIDTH, 1);
    set_cursor(2, HEIGHT - 11);
    print("Turn to test");
    set_cursor(2, HEIGHT - 3);
    print("Hold to exit");
    
    display_show();
}

// Run debug session
static inline void rotary_debug_run(Rotary *encoder) {
    rotary_debug_init();
    
    ESP_LOGI(ROTARY_DEBUG_TAG, "Starting debug session");
    ESP_LOGI(ROTARY_DEBUG_TAG, "Encoder pins: CLK=%d, DT=%d, SW=%d",
             encoder->pin_clk, encoder->pin_dt, encoder->pin_sw);
    
    // Show initial screen
    rotary_debug_draw();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    uint32_t hold_start = 0;
    
    while (1) {
        rotary_debug_update(encoder);
        rotary_debug_draw();
        
        // Hold to exit
        if (gpio_get_level((gpio_num_t)encoder->pin_sw) == 0) {
            if (hold_start == 0) {
                hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            } else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - hold_start) > 1000) {
                ESP_LOGI(ROTARY_DEBUG_TAG, "=== Debug Session Complete ===");
                ESP_LOGI(ROTARY_DEBUG_TAG, "Total CW: %ld", debug_stats.total_cw);
                ESP_LOGI(ROTARY_DEBUG_TAG, "Total CCW: %ld", debug_stats.total_ccw);
                ESP_LOGI(ROTARY_DEBUG_TAG, "Total Presses: %ld", debug_stats.total_presses);
                ESP_LOGI(ROTARY_DEBUG_TAG, "Total Events: %ld", debug_stats.event_count);
                ESP_LOGI(ROTARY_DEBUG_TAG, "Fast Rotations: %ld", debug_stats.missed_count);
                ESP_LOGI(ROTARY_DEBUG_TAG, "Final Position: %ld", debug_stats.position);
                break;
            }
        } else {
            hold_start = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Wait for button release
    while (gpio_get_level((gpio_num_t)encoder->pin_sw) == 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}

#endif
