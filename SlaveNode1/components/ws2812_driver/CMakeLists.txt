idf_component_register(SRCS "ws2812_driver.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_driver_rmt esp_hw_support)
