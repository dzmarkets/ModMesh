//
// File Path: ESP-NOW-MeshCore/components/device_reset/device_reset.c
// Brief:     Source file for the generic Device Reset component.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-05-04
// UpdateDate: 2026-05-05
//

#include "device_reset.h"
#include "shared_config.h"
#include "mesh_manager.h"
#include "status_indicator.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "device_reset";

static void device_reset_task(void *pvParameters)
{
    int hold_count = 0;
    const int required_holds = 30; // 30 * 100ms = 3000ms (3 seconds)

    while (1) {
        if (gpio_get_level(FACTORY_RESET_GPIO) == 0) {
            hold_count++;
            
            if (hold_count == 1) {
                ESP_LOGW(TAG, "Device Reset Button [GPIO %d] PRESSED! Hold for 3s...", FACTORY_RESET_GPIO);
            }

            if (hold_count >= required_holds) {
                ESP_LOGW(TAG, "Factory Reset Triggered!");

                // STEP 1: Visual Warning — Rapidly flash RED for 3 seconds
                // This gives the user a clear visual signal BEFORE anything is erased.
                for (int i = 0; i < 15; i++) {
                    status_indicator_set_state(LED_STATE_DISCONNECTED);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    status_indicator_set_state(LED_STATE_OFF);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }

                // STEP 2: Broadcast Emergency Reset to ALL other nodes
                // Tells all peers to turn off their outputs and wipe their software states
                // BEFORE this device leaves the network, ensuring a safe Zero-State.
                extern void main_broadcast_emergency_reset(void);
                main_broadcast_emergency_reset();
                vTaskDelay(pdMS_TO_TICKS(500)); // Wait for peers to receive and process it

                // STEP 3: Wipe local Mesh Identity from NVS
                mesh_manager_factory_reset();

                // STEP 4: Reboot to apply the clean state
                ESP_LOGW(TAG, "Rebooting device...");
                esp_restart();
            }
        } else {
            if (hold_count > 0 && hold_count < required_holds) {
                ESP_LOGI(TAG, "Reset cancelled. Button released early.");
            }
            hold_count = 0; // Reset counter if button is released
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100ms
    }
}

void device_reset_init(void)
{
    // Initialize the Reset Button GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FACTORY_RESET_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = USE_INTERNAL_PULLUPS ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Give the internal pull-up resistor a moment to stabilize the voltage level
    vTaskDelay(pdMS_TO_TICKS(50));

    // Create background task to monitor the button continuously
    // Stack set to 8192 bytes: the reset task calls into the full networking
    // stack (message_provider -> espnow_control) which requires deep stack space.
    xTaskCreate(device_reset_task, "device_reset_task", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "Device reset task initialized.");
}
