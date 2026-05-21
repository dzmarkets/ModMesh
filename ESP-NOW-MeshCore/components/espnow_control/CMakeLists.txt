#
# File Path: ESP-NOW-MeshCore/components/espnow_control/CMakeLists.txt
# Brief:     CMake build configuration file for espnow_control component.
# Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
# Version:   0.2.0
# CreateDate: 2026-04-25
# UpdateDate: 2026-04-30
#
idf_component_register(SRCS "espnow_control.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash esp_hw_support esp_netif esp_event shared_config)
