#ifndef MODBUS_BRIDGE_H
#define MODBUS_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Callback type for when a complete Modbus frame is received on UART
typedef void (*modbus_recv_cb_t)(const uint8_t* frame, size_t len);

// Initialize UART (Pinned to Core 1 internally by FreeRTOS task)
bool modbus_bridge_init(modbus_recv_cb_t rx_callback);

// Send a Modbus frame over RS-485
void modbus_bridge_transmit(const uint8_t* frame, size_t len);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_BRIDGE_H
