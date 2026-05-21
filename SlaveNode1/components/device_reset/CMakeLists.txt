idf_component_register(SRCS "device_reset.c"
                       INCLUDE_DIRS "include"
                       REQUIRES shared_config status_indicator nvs_flash)
