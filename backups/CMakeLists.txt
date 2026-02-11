idf_component_register(
    SRCS "Main.c"  "wifi_menu.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs driver esp_driver_rmt nvs_flash esp_wifi esp_netif esp_event esp_driver_pcnt  
)
