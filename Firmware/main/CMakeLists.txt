idf_component_register(
    SRCS "Main.c" "ble_handler.c" "ble_menu.c" "wifi_menu.c" "wifi_thingies_menu.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs driver esp_driver_rmt nvs_flash esp_wifi esp_netif esp_event esp_driver_pcnt bt esp_http_server  esp_https_server spiffs
)
