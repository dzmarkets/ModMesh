#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Modbus RTU Shared Configuration
// ---------------------------------------------------------------------------
#define MODBUS_BAUD_RATE        9600
#define MODBUS_UART_PORT        UART_NUM_1

// Example RS-485 pins for MAX3485 (adjust as needed for custom board)
#define MAX485_TXD_GPIO         17
#define MAX485_RXD_GPIO         18
#define MAX485_RE_DE_GPIO       -1

#define MODBUS_MAX_FRAME_SIZE   256

// ---------------------------------------------------------------------------
// ESP-NOW Security Configuration
// ---------------------------------------------------------------------------
#define ESPNOW_WIFI_CHANNEL     1

// Primary Master Key (PMK) - Must be exactly 16 bytes
// Used to encrypt the Local Master Key (LMK) during peer negotiation
static const uint8_t ESPNOW_PMK[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Local Master Key (LMK) - Must be exactly 16 bytes
// Used for AES-128 CCMP encryption of the actual data payloads
static const uint8_t ESPNOW_LMK[16] = {
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
};

// ---------------------------------------------------------------------------
// MAC Addresses
// ---------------------------------------------------------------------------
// MAC Address of Master Node (Gateway connected to PLC)
static const uint8_t MASTER_NODE_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};

// MAC Address of Slave Node 1 (Connected to Slave ID 1)
static const uint8_t SLAVE_NODE_1_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x11};

// MAC Address of Slave Node 2 (Connected to Slave ID 2)
static const uint8_t SLAVE_NODE_2_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x22};

// ---------------------------------------------------------------------------
// Roles
// ---------------------------------------------------------------------------
typedef enum {
    ROLE_MASTER_NODE = 0,
    ROLE_SLAVE_NODE_1 = 1,
    ROLE_SLAVE_NODE_2 = 2
} NodeRole_t;

#endif // SHARED_CONFIG_H
