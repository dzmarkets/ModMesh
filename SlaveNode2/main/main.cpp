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
#include "driver/gpio.h"

#define ACTUATOR_LED_GPIO 6

static const char *TAG = "SlaveNode2";

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
        ESP_LOGE(TAG, "!!! FACTORY RESET PROCESS STARTED - BLINKING LED FOR 3 SECONDS !!!");
        for (int i = 0; i < 30; i++) {
            status_indicator_set_state(LED_STATE_ERROR); // RED
            vTaskDelay(pdMS_TO_TICKS(50));
            status_indicator_set_state(LED_STATE_OFF);   // OFF
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        ESP_LOGE(TAG, "!!! WIPING NVS FLASH AND REBOOTING NOW !!!");
        nvs_flash_erase();
        esp_restart();
        return; // Unreachable
    }

    // Process Modbus Actuator logic
    // Expected Slave ID: 0x02
    if (len >= 4 && payload[0] == 2) {
        uint8_t func_code = payload[1];
        bool led_state_changed = false;
        static int current_led_val = 0;
        uint8_t response[32];
        size_t resp_len = 0;

        if (func_code == 0x05) { // Write Single Coil
            if (len >= 8) {
                uint16_t coil_val = (payload[4] << 8) | payload[5];
                if (coil_val == 0xFF00) {
                    current_led_val = 1;
                    led_state_changed = true;
                } else if (coil_val == 0x0000) {
                    current_led_val = 0;
                    led_state_changed = true;
                }
                
                // Echo the request as response (standard for FC 05)
                memcpy(response, payload, 8);
                resp_len = 8;
            }
        } 
        else if (func_code == 0x06) { // Write Single Register
            if (len >= 8) {
                uint16_t reg_val = (payload[4] << 8) | payload[5];
                current_led_val = (reg_val > 0) ? 1 : 0;
                led_state_changed = true;

                // Echo the request as response (standard for FC 06)
                memcpy(response, payload, 8);
                resp_len = 8;
            }
        }
        else if (func_code == 0x10) { // Write Multiple Holding Registers
            if (len >= 11) {
                uint16_t reg_val = (payload[7] << 8) | payload[8];
                current_led_val = (reg_val > 0) ? 1 : 0;
                led_state_changed = true;

                // Response format: [SlaveID, FuncCode, RegAddrHi, RegAddrLo, QtyHi, QtyLo, CRCHi, CRCLo] (8 bytes)
                memcpy(response, payload, 6);
                uint16_t crc = modbus_crc16(response, 6);
                response[6] = crc & 0xFF;
                response[7] = (crc >> 8) & 0xFF;
                resp_len = 8;
            }
        }
        else if (func_code == 0x01) { // Read Coils
            if (len >= 8) {
                // Response format: [SlaveID, FuncCode, ByteCount(1), DataByte, CRCHi, CRCLo] (6 bytes)
                response[0] = 2;
                response[1] = 1;
                response[2] = 1;
                response[3] = current_led_val ? 0x01 : 0x00;
                uint16_t crc = modbus_crc16(response, 4);
                response[4] = crc & 0xFF;
                response[5] = (crc >> 8) & 0xFF;
                resp_len = 6;
            }
        }
        else if (func_code == 0x03) { // Read Holding Registers
            if (len >= 8) {
                // Response format: [SlaveID, FuncCode, ByteCount(2), DataHi, DataLo, CRCHi, CRCLo] (7 bytes)
                response[0] = 2;
                response[1] = 3;
                response[2] = 2;
                response[3] = 0;
                response[4] = current_led_val ? 1 : 0;
                uint16_t crc = modbus_crc16(response, 5);
                response[5] = crc & 0xFF;
                response[6] = (crc >> 8) & 0xFF;
                resp_len = 7;
            }
        }

        if (led_state_changed) {
            gpio_set_level((gpio_num_t)ACTUATOR_LED_GPIO, current_led_val);
            ESP_LOGW(TAG, "Actuator LED [GPIO %d] set to %s", ACTUATOR_LED_GPIO, current_led_val ? "ON" : "OFF");
        }

        if (resp_len > 0) {
            status_indicator_set_state(LED_STATE_ESPNOW_TX); // YELLOW
            vTaskDelay(pdMS_TO_TICKS(10)); // Tiny delay for stability
            espnow_control_send(MASTER_NODE_MAC, response, resp_len);
            ESP_LOGI(TAG, "Sent Modbus response back to Master (len: %d)", resp_len);
            ESP_LOG_BUFFER_HEX("ESPNOW_TX", response, resp_len);
        }
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting Slave Node 2 (Sensor Bridge / Actuator)");

    // Initialize RGB LED
    status_indicator_configure();

    // Initialize Actuator LED GPIO
    gpio_reset_pin((gpio_num_t)ACTUATOR_LED_GPIO);
    gpio_set_direction((gpio_num_t)ACTUATOR_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)ACTUATOR_LED_GPIO, 0); // Initially OFF

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
    ESP_LOGI(TAG, " SlaveNode2 MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
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

    ESP_LOGI(TAG, "Slave Node 2 initialized as Actuator and listening.");
    
    // Main task can suspend, background tasks handle everything
    vTaskSuspend(NULL);
}
