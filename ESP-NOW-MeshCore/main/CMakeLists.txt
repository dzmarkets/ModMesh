#
# File Path: ESP-NOW-MeshCore/main/CMakeLists.txt
# Brief:     CMake build configuration file for main component.
# Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
# Version:   0.3.0
# CreateDate: 2026-04-25
# UpdateDate: 2026-05-05
#
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES espnow_control status_indicator message_provider
                                message_processor mesh_manager sensors
                                shared_config esp_timer device_reset)
