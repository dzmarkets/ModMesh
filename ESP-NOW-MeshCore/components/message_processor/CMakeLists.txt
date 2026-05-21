#
# File Path: ESP-NOW-MeshCore/components/message_processor/CMakeLists.txt
# Brief:     CMake build configuration file for message_processor component.
# Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
# Version:   0.2.0
# CreateDate: 2026-04-26
# UpdateDate: 2026-04-30
#
idf_component_register(SRCS "message_processor.c"
                    INCLUDE_DIRS "include"
                    REQUIRES espnow_control security_manager actuators mesh_manager shared_config)
