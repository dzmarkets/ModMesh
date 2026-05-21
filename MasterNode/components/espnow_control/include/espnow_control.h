#ifndef ESPNOW_CONTROL_H
#define ESPNOW_CONTROL_H

#include <stdint.h>
#include <stddef.h>
#include "esp_now.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback type for when a complete Modbus frame is received over ESP-NOW
typedef void (*espnow_recv_cb_t)(const uint8_t* src_mac, const uint8_t* payload, size_t len);

// Callback type for when an ESP-NOW transmission status is received (success or failure)
typedef void (*espnow_send_cb_t)(const uint8_t* dest_mac, bool success);

// Register a callback to receive ESP-NOW transmission status updates
void espnow_control_register_send_cb(espnow_send_cb_t cb);

// Initialize Wi-Fi and ESP-NOW
bool espnow_control_init(espnow_recv_cb_t rx_cb);

// Add an encrypted peer
bool espnow_control_add_peer(const uint8_t* mac_addr);

// Send a payload to a specific MAC. Handles fragmentation if len > 246 bytes.
bool espnow_control_send(const uint8_t* dest_mac, const uint8_t* payload, size_t len);

#ifdef __cplusplus
}
#endif

#endif // ESPNOW_CONTROL_H
