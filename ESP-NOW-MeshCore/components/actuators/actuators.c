//
// File Path: components/actuators/actuators.c
// Brief:     Source file for actuators component.
//            Decouples physical output commands from the network task.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.4.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-07
//

#include "actuators.h"
#include "shared_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#if DEVICE_ROLE == ROLE_ACTUATOR || DEVICE_ROLE == ROLE_BOTH

static const char *TAG = "actuators";

// ---------------------------------------------------------------------------
// Decoupled RTOS Queue Definition
// ---------------------------------------------------------------------------
#define MAX_PAYLOAD_LEN 128
typedef struct {
    char payload[MAX_PAYLOAD_LEN];
} actuator_cmd_t;

static QueueHandle_t s_actuator_queue = NULL;

// Forward declaration of the internal processing function
static void process_command(const char *payload);

// ---------------------------------------------------------------------------
// Background Actuator Task (Priority 7)
// ---------------------------------------------------------------------------
static void actuator_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Actuator task started (Decoupled Queue Execution)");
    actuator_cmd_t cmd;

    while (1) {
        // Wait indefinitely until a command arrives in the queue
        if (xQueueReceive(s_actuator_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            process_command(cmd.payload);
        }
    }
}

void actuators_init(void)
{
#if ACTIVE_APP_SAMPLE == 1
    ESP_LOGI(TAG, "Initializing hardware actuators... (MOCK MODE)");
#elif ACTIVE_APP_SAMPLE == 2
    ESP_LOGI(TAG, "Initializing hardware actuators... (REMOTE LED MOMENTARY)");
    gpio_set_direction(REMOTE_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(REMOTE_LED_GPIO, 0); // Default OFF
#elif ACTIVE_APP_SAMPLE == 3
    ESP_LOGI(TAG, "Initializing hardware actuators... (REMOTE LED TOGGLE)");
    gpio_set_direction(REMOTE_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(REMOTE_LED_GPIO, 0); // Default OFF
#elif ACTIVE_APP_SAMPLE == 4
    ESP_LOGI(TAG, "Initializing hardware actuators... (4-NODE LEDS)");
    const int led_pins[4] = {LED1_GPIO, LED2_GPIO, LED3_GPIO, LED4_GPIO};
    for(int i = 0; i < 4; i++) {
        gpio_set_direction(led_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(led_pins[i], 0); // Default OFF
    }
#elif ACTIVE_APP_SAMPLE == 5
    ESP_LOGI(TAG, "Initializing hardware actuators... (MOD-BUS MODE)");
    // Initialize RE/DE pin for MAX485
    gpio_set_direction(MAX485_RE_DE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(MAX485_RE_DE_GPIO, 0); // Default to Receive

    // Initialize UART for MOD-BUS
    uart_config_t uart_config = {
        .baud_rate = MAX485_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(MAX485_UART_PORT, &uart_config);
    uart_set_pin(MAX485_UART_PORT, MAX485_TXD_GPIO, MAX485_RXD_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(MAX485_UART_PORT, 256 * 2, 0, 0, NULL, 0);
#endif

    // --- Decoupled RTOS Architecture Setup ---
    s_actuator_queue = xQueueCreate(10, sizeof(actuator_cmd_t));
    if (s_actuator_queue != NULL) {
        // Priority 7 sits perfectly between the sensor_task (10) and mesh_task (5)
        xTaskCreate(actuator_task, "actuator_task", 4096, NULL, 7, NULL);
    } else {
        ESP_LOGE(TAG, "Failed to create actuator queue!");
    }
}

void actuators_execute(const char *payload)
{
    if (!payload || !s_actuator_queue) return;

    actuator_cmd_t cmd;
    strncpy(cmd.payload, payload, MAX_PAYLOAD_LEN - 1);
    cmd.payload[MAX_PAYLOAD_LEN - 1] = '\0';

    // Push to queue immediately. Non-blocking (timeout 0) to ensure the radio 
    // interrupt task is never delayed, even if the actuator queue is full.
    if (xQueueSend(s_actuator_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Actuator queue full! Dropping command to protect radio task.");
    }
}

static void process_command(const char *payload)
{
    // Filter messages based on Keywords (Pub/Sub routing)
    bool keyword_match = false;
    
    // The payload format from message_provider is: "[NODE_A | MAC | SEQ] [LIGHT]CMD:LED_ON"
    // We must skip the first mesh network header [...] to find the application keywords.
    const char *app_payload = strchr(payload, ']');
    if (app_payload) {
        app_payload++; // Skip the closing bracket of the network header
        
        // Now look for the application keyword block e.g., "[LIGHT]"
        const char *keyword_start = strchr(app_payload, '[');
        if (keyword_start) {
            const char *keyword_end = strchr(keyword_start, ']');
            if (keyword_end) {
                size_t list_len = keyword_end - keyword_start - 1;
                char target_list[128];
                if (list_len < sizeof(target_list)) {
                    strncpy(target_list, keyword_start + 1, list_len);
                    target_list[list_len] = '\0';
                    
                    // Parse the comma-separated keywords in the payload
                    char *payload_token = strtok(target_list, ",");
                    while (payload_token != NULL) {
                        if (strcmp(payload_token, "ALL") == 0) {
                            keyword_match = true;
                            break;
                        }
                        
                        // Check if payload_token exists in our ACTUATOR_KEYWORDS
                        char my_keywords[128];
                        strncpy(my_keywords, ACTUATOR_KEYWORDS, sizeof(my_keywords));
                        char *my_token = strtok(my_keywords, ",");
                        while (my_token != NULL) {
                            if (strcmp(payload_token, my_token) == 0) {
                                keyword_match = true;
                                break;
                            }
                            my_token = strtok(NULL, ",");
                        }
                        
                        if (keyword_match) break;
                        payload_token = strtok(NULL, ",");
                    }
                }
            }
        }
    }
    
    if (!keyword_match) {
        ESP_LOGD(TAG, "Ignoring payload due to keyword mismatch: %s", payload);
        return;
    }

    // --- EMERGENCY NETWORK RESET ---
    if (strstr(payload, "CMD:NETWORK_RESET") != NULL) {
        ESP_LOGW(TAG, "⚡ NETWORK EMERGENCY RESET RECEIVED! Forcing all states to initial.");
        
        // 1. Reset Physical Actuators
#if ACTIVE_APP_SAMPLE == 2 || ACTIVE_APP_SAMPLE == 3
        gpio_set_level(REMOTE_LED_GPIO, 0);
#elif ACTIVE_APP_SAMPLE == 4
        gpio_set_level(LED1_GPIO, 0);
        gpio_set_level(LED2_GPIO, 0);
        gpio_set_level(LED3_GPIO, 0);
        gpio_set_level(LED4_GPIO, 0);
#endif

        // 2. Reset internal sensor software states to prevent re-broadcasting old data
        extern void sensors_force_initial_state(void);
        sensors_force_initial_state();
        
        return; // Halt further processing of this payload
    }

#if ACTIVE_APP_SAMPLE == 1
    // --- MOCK ACTUATOR LOGIC ---
    if (strstr(payload, "TEMP:25.0C") != NULL) {
        ESP_LOGW(TAG, "⚡ ACTUATOR TRIGGERED: Temperature is High! Turning ON Cooling Fan! (Mock)");
    } 
    else if (strstr(payload, "TEMP:24.5C") != NULL) {
        ESP_LOGI(TAG, "⚡ ACTUATOR TRIGGERED: Temperature is Normal. Turning OFF Cooling Fan. (Mock)");
    }
#elif ACTIVE_APP_SAMPLE == 2 || ACTIVE_APP_SAMPLE == 3
    // --- REMOTE POWERING LED ---
    if (strstr(payload, "CMD:LED_ON") != NULL) {
        ESP_LOGW(TAG, "⚡ ACTUATOR TRIGGERED: Remote LED ON!");
        gpio_set_level(REMOTE_LED_GPIO, 1);
    } 
    else if (strstr(payload, "CMD:LED_OFF") != NULL) {
        ESP_LOGI(TAG, "⚡ ACTUATOR TRIGGERED: Remote LED OFF!");
        gpio_set_level(REMOTE_LED_GPIO, 0);
    }
#elif ACTIVE_APP_SAMPLE == 4
    // --- 4-NODE REMOTE CONTROL ---
    int l1, l2, l3, l4;
    if (sscanf(payload, "CMD:LEDS:%d,%d,%d,%d", &l1, &l2, &l3, &l4) == 4) {
        gpio_set_level(LED1_GPIO, l1);
        gpio_set_level(LED2_GPIO, l2);
        gpio_set_level(LED3_GPIO, l3);
        gpio_set_level(LED4_GPIO, l4);
        ESP_LOGI(TAG, "⚡ ACTUATOR TRIGGERED: Set LEDs to %d,%d,%d,%d", l1, l2, l3, l4);
    }
#elif ACTIVE_APP_SAMPLE == 5
    // --- SEND RECEIVED DATA VIA MOD-BUS (MAX485) ---
    ESP_LOGW(TAG, "⚡ ACTUATOR TRIGGERED: Forwarding payload via MOD-BUS!");
    // Set RE/DE to HIGH (Transmit)
    gpio_set_level(MAX485_RE_DE_GPIO, 1);
    
    // Write data to UART
    uart_write_bytes(MAX485_UART_PORT, payload, strlen(payload));
    uart_write_bytes(MAX485_UART_PORT, "\r\n", 2);
    
    // Wait for transmission to finish, then set RE/DE to LOW (Receive)
    uart_wait_tx_done(MAX485_UART_PORT, 100);
    gpio_set_level(MAX485_RE_DE_GPIO, 0);
#endif
}

#endif // DEVICE_ROLE == ROLE_ACTUATOR || DEVICE_ROLE == ROLE_BOTH
