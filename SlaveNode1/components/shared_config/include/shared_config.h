#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

#include <stdint.h>


// ---------------------------------------------------------------------------
// Modbus RTU Shared Configuration
// ---------------------------------------------------------------------------
#define MODBUS_BAUD_RATE        9600
#define MODBUS_UART_PORT        1 // UART_NUM_1

// RS-485 pins for MAX3485
#define MAX485_TXD_GPIO         17
#define MAX485_RXD_GPIO         18
#define MAX485_RE_DE_GPIO       -1

#define MODBUS_MAX_FRAME_SIZE   256

// ---------------------------------------------------------------------------
// LED Configuration
// ---------------------------------------------------------------------------
// 0 = Use built-in WS2812, 1 = Use external discrete RGB LEDs
#define USE_EXTERNAL_RGB_LED    0
// GPIO for WS2812 LED (Default for S3 DevKits is 48)
#define WS2812B_LED_GPIO        48

// ---------------------------------------------------------------------------
// Hardware Reset Configuration
// ---------------------------------------------------------------------------
// Momentary push-button used for soft reboot, diagnostics, and factory reset
#define FACTORY_RESET_GPIO      1

// ---------------------------------------------------------------------------
// ESP-NOW Security Configuration
// ---------------------------------------------------------------------------
#define ESPNOW_WIFI_CHANNEL     1

// Primary Master Key (PMK)
static const uint8_t ESPNOW_PMK[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Local Master Key (LMK)
static const uint8_t ESPNOW_LMK[16] = {
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
};

// ---------------------------------------------------------------------------
// MAC Addresses
// ---------------------------------------------------------------------------
// MAC Address of Master Node (Gateway connected to PLC)
static const uint8_t MASTER_NODE_MAC[6] = {0x94, 0xA9, 0x90, 0x19, 0x6A, 0x1C};

// MAC Address of Slave Node 1 (Connected to Slave ID 1)
static const uint8_t SLAVE_NODE_1_MAC[6] = {0xAC, 0xA7, 0x04, 0xF4, 0x03, 0xEC};

// MAC Address of Slave Node 2 (Connected to Slave ID 2)
static const uint8_t SLAVE_NODE_2_MAC[6] = {0xAC, 0xA7, 0x04, 0xF3, 0xFD, 0x54};

// ---------------------------------------------------------------------------
// Remote Reset Configuration
// ---------------------------------------------------------------------------
#define REMOTE_RESET_LEN 8
static const uint8_t REMOTE_RESET_SIGNATURE[REMOTE_RESET_LEN] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFA, 0xCE, 0x00, 0x01};

#endif // SHARED_CONFIG_H
