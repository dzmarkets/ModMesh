// =============================================================================
// File Path: ESP-NOW-MeshCore/main/main.c
// Brief:     Main application entry point for the ESP-NOW flooding mesh node.
//            This version is optimized for Real-Time performance using an
//            RTOS Multitasking architecture to separate high-speed sensing
//            from background network management.
// =============================================================================
//
//  Role model
//  ----------
//  Every device operates in STA (Station) mode permanently. This ensures
//  maximum stability and avoids radio dropouts that occur when switching
//  between AP and STA modes frequently.
//
//  RTOS Architecture
//  -----------------
//  1. sensor_task (Priority 10): 
//     Handles high-speed sensor polling (every 50ms). This task is time-critical
//     to ensure no user interaction (like a button press) is missed.
//
//  2. mesh_task   (Priority 5):  
//     Handles background mesh management, processing timeouts, updating LED
//     status, and managing the periodic "heartbeat" or "keepalive" transmissions.
//
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.6.1 (Status Indicator Edition)
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
// =============================================================================

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// Project-specific components
#include "shared_config.h"    // Centralized constants and GPIO mappings
#include "espnow_control.h"   // Low-level ESP-NOW radio management
#include "status_indicator.h" // RGB LED status feedback
#include "message_provider.h" // Logic for creating outgoing packets (signing/seq)
#include "message_processor.h"// Logic for handling incoming packets (parsing/actuators)
#include "mesh_manager.h"     // Tracks peer status and device counts
#include "sensors.h"          // Hardware-specific sensor reading logic
#include "device_reset.h"     // Modular Device Reset logic
#include "actuators.h"        // Actuator initialization and execution

static const char *TAG = "main";

// The ESP-NOW broadcast address (all FF's). 
// Sending to this MAC ensures all devices on the same channel hear the packet.
static const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- Shared State (Global variables for inter-task communication) ---
static char g_last_sensor_data[64] = {0}; // Stores the last known sensor string to detect changes
static bool g_transmission_pending = false; // Flag set by sensor_task to notify mesh_task to send

// ---------------------------------------------------------------------------
// ESP-NOW Callbacks
// These are triggered by the ESP-IDF WiFi driver when radio events occur.
// ---------------------------------------------------------------------------

/**
 * @brief Callback triggered whenever a packet is received via ESP-NOW.
 */
static void on_espnow_recv(const uint8_t *src_mac, const uint8_t *data, int data_len)
{
    // Pass the raw binary data to the message processor for decryption and parsing.
    message_processor_handle_received(src_mac, data, data_len);
}

/**
 * @brief Callback triggered after a packet is transmitted to indicate success/fail.
 */
static void on_espnow_send(const uint8_t *dest_mac, esp_now_send_status_t status)
{
    // Log the transmission result for debugging.
    ESP_LOGI(TAG, "Send to " MACSTR " → %s", MAC2STR(dest_mac),
             status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ---------------------------------------------------------------------------
// Helper: broadcast_data
// Sends a packet and waits for any mesh node to acknowledge it.
// ---------------------------------------------------------------------------
static bool broadcast_data(const uint8_t *payload, size_t len, bool is_sensor_data)
{
    bool success = false;
    
    // Visually indicate transmission on the status LED ONLY for sensor data
    if (is_sensor_data) {
        status_indicator_set_state(LED_STATE_SENDING);
    }

    // Briefly switch to AP+STA or broadcast-ready mode if needed by the driver
    espnow_control_set_ap_mode();
    
    // Reset the ACK flag before sending
    message_processor_clear_ack_flag();

    // Trigger the actual hardware transmission
    esp_err_t err = espnow_control_send(broadcast_mac, payload, len);
    
    if (err == ESP_OK) {
        // Wait for a confirmation (ACK) from any other node in the mesh.
        // We use a timeout to prevent blocking the mesh task forever.
        int64_t deadline = esp_timer_get_time() + (int64_t)ACK_TIMEOUT_MS * 1000;
        while (esp_timer_get_time() < deadline) {
            if (message_processor_ack_received()) {
                message_processor_clear_ack_flag();
                success = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20)); // Yield to other tasks while waiting
        }
    }

    if (success) {
        ESP_LOGI(TAG, "ACK received – delivery confirmed by mesh neighbor");
    } else {
        ESP_LOGW(TAG, "No ACK received – node may be isolated or mesh is empty");
    }

    // Restore radio to pure Station mode for reception stability
    espnow_control_set_sta_mode();
    return success;
}

// ---------------------------------------------------------------------------
// Helper: main_broadcast_emergency_reset
// Sends a network-wide command to force all peers to reset their actuators
// and internal states back to default. Used during a factory reset.
// ---------------------------------------------------------------------------
void main_broadcast_emergency_reset(void)
{
    ESP_LOGW(TAG, "Broadcasting [ALL]CMD:NETWORK_RESET to the mesh...");
    message_provider_set_message("[ALL]CMD:NETWORK_RESET");
    uint8_t payload[256];
    size_t len = message_provider_get_next((char *)payload, sizeof(payload));
    if (len > 0) {
        broadcast_data(payload, len, true);
    }
}

// ---------------------------------------------------------------------------
// TASK: sensor_task
// Runs at Priority 10 (Highest). Responsible for reading hardware inputs.
// ---------------------------------------------------------------------------
#if DEVICE_ROLE == ROLE_SENSOR || DEVICE_ROLE == ROLE_BOTH
static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started (High Priority - 50ms poll)");
    char current_data[64] = {0};

    while (1) {
        // Read the current state of sensors (or buttons) from the sensors component
        sensors_read(current_data, sizeof(current_data));

        // If the sensor data has changed (Delta Transmission logic), 
        // we signal the mesh task to broadcast the new data.
        if (strcmp(current_data, g_last_sensor_data) != 0) {
            strncpy(g_last_sensor_data, current_data, sizeof(g_last_sensor_data) - 1);
            g_transmission_pending = true; 
        }
        
        // High frequency polling ensures responsiveness
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}
#endif

// ---------------------------------------------------------------------------
// TASK: mesh_task
// Runs at Priority 5 (Medium). Manages network state and periodic heartbeats.
// ---------------------------------------------------------------------------
static void mesh_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Mesh networking task started (Background Management)");
    int64_t last_send_time = 0;

    while (1) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        bool should_send = false;
        bool is_sensor_data = false;

        // 1. Peer Health Check: Remove nodes that haven't responded in a while
        mesh_manager_check_timeouts();

        // 2. Network Status Visualization:
        //    Update the status LED based on how many mesh peers are currently online.
        int total_known   = mesh_manager_get_device_count() - 1;
        int online_peers  = mesh_manager_get_online_peer_count();
        int ever_seen     = mesh_manager_get_ever_seen_count();

        if (total_known == 0) {
            // No peers in NVS: Factory Reset state or first boot
            status_indicator_set_state(LED_STATE_DISCONNECTED); // RED
        } else if (ever_seen == 0) {
            // Peers are in NVS but none have ever sent a heartbeat yet (just booted)
            // Show RED, not PARTIAL, to avoid false "partial mesh" alarm on cold start
            status_indicator_set_state(LED_STATE_DISCONNECTED); // RED
        } else if (online_peers == total_known) {
            // All expected peers are actively online
            status_indicator_set_state(LED_STATE_CONNECTED);    // SOLID GREEN
        } else {
            // At least one peer was seen before but is now offline (genuine partial mesh)
            status_indicator_set_state(LED_STATE_PARTIAL);      // BLINKING GREEN
        }

        // 3. Transmission Logic (Decide if we need to send a packet)
        if (g_transmission_pending) {
            // Priority 1: Instant send because sensor data changed
            should_send = true;
            is_sensor_data = true;
            g_transmission_pending = false;
        }
        else if ((now_ms - last_send_time) > MESH_KEEPALIVE_INTERVAL_MS) {
            // Priority 2: Periodic heartbeat to let others know we are still alive
            ESP_LOGI(TAG, "Periodic keepalive triggered");
            should_send = true;
            is_sensor_data = false;
        }

        // 4. Packet Execution
        if (should_send) {
            // Set the message body to our latest sensor string
            message_provider_set_message(g_last_sensor_data);
            
            uint8_t payload[256];
            // Format the full mesh packet (adds security, sequence numbers, and node name)
            size_t len = message_provider_get_next((char *)payload, sizeof(payload));

            if (len > 0) {
                broadcast_data(payload, len, is_sensor_data); 
                last_send_time = esp_timer_get_time() / 1000;
            }
        }

        // Background task runs less frequently to save CPU/Power
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ---------------------------------------------------------------------------
// Main Application Entry Point
// ---------------------------------------------------------------------------
void app_main(void)
{
    // --- Step 1: Hardware & Component Initialization ---
    status_indicator_configure();     // Setup the RGB Status LED
    
#if DEVICE_ROLE == ROLE_SENSOR || DEVICE_ROLE == ROLE_BOTH
    sensors_init();            // Setup GPIOs for inputs (buttons/sensors)
#endif

#if DEVICE_ROLE == ROLE_ACTUATOR || DEVICE_ROLE == ROLE_BOTH
    actuators_init();          // Setup GPIOs and RTOS Queue for outputs (LEDs/relays)
#endif

    // --- Step 2: Modular Device Reset Task ---
    // This initializes a background task that continuously monitors the
    // reset button (GPIO 1) and will trigger a wipe and reboot if held for 3s.
    device_reset_init();

    mesh_manager_init();       // Initialize the peer tracking table
    message_processor_init();  // Setup decryption and command handling
    message_provider_init(NODE_LABEL); // Initialize signing and identity

    // --- Step 2: Network Stack Initialization ---
    // Initialize WiFi and ESP-NOW with our local callback functions
    if (espnow_control_init(on_espnow_recv, on_espnow_send) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP-NOW control stack!");
        return;
    }
    
    // Register the broadcast address as a peer so we can send packets to everyone
    espnow_control_add_peer(broadcast_mac);

    // --- Step 3: Launch RTOS Tasks ---
#if DEVICE_ROLE == ROLE_SENSOR || DEVICE_ROLE == ROLE_BOTH
    // The sensor task polls inputs at high speed (Priority 10)
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 10, NULL);
#endif
    
    // The mesh task handles networking in the background (Priority 5)
    xTaskCreate(mesh_task, "mesh_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Mesh Node [%s] started successfully. Role: %d", NODE_LABEL, DEVICE_ROLE);
}
