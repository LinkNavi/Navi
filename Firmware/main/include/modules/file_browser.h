// file_browser.h - Web-Based File Browser/Uploader (FIXED buffer overflows)
#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H
#include "esp_wifi.h"
#include "esp_netif.h"
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
        if (ret == ESP_ERR_INVALID_STATE) return 1; // already mounted
        ESP_LOGE(BROWSER_TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
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
        
        // Truncate name to fit safely in all buffers
        char safe_name[32];
        strncpy(safe_name, entry->d_name, sizeof(safe_name) - 1);
        safe_name[sizeof(safe_name) - 1] = '\0';
        
        char path[64];
        snprintf(path, sizeof(path), "/spiffs/%s", safe_name);
        
        struct stat st;
        if (stat(path, &st) != 0) continue;
        
        // Use a buffer large enough for the JSON with truncated name
        char buf[160];
        snprintf(buf, sizeof(buf), 
                 "%s{\"name\":\"%.30s\",\"size\":%ld,\"type\":\"%s\"}", 
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
    char filepath[128] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf) {
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
                char param[64];
                if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                    size_t remain = sizeof(filepath) - strlen(filepath) - 1;
                    strncat(filepath, param, remain);
                }
            }
            free(buf);
        }
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

static esp_err_t browser_download_handler(httpd_req_t *req) {
    char filepath[128] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf) {
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
                char param[64];
                if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                    size_t remain = sizeof(filepath) - strlen(filepath) - 1;
                    strncat(filepath, param, remain);
                }
            }
            free(buf);
        }
    }
    
    FILE *f = fopen(filepath, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    const char *ext = strrchr(filepath, '.');
    if (ext) {
        if (strcasecmp(ext, ".html") == 0) httpd_resp_set_type(req, "text/html");
        else if (strcasecmp(ext, ".txt") == 0) httpd_resp_set_type(req, "text/plain");
        else if (strcasecmp(ext, ".json") == 0) httpd_resp_set_type(req, "application/json");
        else httpd_resp_set_type(req, "application/octet-stream");
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
    char filepath[128] = "/spiffs/";
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf) {
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
                char param[64];
                if (httpd_query_key_value(buf, "path", param, sizeof(param)) == ESP_OK) {
                    size_t remain = sizeof(filepath) - strlen(filepath) - 1;
                    strncat(filepath, param, remain);
                }
            }
            free(buf);
        }
    }
    
    if (remove(filepath) == 0) {
        ESP_LOGI(BROWSER_TAG, "Deleted: %s", filepath);
        httpd_resp_sendstr(req, "OK");
    } else {
        httpd_resp_send_500(req);
    }
    return ESP_OK;
}

static esp_err_t browser_info_handler(httpd_req_t *req) {
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"total\":%d,\"used\":%d,\"free\":%d}",
             (int)total, (int)used, (int)(total - used));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t browser_ui_handler(httpd_req_t *req) {
    const char *html = 
        "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Navi Files</title><style>"
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:-apple-system,sans-serif;background:#0f0f0f;color:#e0e0e0;padding:20px}"
        ".c{max-width:900px;margin:0 auto}"
        "h1{font-size:24px;margin-bottom:6px;color:#fff}"
        ".sub{color:#888;font-size:13px;margin-bottom:20px}"
        ".ib{display:flex;gap:16px;margin-bottom:14px;padding:10px;background:#1a1a1a;border-radius:8px;font-size:13px}"
        ".ib span{color:#888}.ib b{color:#e0e0e0}"
        ".up{background:#1a1a1a;border:2px dashed #333;border-radius:10px;padding:24px;margin-bottom:20px;text-align:center;cursor:pointer}"
        ".up:hover{border-color:#555}.up.ov{border-color:#06f;background:#001a33}"
        "input[type=file]{display:none}"
        ".bu{background:#06f;color:#fff;padding:10px 20px;border:none;border-radius:8px;cursor:pointer;font-size:14px}"
        ".pi{width:100%;padding:8px 14px;background:#1a1a1a;border:1px solid #333;border-radius:8px;color:#e0e0e0;font-size:13px;margin-top:10px}"
        ".fl{background:#1a1a1a;border-radius:10px;overflow:hidden}"
        ".fh{display:grid;grid-template-columns:30px 1fr 70px 100px;padding:10px 14px;background:#222;font-size:11px;color:#888;text-transform:uppercase}"
        ".fi{display:grid;grid-template-columns:30px 1fr 70px 100px;align-items:center;padding:10px 14px;border-bottom:1px solid #222}"
        ".fi:hover{background:#252525}"
        ".fn{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:13px}"
        ".fs{color:#888;font-size:12px}"
        ".fa{display:flex;gap:6px}"
        ".b{padding:5px 10px;border:none;border-radius:5px;font-size:11px;cursor:pointer}"
        ".bd{background:#1a472a;color:#4ade80}.bx{background:#4a1a1a;color:#f87171}"
        ".em{padding:40px;text-align:center;color:#666}"
        ".t{position:fixed;bottom:20px;right:20px;padding:10px 18px;border-radius:8px;color:#fff;font-size:13px;opacity:0;transition:opacity .3s}"
        ".tok{background:#1a472a;opacity:1}.ter{background:#4a1a1a;opacity:1}"
        "</style></head><body><div class='c'>"
        "<h1>Navi File Manager</h1><div class='sub'>SPIFFS Internal Storage</div>"
        "<div class='ib' id='info'><span>Loading...</span></div>"
        "<div class='up' id='ua' onclick=\"document.getElementById('f').click()\">"
        "<div style='font-size:32px;margin-bottom:6px'>+</div>"
        "<div style='font-size:14px;margin-bottom:6px'>Drop files or click to upload</div>"
        "<input type='file' id='f' multiple>"
        "<button class='bu' onclick=\"event.stopPropagation();document.getElementById('f').click()\">Choose Files</button>"
        "<input class='pi' id='p' placeholder='Custom path (optional)' onclick='event.stopPropagation()'>"
        "</div><div class='fl'>"
        "<div class='fh'><div></div><div>Name</div><div>Size</div><div></div></div>"
        "<div id='l'></div></div><div class='t' id='t'></div></div>"
        "<script>"
        "const ua=document.getElementById('ua'),fi=document.getElementById('f'),t=document.getElementById('t');"
        "['dragenter','dragover','dragleave','drop'].forEach(e=>ua.addEventListener(e,v=>{v.preventDefault();v.stopPropagation()}));"
        "ua.addEventListener('dragenter',()=>ua.classList.add('ov'));"
        "ua.addEventListener('dragleave',()=>ua.classList.remove('ov'));"
        "ua.addEventListener('drop',e=>{ua.classList.remove('ov');hf(e.dataTransfer.files)});"
        "fi.addEventListener('change',()=>hf(fi.files));"
        "function mg(s,ok){t.textContent=s;t.className='t '+(ok?'tok':'ter');setTimeout(()=>t.className='t',2500)}"
        "function hf(fs){Array.from(fs).forEach(f=>{"
        "const p=document.getElementById('p').value||f.name;"
        "fetch('/api/upload?path='+encodeURIComponent(p),{method:'POST',body:f})"
        ".then(r=>{if(r.ok){mg('Uploaded: '+f.name,1);ld()}else mg('Failed',0)})"
        ".catch(()=>mg('Error',0))})}"
        "function ld(){fetch('/api/list').then(r=>r.json()).then(d=>{"
        "const l=document.getElementById('l');"
        "if(!d.length){l.innerHTML='<div class=\"em\">No files</div>';return}"
        "l.innerHTML=d.map(f=>"
        "`<div class='fi'><div>${f.type==='dir'?'D':'F'}</div>"
        "<div class='fn'>${f.name}</div>"
        "<div class='fs'>${f.size<1024?f.size+'B':(f.size/1024).toFixed(1)+'K'}</div>"
        "<div class='fa'><button class='b bd' onclick='dl(\"${f.name}\")'>Get</button>"
        "<button class='b bx' onclick='de(\"${f.name}\")'>Del</button></div></div>`).join('')}).catch(()=>{});"
        "fetch('/api/info').then(r=>r.json()).then(d=>{"
        "document.getElementById('info').innerHTML="
        "'<span>Total: <b>'+(d.total/1024)+'K</b></span>"
        "<span>Used: <b>'+(d.used/1024)+'K</b></span>"
        "<span>Free: <b>'+(d.free/1024)+'K</b></span>'}).catch(()=>{})}"
        "function dl(n){window.open('/api/download?path='+encodeURIComponent(n))}"
        "function de(n){if(confirm('Delete '+n+'?'))fetch('/api/delete?path='+encodeURIComponent(n)).then(r=>{if(r.ok){mg('Deleted',1);ld()}else mg('Failed',0)})}"
        "ld()</script></body></html>";
    
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static inline uint8_t file_browser_start(void) {
    if (browser_server) return 0;
    if (!file_browser_init_spiffs()) return 0;
    
    // Start AP if not already running
    esp_err_t err;
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { ESP_ERROR_CHECK(err); }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { ESP_ERROR_CHECK(err); }
    
    if (!esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) {
        esp_netif_create_default_wifi_ap();
    }
    
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_cfg);
    
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "Navi-Files",
            .ssid_len = 10,
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        }
    };
   esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    
    ESP_LOGI(BROWSER_TAG, "AP 'Navi-Files' started");
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 8;
    
    if (httpd_start(&browser_server, &config) != ESP_OK) {
        ESP_LOGE(BROWSER_TAG, "Server start failed");
        return 0;
    }
    
    httpd_uri_t uri_ui = {.uri = "/", .method = HTTP_GET, .handler = browser_ui_handler};
    httpd_uri_t uri_list = {.uri = "/api/list", .method = HTTP_GET, .handler = browser_list_handler};
    httpd_uri_t uri_upload = {.uri = "/api/upload", .method = HTTP_POST, .handler = browser_upload_handler};
    httpd_uri_t uri_download = {.uri = "/api/download", .method = HTTP_GET, .handler = browser_download_handler};
    httpd_uri_t uri_delete = {.uri = "/api/delete", .method = HTTP_GET, .handler = browser_delete_handler};
    httpd_uri_t uri_info = {.uri = "/api/info", .method = HTTP_GET, .handler = browser_info_handler};
    
    httpd_register_uri_handler(browser_server, &uri_ui);
    httpd_register_uri_handler(browser_server, &uri_list);
    httpd_register_uri_handler(browser_server, &uri_upload);
    httpd_register_uri_handler(browser_server, &uri_download);
    httpd_register_uri_handler(browser_server, &uri_delete);
    httpd_register_uri_handler(browser_server, &uri_info);
    
    ESP_LOGI(BROWSER_TAG, "File browser on :8080");
    return 1;
}

static inline void file_browser_stop(void) {
    if (browser_server) {
        httpd_stop(browser_server);
        browser_server = NULL;
        ESP_LOGI(BROWSER_TAG, "Stopped");
    }
}

#endif
