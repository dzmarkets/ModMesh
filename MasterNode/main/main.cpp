#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "modbus_bridge.h"
#include "espnow_control.h"
#include "shared_config.h"
#include "status_indicator.h"
#include "device_reset.h"
#include "esp_mac.h"

static const char *TAG = "MasterNode";

// ---------------------------------------------------------------------------
// Modbus CRC16 Calculation Helper
// ---------------------------------------------------------------------------
static uint16_t modbus_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ---------------------------------------------------------------------------
// Transaction tracking state to allow immediate Exception Responses
// ---------------------------------------------------------------------------
struct PendingModbusRequest {
    volatile bool active;
    uint8_t slave_id;
    uint8_t function_code;
    uint8_t dest_mac[6];
};
static PendingModbusRequest s_pending_request = { false, 0, 0, {0} };

// ---------------------------------------------------------------------------
// Callback: Received a complete Modbus frame from the PLC via RS-485
// Runs on Core 1 context
// ---------------------------------------------------------------------------
void onModbusRequestReceived(const uint8_t* frame, size_t len) {
    if (len < 4) return;
    
    status_indicator_set_state(LED_STATE_MODBUS_RX);
    
    uint8_t slave_id = frame[0];
    ESP_LOGI(TAG, "PLC requested Slave ID %d (len: %d)", slave_id, len);
    ESP_LOG_BUFFER_HEX("MODBUS_RX", frame, len);

    if (slave_id == 1 || slave_id == 2) {
        status_indicator_set_state(LED_STATE_ESPNOW_TX);
        s_pending_request.slave_id = slave_id;
        s_pending_request.function_code = frame[1];
        s_pending_request.active = true;

        if (slave_id == 1) {
            memcpy((void*)s_pending_request.dest_mac, SLAVE_NODE_1_MAC, 6);
            espnow_control_send(SLAVE_NODE_1_MAC, frame, len);
        } else {
            memcpy((void*)s_pending_request.dest_mac, SLAVE_NODE_2_MAC, 6);
            espnow_control_send(SLAVE_NODE_2_MAC, frame, len);
        }
    } else {
        ESP_LOGW(TAG, "Unknown Slave ID: %d", slave_id);
    }
}

// ---------------------------------------------------------------------------
// Callback: Received a Modbus response from a Slave Node via ESP-NOW
// Runs on Core 0 context
// ---------------------------------------------------------------------------
void onEspNowResponseReceived(const uint8_t* src_mac, const uint8_t* payload, size_t len) {
    status_indicator_set_state(LED_STATE_ESPNOW_RX);
    
    ESP_LOGI(TAG, "Received ESP-NOW response from Slave, routing to PLC (len: %d)...", len);
    ESP_LOG_BUFFER_HEX("ESPNOW_RX", payload, len);
    
    // Clear pending transaction
    s_pending_request.active = false;
    
    // Transmit back to PLC over RS-485
    status_indicator_set_state(LED_STATE_MODBUS_TX);
    modbus_bridge_transmit(payload, len);
}

// ---------------------------------------------------------------------------
// Callback: ESP-NOW Transmission Status received (Runs on Core 0 context)
// ---------------------------------------------------------------------------
void onEspNowSendStatusReceived(const uint8_t* dest_mac, bool success) {
    if (!success) {
        if (s_pending_request.active && memcmp((const void*)s_pending_request.dest_mac, dest_mac, 6) == 0) {
            // Log which specific Slave Node went offline/unreachable
            if (memcmp(dest_mac, SLAVE_NODE_1_MAC, 6) == 0) {
                ESP_LOGE(TAG, "!!! Slave Node 1 (" MACSTR ") is OFFLINE / UNREACHABLE !!!", MAC2STR(dest_mac));
            } else if (memcmp(dest_mac, SLAVE_NODE_2_MAC, 6) == 0) {
                ESP_LOGE(TAG, "!!! Slave Node 2 (" MACSTR ") is OFFLINE / UNREACHABLE !!!", MAC2STR(dest_mac));
            } else {
                ESP_LOGE(TAG, "!!! Unknown Slave Node (" MACSTR ") is OFFLINE / UNREACHABLE !!!", MAC2STR(dest_mac));
            }
            
            ESP_LOGW(TAG, "Immediately returning Modbus exception 0x0B (Gateway Target Device Failed to Respond) to PLC");
            
            // Build Exception Frame: [SlaveID, FuncCode|0x80, ExceptionCode (0x0B), CRCLo, CRCHi]
            uint8_t exception_frame[5];
            exception_frame[0] = s_pending_request.slave_id;
            exception_frame[1] = s_pending_request.function_code | 0x80;
            exception_frame[2] = 0x0B; // Gateway Target Device Failed to Respond
            
            uint16_t crc = modbus_crc16(exception_frame, 3);
            exception_frame[3] = crc & 0xFF;
            exception_frame[4] = (crc >> 8) & 0xFF;
            
            // Clear pending transaction
            s_pending_request.active = false;
            
            // Transmit exception immediately back to PLC
            status_indicator_set_state(LED_STATE_MODBUS_TX);
            modbus_bridge_transmit(exception_frame, sizeof(exception_frame));
            ESP_LOGI(TAG, "Exception frame sent to PLC");
            ESP_LOG_BUFFER_HEX("MODBUS_TX_EXCEPTION", exception_frame, sizeof(exception_frame));
        }
    }
}

// ---------------------------------------------------------------------------
// Callback: Reset button held >= 6 seconds. Broadcasts reset to all Slaves.
// ---------------------------------------------------------------------------
extern "C" void device_reset_on_network_reset_requested(void) {
    ESP_LOGW(TAG, "!!! BROADCASTING REMOTE FACTORY RESET TO ALL SLAVE NODES !!!");
    status_indicator_set_state(LED_STATE_WARNING);

    // Send to Slave Node 1
    if (espnow_control_send(SLAVE_NODE_1_MAC, REMOTE_RESET_SIGNATURE, REMOTE_RESET_LEN)) {
        ESP_LOGI(TAG, "Successfully transmitted remote reset signal to Slave 1");
    } else {
        ESP_LOGE(TAG, "Failed to transmit remote reset signal to Slave 1");
    }

    // Send to Slave Node 2
    if (espnow_control_send(SLAVE_NODE_2_MAC, REMOTE_RESET_SIGNATURE, REMOTE_RESET_LEN)) {
        ESP_LOGI(TAG, "Successfully transmitted remote reset signal to Slave 2");
    } else {
        ESP_LOGE(TAG, "Failed to transmit remote reset signal to Slave 2");
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting Master Node (Bridge Mode)");

    // Initialize RGB LED
    status_indicator_configure();

    // Initialize Smart Reset Button
    device_reset_init();

    // Initialize the ESP-NOW Router (Core 0 tasks internally)
    if (!espnow_control_init(onEspNowResponseReceived)) {
        ESP_LOGE(TAG, "Failed to start ESP-NOW Router");
        status_indicator_set_state(LED_STATE_ERROR);
        return;
    }

    // Register the send status callback
    espnow_control_register_send_cb(onEspNowSendStatusReceived);

    // Print our MAC Address so the user can configure the Slaves
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, " MasterNode MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "===========================================");

    // Add Slave Nodes as encrypted peers
    espnow_control_add_peer(SLAVE_NODE_1_MAC);
    espnow_control_add_peer(SLAVE_NODE_2_MAC);

    // Initialize the Modbus UART (Core 1 tasks internally)
    if (!modbus_bridge_init(onModbusRequestReceived)) {
        ESP_LOGE(TAG, "Failed to start Modbus UART");
        status_indicator_set_state(LED_STATE_ERROR);
        return;
    }

    ESP_LOGI(TAG, "Master Node initialized and listening.");
    
    // Main task can suspend, background tasks handle everything
    vTaskSuspend(NULL);
}
