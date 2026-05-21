idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES espnow_control modbus_bridge shared_config ws2812_driver status_indicator device_reset)

