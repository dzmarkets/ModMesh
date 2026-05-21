#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "modbus_bridge.h"
#include "espnow_control.h"
#include "shared_config.h"
#include "status_indicator.h"
#include "device_reset.h"
#include "esp_mac.h"
#include "nvs_flash.h"

static const char *TAG = "SlaveNode1";

// ---------------------------------------------------------------------------
// Callback: Received a complete Modbus frame from the Sensor via RS-485 (Core 1)
// ---------------------------------------------------------------------------
void onModbusResponseReceived(const uint8_t* frame, size_t len) {
    if (len < 4) return;
    
    status_indicator_set_state(LED_STATE_MODBUS_RX); // BLUE
    
    ESP_LOGI(TAG, "Sensor response received (len: %d)", len);
    ESP_LOG_BUFFER_HEX("MODBUS_RX", frame, len);

    // Beaming back to Master Node over ESP-NOW
    status_indicator_set_state(LED_STATE_ESPNOW_TX); // YELLOW
    if (espnow_control_send(MASTER_NODE_MAC, frame, len)) {
        ESP_LOGI(TAG, "Successfully sent response back to Master Node");
    } else {
        ESP_LOGE(TAG, "Failed to send response back to Master Node");
    }
}

// ---------------------------------------------------------------------------
// Callback: Received a Modbus query from the Master Node via ESP-NOW (Core 0)
// ---------------------------------------------------------------------------
void onEspNowRequestReceived(const uint8_t* src_mac, const uint8_t* payload, size_t len) {
    status_indicator_set_state(LED_STATE_ESPNOW_RX); // CYAN
    
    ESP_LOGI(TAG, "Received ESP-NOW query from Master (len: %d)", len);
    ESP_LOG_BUFFER_HEX("ESPNOW_RX", payload, len);
    
    // Check sender is indeed Master Node
    if (memcmp(src_mac, MASTER_NODE_MAC, 6) != 0) {
        ESP_LOGW(TAG, "Received packet from unknown MAC: " MACSTR ". Ignored.", MAC2STR(src_mac));
        return;
    }

    // Check if this is a remote factory reset command
    if (len == REMOTE_RESET_LEN && memcmp(payload, REMOTE_RESET_SIGNATURE, REMOTE_RESET_LEN) == 0) {
        ESP_LOGE(TAG, "!!! RECEIVED CRITICAL REMOTE FACTORY RESET COMMAND !!!");
        status_indicator_set_state(LED_STATE_ERROR); // Solid Red
        vTaskDelay(pdMS_TO_TICKS(500)); // Delay to allow logs to flush and LED to hold
        nvs_flash_erase();
        esp_restart();
        return; // Unreachable
    }

    // Transmit over RS-485 to the physical sensor
    status_indicator_set_state(LED_STATE_MODBUS_TX); // WHITE
    modbus_bridge_transmit(payload, len);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting Slave Node 1 (Sensor Bridge)");

    // Initialize RGB LED
    status_indicator_configure();

    // Initialize Smart Reset Button
    device_reset_init();

    // Initialize the ESP-NOW Router (Core 0 tasks internally)
    if (!espnow_control_init(onEspNowRequestReceived)) {
        ESP_LOGE(TAG, "Failed to start ESP-NOW Router");
        status_indicator_set_state(LED_STATE_ERROR);
        return;
    }

    // Print our MAC Address so the user can configure the Master Node
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, " SlaveNode1 MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "===========================================");

    // Add Master Node as encrypted peer
    espnow_control_add_peer(MASTER_NODE_MAC);

    // Initialize the Modbus UART (Core 1 tasks internally)
    if (!modbus_bridge_init(onModbusResponseReceived)) {
        ESP_LOGE(TAG, "Failed to start Modbus UART");
        status_indicator_set_state(LED_STATE_ERROR);
        return;
    }

    ESP_LOGI(TAG, "Slave Node 1 initialized and listening.");
    
    // Main task can suspend, background tasks handle everything
    vTaskSuspend(NULL);
}
