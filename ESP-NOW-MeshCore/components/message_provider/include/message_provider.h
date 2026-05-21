//
// File Path: ESP-NOW-MeshCore/components/message_provider/include/message_provider.h
// Brief:     Header file for message_provider component.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
//

#ifndef MESSAGE_PROVIDER_H
#define MESSAGE_PROVIDER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the message provider with a device name
 * @param device_name The human-readable name of this node (e.g., "NODE_A")
 */
void message_provider_init(const char *device_name);

/**
 * @brief Manually change the payload message (e.g., "Message Two").
 * @param msg The string to broadcast.
 */
void message_provider_set_message(const char *msg);

/**
 * @brief Get the next message to send via ESP-NOW
 * @param buffer Pointer to the buffer where the message will be written
 * @param max_len Maximum length of the buffer
 * @return The actual length of the message written to the buffer
 */
size_t message_provider_get_next(char *buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_PROVIDER_H
