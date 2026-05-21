idf_component_register(
    SRCS "ModbusUART.cpp" "EspNowSecurityRouter.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi driver nvs_flash esp_netif esp_event esp_driver_uart esp_driver_gpio
)
