#
# File Path: ESP-NOW-MeshCore/components/status_indicator/CMakeLists.txt
# Brief:     CMake build configuration file for status_indicator component.
# Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
# Version:   0.3.0
# CreateDate: 2026-04-26
# UpdateDate: 2026-05-05
#
idf_component_register(SRCS "status_indicator.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_hw_support shared_config ws2812_driver esp_timer)
