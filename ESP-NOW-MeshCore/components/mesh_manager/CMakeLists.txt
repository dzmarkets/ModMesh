#
# File Path: ESP-NOW-MeshCore/components/mesh_manager/CMakeLists.txt
# Brief:     CMake build configuration file for mesh_manager component.
# Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
# Version:   0.1.0
# CreateDate: 2026-04-30
# UpdateDate: 2026-05-02
#
idf_component_register(SRCS "mesh_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash shared_config esp_hw_support esp_timer)
