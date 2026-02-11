// file_browser.h - Web-Based File Browser/Uploader
#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_log.h"

static const char *BROWSER_TAG = "FileBrowser";
static httpd_handle_t browser_server = NULL;

// Initialize SPIFFS
static inline uint8_t file_browser_init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(BROWSER_TAG, "SPIFFS mount failed");
        return 0;
    }
    
    // Create directories
    mkdir("/spiffs/sites", 0755);
    mkdir("/spiffs/captures", 0755);
    
    ESP_LOGI(BROWSER_TAG, "SPIFFS initialized");
    return 1;
}

// List files as JSON
static esp_err_t browser_list_handler(httpd_req_t *req) {
    DIR *dir = opendir("/spiffs");
    if (!dir) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");
    
    struct dirent *entry;
    uint8_t first = 1;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char path[128];
        snprintf(path, sizeof(path), "/spiffs/%s", entry->d_name);
        
        struct stat st;
        if (stat(path, &st) == 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s{\"name\":\"%s\",\"size\":%ld,\"type\":\"%s\"}", 
                     first ? "" : ",",
                     entry->d_name, 
                     st.st_size,
                     S_ISDIR(st.st_mode) ? "dir" : "file");
            httpd_resp_sendstr_chunk(req, buf);
            first = 0;
        }
    }
    closedir(dir);
    
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Upload file handler
static esp_err_t browser_upload_handler(httpd_req_t *req) {
    char filepath[128] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[64];
            if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                strncat(filepath, param, sizeof(filepath) - strlen(filepath) - 1);
            }
        }
        free(buf);
    }
    
    FILE *f = fopen(filepath, "w");
    if (!f) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    char buf[512];
    int received;
    int total = 0;
    
    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, received, f);
        total += received;
    }
    
    fclose(f);
    
    ESP_LOGI(BROWSER_TAG, "Uploaded: %s (%d bytes)", filepath, total);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// Download file handler
static esp_err_t browser_download_handler(httpd_req_t *req) {
    char filepath[128] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[64];
            if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                strncat(filepath, param, sizeof(filepath) - strlen(filepath) - 1);
            }
        }
        free(buf);
    }
    
    FILE *f = fopen(filepath, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    char buf[512];
    size_t read;
    while ((read = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, read);
    }
    
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Delete file handler
static esp_err_t browser_delete_handler(httpd_req_t *req) {
    char filepath[128] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[64];
            if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                strncat(filepath, param, sizeof(filepath) - strlen(filepath) - 1);
            }
        }
        free(buf);
    }
    
    if (remove(filepath) == 0) {
        ESP_LOGI(BROWSER_TAG, "Deleted: %s", filepath);
        httpd_resp_sendstr(req, "OK");
    } else {
        httpd_resp_send_500(req);
    }
    
    return ESP_OK;
}

// Main file browser UI
static esp_err_t browser_ui_handler(httpd_req_t *req) {
    const char *html = 
        "<!DOCTYPE html><html><head><title>File Browser</title>"
        "<style>body{font-family:Arial;margin:20px}table{width:100%;border-collapse:collapse}"
        "th,td{padding:8px;text-align:left;border-bottom:1px solid #ddd}"
        "button{padding:5px 10px;margin:2px;cursor:pointer}"
        ".upload{margin:20px 0;padding:15px;background:#f5f5f5;border-radius:5px}</style>"
        "</head><body><h1>File Browser</h1>"
        "<div class='upload'><h3>Upload File</h3>"
        "<input type='file' id='file'><input id='path' placeholder='Path (e.g., sites/portal.html)'>"
        "<button onclick='upload()'>Upload</button></div>"
        "<table id='files'><tr><th>Name</th><th>Size</th><th>Actions</th></tr></table>"
        "<script>"
        "function load(){fetch('/api/list').then(r=>r.json()).then(d=>{"
        "let t='<tr><th>Name</th><th>Size</th><th>Actions</th></tr>';"
        "d.forEach(f=>{"
        "t+=`<tr><td>${f.name}</td><td>${f.size}B</td>`;"
        "t+=`<td><button onclick='download(\"${f.name}\")'>Download</button>`;"
        "t+=`<button onclick='del(\"${f.name}\")'>Delete</button></td></tr>`;"
        "});document.getElementById('files').innerHTML=t;});}"
        "function upload(){"
        "let f=document.getElementById('file').files[0];"
        "let p=document.getElementById('path').value||f.name;"
        "fetch('/api/upload?path='+p,{method:'POST',body:f})"
        ".then(()=>{alert('Uploaded');load();});}"
        "function download(n){window.open('/api/download?path='+n);}"
        "function del(n){if(confirm('Delete '+n+'?'))fetch('/api/delete?path='+n).then(load);}"
        "load();"
        "</script></body></html>";
    
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

// Start file browser server
static inline uint8_t file_browser_start(void) {
    if (browser_server) return 0;
    
    // Init SPIFFS first
    if (!file_browser_init_spiffs()) return 0;
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    if (httpd_start(&browser_server, &config) != ESP_OK) {
        ESP_LOGE(BROWSER_TAG, "Failed to start server");
        return 0;
    }
    
    httpd_uri_t uri_ui = {.uri = "/", .method = HTTP_GET, .handler = browser_ui_handler};
    httpd_uri_t uri_list = {.uri = "/api/list", .method = HTTP_GET, .handler = browser_list_handler};
    httpd_uri_t uri_upload = {.uri = "/api/upload", .method = HTTP_POST, .handler = browser_upload_handler};
    httpd_uri_t uri_download = {.uri = "/api/download", .method = HTTP_GET, .handler = browser_download_handler};
    httpd_uri_t uri_delete = {.uri = "/api/delete", .method = HTTP_GET, .handler = browser_delete_handler};
    
    httpd_register_uri_handler(browser_server, &uri_ui);
    httpd_register_uri_handler(browser_server, &uri_list);
    httpd_register_uri_handler(browser_server, &uri_upload);
    httpd_register_uri_handler(browser_server, &uri_download);
    httpd_register_uri_handler(browser_server, &uri_delete);
    
    ESP_LOGI(BROWSER_TAG, "File browser started on port 8080");
    return 1;
}

// Stop file browser
static inline void file_browser_stop(void) {
    if (browser_server) {
        httpd_stop(browser_server);
        browser_server = NULL;
        ESP_LOGI(BROWSER_TAG, "File browser stopped");
    }
}

#endif
