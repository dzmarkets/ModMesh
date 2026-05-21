//
// File Path: ESP-NOW-MeshCore/components/shared_config/include/mesh_types.h
// Brief:     Shared wire-protocol type definitions used across multiple components.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-30
// UpdateDate: 2026-05-05
//

#ifndef MESH_TYPES_H
#define MESH_TYPES_H

/**
 * @brief Byte-0 tag embedded in every ESP-NOW packet.
 *        All devices must agree on this encoding.
 */
typedef enum {
    MSG_TYPE_DATA = 0x01,   /**< Sensor / forwarded data payload (encrypted) */
    MSG_TYPE_ACK  = 0x02,   /**< Receipt acknowledgement (plain MAC)          */
} msg_type_t;

#endif // MESH_TYPES_H
