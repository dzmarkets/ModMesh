idf_component_register(SRCS "espnow_control.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi nvs_flash esp_netif esp_event shared_config)
