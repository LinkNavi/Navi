// file_browser.h - Web-Based File Browser/Uploader for SPIFFS
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
    
    mkdir("/spiffs/sites", 0755);
    mkdir("/spiffs/captures", 0755);
    
    ESP_LOGI(BROWSER_TAG, "SPIFFS initialized");
    return 1;
}

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
        
        // Truncate filename early to prevent buffer overflow
        char safe_name[32];
        strncpy(safe_name, entry->d_name, sizeof(safe_name) - 1);
        safe_name[sizeof(safe_name) - 1] = '\0';
        
        char path[64];
        snprintf(path, sizeof(path), "/spiffs/%s", safe_name);
        
        struct stat st;
        if (stat(path, &st) != 0) continue;
        
        char buf[128];
        snprintf(buf, sizeof(buf), 
            "%s{\"name\":\"%s\",\"size\":%ld,\"type\":\"%s\"}", 
            first ? "" : ",",
            safe_name, 
            st.st_size,
            S_ISDIR(st.st_mode) ? "dir" : "file");
        
        httpd_resp_sendstr_chunk(req, buf);
        first = 0;
    }
    
    closedir(dir);
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t browser_upload_handler(httpd_req_t *req) {
    char filepath[256] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf && httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[128];
            if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                strncat(filepath, param, sizeof(filepath) - strlen(filepath) - 1);
            }
        }
        if (buf) free(buf);
    }
    
    FILE *f = fopen(filepath, "w");
    if (!f) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    char buf[512];
    int received, total = 0;
    
    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, received, f);
        total += received;
    }
    
    fclose(f);
    ESP_LOGI(BROWSER_TAG, "Uploaded: %s (%d bytes)", filepath, total);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static esp_err_t browser_download_handler(httpd_req_t *req) {
    char filepath[256] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf && httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[128];
            if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                strncat(filepath, param, sizeof(filepath) - strlen(filepath) - 1);
            }
        }
        if (buf) free(buf);
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

static esp_err_t browser_delete_handler(httpd_req_t *req) {
    char filepath[256] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf && httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[128];
            if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                strncat(filepath, param, sizeof(filepath) - strlen(filepath) - 1);
            }
        }
        if (buf) free(buf);
    }
    
    if (remove(filepath) == 0) {
        ESP_LOGI(BROWSER_TAG, "Deleted: %s", filepath);
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }
    
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t browser_ui_handler(httpd_req_t *req) {
    const char *html = 
        "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>File Manager</title><style>"
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0f0f0f;color:#e0e0e0;padding:20px}"
        ".container{max-width:900px;margin:0 auto}"
        "h1{font-size:28px;font-weight:600;margin-bottom:8px;color:#fff}"
        ".subtitle{color:#888;font-size:14px;margin-bottom:24px}"
        ".upload-area{background:#1a1a1a;border:2px dashed #333;border-radius:12px;padding:32px;margin-bottom:24px;text-align:center;transition:all .3s}"
        ".upload-area:hover{border-color:#555;background:#222}"
        ".upload-area.drag-over{border-color:#0066ff;background:#001a33}"
        "input[type=file]{display:none}"
        ".upload-btn{background:#0066ff;color:#fff;padding:12px 24px;border:none;border-radius:8px;cursor:pointer;font-size:14px;font-weight:500;transition:background .2s}"
        ".upload-btn:hover{background:#0052cc}"
        ".path-input{width:100%;padding:10px 16px;background:#1a1a1a;border:1px solid #333;border-radius:8px;color:#e0e0e0;font-size:14px;margin-top:12px}"
        ".files{background:#1a1a1a;border-radius:12px;overflow:hidden}"
        ".file-header{display:grid;grid-template-columns:40px 1fr 100px 140px;padding:12px 16px;background:#222;border-bottom:1px solid #333;font-size:12px;font-weight:600;color:#888;text-transform:uppercase}"
        ".file-item{display:grid;grid-template-columns:40px 1fr 100px 140px;align-items:center;padding:12px 16px;border-bottom:1px solid #222;transition:background .2s}"
        ".file-item:hover{background:#252525}"
        ".file-item:last-child{border-bottom:none}"
        ".file-icon{font-size:20px}"
        ".file-name{color:#e0e0e0;font-size:14px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".file-size{color:#888;font-size:13px}"
        ".file-actions{display:flex;gap:8px}"
        ".btn{padding:6px 12px;border:none;border-radius:6px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s}"
        ".btn-download{background:#1a472a;color:#4ade80}"
        ".btn-download:hover{background:#22543d}"
        ".btn-delete{background:#4a1a1a;color:#f87171}"
        ".btn-delete:hover{background:#5a2828}"
        ".empty{padding:48px;text-align:center;color:#666}"
        ".progress{display:none;background:#0066ff;height:3px;position:fixed;top:0;left:0;right:0;animation:progress 1s ease-in-out infinite}"
        "@keyframes progress{0%{width:0}50%{width:70%}100%{width:100%}}"
        "</style></head><body><div class='container'>"
        "<h1>📁 File Manager</h1><div class='subtitle'>SPIFFS Storage</div>"
        "<div class='upload-area' id='uploadArea'>"
        "<div style='font-size:48px;margin-bottom:12px'>📤</div>"
        "<div style='font-size:16px;font-weight:500;margin-bottom:8px'>Drop files here or click to upload</div>"
        "<div style='color:#888;font-size:13px;margin-bottom:16px'>Max 5MB per file</div>"
        "<input type='file' id='file' multiple>"
        "<button class='upload-btn' onclick='document.getElementById(\"file\").click()'>Choose Files</button>"
        "<input type='text' class='path-input' id='path' placeholder='Path (e.g., sites/portal.html)'>"
        "</div><div class='files'>"
        "<div class='file-header'><div></div><div>Name</div><div>Size</div><div>Actions</div></div>"
        "<div id='fileList'></div></div><div class='progress' id='progress'></div></div>"
        "<script>"
        "const uploadArea=document.getElementById('uploadArea');"
        "const fileInput=document.getElementById('file');"
        "const progress=document.getElementById('progress');"
        "['dragenter','dragover','dragleave','drop'].forEach(e=>{"
        "uploadArea.addEventListener(e,ev=>ev.preventDefault());});"
        "uploadArea.addEventListener('dragenter',()=>uploadArea.classList.add('drag-over'));"
        "uploadArea.addEventListener('dragleave',()=>uploadArea.classList.remove('drag-over'));"
        "uploadArea.addEventListener('drop',e=>{"
        "uploadArea.classList.remove('drag-over');handleFiles(e.dataTransfer.files);});"
        "fileInput.addEventListener('change',()=>handleFiles(fileInput.files));"
        "function handleFiles(files){Array.from(files).forEach(f=>{"
        "const p=document.getElementById('path').value||f.name;"
        "progress.style.display='block';"
        "fetch('/api/upload?path='+encodeURIComponent(p),{method:'POST',body:f})"
        ".then(()=>{progress.style.display='none';load();})"
        ".catch(()=>progress.style.display='none');});}"
        "function load(){fetch('/api/list').then(r=>r.json()).then(d=>{"
        "const list=document.getElementById('fileList');"
        "if(!d.length){list.innerHTML='<div class=\"empty\">No files yet</div>';return;}"
        "list.innerHTML=d.map(f=>"
        "`<div class='file-item'>"
        "<div class='file-icon'>${f.type==='dir'?'📁':'📄'}</div>"
        "<div class='file-name'>${f.name}</div>"
        "<div class='file-size'>${f.size<1024?f.size+'B':(f.size/1024).toFixed(1)+'KB'}</div>"
        "<div class='file-actions'>"
        "<button class='btn btn-download' onclick='download(\"${f.name}\")'>Download</button>"
        "<button class='btn btn-delete' onclick='del(\"${f.name}\")'>Delete</button>"
        "</div></div>`).join('');});}"
        "function download(n){window.open('/api/download?path='+encodeURIComponent(n));}"
        "function del(n){if(confirm('Delete '+n+'?'))fetch('/api/delete?path='+encodeURIComponent(n)).then(load);}"
        "load();</script></body></html>";
    
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static inline uint8_t file_browser_start(void) {
    if (browser_server) return 0;
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

static inline void file_browser_stop(void) {
    if (browser_server) {
        httpd_stop(browser_server);
        browser_server = NULL;
        ESP_LOGI(BROWSER_TAG, "File browser stopped");
    }
}

#endif
