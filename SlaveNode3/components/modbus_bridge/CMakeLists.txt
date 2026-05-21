idf_component_register(SRCS "modbus_bridge.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_uart esp_driver_gpio shared_config)
