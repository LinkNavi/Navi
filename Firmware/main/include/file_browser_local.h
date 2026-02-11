// file_browser_local.h - On-device file browser with display
#ifndef FILE_BROWSER_LOCAL_H
#define FILE_BROWSER_LOCAL_H

#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "drivers/display.h"

#define MAX_FILES 32
#define MAX_FILENAME 64
#define MAX_FILE_CONTENT 8192

typedef struct {
    char name[MAX_FILENAME];
    uint8_t is_dir;
    uint32_t size;
} FileEntry;

typedef struct {
    FileEntry files[MAX_FILES];
    uint8_t count;
    uint8_t selected;
    uint8_t scroll_offset;
    char current_path[256];
} FileBrowser;

static FileBrowser browser;

static char text_content[MAX_FILE_CONTENT];
static uint16_t text_lines = 0;
static uint16_t text_scroll = 0;
static uint8_t text_viewer_active = 0;

static inline void file_browser_init(const char *path) {
    strncpy(browser.current_path, path, sizeof(browser.current_path) - 1);
    browser.current_path[sizeof(browser.current_path) - 1] = '\0';
    browser.count = 0;
    browser.selected = 0;
    browser.scroll_offset = 0;
}

static inline uint8_t file_browser_scan(void) {
    browser.count = 0;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "/spiffs%s", browser.current_path);
    
    DIR *dir = opendir(full_path);
    if (!dir) return 0;
    
    if (strcmp(browser.current_path, "/") != 0) {
        strcpy(browser.files[0].name, "..");
        browser.files[0].is_dir = 1;
        browser.files[0].size = 0;
        browser.count++;
    }
    
    struct dirent *entry;
    struct stat st;
    
    while ((entry = readdir(dir)) != NULL && browser.count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char entry_path[768];
        int written = snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path, entry->d_name);
        if (written < 0 || written >= sizeof(entry_path)) continue;
        
        if (stat(entry_path, &st) == 0) {
            strncpy(browser.files[browser.count].name, entry->d_name, MAX_FILENAME - 1);
            browser.files[browser.count].name[MAX_FILENAME - 1] = '\0';
            browser.files[browser.count].is_dir = S_ISDIR(st.st_mode);
            browser.files[browser.count].size = st.st_size;
            browser.count++;
        }
    }
    
    closedir(dir);
    return browser.count;
}

static inline void file_browser_enter(uint8_t index) {
    if (index >= browser.count) return;
    
    FileEntry *entry = &browser.files[index];
    if (!entry->is_dir) return;
    
    if (strcmp(entry->name, "..") == 0) {
        char *last_slash = strrchr(browser.current_path, '/');
        if (last_slash && last_slash != browser.current_path) {
            *last_slash = '\0';
        } else {
            strcpy(browser.current_path, "/");
        }
    } else {
        size_t len = strlen(browser.current_path);
        if (browser.current_path[len - 1] != '/') {
            strncat(browser.current_path, "/", sizeof(browser.current_path) - len - 1);
        }
        strncat(browser.current_path, entry->name, sizeof(browser.current_path) - strlen(browser.current_path) - 1);
    }
    
    browser.selected = 0;
    browser.scroll_offset = 0;
    file_browser_scan();
}

static inline uint8_t file_browser_read_text(uint8_t index) {
    if (index >= browser.count) return 0;
    
    FileEntry *entry = &browser.files[index];
    if (entry->is_dir) return 0;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "/spiffs%s/%s", browser.current_path, entry->name);
    
    FILE *f = fopen(full_path, "r");
    if (!f) return 0;
    
    size_t read = fread(text_content, 1, sizeof(text_content) - 1, f);
    fclose(f);
    
    text_content[read] = '\0';
    
    text_lines = 0;
    for (size_t i = 0; i < read; i++) {
        if (text_content[i] == '\n') text_lines++;
    }
    if (read > 0 && text_content[read - 1] != '\n') text_lines++;
    
    text_scroll = 0;
    text_viewer_active = 1;
    
    return 1;
}

static inline void file_browser_draw(void) {
    display_clear();
    
    fill_rect(0, 0, WIDTH, 12, 1);
    set_cursor(2, 8);
    set_font(FONT_TOMTHUMB);
    
    const char *path_display = browser.current_path;
    if (strlen(path_display) > 20) {
        path_display = browser.current_path + strlen(browser.current_path) - 20;
    }
    
    const char *s = path_display;
    int16_t tx = 2;
    while (*s) {
        if (*s >= TomThumb.first && *s <= TomThumb.last) {
            const GFXglyph *g = &TomThumb.glyph[*s - TomThumb.first];
            const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
            
            uint16_t bit_idx = 0;
            for (uint8_t yy = 0; yy < g->height; yy++) {
                for (uint8_t xx = 0; xx < g->width; xx++) {
                    if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                        int16_t px = tx + g->xOffset + xx;
                        int16_t py = 8 + g->yOffset + yy;
                        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                            framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                        }
                    }
                    bit_idx++;
                }
            }
            tx += g->xAdvance;
        }
        s++;
    }
    
    draw_hline(0, 12, WIDTH, 1);
    
    uint8_t visible = (HEIGHT - 24) / 10;
    uint8_t y = 14;
    
    for (uint8_t i = browser.scroll_offset; i < browser.count && i < browser.scroll_offset + visible; i++) {
        FileEntry *entry = &browser.files[i];
        
        if (i == browser.selected) {
            fill_rect(2, y, WIDTH - 4, 10, 1);
        }
        
        set_cursor(4, y + 7);
        print(entry->is_dir ? (i == browser.selected ? ">" : "D") : (i == browser.selected ? ">" : "F"));
        
        char display_name[22];
        strncpy(display_name, entry->name, 21);
        display_name[21] = '\0';
        
        set_cursor(cursor_x + 2, y + 7);
        
        if (i == browser.selected) {
            const char *n = display_name;
            while (*n) {
                if (*n >= TomThumb.first && *n <= TomThumb.last) {
                    const GFXglyph *g = &TomThumb.glyph[*n - TomThumb.first];
                    const uint8_t *bitmap = TomThumb.bitmap + g->bitmapOffset;
                    
                    uint16_t bit_idx = 0;
                    for (uint8_t yy = 0; yy < g->height; yy++) {
                        for (uint8_t xx = 0; xx < g->width; xx++) {
                            if (bitmap[bit_idx >> 3] & (0x80 >> (bit_idx & 7))) {
                                int16_t px = cursor_x + g->xOffset + xx;
                                int16_t py = y + 7 + g->yOffset + yy;
                                if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
                                    framebuffer[px + (py/8)*WIDTH] &= ~(1 << (py&7));
                                }
                            }
                            bit_idx++;
                        }
                    }
                    cursor_x += g->xAdvance;
                }
                n++;
            }
        } else {
            print(display_name);
        }
        
        y += 10;
    }
    
    draw_hline(0, HEIGHT - 10, WIDTH, 1);
    set_cursor(2, HEIGHT - 3);
    char status[32];
    snprintf(status, sizeof(status), "%d/%d", browser.selected + 1, browser.count);
    print(status);
    
    display_show();
}

static inline void file_browser_draw_text(void) {
    display_clear();
    
    fill_rect(0, 0, WIDTH, 10, 1);
    set_cursor(2, 7);
    set_font(FONT_TOMTHUMB);
    
    const char *title = "Text Viewer";
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
    
    uint8_t visible_lines = (HEIGHT - 20) / 6;
    uint16_t y = 12;
    uint16_t line_num = 0;
    char *line_start = text_content;
    
    for (uint16_t i = 0; i < text_scroll && line_start; i++) {
        line_start = strchr(line_start, '\n');
        if (line_start) line_start++;
    }
    
    while (line_start && line_num < visible_lines) {
        char *line_end = strchr(line_start, '\n');
        size_t line_len = line_end ? (line_end - line_start) : strlen(line_start);
        
        if (line_len > 25) line_len = 25;
        
        char line_buf[26];
        strncpy(line_buf, line_start, line_len);
        line_buf[line_len] = '\0';
        
        set_cursor(2, y);
        print(line_buf);
        
        y += 6;
        line_num++;
        
        if (line_end) {
            line_start = line_end + 1;
        } else {
            break;
        }
    }
    
    draw_hline(0, HEIGHT - 8, WIDTH, 1);
    set_cursor(2, HEIGHT - 2);
    char status[32];
    snprintf(status, sizeof(status), "Line %d/%d", text_scroll + 1, text_lines);
    print(status);
    
    if (text_lines > visible_lines) {
        uint8_t bar_height = (HEIGHT - 20) * visible_lines / text_lines;
        uint8_t bar_pos = (HEIGHT - 20) * text_scroll / text_lines;
        fill_rect(WIDTH - 3, 11 + bar_pos, 2, bar_height, 1);
    }
    
    display_show();
}

static inline void file_browser_next(void) {
    if (browser.selected < browser.count - 1) {
        browser.selected++;
        uint8_t visible = (HEIGHT - 24) / 10;
        if (browser.selected >= browser.scroll_offset + visible) {
            browser.scroll_offset++;
        }
    }
}

static inline void file_browser_prev(void) {
    if (browser.selected > 0) {
        browser.selected--;
        if (browser.selected < browser.scroll_offset) {
            browser.scroll_offset--;
        }
    }
}

static inline void text_viewer_scroll_down(void) {
    uint8_t visible_lines = (HEIGHT - 20) / 6;
    if (text_scroll + visible_lines < text_lines) {
        text_scroll++;
    }
}

static inline void text_viewer_scroll_up(void) {
    if (text_scroll > 0) {
        text_scroll--;
    }
}

#endif
