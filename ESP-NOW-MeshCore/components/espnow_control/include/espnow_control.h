//
// File Path: ESP-NOW-MeshCore/components/espnow_control/include/espnow_control.h
// Brief:     Header file for espnow_control component.
//            Manages Wi-Fi and ESP-NOW transport layer.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
//

#ifndef ESPNOW_CONTROL_H
#define ESPNOW_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_now.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

/**
 * @brief Callback invoked when an ESP-NOW packet is received.
 * @param src_mac  6-byte MAC address of the sender.
 * @param data     Raw encrypted payload.
 * @param data_len Payload length in bytes.
 */
typedef void (*espnow_recv_cb_t)(const uint8_t *src_mac, const uint8_t *data, int data_len);

/**
 * @brief Callback invoked after an ESP-NOW send attempt completes.
 * @param dest_mac  6-byte MAC of the destination.
 * @param status    ESP_NOW_SEND_SUCCESS or ESP_NOW_SEND_FAIL.
 */
typedef void (*espnow_send_cb_t)(const uint8_t *dest_mac, esp_now_send_status_t status);

// ---------------------------------------------------------------------------
// Initialisation / Deinitialisation
// ---------------------------------------------------------------------------

/**
 * @brief Initialise Wi-Fi in STA mode and ESP-NOW.
 *        The device starts as a passive receiver (STA mode).
 * @param recv_cb Receive callback (may be NULL).
 * @param send_cb Send-status callback (may be NULL).
 * @return ESP_OK on success.
 */
esp_err_t espnow_control_init(espnow_recv_cb_t recv_cb, espnow_send_cb_t send_cb);

/**
 * @brief Deinitialise ESP-NOW and Wi-Fi.
 * @return ESP_OK on success.
 */
esp_err_t espnow_control_deinit(void);

// ---------------------------------------------------------------------------
// Peer management
// ---------------------------------------------------------------------------

/**
 * @brief Add (or refresh) an ESP-NOW peer entry.
 * @param peer_mac 6-byte MAC address.
 * @return ESP_OK on success.
 */
esp_err_t espnow_control_add_peer(const uint8_t *peer_mac);

/**
 * @brief Return the number of unique peers registered so far.
 */
int espnow_control_get_peer_count(void);

// ---------------------------------------------------------------------------
// Mode switching (STA ↔ STA+AP)
// ---------------------------------------------------------------------------

/**
 * @brief Switch to STA+AP combined mode to enable broadcasting.
 *        Must be called before espnow_control_send().
 */
void espnow_control_set_ap_mode(void);

/**
 * @brief Return to pure STA (receiver) mode.
 *        Lower power, receives messages from other nodes.
 */
void espnow_control_set_sta_mode(void);

/**
 * @brief Query the current Wi-Fi mode.
 * @return true if currently in AP (or STA+AP) mode.
 */
bool espnow_control_is_ap_mode(void);

// ---------------------------------------------------------------------------
// Send
// ---------------------------------------------------------------------------

/**
 * @brief Send data via ESP-NOW to a specific peer or broadcast.
 * @param peer_mac 6-byte destination MAC (use {0xFF,…} for broadcast).
 * @param data     Payload bytes.
 * @param len      Payload length.
 * @return ESP_OK on success.
 */
esp_err_t espnow_control_send(const uint8_t *peer_mac, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // ESPNOW_CONTROL_H
