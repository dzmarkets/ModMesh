#include "device_reset.h"
#include "shared_config.h"
#include "status_indicator.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "device_reset";

// Weak function declaration to allow main.cpp to handle network-wide reset
extern void device_reset_on_network_reset_requested(void) __attribute__((weak));

static void print_diagnostics(void) {
    status_indicator_set_state(LED_STATE_DIAGNOSTIC);
    ESP_LOGI(TAG, "================ DIAGNOSTICS ================");
    ESP_LOGI(TAG, "Node Type: Master (Bridge)");
    ESP_LOGI(TAG, "Modbus Baud: %d", MODBUS_BAUD_RATE);
    ESP_LOGI(TAG, "Wi-Fi Channel: %d", ESPNOW_WIFI_CHANNEL);
    ESP_LOGI(TAG, "Free Heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "===========================================");
    vTaskDelay(pdMS_TO_TICKS(500));
    status_indicator_set_state(LED_STATE_IDLE);
}

static void button_task(void *pvParameters)
{
    // Active low (pressed = 0)
    gpio_set_direction((gpio_num_t)FACTORY_RESET_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)FACTORY_RESET_GPIO, GPIO_PULLUP_ONLY);

    uint32_t press_duration = 0;
    bool is_pressed = false;
    uint32_t last_release_time = 0;
    uint8_t click_count = 0;
    bool warning_shown = false;

    while (1) {
        bool current_state = (gpio_get_level((gpio_num_t)FACTORY_RESET_GPIO) == 0);
        
        if (current_state && !is_pressed) {
            // Button just pressed
            is_pressed = true;
            press_duration = 0;
            
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_release_time < 400) {
                click_count++;
            } else {
                click_count = 1;
            }
        } else if (!current_state && is_pressed) {
            // Button just released
            is_pressed = false;
            last_release_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            if (press_duration < 1000 && click_count == 1) {
                // Wait to see if it's a double click
                vTaskDelay(pdMS_TO_TICKS(400));
                if (!is_pressed) {
                    ESP_LOGW(TAG, "Short press detected. Soft Rebooting...");
                    status_indicator_set_state(LED_STATE_MODBUS_RX); // Blue flash
                    vTaskDelay(pdMS_TO_TICKS(200));
                    esp_restart();
                }
            } else if (press_duration < 1000 && click_count == 2) {
                ESP_LOGI(TAG, "Double click detected. Printing diagnostics.");
                print_diagnostics();
                click_count = 0;
            } else if (press_duration >= 6000) {
                ESP_LOGW(TAG, "Long press released >= 6s. Executing Network-wide Factory Reset!");
                if (device_reset_on_network_reset_requested) {
                    device_reset_on_network_reset_requested();
                }
                vTaskDelay(pdMS_TO_TICKS(1000)); // Allow 1s for ESP-NOW packets to transmit
                nvs_flash_erase();
                esp_restart();
            } else if (press_duration >= 3000) {
                ESP_LOGW(TAG, "Long press released between 3s and 6s. Executing Local Factory Reset!");
                nvs_flash_erase();
                esp_restart();
            } else if (press_duration >= 1000) {
                // Aborted long press
                ESP_LOGI(TAG, "Long press aborted.");
                status_indicator_set_state(LED_STATE_IDLE);
            }
            warning_shown = false;
        }

        if (is_pressed) {
            press_duration += 10;
            
            if (press_duration >= 6000) {
                // Rapid Red/White flash
                if ((press_duration / 50) % 2 == 0) {
                    status_indicator_set_state(LED_STATE_ERROR); // Red
                } else {
                    status_indicator_set_state(LED_STATE_MODBUS_TX); // White
                }
            } else if (press_duration >= 3000) {
                // Rapid Red Flash
                if ((press_duration / 100) % 2 == 0) {
                    status_indicator_set_state(LED_STATE_ERROR);
                } else {
                    status_indicator_set_state(LED_STATE_OFF);
                }
            } else if (press_duration >= 1000 && !warning_shown) {
                // Warning Orange
                status_indicator_set_state(LED_STATE_WARNING);
                warning_shown = true;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms poll rate
    }
}

void device_reset_init(void)
{
    ESP_LOGI(TAG, "Initializing Smart Reset Button on GPIO %d", FACTORY_RESET_GPIO);
    xTaskCreate(button_task, "reset_btn_task", 4096, NULL, 5, NULL);
}
