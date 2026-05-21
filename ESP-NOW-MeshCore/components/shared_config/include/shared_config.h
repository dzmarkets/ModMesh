//
// File Path: components/shared_config/include/shared_config.h
// Brief:     Centralized configuration file for easily changeable settings.
//            All GPIO pins and system-wide constants are defined here.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.4.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-07
//

#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

// =============================================================================
// --- Node Identification ---
// =============================================================================
// Unique label for this node (each device in the mesh MUST have a unique label for diagnostics)
#define NODE_LABEL "NODE_C" // NODE_A, NODE_B, NODE_C

// =============================================================================
// --- Topic / Keyword Routing (Pub/Sub) ---
// =============================================================================
// Keywords that THIS actuator listens for. If a sensor tags its data with any 
// of these keywords (or "ALL"), this actuator will process it.
// Use a comma-separated list for multiple keywords (e.g., "LIGHT,HVAC").
#define ACTUATOR_KEYWORDS "LIGHT_B" // "LIGHT_A", "LIGHT_C", "LIGHT_B"

// Keywords that THIS sensor attaches to its outgoing data.
// Use a comma-separated list (e.g., "LIGHT,HVAC") or "ALL" for broadcast.
#define DATA_KEYWORDS "LIGHT_C" // "LIGHT_A", "LIGHT_B", "LIGHT_C"

// =============================================================================
// --- Device Role ---
// =============================================================================
// Define the role of this specific device to optimize its task allocation:
#define ROLE_SENSOR   1  // Reads data, does not process incoming actuator commands
#define ROLE_ACTUATOR 2  // Processes commands, does not read sensor data
#define ROLE_BOTH     3  // Default, does both

// Set the active role for this device
#define DEVICE_ROLE ROLE_BOTH

// =============================================================================
// --- Network Security ---
// =============================================================================
// 32-character Hexadecimal API Key (similar to standard web APIs)
#define NETWORK_API_KEY "A7F9E2B4C6D8105F39A0B2C4D6E8F102"

// =============================================================================
// --- LED Configuration ---
// =============================================================================
// Set to 1 to use external 3-pin RGB LED, 0 to use internal WS2812B addressable LED
#define USE_EXTERNAL_RGB_LED 0

// Internal WS2812B LED configuration (if USE_EXTERNAL_RGB_LED == 0)
#define WS2812B_LED_GPIO    48

// =============================================================================
// --- RGB LED GPIO Pins ---
// =============================================================================
// Common-cathode RGB LED (or WS2812 addressable LED, see below).
// If using a 3-pin common-cathode RGB LED, connect each anode through a
// current-limiting resistor (~220Ω) to the corresponding GPIO pin.
// GPIO levels: HIGH = LED ON, LOW = LED OFF (active-high).
#define RGB_LED_RED_GPIO    10    // GPIO for the RED   channel
#define RGB_LED_GREEN_GPIO  11    // GPIO for the GREEN channel
#define RGB_LED_BLUE_GPIO   12    // GPIO for the BLUE  channel

// =============================================================================
// --- Mesh Network Parameters ---
// =============================================================================
// Minimum number of devices required for mesh operation
#define MESH_MIN_DEVICES    2

// Maximum number of devices allowed in the mesh network.
// Increased to 25 to support larger scale deployments.
#define MESH_MAX_DEVICES    25

// NVS namespace and key used to persist the device count across reboots
#define MESH_NVS_NAMESPACE  "mesh_cfg"
#define MESH_NVS_KEY_COUNT  "dev_count"

// WiFi channel used by all devices (must be identical on every node)
#define ESPNOW_WIFI_CHANNEL 1

// Timeout in ms to wait for ACK receipts after broadcasting a message
// Reduced for real-time responsiveness (3000 -> 300)
#define ACK_TIMEOUT_MS      300

// --- Active Peer Monitoring ---
// Time in ms before a peer is considered "OFFLINE" if no message is received
// Reduced to 6s for ultra-fast response times
#define MESH_PEER_TIMEOUT_MS      6000

// Interval in ms to force a "Check-in" transmission even if sensor data is unchanged
// Reduced to 1s for high-speed connectivity awareness
#define MESH_KEEPALIVE_INTERVAL_MS 1000

// =============================================================================
// --- Button / Input Configuration ---
// =============================================================================
// Set to 1 to use internal ESP32 pull-up resistors, 0 to use external resistors.
// Note: External resistors (10kΩ) are recommended for maximum industrial stability.
#define USE_INTERNAL_PULLUPS 1

// =============================================================================
// --- Application Samples Configuration ---
// =============================================================================
// Choose the active application sample:
// 1: Simulation (Temp/Hum mock, Fan mock)
// 2: Remote Powering LED (Momentary Button, LED actuator)
// 3: Remote Powering LED (Toggle Button, LED actuator)
// 4: 4-Node Remote Control (4 Toggle Buttons, 4 LED actuators)
// 5: MOD-BUS MAX485 (Send received data via Modbus)
#define ACTIVE_APP_SAMPLE 3

// Sample 2 & 3: Remote Powering LED GPIOs
#define BUTTON_SENSOR_GPIO  2
#define REMOTE_LED_GPIO     6

// Sample 4: 4-Node Remote Control GPIOs
#define BTN1_GPIO  2
#define BTN2_GPIO  3
#define BTN3_GPIO  4
#define BTN4_GPIO  5

#define LED1_GPIO  6
#define LED2_GPIO  7
#define LED3_GPIO  8
#define LED4_GPIO  9

// Sample 5: MOD-BUS (MAX485) GPIOs
#define MAX485_RE_DE_GPIO   16 // Combined control pin (Logic High = Transmit, Low = Receive)
#define MAX485_TXD_GPIO     17 // Driver Input (Connect to ESP32 TX)
#define MAX485_RXD_GPIO     18 // Receiver Output (Connect to ESP32 RX)
#define MAX485_UART_PORT    1  // UART Port for MAX485 (UART1 on ESP32-S3)
#define MAX485_BAUD_RATE    9600 // Communication speed

// --- Factory Reset Configuration ---
#define FACTORY_RESET_GPIO  1  // Moved from 0 to 1 to avoid strapping pin conflicts

// =============================================================================
// --- Future Configs ---
// =============================================================================
// You can add any future system-wide parameters here.

#endif // SHARED_CONFIG_H
