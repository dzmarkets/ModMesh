//
// File Path: ESP-NOW-MeshCore/components/message_processor/include/message_processor.h
// Brief:     Header file for message_processor component.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
//

#ifndef MESSAGE_PROCESSOR_H
#define MESSAGE_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "mesh_types.h"  // shared MSG_TYPE_DATA / MSG_TYPE_ACK enum

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the message processor.
 *        Must be called once before processing any packets.
 *        Stores the local MAC so it can be embedded in ACK messages.
 */
void message_processor_init(void);

/**
 * @brief Process an incoming ESP-NOW raw packet.
 *        Handles both MSG_TYPE_DATA and MSG_TYPE_ACK frames.
 *
 *  DATA path:
 *    1. Decrypt the ciphertext.
 *    2. Check the flooding cache (drop duplicates).
 *    3. Register the sender with mesh_manager (device count).
 *    4. Send an ACK back to the original sender.
 *    5. Execute actuators.
 *    6. Rebroadcast the encrypted packet (multi-hop flooding).
 *
 *  ACK path:
 *    1. Log the receipt confirmation.
 *    2. The main loop uses this to confirm delivery.
 *
 * @param src_mac  MAC address of the immediate sender.
 * @param data     Raw packet bytes.
 * @param data_len Packet length.
 */
void message_processor_handle_received(const uint8_t *src_mac,
                                       const uint8_t *data, int data_len);

/**
 * @brief Query whether an ACK has been received since the last
 *        call to message_processor_clear_ack_flag().
 * @return true  if at least one ACK was received.
 */
bool message_processor_ack_received(void);

/**
 * @brief Reset the ACK-received flag (call after the main loop has handled it).
 */
void message_processor_clear_ack_flag(void);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_PROCESSOR_H
